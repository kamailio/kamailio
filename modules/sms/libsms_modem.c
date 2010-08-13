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
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/ioctl.h>
#ifdef __sun
#include <sys/filio.h>
#endif
#include "libsms_modem.h"
#include "../../dprint.h"

#define  MAX_BUF        2048
#define  CDS_HDR        "\r\n+CDS:"
#define  CDS_HDR_LEN    (strlen(CDS_HDR))
#define optz(_n,_l)     (buf+buf_len-(((_n)+(_l)>buf_len)?buf_len:(_n)+(_l)))

/* global variables */
int         sms_report_type;
cds_report  cds_report_func;



int put_command( struct modem *mdm, char* cmd, int cmd_len, char* answer,
											int max, int timeout,char* exp_end)
{
	static char buf[MAX_BUF];
	static int  buf_len = 0;
	char* pos;
	char* foo;
	char* ptr;
	char* to_move;
	char* answer_s;
	char* answer_e;
	int   timeoutcounter;
	int   available;
	int   status;
	int   exp_end_len;
	int   n;

	/* check if fd is "clean" for reading */
	timeoutcounter = 0;
	ioctl(mdm->fd,TIOCMGET,&status);
	while (!(status & TIOCM_CTS))
	{
		usleep( READ_SLEEP );
		timeoutcounter++;
		ioctl(mdm->fd,TIOCMGET,&status);
		if (timeoutcounter>=timeout) {
			LM_INFO("Modem is not clear to send\n");
			return 0;
		}
	}
#ifdef SHOW_SMS_MODEM_COMMAND
	LM_DBG("-<%d>-->[%.*s] \n",cmd_len,cmd_len,cmd);
#endif
	/* send the command to the modem */
	write(mdm->fd,cmd,cmd_len);
	tcdrain(mdm->fd);

	/* read from the modem */
	exp_end_len = exp_end?strlen(exp_end):0;
	answer_s = buf;
	answer_e = 0;
	to_move = 0;
	do
	{
		/* try to read some bytes */
		ioctl(mdm->fd,FIONREAD,&available);
		/* how many bytes are available to read? */
		if (available<1)  /* if 0 then wait a little bit and retry */
		{
			usleep( READ_SLEEP );
			timeoutcounter++;
			ioctl(mdm->fd,FIONREAD,&available);
		}
		if (available>0)
		{
			/* How many bytes do I want to read maximum?
			Not more than buffer size. And how many bytes are available? */
			n = (available>MAX_BUF-buf_len-1)?MAX_BUF-buf_len-1:available;
			/* read data */
			n = read( mdm->fd, buf+buf_len, n);
			if (n<0) {
				LM_ERR("error reading from modem: %s\n", strerror(errno));
				goto error;
			}
			if (n) {
				buf_len += n;
				buf[buf_len] = 0;
				//LM_DBG("read = [%s]\n",buf+buf_len-n);
				foo = pos = 0;
				if ( (!exp_end && ((pos=strstr(optz(n,4),"OK\r\n"))
				|| (foo=strstr(optz(n,5),"ERROR"))))
				|| (exp_end && (pos=strstr(optz(n,exp_end_len),exp_end)) )) {
					/* we found the end */
					//LM_DBG("end found = %s\n",
					//	(foo?"ERROR":(exp_end?exp_end:"OK")));
					/* for ERROR we still have to read EOL */
					if (!foo || (foo=strstr(foo+5,"\r\n"))) {
						answer_e = foo?foo+2:(pos+(exp_end?exp_end_len:4));
						timeoutcounter = timeout;
					}
				}
			}
		}
	/* repeat until timout */
	}while (timeoutcounter<timeout);

	if (!answer_e)
		answer_e = buf+buf_len;

	/* CDS report is activ? */
	if (sms_report_type==CDS_REPORT) {
		to_move = 0;
		ptr = buf;
		/* do we have a CDS reply inside? */
		while ( (pos=strstr(ptr,CDS_HDR)) ) {
			if (pos!=ptr) {  /* here we have the command response */
				answer_s = ptr;
			}
			/* look for the end of CDS response */
			ptr = pos+CDS_HDR_LEN;
			for( n=0 ; n<2&&(foo=strstr(ptr,"\r\n")) ; ptr=foo+2,n++ );
			if (n<2) { /* we haven't read the entire CDS response */
				LM_DBG("CDS end not found!\n");
				to_move = pos;
				ptr = buf + buf_len;
			}else{
				/* process the CDS */
				LM_DBG("CDS=[%.*s]\n",(int)(ptr-pos),pos);
				cds_report_func(mdm,pos,ptr-pos);
			}
		}
		if ((*ptr)) {
			answer_s = ptr;
			ptr = answer_e;
		}
		if (ptr!=buf+buf_len)
			to_move = ptr;
	}

	/* copy the response in answer buffer - as much as fits */
	if (answer && max) {
		n = max-1<answer_e-answer_s?max-1:answer_e-answer_s;
		memcpy(answer,answer_s,n);
		answer[n] = 0;
	}
	/* shift left the remaining data into the buffer - if needs */
	if (sms_report_type==CDS_REPORT && to_move) {
		buf_len = buf_len - (to_move-buf);
		memcpy(buf,to_move,buf_len);
		buf[buf_len] = 0;
		LM_DBG("buffer shifted left=[%d][%s]\n",buf_len,buf);
	} else {
		buf_len = 0;
	}

#ifdef SHOW_SMS_MODEM_COMMAND
	LM_DBG("<-[%s] \n",answer);
#endif
	return answer_e-answer_s;

error:
	return 0;
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




int initmodem(struct modem *mdm, cds_report cds_report_f)
{
	char command[100];
	char answer[100];
	int retries=0;
	int success=0;
	int clen=0;
	int n;

	LM_INFO("init modem %s on %s.\n",mdm->name,mdm->device);

	if (mdm->pin[0]) {
		/* Checking if modem needs PIN */
		put_command(mdm,"AT+CPIN?\r",9,answer,sizeof(answer),50,0);
		if (strstr(answer,"+CPIN: SIM PIN")) {
			LM_INFO("Modem needs PIN, entering PIN...\n");
			clen=sprintf(command,"AT+CPIN=\"%s\"\r",mdm->pin);
			put_command(mdm,command,clen,answer,sizeof(answer),100,0);
			put_command(mdm,"AT+CPIN?\r",9,answer,sizeof(answer),50,0);
			if (!strstr(answer,"+CPIN: READY")) {
				if (strstr(answer,"+CPIN: SIM PIN")) {
					LM_ERR("Modem did not accept this PIN\n");
					goto error;
				} else if (strstr(answer,"+CPIN: SIM PUK")) {
					LM_ERR("YourPIN is locked! Unlock it manually!\n");
					goto error;
				} else {
					goto error;
				}
			}
			LM_INFO("PIN Ready!\n");
			sleep(5);
		}
	}

	if (mdm->mode==MODE_DIGICOM)
		success=1;
	else {
		LM_INFO("Checking if Modem is registered to the network\n");
		success=0;
		retries=0;
		do
		{
			retries++;
			put_command(mdm,"AT+CREG?\r",9,answer,sizeof(answer),100,0);
			if (strchr(answer,'1') )
			{
				LM_INFO("Modem is registered to the network\n");
				success=1;
			} else if (strchr(answer,'2')) {
				// added by bogdan
				LM_WARN("Modems seems to try to reach the network!"
						" Let's wait a little bit\n");
				retries--;
				sleep(2);
			} else if (strchr(answer,'5')) {
				// added by Thomas Stoeckel
				LM_INFO("Modem is registered to a roaming partner network\n");
				success=1;
			} else if (strstr(answer,"ERROR")) {
				LM_WARN("Ignoring that modem does not support +CREG command.\n");
				success=1;
			} else {
				LM_NOTICE("Waiting 2 sec. before retrying\n");
				sleep(2);
			}
		}while ((success==0)&&(retries<20));
	}

	if (success==0) {
		LM_ERR("Modem is not registered to the network\n");
		goto error;
	}

	for( n=0 ; n<2+2*(sms_report_type==CDS_REPORT) ; n++) {
		/* build the command */
		switch (n) {
			case 0:
				strcpy(command,"AT+CMGF=0\r");
				command[8]+=(mdm->mode==MODE_ASCII || mdm->mode==MODE_DIGICOM);
				clen = 10;
				break;
			case 1:
				strcpy(command,"AT S7=45 S0=0 L1 V1 X4 &c1 E1 Q0\r");
				clen = 33;
				break;
			case 2:
				strcpy(command,"AT+CSMP=49,167,0,241\r");
				clen = 21;
				break;
			case 3:
				strcpy(command,"AT+CNMI=1,1,0,1,0\r");
				clen = 18;
				break;
		}
		/* send it to modem */
		retries=0;
		success=0;
		do {
			retries++;
			/*querying the modem*/
			put_command(mdm,command,clen,answer,sizeof(answer),100,0);
			/*dealing with the answer*/
			if (strstr(answer,"ERROR")) {
				LM_NOTICE("Waiting 1 sec. before to retrying\n");
				sleep(1);
			} else
				success=1;
		}while ((success==0)&&(retries<3));
		/* have we succeeded? */
		if (success==0) {
			LM_ERR("cmd [%.*s] returned ERROR\n", clen-1,command);
			goto error;
		}
	} /* end for */

	if ( sms_report_type==CDS_REPORT && !cds_report_f) {
		LM_ERR("no CDS_REPORT function given\n");
		goto error;
	}
	cds_report_func = cds_report_f;

	if (mdm->smsc[0]) {
		LM_INFO("Changing SMSC to \"%s\"\n",mdm->smsc);
		setsmsc(mdm,mdm->smsc);
	}



	return 0;
error:
	return -1;
}




int checkmodem(struct modem *mdm)
{
	char answer[500];

	/* Checking if modem needs PIN */
	put_command(mdm,"AT+CPIN?\r",9,answer,sizeof(answer),50,0);
	if (!strstr(answer,"+CPIN: READY")) {
		LM_WARN("modem wants the PIN again!\n");
		goto reinit;
	}

	if (mdm->mode!=MODE_DIGICOM) {
		put_command(mdm,"AT+CREG?\r",9,answer,sizeof(answer),100,0);
		if (!strchr(answer,'1') ) {
			LM_WARN("Modem is not registered to the network\n");
			goto reinit;
		}
	}

	return 1;
reinit:
	LM_WARN("re -init the modem!!\n");
	initmodem(mdm,cds_report_func);
	return -1;
}




int setsmsc(struct modem *mdm, char *smsc)
{
	char command[100];
	char answer[50];
	int  clen;

	if (smsc && smsc[0]) {
		clen=sprintf(command,"AT+CSCA=\"+%s\"\r",smsc);
		put_command(mdm,command,clen,answer,sizeof(answer),50,0);
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

