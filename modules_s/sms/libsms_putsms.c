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

/*
char message[500];
int is_binary;  // 1 is binary file
int udh;        // disable UDH bit
int messagelen; // length of message
char filename[500];
char logfile[256]={0};
int loglevel=9;
char to[50];
int cs_convert;
int report;
*/


void cut_ctrl(char* message) /* removes all ctrl chars */
{
  char tmp[500];
  int posdest=0;
  int possource;
  int count;
  count=strlen(message);
  for (possource=0; possource<=count; possource++)
  {
    if ((message[possource]>=' ') || (message[possource]==0))
      tmp[posdest++]=message[possource];
  }
  strcpy(message,tmp);
}

#ifdef old
void parsearguments(int argc,char** argv)
{
  int result;
  char tmp[500];
  
  /* set default values */
  strcpy(device,"/dev/ttyS0");
  strcpy(mode,"new");
  pin[0]=0;
  filename[0]=0;
  smsc[0]=0;
  message[0]=0;
  to[0]=0;
  cs_convert=0;
  baudrate=19200;
  is_binary=0;
  initstring[0]=0;
  report=0;
  errorsleeptime=10;
  modemname[0]=0;
  smsc[0]=0;
  udh=1;
  /* parse arguments */
  do
  {
    result=getopt(argc,argv,"n:b:l:L:ce:rhi:d:p:m:s:f:F:V:u");
    switch (result)
    {
      case 'h': help();
                break;
      case 'b': baudrate=atoi(optarg);
                break;
      case 'c': cs_convert=1;
                break;
      case 'e': errorsleeptime=atoi(optarg);
      		break;
      case 'r': report=1;
    		break;
      case 'u': udh=0;
    		break;
      case 'd': strcpy(device,optarg);
                break;
      case 'p': strcpy(pin,optarg);
                break;
      case 'l': strcpy(logfile,optarg);
                break;
      case 'L': loglevel=atoi(optarg);
                break;
      case 'm': strcpy(mode,optarg);
                break;
      case 'n': strcpy(modemname,optarg);
                break;
      case 's': strcpy(smsc,optarg);
                break;
      case 'f': strcpy(filename,optarg);
    		is_binary=0;
                break;
      case 'i': strcpy(initstring,optarg);
      	        strcat(initstring,"\r");
		break;
      case 'F': strcpy(filename,optarg);
    		is_binary=1;
		break;
      case 'V': printf("Version %s, Copyright (c) 2000-2002 by Stefan Frings, s.frings@mail.isis.de\n",putsms_version);
                exit(0);
    }
  }
  while (result>0);
  
  switch (baudrate)
  {
    case 300:    baudrate=B300; break;
    case 1200:   baudrate=B1200; break;
    case 2400:   baudrate=B2400; break;
    case 9600:   baudrate=B9600; break;
    case 19200:  baudrate=B19200; break;
    case 38400:  baudrate=B38400; break;
#ifdef B57600    
    case 57600:  baudrate=B57600; break;
#endif
#ifdef B115200
    case 115200: baudrate=B115200; break;
#endif
#ifdef B230400
    case 230400: baudrate=B230400; break;
#endif
    default: writelogfile(LOG_ERR,"Baudrate not supported"); exit(1);
  }    
  
  if (modemname[0]==0)
    strcpy(modemname,device);

  /* parse number and text  */
  if (optind==(argc-2)) /* number and text as arguments defined */
  {
    if (filename[0])
    {
      fprintf(stderr,"Message as file AND as argument specified.\n");
      exit(5);
    }
    strcpy(message,argv[optind+1]);
    strcpy(to,argv[optind]);
  }
  else if (optind==(argc-1)) /* Only number as text defined */
    strcpy(to,argv[optind]);

  loadmessagefile();

  /* Check number and text*/
  if (message[0]==0)
  {
    writelogfile(LOG_ERR,"You did not specify a message or a message file");
    exit(5);
  }
  if (to[0]==0)
  {
    writelogfile(LOG_ERR,"You did not specify a destination or a message file");
    exit(5);
  }
  /* Check if binary file allowed */
  if (is_binary && (strcmp(mode,"ascii")==0))
  {
    writelogfile(LOG_ERR,"Binary files are not allowd in ascii mode");
    exit(5);
  }
}
#endif




void swapchars(char* string) /* Swaps every second character */
{
	int Length;
	int position;
	char c;

	Length=strlen(string);
	for (position=0; position<Length-1; position+=2)
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
	int coding;
	int flags;
	int msg_len;
	char tmp[500];

	msg_len = strlen(msg->text);
	strcpy(tmp,msg->to);
	// terminate the number with F if the length is odd
	if (strlen(tmp)%2)
		strcat(tmp,"F");
	// Swap every second character
	swapchars(tmp);
	flags=1; // SMS-Sumbit MS to SMSC
	coding=240+1; // Dummy + Class 1
	if (msg->is_binary)
	{
		coding+=4; // 8 Bit
		if (msg->udh)
			flags+=64; // User Data Header
	}
	if (mdm->mode==MODE_OLD)
		flags+=16; // Validity field
	if (mdm->report)
		flags+=32; /* Request Status Report */
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
					ERROR_SLEEP_TIME);
				sleep(ERROR_SLEEP_TIME);
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

