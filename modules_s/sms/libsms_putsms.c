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

	memcpy(tmp,msg->to,msg->to_len);
	foo = msg->to_len;
	tmp[foo] = 0;
	// terminate the number with F if the length is odd
	if ( foo%2 ) {
		tmp[foo]='F';
		tmp[++foo] = 0;
	}
	// Swap every second character
	swapchars(tmp,foo);
	flags=1; // SMS-Sumbit MS to SMSC
	coding=240+1; // Dummy + Class 1
	if (msg->is_binary)
	{
		coding+=4; // 8 Bit
		if (msg->udh)
			flags+=64; // User Data Header
	}
	if (mdm->mode!=MODE_OLD)
		flags+=16; // Validity field
	/* concatenate the first part of the PDU string */
	if (mdm->mode==MODE_OLD)
		pdu_len += sprintf(pdu,"%02X00%02X91%s00%02X%02X",flags,
			msg->to_len,tmp,coding,msg->text_len);
	else
		pdu_len += sprintf(pdu,"00%02X00%02X91%s00%02XA7%02X",flags,
			msg->to_len,tmp,coding,msg->text_len);
	/* Create the PDU string of the message */
	if (msg->is_binary)
		pdu_len += binary2pdu(msg->text,msg->text_len,pdu+pdu_len);
	else
		pdu_len += ascii2pdu(msg->text,msg->text_len,pdu+pdu_len,
			msg->cs_convert);
	/* concatenate the text to the PDU string */
	return pdu_len;
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

	pdu_len = make_pdu(sms_messg, mdm, pdu);
	if (mdm->mode==MODE_OLD)
		clen = sprintf(command,"AT+CMGS=%i\r",pdu_len/2);
	else if (mdm->mode==MODE_ASCII)
		clen = sprintf(command,"AT+CMGS=\"+%.*s\"\r",sms_messg->to_len,
			sms_messg->to);
	else
		clen = sprintf(command,"AT+CMGS=%i\r",pdu_len/2-1);

	if (mdm->mode==MODE_ASCII)
		clen2=sprintf(command2,"%.*s\x1A",sms_messg->text_len,sms_messg->text);
	else
		clen2=sprintf(command2,"%.*s\x1A",pdu_len,pdu);

	for(err_code=0,retries=0;err_code<2 && retries<10; retries++)
	{
		if (put_command(mdm->fd,command,clen,answer,sizeof(answer),50,0)
		&& put_command(mdm->fd,command2,clen2,answer,sizeof(answer),300,0)
		&& !strstr(answer,"ERROR") )
		{
			/* no error during sending and the modem didn't said error */
			if (!strstr(answer,"OK"))
				LOG(L_WARN,"WARNING: putsms: Maybe could not send message,"
					" modem did not confirm submission.\n");
			err_code = 2;
		} else {
			/* we have an error */
			if (checkmodem(mdm)!=0) {
				err_code = 0;
				LOG(L_WARN,"WARNING: putsms: resending last sms! \n");
			} else if (err_code==0) {
				LOG(L_WARN,"WARNING: putsms :possible corrupted sms."
					" Let's try again!\n");
				err_code = 1;
			}else {
				LOG(L_ERR,"ERROR: We have a FUBAR sms!! drop it!\n");
				err_code = 3;
			}
		}
	}

	if (err_code==0)
		LOG(L_WARN,"WARNNING: something spuky is going on with the modem!"
			" Re-inited and tried fro 10 times without success!\n");
	return (err_code==0?-2:(err_code==3?-1:1));
}

