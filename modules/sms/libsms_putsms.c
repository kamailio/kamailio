/*
SMS Server Tools
Copyright (C) 2000-2002 Stefan Frings

This program is free software unless you got it under another license directly
from the author. You can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation.
Either version 2 of the License, or (at your option) any later version.

http://www.isis.de/members/~s.frings
mailto:s.frings@mail.isis.de
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include "sms_funcs.h"
#include "libsms_charset.h"
#include "libsms_modem.h"



static char hexa[16] = {
	'0','1','2','3','4','5','6','7',
	'8','9','A','B','C','D','E','F'
	};


void swapchars(char* string, int len) /* Swaps every second character */
{
	int position;
	char c;

	for (position=0; position<len-1; position+=2)
	{
		c=string[position];
		string[position]=string[position+1];
		string[position+1]=c;
	}
}




/* Work with the complex bit building to generate a 7 bit PDU string
   encapsulated in 8 bit */
int ascii2pdu(char* ascii, int asciiLength, char* pdu, int cs_convert)
{
	static char tmp[500];
	int pdubitposition=0;
	int pdubyteposition=0;
	int character;
	int bit;
	int pdubitnr;
	char converted;
	unsigned char foo;

	memset(tmp,0,asciiLength);
	for (character=0;character<asciiLength;character++)
	{
		if (cs_convert)
			converted=ascii2sms(ascii[character]);
		else
			converted=ascii[character];
		for (bit=0;bit<7;bit++)
		{
			pdubitnr=7*character+bit;
			pdubyteposition=pdubitnr/8;
			pdubitposition=pdubitnr%8;
			if (converted & (1<<bit))
				tmp[pdubyteposition]=tmp[pdubyteposition]|(1<<pdubitposition);
			else
				tmp[pdubyteposition]=tmp[pdubyteposition]&~(1<<pdubitposition);
		}
	}
	tmp[pdubyteposition+1]=0;
	for (character=0;character<=pdubyteposition; character++)
	{
		foo = tmp[character] ;
		pdu[2*character  ] = hexa[foo>>4];
		pdu[2*character+1] = hexa[foo&0x0f];
	}
	pdu[2*(pdubyteposition+1)]=0;
	return 2*(pdubyteposition+1);
}




/* Create a HEX Dump */
int binary2pdu(char* binary, int length, char* pdu)
{
	int character;
	unsigned char foo;

	for (character=0;character<length; character++)
	{
		foo = binary[character];
		pdu[2*character  ] = hexa[foo>>4];
		pdu[2*character+1] = hexa[foo&0x0f];
	}
	pdu[2*length]=0;
	return 2*length;
}




/* make the PDU string. The destination variable pdu has to be big enough. */
int make_pdu(struct sms_msg *msg, struct modem *mdm, char* pdu)
{
	int  coding;
	int  flags;
	char tmp[500];
	int  pdu_len=0;
	int  foo;

	memcpy(tmp,msg->to.s,msg->to.len);
	foo = msg->to.len;
	tmp[foo] = 0;
	// terminate the number with F if the length is odd
	if ( foo%2 ) {
		tmp[foo]='F';
		tmp[++foo] = 0;
	}
	// Swap every second character
	swapchars(tmp,foo);
	flags = 0x01;   /* SMS-Submit MS to SMSC */
	if (sms_report_type!=NO_REPORT)
		flags |= 0x20 ; /* status report request */
	coding=240+1; // Dummy + Class 1
	if (mdm->mode!=MODE_OLD)
		flags+=16; // Validity field
	/* concatenate the first part of the PDU string */
	if (mdm->mode==MODE_OLD)
		pdu_len += sprintf(pdu,"%02X00%02X91%s00%02X%02X",flags,
			msg->to.len,tmp,coding,msg->text.len);
	else
		pdu_len += sprintf(pdu,"00%02X00%02X91%s00%02XA7%02X",flags,
			msg->to.len,tmp,coding,msg->text.len);
	/* Create the PDU string of the message */
	/* pdu_len += binary2pdu(msg->text.s,msg->text.len,pdu+pdu_len); */
	pdu_len += ascii2pdu(msg->text.s,msg->text.len,pdu+pdu_len,1/*convert*/);
	/* concatenate the text to the PDU string */
	return pdu_len;
}




/* search into modem reply for the sms id */
static inline int fetch_sms_id(char *answer)
{
	char *p;
	int  id;

	p = strstr(answer,"+CMGS:");
	if (!p)
		goto error;
	p += 6;
	/* parse to the first digit */
	while(p && *p && (*p==' ' || *p=='\r' || *p=='\n'))
		p++;
	if (*p<'0' || *p>'9')
		goto error;
	/* convert the number*/
	id = 0;
	while (p && *p>='0' && *p<='9')
		id = id*10 + *(p++)-'0';

	return id;
error:
	return -1;
}




/* send sms */
int putsms( struct sms_msg *sms_messg, struct modem *mdm)
{
	char command[500];
	char command2[500];
	char answer[500];
	char pdu[500];
	int clen,clen2;
	int retries;
	int err_code;
	int pdu_len;
	int sms_id;

	pdu_len = make_pdu(sms_messg, mdm, pdu);
	if (mdm->mode==MODE_OLD)
		clen = sprintf(command,"AT+CMGS=%i\r",pdu_len/2);
	else if (mdm->mode==MODE_ASCII)
		clen = sprintf(command,"AT+CMGS=\"+%.*s\"\r",sms_messg->to.len,
			sms_messg->to.s);
	else
		clen = sprintf(command,"AT+CMGS=%i\r",pdu_len/2-1);

	if (mdm->mode==MODE_ASCII)
		clen2=sprintf(command2,"%.*s\x1A",sms_messg->text.len,
		sms_messg->text.s);
	else
		clen2=sprintf(command2,"%.*s\x1A",pdu_len,pdu);

	sms_id = 0;
	for(err_code=0,retries=0;err_code<2 && retries<mdm->retry; retries++)
	{
		if (put_command(mdm,command,clen,answer,sizeof(answer),50,"\r\n> ")
		&& put_command(mdm,command2,clen2,answer,sizeof(answer),1000,0)
		&& strstr(answer,"OK") )
		{
			/* no error during sending and the modem said OK */
			err_code = 2;
			/* if reports were request, we have to fetch the sms id from
			the modem reply to keep trace of the status reports */
			if (sms_report_type!=NO_REPORT) {
				sms_id = fetch_sms_id(answer);
				if (sms_id==-1)
					err_code = 1;
			}
		} else {
			/* we have an error */
			if (checkmodem(mdm)==-1) {
				err_code = 0;
				LM_WARN("resending last sms! \n");
			} else if (err_code==0) {
				LM_WARN("possible corrupted sms. Let's try again!\n");
				err_code = 1;
			}else {
				LM_ERR("We have a FUBAR sms!! drop it!\n");
				err_code = 3;
			}
		}
	}

	if (err_code==0)
		LM_WARN("something spooky is going on with the modem!"
			" Re-inited and re-tried for %d times without success!\n",
			mdm->retry);
	return (err_code==0?-2:(err_code==2?sms_id:-1));
}

