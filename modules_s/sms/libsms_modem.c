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




int put_command(int fd, char* command, char* answer, int max, int timeout,
																char* expect)
{
	int count=0;
	int readcount;
	int toread;
	char tmp[100];
	int timeoutcounter=0;
	int found=0;
	int available;
	int status;

	if (command==0 || command[0]==0 ) {
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

	//DBG("DEBUG: put_command: ->%s \n",command);
	write(fd,command,strlen(command));
	tcdrain(fd);

	answer[0]=0;
	do
	{
		// try to read some bytes.
		ioctl(fd,FIONREAD,&available);
		// how many bytes are available to read?
		if (available<1)  // if 0 then wait a little bit and retry
		{
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
			// read data
			readcount=read(fd,tmp,toread);
			if (readcount<0)
				readcount=0;
			tmp[readcount]=0;
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

	//DBG("DEBUG:put_command: <-[%s] \n",answer);
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




int initmodem(struct modem *mdm, char *smsc)
{
	char command[100];
	char answer[500];
	int retries=0;
	int success=0;
	int errorsleeptime=ERROR_SLEEP_TIME;

	/*if (initstring[0])
	{
		writelogfile(LOG_INFO,"Initializing modem");
		put_command(initstring,answer,sizeof(answer),100,0);
	}*/

	if (mdm->pin[0]) {
		/* Checking if modem needs PIN */
		put_command(mdm->fd,"AT+CPIN?\r",answer,sizeof(answer),50,"+CPIN:");
		if (strstr(answer,"+CPIN: SIM PIN")) {
			LOG(L_INFO,"INFO:initmodem: Modem needs PIN, entering PIN...\n");
			sprintf(command,"AT+CPIN=\"%s\"\r",mdm->pin);
			put_command(mdm->fd,command,answer,sizeof(answer),300,0);
			put_command(mdm->fd,"AT+CPIN?\r",answer,sizeof(answer),
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
			put_command(mdm->fd,"AT+CREG?\r",answer,sizeof(answer),100,0);
			if (strstr(answer,"1"))
			{
				LOG(L_INFO,"INFO:initmodem: Modem is registered to the"
					" network\n");
				success=1;
			} else if (strstr(answer,"5")) {
				// added by Thomas Stoeckel
				LOG(L_INFO,"INFO:initmodem: Modem is registered to a"
					" roaming partner network\n");
				success=1;
			} else if (strstr(answer,"ERROR")) {
				LOG(L_WARN,"WARNING:initmodem: Ignoring that modem does"
					" not support +CREG command.\n");
				success=1;
			} else {
				LOG(L_NOTICE,"NOTICE:initmodem: Waiting %i sec. before to"
					" retrying\n",errorsleeptime);
				sleep(errorsleeptime);
			}
		}while ((success==0)&&(retries<10));
	}

	if (success==0) {
		LOG(L_ERR,"ERROR:initmodem: Modem is not registered to the network\n");
		goto error;
	}

	if (mdm->mode==MODE_ASCII || mdm->mode==MODE_DIGICOM) {
		//LOG(L_INFO,"INFO:initmodem:Selecting ASCII mode 1\n");
		strcpy(command,"AT+CMGF=1\r");
	} else {
		//LOG(L_INFO,"INFO:initmodem:Selecting PDU mode 0\n");
		strcpy(command,"AT+CMGF=0\r");
	}

	retries=0;
	success=0;
	do {
		retries++;
		put_command(mdm->fd,command,answer,sizeof(answer),50,0);
		if (strstr(answer,"ERROR")) {
			LOG(L_NOTICE,"NOTICE:initmodem: Waiting %i sec. before to"
				" retrying\n",errorsleeptime);
			sleep(errorsleeptime);
		} else
			success=1;
	}while ((success==0)&&(retries<3));

	if (success==0) {
		LOG(L_ERR,"ERROR:initmodem: Modem did not accept PDU mode\n");
		goto error;
	}

	if (smsc && smsc[0]) {
		DBG("DEBUG:initmodem: Changing SMSC\n");
		sprintf(command,"AT+CSCA=\"+%s\"\r",smsc);
		put_command(mdm->fd,command,answer,sizeof(answer),50,0);
	}

	return 0;
error:
	return -1;
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

