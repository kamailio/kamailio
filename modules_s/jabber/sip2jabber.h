/*
 * $Id$
 *
 * JABBER module - headers for functions used for SIP 2 JABBER communication
 *
 */

#ifndef _SIP2JABBER_H_
#define _SIP2JABBER_H_

#include "../../str.h"

typedef struct _jbconnection
{
	int sock;
	int port;
	int seq_nr;
	char *hostname;
	char *stream_id;

	char *username;
	char *passwd;
	char *resource;
} tjbconnection, *jbconnection;

/** --- **/
jbconnection jb_init_jbconnection(char*, int);
int jb_free_jbconnection(jbconnection);

int jb_connect_to_server(jbconnection);
int jb_disconnect(jbconnection);

int jb_get_roster(jbconnection);

int jb_user_auth_to_server(jbconnection, char*, char*, char*);
int jb_send_presence(jbconnection, char*, char*, char*);

int jb_send_msg(jbconnection, char*, int, char*, int);
int jb_send_sig_msg(jbconnection, char*, int, char*, int, char*, int);

char *shahash(const char *);

#endif
