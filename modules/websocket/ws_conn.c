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

#include "../../locking.h"
#include "../../tcp_conn.h"
#include "../../mem/mem.h"
#include "ws_conn.h"

struct ws_connection **wsconn_hash = NULL;
gen_lock_t *wsconn_lock = NULL;

char *wsconn_state_str[] =
{
	"CONNECTING",
	"OPEN",
	"CLOSING",
	"CLOSED"
};

static inline void _wsconn_rm(ws_connection_t *wsc);

int wsconn_init(void)
{
	wsconn_lock = lock_alloc();
	if (wsconn_lock == NULL)
	{
		LM_ERR("allocating lock\n");
		return -1;
	}
	if (lock_init(wsconn_lock) == 0)
	{
		LM_ERR("initialising lock\n");
		lock_dealloc((void *) wsconn_lock);
		wsconn_lock = NULL;
		return -1;
	}

	wsconn_hash =
		(ws_connection_t **) shm_malloc(TCP_ID_HASH_SIZE *
						sizeof(ws_connection_t));
	if (wsconn_hash == NULL)
	{
		LM_ERR("allocating WebSocket hash-table\n");
		lock_dealloc((void *) wsconn_lock);
		wsconn_lock = NULL;
		return -1;
	}
	memset((void *) wsconn_hash, 0,
		TCP_ID_HASH_SIZE * sizeof(ws_connection_t *));

	return 0;
}

void wsconn_destroy(void)
{
	int h;

	if (wsconn_hash)
	{
		WSCONN_UNLOCK;
		WSCONN_LOCK;
		for (h = 0; h < TCP_ID_HASH_SIZE; h++)
		{
			ws_connection_t *wsc = wsconn_hash[h];
			while (wsc)
			{
				ws_connection_t *next = wsc->next;
				_wsconn_rm(wsc);
				wsc = next;
			}
		}
		WSCONN_UNLOCK;

		shm_free(wsconn_hash);
		wsconn_hash = NULL;
	}

	if (wsconn_lock)
	{
		lock_destroy(wsconn_lock);
		lock_dealloc((void *) wsconn_lock);
		wsconn_lock = NULL;
	}
}

int wsconn_add(struct tcp_connection *con)
{
	ws_connection_t *wsc;

	if (!con)
	{
		LM_ERR("wsconn_add: null pointer\n");
		return -1;
	}

	wsc = shm_malloc(sizeof(ws_connection_t));
	if (wsc == NULL)
	{
		LM_ERR("allocating shared memory\n");
		return -1;
	}
	memset(wsc, 0, sizeof(ws_connection_t));

	wsc->con = con;
	wsc->id_hash = con->id_hash;
	wsc->last_used = (int)time(NULL);
	wsc->state = WS_S_OPEN;

	/* Make sure Kamailio core sends future messages on this connection
	   directly to this module */
	con->flags |= F_CONN_WS;

	WSCONN_LOCK;
	wsc->next = wsconn_hash[wsc->id_hash];
	wsc->prev = NULL;
	if (wsconn_hash[wsc->id_hash]) wsconn_hash[wsc->id_hash]->prev = wsc;
	wsconn_hash[wsc->id_hash] = wsc;
	WSCONN_UNLOCK;

	return 0;
}

static inline void _wsconn_rm(ws_connection_t *wsc)
{
	if (wsconn_hash[wsc->id_hash] == wsc)
		wsconn_hash[wsc->id_hash] = wsc->next;
	if (wsc->next) wsc->next->prev = wsc->prev;
	if (wsc->prev) wsc->prev->next = wsc->next;
	shm_free(wsc);
	wsc = NULL;
}

int wsconn_rm(ws_connection_t *wsc)
{
	if (!wsc)
	{
		LM_ERR("wsconn_rm: null pointer\n");
		return -1;
	}

	WSCONN_LOCK;
	_wsconn_rm(wsc);
	WSCONN_UNLOCK;

	return 0;
}

ws_connection_t *wsconn_find(struct tcp_connection *con)
{
	ws_connection_t *wsc;

	if (!con)
	{
		LM_ERR("wsconn_find: null pointer\n");
		return NULL;
	}

	WSCONN_LOCK;
	for (wsc = wsconn_hash[con->id_hash]; wsc; wsc = wsc->next)
	{
		if (wsc->id_hash == con->id_hash)
			return wsc;
	}
	WSCONN_UNLOCK;

	return NULL;
}
