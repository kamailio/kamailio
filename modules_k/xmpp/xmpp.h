/*
 * $Id$
 *
 * XMPP Module
 * This file is part of openser, a free SIP server.
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andreea Spirea
 *
 */

#ifndef _MOD_XMPP_H
#define _MOD_XMPP_H

enum xmpp_pipe_cmd_type {
	XMPP_PIPE_SEND_MESSAGE = 1,
};

struct xmpp_pipe_cmd {
	enum xmpp_pipe_cmd_type type;
	char *from, *to, *body, *id;
};


/* configuration parameters */
extern char domain_separator;
extern char *gateway_domain;
extern char *xmpp_domain;
extern char *xmpp_host;
extern int xmpp_port;
extern char *xmpp_password;

/* mod_xmpp.c */
extern int xmpp_send_sip_msg(char *from, char *to, char *msg);
extern void xmpp_free_pipe_cmd(struct xmpp_pipe_cmd *cmd);

/* util.c */
extern char *decode_sip_uri_to_jid(char *uri);
extern char *encode_sip_uri_to_jid(char *uri);
extern char *decode_jid_to_sip_uri(char *jid);
extern char *encode_jid_to_sip_uri(char *jid);
extern char *extract_domain(char *jid);
extern char *random_secret(void);
extern char *db_key(char *secret, char *domain, char *id);
 

/* xmpp_server.c */
extern int xmpp_server_child_process(int data_pipe);

/* xmpp_component.c */
extern int xmpp_component_child_process(int data_pipe);

/* sha.c */
extern char *shahash(const char *str);

#endif
