/*
 * $Id$
 *
 * Copyright (C) 2012-2013 Crocodile RCS Ltd
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 *
 */

#ifndef _WS_CONN_H
#define _WS_CONN_H

#include "../../lib/kcore/kstats_wrapper.h"
#include "../../lib/kmi/tree.h"

typedef enum
{
	WS_S_CONNECTING	= 0,	/* Never used - included for completeness */
	WS_S_OPEN,
	WS_S_CLOSING,
	WS_S_CLOSED		/* Never used - included for completeness */
} ws_conn_state_t;

typedef struct ws_connection
{
	ws_conn_state_t state;
	int awaiting_pong;

	int last_used;
	struct ws_connection *used_prev;
	struct ws_connection *used_next;

	int id;			/* id and id_hash are identical to the values */
	unsigned id_hash;	/* for the corresponding TCP/TLS connection */
	struct ws_connection *id_prev;
	struct ws_connection *id_next;

	struct receive_info rcv;

	unsigned int sub_protocol;
} ws_connection_t;

typedef struct
{
	ws_connection_t *head;
	ws_connection_t *tail;
} ws_connection_used_list_t;

typedef enum
{
	WSCONN_EVENTROUTE_NO = 0,
	WSCONN_EVENTROUTE_YES
} ws_conn_eventroute_t;

extern ws_connection_used_list_t *wsconn_used_list;

extern char *wsconn_state_str[];

extern stat_var *ws_current_connections;
extern stat_var *ws_max_concurrent_connections;
extern stat_var *ws_sip_current_connections;
extern stat_var *ws_sip_max_concurrent_connections;
extern stat_var *ws_msrp_current_connections;
extern stat_var *ws_msrp_max_concurrent_connections;

int wsconn_init(void);
void wsconn_destroy(void);
int wsconn_add(struct receive_info rcv, unsigned int sub_protocol);
int wsconn_rm(ws_connection_t *wsc, ws_conn_eventroute_t run_event_route);
int wsconn_update(ws_connection_t *wsc);
void wsconn_close_now(ws_connection_t *wsc);
ws_connection_t *wsconn_get(int id);
struct mi_root *ws_mi_dump(struct mi_root *cmd, void *param);

#endif /* _WS_CONN_H */
