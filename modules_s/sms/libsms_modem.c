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


#ifdef old
int put_command(char* command,char* answer,int max,int timeout,char* expect)
{
  int count=0;
  int readcount;
  int toread;
  char tmp[100];
  int timeoutcounter=0;
  int found=0;
  int available;
  int status;

  // send command
  if (command) if (command[0])
  {
  // Cycwin does not support TIOC functions and has not workaround for this.
  // So do not check CTS on Cygwin.
#ifndef WINDOWS
    ioctl(modem,TIOCMGET,&status);
    while (!(status & TIOCM_CTS))
    {
      // write(1,",",1);
      usleep(100000);
      timeoutcounter++;
      ioctl(modem,TIOCMGET,&status);
      if (timeoutcounter>=timeout)
      {
        writelogfile(LOG_INFO,"\nModem is not clear to send");
        return 0;
      }
    }
#endif
    writelogfile(LOG_DEBUG,"->%s",command);
    write(modem,command,strlen(command));
    tcdrain(modem);
  }


  answer[0]=0;
  do
  {
    // try to read some bytes.

    // Cygwin does not support TIOC functions and has not workaround for this.
    // So do not check number of available bytes
#ifndef WINDOWS
    ioctl(modem,FIONREAD,&available);	// how many bytes are available to read?
    if (available<1)			// if 0 then wait a little bit and retry
    {
      usleep(100000);
      //write(1,".",1);
      timeoutcounter++;
      ioctl(modem,FIONREAD,&available);
    }
#else
    // Only for Windows. Read as much as possible
    usleep(100000);
    //write(1,".",1);
    timeoutcounter++;
    available=sizeof(tmp)-1;
#endif
    if (available>0)
    {
      // How many bytes do I wan t to read maximum? Not more than tmp buffer size.
      toread=max-count-1;
      if (toread>sizeof(tmp)-1)
        toread=sizeof(tmp)-1;
      // And how many bytes are available?
      if (available<toread)
        toread=available;
      // read data
      readcount=read(modem,tmp,toread);
      if (readcount<0)
        readcount=0;
      tmp[readcount]=0;
      // add read bytes to the output buffer
      if (readcount)
      {
        strcat(answer,tmp);
        count+=readcount;

        // if we have more time to read, check if we got already the expected string
        if ((timeoutcounter<timeout) && (found==0))
        {
          // check if it's the expected answer
          if ((strstr(answer,"OK\r")) || (strstr(answer,"ERROR")))
            found=1;
          if (expect) if (expect[0])
            if (strstr(answer,expect))
              found=1;
          // if found then set timoutcounter to read 0.1s more
          if (found)
            timeoutcounter=timeout-1;
        }
      }
    }
  }
  // repeat until timout
  while (timeoutcounter<timeout);

  writelogfile(LOG_DEBUG,"<-%s",answer);

  return count;
}

void setmodemparams() /* setup serial port */
{
  struct termios newtio;
  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = baudrate | CRTSCTS | CS8 | CLOCAL | CREAD | O_NDELAY;
// uncomment next line to disable hardware handshake
// newtio.c_cflag &= ~CRTSCTS;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;
  newtio.c_lflag = 0;
#ifdef WINDOWS
  newtio.c_cc[VTIME]    = 0;
#else
  newtio.c_cc[VTIME]    = 1;
#endif
  newtio.c_cc[VMIN]     = 0;
  tcflush(modem, TCIOFLUSH);
  tcsetattr(modem,TCSANOW,&newtio);
}

void initmodem()
{
  char command[100];
  char answer[500];
  int retries=0;
  int success=0;

  if (initstring[0])
  {
    writelogfile(LOG_INFO,"Initializing modem");
    put_command(initstring,answer,sizeof(answer),100,0);
  }

  if (pin[0])
  {
    writelogfile(LOG_INFO,"Checking if modem needs PIN");
    put_command("AT+CPIN?\r",answer,sizeof(answer),50,"+CPIN:");
    if (strstr(answer,"+CPIN: SIM PIN"))
    {
      writelogfile(LOG_NOTICE,"Modem needs PIN, entering PIN...");
      sprintf(command,"AT+CPIN=\"%s\"\r",pin);
      put_command(command,answer,sizeof(answer),300,0);
      put_command("AT+CPIN?\r",answer,sizeof(answer),50,"+CPIN:");
      if (strstr(answer,"+CPIN: SIM PIN"))
      {
        writelogfile(LOG_ERR,"Modem did not accept this PIN");
        exit(2);
      }
      else if (strstr(answer,"+CPIN: READY"))
        writelogfile(LOG_INFO,"PIN Ready");
    }
    if (strstr(answer,"+CPIN: SIM PUK"))
    {
      writelogfile(LOG_CRIT,"Your PIN is locked. Unlock it manually");
      exit(2);
    }
  }

  if (strcmp(mode,"digicom")==0)
    success=1;
  else
  {
    writelogfile(LOG_INFO,"Checking if Modem is registered to the network");
    success=0;
    retries=0;
    do
    {
      retries++;
      put_command("AT+CREG?\r",answer,sizeof(answer),100,0);
      if (strstr(answer,"1"))
      {
        writelogfile(LOG_INFO,"Modem is registered to the network");
        success=1;
      }
      else if (strstr(answer,"5"))
      {
      	// added by Thomas Stoeckel
      	writelogfile(LOG_INFO,"Modem is registered to a roaming partner network");
	success=1;
      }
      else if (strstr(answer,"ERROR"))
      {
        writelogfile(LOG_INFO,"Ignoring that modem does not support +CREG command.");
	success=1;
      }
      else
      {
        writelogfile(LOG_NOTICE,"Waiting %i sec. before to retrying",errorsleeptime);
        sleep(errorsleeptime);
      }
    }
    while ((success==0)&&(retries<10));
  }

  if (success==0)
  {
    writelogfile(LOG_ERR,"Error: Modem is not registered to the network");
    exit(3);
  }


  if ((strcmp(mode,"ascii")==0) || (strcmp(mode,"digicom")==0))
  {
    writelogfile(LOG_INFO,"Selecting ASCII mode 1");
    strcpy(command,"AT+CMGF=1\r");
  }
  else
  {
    writelogfile(LOG_INFO,"Selecting PDU mode 0");
    strcpy(command,"AT+CMGF=0\r");
  }

  retries=0;
  success=0;
  do
  {
    retries++;
    put_command(command,answer,sizeof(answer),50,0);
    if (strstr(answer,"ERROR"))
    {
      writelogfile(LOG_NOTICE,"Waiting %i sec. before to retrying",errorsleeptime);
      sleep(errorsleeptime);
    }
    else
      success=1;
  }
  while ((success==0)&&(retries<3));
  
  if (success==0)
  {
    writelogfile(LOG_ERR,"Error: Modem did not accept PDU mode");
    exit(3);
  }

  if (smsc[0])
  {
    writelogfile(LOG_INFO,"Changing SMSC");
    sprintf(command,"AT+CSCA=\"+%s\"\r",smsc);
    put_command(command,answer,sizeof(answer),50,0);
  }

}
#endif




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

