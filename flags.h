/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*!
* \file
* \brief Kamailio core :: Flag handling
* \ingroup core
* Module: \ref core
*/



#ifndef _FLAGS_H
#define _FLAGS_H

enum { FL_WHITE=1, FL_YELLOW, FL_GREEN, FL_RED, FL_BLUE, FL_MAGENTA,
	   FL_BROWN, FL_BLACK, FL_ACC, FL_MAX };

typedef unsigned int flag_t;

#define MAX_FLAG  ((unsigned int)( sizeof(flag_t) * CHAR_BIT - 1 ))

struct sip_msg;

int setflag( struct sip_msg* msg, flag_t flag );
int resetflag( struct sip_msg* msg, flag_t flag );
int isflagset( struct sip_msg* msg, flag_t flag );


/* Script flag functions. Script flags are global flags that keep their
 * value regardless of the SIP message being processed.
 */

/* Set the value of all the global flags */
int setsflagsval(flag_t val);

/* Set the given flag to 1. Parameter flag contains the index of the flag */
int setsflag(flag_t flag);

/* Reset the given flag to 0. Parameter flag contains the index of the flag */
int resetsflag(flag_t flag);

/* Returns 1 if the given flag is set and -1 otherwise */
int issflagset(flag_t flag);

/* Get the value of all the script flags combined */
flag_t getsflags(void);

int flag_in_range( flag_t flag );

int register_flag(char* name, int pos);
int get_flag_no(char* name, int len);
int check_flag(int pos);
void init_named_flags(void);

#endif
