/*
SMS Server Tools
Copyright (C) 2000 Stefan Frings

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
#include <sys/ioctl.h>
#include "libsms_modem.h"
#include "../../dprint.h"




int put_command(int fd, char* command, int clen, char* answer, int max,
													int timeout,char* expect)
{
	int count=0;
	int readcount;
	int toread;
	char tmp[100];
	int timeoutcounter=0;
	int found=0;
	int available;
	int status;

	if (command==0 || command[0]==0 || clen==0) {
		LOG(L_ERR,"ERROR:put_command: NULL comand received! \n");
		return 0;
	}

	ioctl(fd,TIOCMGET,&status);
	while (!(status & TIOCM_CTS))
	{
		usleep(100000);
		timeoutcounter++;
		ioctl(fd,TIOCMGET,&status);
		if (timeoutcounter>=timeout) {
			LOG(L_INFO,"INFO:put_command: Modem is not clear to send\n");
			return 0;
		}
	}

	DBG("DEBUG: put_command: -<%d>-->[%.*s] \n",clen,clen,command);
	write(fd,command,clen);
	tcdrain(fd);

	answer[0]=0;
	do
	{
		// try to read some bytes.
		ioctl(fd,FIONREAD,&available);
		// how many bytes are available to read?
		if (available<1)  // if 0 then wait a little bit and retry
		{
			//DBG("nothing to read-> wait\n");
			usleep(100000);
			timeoutcounter++;
			ioctl(fd,FIONREAD,&available);
		}
		if (available>0)
		{
			/* How many bytes do I wan t to read maximum? 
			Not more than tmp buffer size. */
			toread=max-count-1;
			if (toread>sizeof(tmp)-1)
				toread=sizeof(tmp)-1;
			// And how many bytes are available?
			if (available<toread)
				toread=available;
			//DBG("available=%d , reading %d bytes!\n",available,toread);
			// read data
			readcount=read(fd,tmp,toread);
			if (readcount<0)
				readcount=0;
			tmp[readcount]=0;
			//DBG("read [%s]\n",tmp);
			// add read bytes to the output buffer
			if (readcount) {
				strcat(answer,tmp);
				count+=readcount;
				/* if we have more time to read, check if we got already
				the expected string */
				if ((timeoutcounter<timeout) && (found==0)) {
					// check if it's the expected answer
					if ((strstr(answer,"OK\r")) || (strstr(answer,"ERROR")))
						found=1;
					if (expect && expect[0] && strstr(answer,expect))
						found=1;
					// if found then set timoutcounter to read 0.1s more
					if (found)
						timeoutcounter=timeout-1;
				}
			}
		}
	// repeat until timout
	}while (timeoutcounter<timeout);

	DBG("DEBUG:put_command: <-[%s] \n",answer);
	return count;
}




/* setup serial port */
int setmodemparams( struct modem *mdm )
{
	struct termios newtio;

	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = mdm->baudrate | CRTSCTS | CS8 | CLOCAL | CREAD | O_NDELAY;
	//uncomment next line to disable hardware handshake
	//newtio.c_cflag &= ~CRTSCTS;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;
	newtio.c_cc[VTIME]    = 1;
	newtio.c_cc[VMIN]     = 0;
	tcflush(mdm->fd, TCIOFLUSH);
	tcsetattr(mdm->fd,TCSANOW,&newtio);
	return 0;
}




int initmodem(struct modem *mdm)
{
	char command[100];
	char answer[500];
	int retries=0;
	int success=0;
	int clen=0;

	if (mdm->pin[0]) {
		LOG(L_INFO,"INFO:initmodem: let's check if modem wants the PIN\n");
		/* Checking if modem needs PIN */
		put_command(mdm->fd,"AT+CPIN?\r",9,answer,sizeof(answer),50,"+CPIN:");
		if (strstr(answer,"+CPIN: SIM PIN")) {
			LOG(L_INFO,"INFO:initmodem: Modem needs PIN, entering PIN...\n");
			clen=sprintf(command,"AT+CPIN=\"%s\"\r",mdm->pin);
			put_command(mdm->fd,command,clen,answer,sizeof(answer),300,0);
			put_command(mdm->fd,"AT+CPIN?\r",9,answer,sizeof(answer),
				50,"+CPIN:");
			if (!strstr(answer,"+CPIN: READY")) {
				if (strstr(answer,"+CPIN: SIM PIN")) {
					LOG(L_ERR,"ERROR:initmodem: Modem did not accept"
						" this PIN\n");
					goto error;
				} else if (strstr(answer,"+CPIN: SIM PUK")) {
					LOG(L_ERR,"ERROR:initmodem: YourPIN is locked!"
						" Unlock it manually!\n");
					goto error;
				} else {
					goto error;
				}
			}
			LOG(L_INFO,"INFO:initmodem: PIN Ready!\n");
			sleep(5);
		}
	}

	if (mdm->mode==MODE_DIGICOM)
		success=1;
	else {
		LOG(L_INFO,"INFO:initmodem: Checking if Modem is registered to"
			" the network\n");
		success=0;
		retries=0;
		do
		{
			retries++;
			put_command(mdm->fd,"AT+CREG?\r",9,answer,sizeof(answer),100,0);
			if (strchr(answer,'1') )
			{
				LOG(L_INFO,"INFO:initmodem: Modem is registered to the"
					" network\n");
				success=1;
			} else if (strchr(answer,'2')) {
				// addede by bogdan
				LOG(L_WARN,"WARNING:initmodem: Modems seems to try to "
					"reach the network! Let's wait a little bit\n");
				retries--;
				sleep(2*mdm->retry);
			} else if (strchr(answer,'5')) {
				// added by Thomas Stoeckel
				LOG(L_INFO,"INFO:initmodem: Modem is registered to a"
					" roaming partner network\n");
				success=1;
			} else if (strstr(answer,"ERROR")) {
				LOG(L_WARN,"WARNING:initmodem: Ignoring that modem does"
					" not support +CREG command.\n");
				success=1;
			} else {
				LOG(L_NOTICE,"NOTICE:initmodem: Waiting %i sec. before"
					" retrying\n",mdm->retry);
				sleep(2*mdm->retry);
			}
		}while ((success==0)&&(retries<20));
	}

	if (success==0) {
		LOG(L_ERR,"ERROR:initmodem: Modem is not registered to the network\n");
		goto error;
	}

	if (mdm->mode==MODE_ASCII || mdm->mode==MODE_DIGICOM)
		strcpy(command,"AT+CMGF=1\r");
	else
		strcpy(command,"AT+CMGF=0\r");

	retries=0;
	success=0;
	do {
		retries++;
		/*quering the modem*/
		put_command(mdm->fd,command,10,answer,sizeof(answer),50,0);
		/*dealing with the answer*/
		if (strstr(answer,"ERROR")) {
			LOG(L_NOTICE,"NOTICE:initmodem: Waiting %i sec. before to"
				" retrying\n",mdm->retry);
			sleep(mdm->retry);
		} else
			success=1;
	}while ((success==0)&&(retries<3));

	if (success==0) {
		LOG(L_ERR,"ERROR:initmodem: Modem did not accept PDU mode\n");
		goto error;
	}

	/* added for probing */
	put_command(mdm->fd,"AT+CSMP=49,167,0,242\r",21,answer,
		sizeof(answer),50,0);
	put_command(mdm->fd,"AT+CNMI=1,1,0,1,0\r",18,answer,sizeof(answer),50,0);

	return 0;
error:
	return -1;
}




int checkmodem(struct modem *mdm)
{
	char answer[500];

	/* Checking if modem needs PIN */
	put_command(mdm->fd,"AT+CPIN?\r",9,answer,sizeof(answer),50,"+CPIN:");
	if (!strstr(answer,"+CPIN: READY")) {
		LOG(L_WARN,"WARNING:sms_checkmodem: modem wants the PIN again!\n");
		goto reinit;
	}

	if (mdm->mode!=MODE_DIGICOM) {
		put_command(mdm->fd,"AT+CREG?\r",9,answer,sizeof(answer),100,0);
		if (!strchr(answer,'1') ) {
			LOG(L_WARN,"WARNING:sms_checkmodem: Modem is not registered to the"
					" network\n");
			goto reinit;
		}
	}

	return 0;
reinit:
	LOG(L_WARN,"WARNING:sms_checkmodem: re -init the modem!!\n");
	initmodem(mdm);
	return -1;
}




int setsmsc(struct modem *mdm, char *smsc)
{
	char command[100];
	char answer[50];
	int  clen;

	if (smsc && smsc[0]) {
		DBG("DEBUG:initmodem: Changing SMSC\n");
		clen=sprintf(command,"AT+CSCA=\"+%s\"\r",smsc);
		put_command(mdm->fd,command,clen,answer,sizeof(answer),50,0);
	}
	return 0;
}




int openmodem( struct modem *mdm)
{
	mdm->fd = open(mdm->device, O_RDWR | O_NOCTTY );
	if (mdm->fd <0)
		return -1;

	tcgetattr(mdm->fd,&(mdm->oldtio));
	return 0;
}




int closemodem(struct modem *mdm)
{
	tcsetattr(mdm->fd,TCSANOW,&(mdm->oldtio));
	close(mdm->fd);
	return 0;
}

