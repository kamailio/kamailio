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


#ifndef _LIBSMS_MODEM_H
#define _LIBSMS_MODEM_H

#include <termios.h>
#include "sms_funcs.h"


#define MODE_OLD      1
#define MODE_DIGICOM  2
#define MODE_ASCII    3
#define MODE_NEW      4

#define READ_SLEEP   10000
#define READ_TIMEOUT  10

typedef int(*cds_report)( struct modem* , char* , int );


/* put_command
   Sends a command to the modem and waits max timout*0.1 seconds for an answer.
   The function returns the length of the answer.
   The answer can be Ok, ERROR or expect.
   The command may be empty or NULL  */

int put_command( struct modem *mdm, char* command, int clen, char* answer,
											int max, int timeout,char* expect);

int setmodemparams( struct modem *mdm);

int checkmodem(struct modem *mdm);

int initmodem(struct modem *mdm, cds_report cds_report_f);

int setsmsc(struct modem *mdm, char *smsc);

int openmodem(struct modem *mdm);

int closemodem(struct modem *mdm);


#endif
