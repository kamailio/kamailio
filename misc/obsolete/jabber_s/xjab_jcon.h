/*
 * $Id$
 *
 * eXtended JABber module - headers for functions used for JABBER srv connection
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifndef _XJAB_JCON_H_
#define _XJAB_JCON_H_

#include "../../str.h"
#include "xjab_jconf.h"
#include "xjab_base.h"
#include "tree234.h"
#include "xjab_presence.h"

#define XJ_NET_NUL	0
#define XJ_NET_ALL	0xFFFFFFFF
#define XJ_NET_JAB	1
#define XJ_NET_AIM	2
#define XJ_NET_ICQ	4
#define XJ_NET_MSN	8
#define XJ_NET_YAH	16

#define XJ_AIM_NAME "aim."
#define XJ_AIM_LEN  4
#define XJ_ICQ_NAME "icq"
#define XJ_ICQ_LEN  3
#define XJ_MSN_NAME "msn."
#define XJ_MSN_LEN  4
#define XJ_YAH_NAME "yahoo."
#define XJ_YAH_LEN  6

#define XJ_JMSG_NORMAL		1
#define XJ_JMSG_CHAT		2
#define XJ_JMSG_GROUPCHAT	4

#define XJ_JCMD_SUBSCRIBE	1
#define XJ_JCMD_UNSUBSCRIBE	2

typedef struct _xj_jcon
{
	int sock;			// communication socket
	int port;			// port of the server
	int juid;			// internal id of the Jabber user
	int seq_nr;			// sequence number
	char *hostname;		// hostname of the Jabber server
	char *stream_id;	// stream id of the session

	char *resource;		// resource ID

	xj_jkey jkey;		// id of connection
	int expire;			// time when the open connection is expired
	int allowed;		// allowed IM networks
	int ready;			// time when the connection is ready for sending messages

	int nrjconf;		// number of open conferences
	tree234 *jconf;		// open conferences
	xj_pres_list plist;	// presence list
} t_xj_jcon, *xj_jcon;

/** --- **/
xj_jcon xj_jcon_init(char*, int);
int xj_jcon_free(xj_jcon);

int xj_jcon_connect(xj_jcon);
int xj_jcon_disconnect(xj_jcon);

void xj_jcon_set_juid(xj_jcon, int);
int  xj_jcon_get_juid(xj_jcon);

int xj_jcon_get_roster(xj_jcon);
int xj_jcon_set_roster(xj_jcon, char*, char*);

int xj_jcon_user_auth(xj_jcon, char*, char*, char*);
int xj_jcon_send_presence(xj_jcon, char*, char*, char*, char*);
int xj_jcon_send_subscribe(xj_jcon, char*, char*, char*);

int xj_jcon_send_msg(xj_jcon, char*, int, char*, int, int);
int xj_jcon_send_sig_msg(xj_jcon, char*, int, char*, int, char*, int);

int xj_jcon_is_ready(xj_jcon, char *, int, char);

xj_jconf xj_jcon_get_jconf(xj_jcon, str*, char);
xj_jconf xj_jcon_check_jconf(xj_jcon, char*);
int xj_jcon_del_jconf(xj_jcon, str*, char, int);
int xj_jcon_jconf_presence(xj_jcon, xj_jconf, char*, char*);

/**********             ***/

int xj_jcon_set_attrs(xj_jcon, xj_jkey, int, int);
int xj_jcon_update(xj_jcon, int);

/**********             ***/

#endif

