/*
 * $Id$
 *
 * JABBER module - headers for functions used for SIP 2 JABBER communication
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef _SIP2JABBER_H_
#define _SIP2JABBER_H_

#include "../../str.h"

typedef struct _jbconnection
{
	int sock;        // communication socket
	int port;        // port of the server
	int juid;        // internal id of the Jabber user
	int seq_nr;      // sequence number
	char *hostname;  // hosname of the Jabber server
	char *stream_id; // stream id of the session

	/****
	char *username;
	char *passwd;
	*/
	char *resource;  // resource ID
} tjbconnection, *jbconnection;

/** --- **/
jbconnection jb_init_jbconnection(char*, int);
int jb_free_jbconnection(jbconnection);

int jb_connect_to_server(jbconnection);
int jb_disconnect(jbconnection);

void jb_set_juid(jbconnection, int);
int  jb_get_juid(jbconnection);

int jb_get_roster(jbconnection);

int jb_user_auth_to_server(jbconnection, char*, char*, char*);
int jb_send_presence(jbconnection, char*, char*, char*);

int jb_send_msg(jbconnection, char*, int, char*, int);
int jb_send_sig_msg(jbconnection, char*, int, char*, int, char*, int);

char *shahash(const char *);

#endif

