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

#ifdef cucu

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include "charset.h"
#include "version.h"
#include "modeminit.h"
#include "logging.h"

char to[255] = {0};
char subject[255] = {0};
char outputfile[255] = {0};
char logfile[255] = {0};
int loglevel=9;
int cs_convert;
int keep;
int is_binary=0;       // 1 if the sms is in binary format
int with_udh=0;        // 1 if udh bit is set
int is_statusreport=0; // 1 if the sms is a status report


int octet2bin(char* octet) /* converts an octet to a 8-Bit value */
{
  int result=0;
  if (octet[0]>57)
    result+=octet[0]-55;
  else
    result+=octet[0]-48;
  result=result<<4;
  if (octet[1]>57)
    result+=octet[1]-55;
  else
    result+=octet[1]-48;
  return result;
}

int pdu2ascii(char* pdu, char* ascii) /* converts a PDU-String to Ascii */
{                                     /* the first octet is the length */
  int bitposition=0;                  /* return the length of ascii */
  int byteposition;
  int byteoffset;
  int charcounter;
  int bitcounter;
  int count;
  int octetcounter;
  char c;
  char binary[500];
  
  /* First convert all octets to bytes */
  count=octet2bin(pdu);
  for (octetcounter=0; octetcounter<count; octetcounter++)
    binary[octetcounter]=octet2bin(pdu+(octetcounter<<1)+2);
  
  /* Then convert from 8-Bit to 7-Bit encapsulated in 8 bit */
  for (charcounter=0; charcounter<count; charcounter++)
  {
    c=0;
    for (bitcounter=0; bitcounter<7; bitcounter++)
    {
      byteposition=bitposition/8;
      byteoffset=bitposition%8;
      if (binary[byteposition]&(1<<byteoffset))
        c=c|128;
      bitposition++;
      c=(c>>1)&127; /* The shift fills with 1, but I want 0 */
    }
    if (cs_convert)
      ascii[charcounter]=sms2ascii(c);
    else if (c==0)
      ascii[charcounter]=183;
    else
      ascii[charcounter]=c;
  }
  ascii[count]=0;
  return count;
}

int pdu2binary(char* pdu, char* binary)
{
  int count;
  int octetcounter;
  count=octet2bin(pdu);
  for (octetcounter=0; octetcounter<count; octetcounter++)
    binary[octetcounter]=octet2bin(pdu+(octetcounter<<1)+2);
  binary[count]=0;
  return count;
}


int getsms(int sim,char* pdu)  /* reads a SMS from the SIM-memory 1-10 */
{                              /* returns number of SIM memory if successful */
			       /* on digicom the return value can be != sim */	
  char command[5000];
  char answer[500];
  char* position;
  char* beginning;
  char* end;
  char tmp[32];
  int i;

  if (strcmp(mode,"digicom")==0) // Digicom reports date+time only with AT+CMGL
  {
    writelogfile(LOG_INFO,"Trying to get stored message");
    sprintf(command,"AT+CMGL=\"ALL\"\r");
    put_command(command,answer,sizeof(answer),200,0);
    /* search for beginning of the answer */
    for (i=1; i<11; i++)
    {
      sprintf(tmp,"+CMGL: %i",i);
      position=strstr(answer,tmp); 
      if (position)
      {
        writelogfile(LOG_INFO,"Found a message at memory %i",i);
        sim=i;
	break;
      }
    }
  }
  
  else
  {
    writelogfile(LOG_INFO,"Trying to get stored message %i",sim);
    sprintf(command,"AT+CMGR=%i\r",sim);
    put_command(command,answer,sizeof(answer),50,0);
    /* search for beginning of the answer */
    position=strstr(answer,"+CMGR:");
  }

  if (position==0) /* keine SMS empfangen, weil Modem nicht mit +CMGR oder +CMGL geantwortet hat */
    return 0;
  beginning=position+7;
  if (strstr(answer,",,0\r")) /* keine SMS, weil Modem mit +CMGR: 0,,0 geantwortet hat */
    return 0;

  /* After that we have the PDU or ASCII string */
  end=strstr(beginning,"\r");
  if ((end==0) || ((end-beginning)<4))
    return 0;
  end=strstr(end+1,"\r");
  if ((end==0) || ((end-beginning)<4))
    return 0;
  /* Now we have the end of the PDU or ASCII string */
  *end=0;
  strcpy(pdu,beginning);
  return sim;
}

void deletesms(int sim) /* deletes the selected sms from the sim card */
{
  char command[100];
  char answer[500];
  writelogfile(LOG_INFO,"Deleting message %i",sim);
  sprintf(command,"AT+CMGD=%i\r",sim);
  put_command(command,answer,sizeof(answer),50,0);
}


void check_memory(int *used_memory,int *max_memory) // checks the size of the SIM memory
{
  char command[100];
  char answer[500];
  char* posi;
  int laenge;
  // Set default values in case that the modem does not support the +CPMS command
  *used_memory=1;
  *max_memory=10;

  writelogfile(LOG_INFO,"Checking memory size");
  put_command("AT+CPMS?\r",answer,sizeof(answer),50,0);
  if (posi=strstr(answer,"+CPMS:"))
  {
    // Modem supports CPMS command. Read memory size
    if (posi=strchr(posi,','))
    {
      posi++;
      if (laenge=strcspn(posi,",\r"))
      {
        posi[laenge]=0;
        *used_memory=atoi(posi);
        posi+=laenge+1;
	if (laenge=strcspn(posi,",\r"))
	{
          *max_memory=atoi(posi);
  	  writelogfile(LOG_INFO,"Used memory is %i of %i",*used_memory,*max_memory);
	  return;
	}
      }
    }
  }
  writelogfile(LOG_INFO,"Command failed, using defaults.");
}

/* splits an ASCII string into the parts */
/* returns length of ascii */
int splitascii(char* source, char* sendr, char* name, char* date, char* time, char* ascii)
{
  char* start;
  char* end;
  sendr[0]=0;
  date[0]=0;
  time[0]=0;
  ascii[0]=0;
  name[0]=0;
  /* the text is after the \r */
  start=strstr(source,"\r");
  if (start==0)
    return strlen(ascii);
  start++;
  strcpy(ascii,start);
  /* get the senders MSISDN */
  start=strstr(source,"\",\"");
  if (start==0)
    return strlen(ascii);
  start+=3;
  end=strstr(start,"\",");
  if (end==0)
    return strlen(ascii);
  *end=0;
  strcpy(sendr,start);
  /* Siemens M20 inserts the senders name between MSISDN and date */
  start=end+3;
  // Workaround for Thomas Stoeckel //
  if (start[0]=='\"')
    start++;
  if (start[2]!='/')  // if next is not a date is must be the name
  {
    end=strstr(start,"\",");
    if (end==0)
      return strlen(ascii);
    *end=0;
    strcpy(name,start);
  }
  /* Get the date */
  start=end+3;
  sprintf(date,"%c%c-%c%c-%c%c",start[3],start[4],start[0],start[1],start[6],start[7]);
  /* Get the time */
  start+=9;
  sprintf(time,"%c%c:%c%c:%c%c",start[0],start[1],start[3],start[4],start[7],start[7]);
  return strlen(ascii);
}


// Subroutine for splitpdu() for messages type 0 (SMS-Deliver)
// Returns the length of the ascii string. In binary mode ascii contains the binary SMS
int split_type_0(char* Pointer,char* sendr,char* date,char* time,char* ascii)
{
  int Length;
  int padding;
  Length=octet2bin(Pointer);
  padding=Length%2;
  Pointer+=4;
  strncpy(sendr,Pointer,Length+padding);
  swapchars(sendr);
  /* remove Padding characters after swapping */
  sendr[Length]=0;
  Pointer=Pointer+Length+padding+3;
  if ((Pointer[0] & 4)==4)
    is_binary=1;
  else
    is_binary=0;
  Pointer++;
  sprintf(date,"%c%c-%c%c-%c%c",Pointer[3],Pointer[2],Pointer[5],Pointer[4],Pointer[1],Pointer[0]);
  Pointer=Pointer+6;
  sprintf(time,"%c%c:%c%c:%c%c",Pointer[1],Pointer[0],Pointer[3],Pointer[2],Pointer[5],Pointer[4]);
  Pointer=Pointer+8;
  if (is_binary)
    return pdu2binary(Pointer,ascii);
  else
    return pdu2ascii(Pointer,ascii);
}


// Subroutine for splitpdu() for messages type 2 (Staus Report)
// Returns the length of the ascii string. In binary mode ascii contains the binary SMS
int split_type_2(char* position,char* from, char* date,char* time,char* result)
{
  int length;
  int padding;
  int status;
  char temp[32];
  strcat(result,"SMS STATUS REPORT\n");
  // get recipient address
  position+=2;
  length=octet2bin(position);
  padding=length%2;
  position+=4;
  strncpy(from,position,length+padding);
  from[length]=0;
  swapchars(from);
  strcat(result,"\nDischarge_timestamp: ");
  // get SMSC timestamp
  position+=length+padding;
  sprintf(date,"%c%c-%c%c-%c%c",position[3],position[2],position[5],position[4],position[1],position[0]);
  sprintf(time,"%c%c:%c%c:%c%c",position[7],position[6],position[9],position[8],position[11],position[10]);
  // get Discharge timestamp
  position+=14;
  sprintf(temp,"%c%c-%c%c-%c%c %c%c:%c%c:%c%c",position[3],position[2],position[5],position[4],position[1],position[0],position[7],position[6],position[9],position[8],position[11],position[10]);
  strcat(result,temp);
  strcat(result,"\nStatus: ");
  // get Status
  position+=14;
  status=octet2bin(position);
  sprintf(temp,"%i,",status);
  strcat(result,temp);
  switch (status)
  {
    case 0: strcat(result,"Ok,short message received by the SME"); break;
    case 1: strcat(result,"Ok,short message forwarded by the SC to the SME but the SC is unable to confirm delivery"); break;
    case 2: strcat(result,"Ok,short message replaced by the SC"); break;

    case 32: strcat(result,"Still trying,congestion"); break;
    case 33: strcat(result,"Still trying,SME busy"); break;
    case 34: strcat(result,"Still trying,no response from SME"); break;
    case 35: strcat(result,"Still trying,service rejected"); break;
    case 36: strcat(result,"Still trying,quality of service not available"); break;
    case 37: strcat(result,"Still trying,error in SME"); break;

    case 64: strcat(result,"Error,remote procedure error"); break;
    case 65: strcat(result,"Error,incompatible destination"); break;
    case 66: strcat(result,"Error,connection rejected by SME"); break;
    case 67: strcat(result,"Error,not obtainable"); break;
    case 68: strcat(result,"Error,quality of service not available"); break;
    case 69: strcat(result,"Error,no interworking available"); break;
    case 70: strcat(result,"Error,SM validity period expired"); break;
    case 71: strcat(result,"Error,SM deleted by originating SME"); break;
    case 72: strcat(result,"Error,SM deleted by SC administration"); break;
    case 73: strcat(result,"Error,SM does not exist"); break;

    case 96: strcat(result,"Error,congestion"); break;
    case 97: strcat(result,"Error,SME busy"); break;
    case 98: strcat(result,"Error,no response from SME"); break;
    case 99: strcat(result,"Error,service rejected"); break;
    case 100: strcat(result,"Error,quality of service not available"); break;
    case 101: strcat(result,"Error,error in SME"); break;

    default: strcat(result,"unknown");
  }
  is_statusreport=1;
  return strlen(result);
}

/* Splits a PDU string into the parts */
/* Returns the length of the ascii string. In binary mode ascii contains the binary SMS */
int splitpdu(char* pdu, char* sendr, char* from, char* name, char* date, char* time, char* ascii, char* smsc)
{
  int Length;
  int padding;
  int Type;
  char* Pointer;
  char* start;
  char* end;
  sendr[0]=0;
  date[0]=0;
  time[0]=0;
  ascii[0]=0;
  name[0]=0;
  smsc[0]=0;
  /* Get the senders Name if given. Depends on the modem. */
  start=strstr(pdu,"\",\"");
  if (start!=0)
  {
    start+=3;
    end=strstr(start,"\",");
    if (end!=0)
    {
      *end=0;
     strcpy(name,start);
    }
  }
  else
    end=pdu;
  /* the pdu is after the first \r */
  start=strstr(end+1,"\r");
  if (start==0)
    return;
  pdu=++start;
  /* removes unwanted ctrl chars at the beginning */
  while (pdu[0] && (pdu[0]<=' '))
    pdu++;
  Pointer=pdu;
  if (strcmp(mode,"old")!=0)
  {
    // get senders smsc
    Length=octet2bin(pdu)*2-2;
    if (Length>0)
    {
      Pointer=pdu+4;
      strncpy(smsc,Pointer,Length);
      swapchars(smsc);
      /* remove Padding characters after swapping */
      if (smsc[Length-1]=='F')
        smsc[Length-1]=0;
      else
        smsc[Length]=0;
    }
    Pointer=pdu+Length+4;
  }
  if (octet2bin(Pointer)&4) // Is UDH bit set?
    with_udh=1;
  else
    with_udh=0;
  Type=octet2bin(Pointer) & 3;
  Pointer+=2;
  if (Type==0) // SMS Deliver
    return split_type_0(Pointer,sendr,date,time,ascii);
  else if (Type==2)  // Status Report
    return split_type_2(Pointer,sendr,date,time,ascii);
  else // Unsupported type
  {
    sprintf(ascii,"Message format (%i) is not supported. Cannote decode.\n%s\n",Type,pdu);
    return strlen(ascii);
  }
}


void help()
{
  printf("getsms gets the first available SMS from an GSM 07.05 compatible modem.\n");
  printf("Usage:\n");
  printf("              getsms [options]\n");
  printf("Options:\n");
  printf("              -bx  set baudrate to x (default %i)\n",baudrate);
  printf("              -c   use character set conversion\n");
  printf("              -dx  set modem device to x (default %s)\n",device);
  printf("              -ex  wait x seconds before retry (default %d)\n",errorsleeptime);
  printf("              -h   this help\n");
  printf("              -ix  modem init string x\n");
  printf("              -k   keep all SMS in the modems memory\n",device);
  printf("              -lx  use logfile x (filename or handle) (default syslog)\n");
  printf("              -Lx  use loglevel x (default %i)\n",loglevel);
  printf("              -mx  set PDU mode to x (default %s)\n",mode);
  printf("                   x can be old, new or ascii\n");
  printf("              -nx  set modem name to x\n");
  printf("              -ox  file result to write (default stdout)\n");
  printf("              -px  set pin to x (only needed if modem not initialized)\n");
  printf("              -sx  generate a Subject: line with value x\n");
  printf("              -tx  generate a To: line with value x\n");
  printf("              -V   print version info and copyright\n\n");
  printf("All options may be omitted. Output is written do stdout.\n");
  printf("After getting the SMS it will be deleted from the SIM card.\n\n");
  printf("Example:\n");
  printf("              getsms -d/dev/ttyS0 -p1234 -s\"This is a test\"\n");
  printf("Return codes:\n");
  printf("              0   received one SMS\n");
  printf("              1   cannot open device\n");
  printf("              2   cannot verify PIN or modem not ready.\n");
  printf("              3   modem not registered to the network\n");
  printf("              4   no SMS available\n");
  printf("		5   received SMS Status Report\n");
  exit(0);
}

void parsearguments(int argc,char** argv)
{
  int result;
  /* set default values */
  strcpy(device,"/dev/ttyS0");
  strcpy(mode,"new");
  pin[0]=0;
  to[0]=0;
  baudrate=19200;
  cs_convert=0;
  keep=0;
  strcpy(subject,"SMS from GSM modem");
  initstring[0]=0;
  errorsleeptime=10;
  modemname[0]=0;

  do
  {
    result=getopt(argc,argv,"n:b:e:o:l:L:khi:cd:t:s:m:p:V");
    switch (result)
    {
      case 'h': help(); 
                break;
      case 'k': keep=1;
                break;
      case 'c': cs_convert=1;
      		break;
      case 'd': strcpy(device,optarg);
                break;
      case 'e': errorsleeptime=atoi(optarg);
      		break;
      case 'b': baudrate=atoi(optarg);
                break;
      case 'i': strcpy(initstring,optarg);
      		strcat(initstring,"\r");
      		break;
      case 'l': strcpy(logfile,optarg);
                break;
      case 'L': loglevel=atoi(optarg);
                break;		
      case 'm': strcpy(mode,optarg);
                break;
      case 'n': strcpy(modemname,optarg);
                break;
      case 't': strcpy(to,optarg);
                break;
      case 's': strcpy(subject,optarg);
                break;
      case 'p': strcpy(pin,optarg);
                break;
      case 'o': strcpy(outputfile, optarg);
                break;
      case 'V': printf("Version %s, Copyright (c) 2000-2002 by Stefan Frings, s.frings@mail.isis.de\n",getsms_version);
                exit(0);
    }
  }
  while (result>0);
  
  if (modemname[0]==0)
    strcpy(modemname,device);
  
  switch (baudrate)
  {
    case 300:   baudrate=B300; break;
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
}


int main(int argc,char** argv)
{
  int userdatalength;
  int sim;
  char pdu[500];
  char ascii[500];
  char sendr[31];
  char smsc[31];
  char name[64];
  char date[9];
  char time[9];
  int found;
  char tmp[100];
  int max_memory,used_memory;
  FILE *fd;
  parsearguments(argc,argv);
  snprintf(tmp,sizeof(tmp),"getsms (%s)",modemname); tmp[sizeof(tmp)-1]=0;
  openlogfile(tmp,logfile,LOG_DAEMON,loglevel);
  writelogfile(LOG_INFO,"Checking incoming SMS");
  openmodem();
  setmodemparams();
  initmodem();
  check_memory(&used_memory,&max_memory);
  found=0;
  if (used_memory)
  {
    for (sim=1; sim<=max_memory; sim++)
    {
      if (found=getsms(sim,pdu))
      {
        // Ok, now we split the PDU string into parts and show it
        if ((strcmp(mode,"ascii")==0) || (strcmp(mode,"digicom")==0))
          userdatalength=splitascii(pdu,sendr,name,date,time,ascii);
        else
	  userdatalength=splitpdu(pdu,sendr,to,name,date,time,ascii,smsc);

	writelogfile(LOG_NOTICE, "SMS received, From: %s, Sent: %s %s",sendr,date,time);
        if (is_binary==0)
          writelogfile(LOG_DEBUG,"Message: %s", ascii);
        else
          writelogfile(LOG_DEBUG,"Message has binary data\n\n",userdatalength);

        if (outputfile[0])
	  fd = fopen(outputfile, "w");
	else
	  fd=stdout;
	if (fd)
	{
	  fprintf(fd, "From: %s\n",sendr);
	  if (smsc[0])
	    fprintf(fd,"From_SMSC: %s\n",smsc);
	  if (name[0])
	    fprintf(fd,"Name: %s\n",name);
	  if (to[0])
	    fprintf(fd, "To: %s\n",to);
	  if (date[0] || time[0])
	    fprintf(fd, "Sent: %s %s\n",date,time);
	  if (subject[0])
	    fprintf(fd, "Subject: %s\n",subject);
          if (is_binary==0)
  	    fprintf(fd, "\n%s\n",ascii);
	  else
	  {
	    if (with_udh)
	      fprintf(fd,"UDH: true\n");
	    else
	      fprintf(fd,"UDH: false\n");
	    fprintf(fd,"Binary: true\n\n");
	    if (outputfile[0])
	      fwrite(ascii,1,userdatalength,fd);
	    else
	      fprintf(fd, "binary not displayed\n");
	  }
	  fclose(fd);
	}
	else
	  writelogfile(LOG_ERR,"Cannot create file %s!", outputfile);
        if (keep==0)
          deletesms(found);
        break; // we want only get the first available SMS
      }
      // No SMS found, try next memory if not digicom
      if (strcmp(mode,"digicom")==0)
        break;
    }
  }

  tcsetattr(modem,TCSANOW,&oldtio);

  // If no SMS found return an error
  if (found==0)
  {
    writelogfile(LOG_INFO,"No SMS available");
    exit(4);
  }
  return 0;
}
#endif

