/*
 * $Id$
 *
 * Copyright (C) 2012 Crocodile RCS Ltd
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
 */

#ifndef _WS_CONN_H
#define _WS_CONN_H

#include "../../locking.h"
#include "../../tcp_conn.h"

typedef enum
{
	WS_S_CONNECTING	= 0,
	WS_S_OPEN,
	WS_S_CLOSING,
	WS_S_CLOSED
} ws_conn_state_t;

typedef struct ws_connection
{
	struct tcp_connection *con;

	ws_conn_state_t state;
	unsigned int id_hash;
	unsigned int last_used;

	struct ws_connection *prev;
	struct ws_connection *next;
} ws_connection_t;

extern ws_connection_t **wsconn_hash;
extern gen_lock_t *wsconn_lock;
extern char *wsconn_state_str[];

int wsconn_init(void);
void wsconn_destroy(void);
int wsconn_add(struct tcp_connection *con);
int wsconn_rm(ws_connection_t *wsc);
ws_connection_t *wsconn_find(struct tcp_connection *con);

#define WSCONN_LOCK	lock_get(wsconn_lock);
#define WSCONN_UNLOCK	lock_release(wsconn_lock);

#endif /* _WS_CONN_H */
