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
void ascii2pdu(char* ascii,char* pdu, int cs_convert)
{
	static char tmp[500];
	char octett[10];
	int pdubitposition=0;
	int pdubyteposition=0;
	int asciiLength;
	int character;
	int bit;
	int pdubitnr;
	char converted;

	asciiLength=strlen(ascii);
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
	pdu[0]=0;
	for (character=0;character<=pdubyteposition; character++)
	{
		sprintf(octett,"%02X",(unsigned char) tmp[character]);
		strcat(pdu,octett);
	}
}




/* Create a HEX Dump */
void binary2pdu(char* binary, int length, char* pdu)
{
	int character;
	char octett[10];

	pdu[0]=0;
	for (character=0;character<length; character++)
	{
		sprintf(octett,"%02X",(unsigned char) binary[character]);
		strcat(pdu,octett);
	}
}




/* make the PDU string. The destination variable pdu has to be big enough. */
void make_pdu(struct sms_msg *msg, struct modem *mdm, char* pdu)
{
	int  coding;
	int  flags;
	int  msg_len;
	char tmp[500];
	int  foo;

	msg_len = strlen(msg->text);
	strcpy(tmp,msg->to);
	// terminate the number with F if the length is odd
	if ( (foo=strlen(tmp))%2 ) {
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
		sprintf(pdu,"%02X00%02X91%s00%02X%02X",flags,strlen(msg->to),tmp,
			coding,msg_len);
	else
		sprintf(pdu,"00%02X00%02X91%s00%02XA7%02X",flags,strlen(msg->to),
			tmp,coding,msg_len);
	/* Create the PDU string of the message */
	if (msg->is_binary)
		binary2pdu(msg->text,msg_len,tmp);
	else
		ascii2pdu(msg->text,tmp,msg->cs_convert);
	/* concatenate the text to the PDU string */
	strcat(pdu,tmp);
}




/* send sms */
int putsms( struct sms_msg *sms_messg, struct modem *mdm)
{
	char command[500];
	char command2[500];
	char answer[500];
	char pdu[500];
	int retries;

	make_pdu(sms_messg, mdm, pdu);
	if (mdm->mode==MODE_OLD)
		sprintf(command,"AT+CMGS=%i\r",strlen(pdu)/2);
	else if (mdm->mode==MODE_ASCII)
		sprintf(command,"AT+CMGS=\"+%s\"\r",sms_messg->to);
		// for Siemens M20
		// sprintf(command,"AT+CMGS=\"%s\",129,\r",to);
	else
		sprintf(command,"AT+CMGS=%i\r",strlen(pdu)/2-1);

	if (mdm->mode==MODE_ASCII)
		sprintf(command2,"%s\x1A",sms_messg->text);  
	else
		sprintf(command2,"%s\x1A",pdu);

	retries=0;
	while (1)
	{
		retries+=1;
		put_command(mdm->fd,command,answer,sizeof(answer),50,0);
		put_command(mdm->fd,command2,answer,sizeof(answer),300,0);
		if (strstr(answer,"ERROR"))
		{
			LOG(L_ERR,"ERROR: putsms: Uups, the modem said ERROR.\n");
			tcsetattr(mdm->fd,TCSANOW,&(mdm->oldtio));
			if (retries<2)
			{
				LOG(L_ERR,"ERROR: putsms: trying again in %i sec.",
					mdm->retry);
				sleep(mdm->retry);
				//the next line is a workaround for an unknown buggy gsm modem
				put_command(mdm->fd,"\r\x1A\r",answer,sizeof(answer),10,0);
				sleep(1);
			} else
				return -1;
		} else {
			if (!strstr(answer,"OK"))
				LOG(L_WARN,"WARNING: putsms: Maybe could not send message,"
					" modem did not confirm submission.");
			return 0;
		}
	}
}

