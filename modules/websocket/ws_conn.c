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
#include "../../str.h"
#include "../../tcp_conn.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include "../../lib/kmi/tree.h"
#include "../../mem/mem.h"
#include "ws_conn.h"
#include "ws_mod.h"

/* Maximum number of connections to display when using the ws.dump MI command */
#define MAX_WS_CONNS_DUMP	50

struct ws_connection **wsconn_hash = NULL;
gen_lock_t *wsconn_lock = NULL;
gen_lock_t *wsstat_lock = NULL;

stat_var *ws_current_connections;
stat_var *ws_max_concurrent_connections;

char *wsconn_state_str[] =
{
	"CONNECTING",	/* WS_S_CONNECTING */
	"OPEN",		/* WS_S_OPEN */
	"CLOSING",	/* WS_S_CLOSING */
	"CLOSED"	/* WS_S_CLOSED */
};

static inline void _wsconn_rm(ws_connection_t *wsc);

int wsconn_init(void)
{
	wsconn_lock = lock_alloc();
	if (wsconn_lock == NULL)
	{
		LM_ERR("allocating lock\n");
		goto error;
	}
	if (lock_init(wsconn_lock) == 0)
	{
		LM_ERR("initialising lock\n");
		goto error;
	}

	wsstat_lock = lock_alloc();
	if (wsstat_lock == NULL)
	{
		LM_ERR("allocating lock\n");
		goto error;
	}
	if (lock_init(wsstat_lock) == NULL)
	{
		LM_ERR("initialising lock\n");
		goto error;
	}

	wsconn_hash =
		(ws_connection_t **) shm_malloc(TCP_ID_HASH_SIZE *
						sizeof(ws_connection_t));
	if (wsconn_hash == NULL)
	{
		LM_ERR("allocating WebSocket hash-table\n");
		goto error;
	}
	memset((void *) wsconn_hash, 0,
		TCP_ID_HASH_SIZE * sizeof(ws_connection_t *));

	return 0;

error:
	if (wsconn_lock) lock_dealloc((void *) wsconn_lock);
	if (wsstat_lock) lock_dealloc((void *) wsstat_lock);
	wsconn_lock = wsstat_lock = NULL;

	return -1;
}

void wsconn_destroy(void)
{
	int h;

	if (wsconn_hash)
	{
		lock_release(wsconn_lock);
		lock_get(wsconn_lock);
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
		lock_release(wsconn_lock);

		shm_free(wsconn_hash);
		wsconn_hash = NULL;
	}

	if (wsconn_lock)
	{
		lock_destroy(wsconn_lock);
		lock_dealloc((void *) wsconn_lock);
		wsconn_lock = NULL;
	}

	if (wsstat_lock)
	{
		lock_destroy(wsstat_lock);
		lock_dealloc((void *) wsstat_lock);
		wsstat_lock = NULL;
	}
}

int wsconn_add(struct tcp_connection *con)
{
	int cur_cons, max_cons;
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

	lock_get(wsconn_lock);
	wsc->next = wsconn_hash[wsc->id_hash];
	wsc->prev = NULL;
	if (wsconn_hash[wsc->id_hash]) wsconn_hash[wsc->id_hash]->prev = wsc;
	wsconn_hash[wsc->id_hash] = wsc;
	lock_release(wsconn_lock);

	/* Update connection statistics */
	lock_get(wsstat_lock);
	update_stat(ws_current_connections, 1);
	cur_cons = get_stat_val(ws_current_connections);
	max_cons = get_stat_val(ws_max_concurrent_connections);
	if (max_cons < cur_cons)
		update_stat(ws_max_concurrent_connections, cur_cons - max_cons);
	lock_release(wsstat_lock);

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
	update_stat(ws_current_connections, -1);
}

int wsconn_rm(ws_connection_t *wsc)
{
	if (!wsc)
	{
		LM_ERR("wsconn_rm: null pointer\n");
		return -1;
	}

	lock_get(wsconn_lock);
	_wsconn_rm(wsc);
	lock_release(wsconn_lock);

	return 0;
}

int wsconn_update(ws_connection_t *wsc)
{
	if (!wsc)
	{
		LM_ERR("wsconn_rm: null pointer\n");
		return -1;
	}

	wsc->last_used = (int) time(NULL);
	return 0;
}

void wsconn_close_now(ws_connection_t *wsc)
{
	wsc->con->send_flags.f |= SND_F_CON_CLOSE;
	wsc->con->state = S_CONN_BAD;
	wsc->con->timeout = get_ticks_raw();
	if (wsconn_rm(wsc) < 0)
		LM_ERR("removing WebSocket connection\n");
}

ws_connection_t *wsconn_find(struct tcp_connection *con)
{
	ws_connection_t *wsc;

	if (!con)
	{
		LM_ERR("wsconn_find: null pointer\n");
		return NULL;
	}

	lock_get(wsconn_lock);
	for (wsc = wsconn_hash[con->id_hash]; wsc; wsc = wsc->next)
	{
		if (wsc->con->id == con->id)
		{
			lock_release(wsconn_lock);
			return wsc;
		}
	}

	lock_release(wsconn_lock);
	return NULL;
}

struct mi_root *ws_mi_dump(struct mi_root *cmd, void *param)
{
	int h, connections = 0, truncated = 0, interval;
	char *src_proto, *dst_proto;
	char src_ip[IP6_MAX_STR_SIZE + 1], dst_ip[IP6_MAX_STR_SIZE + 1];
	ws_connection_t *wsc;
	struct mi_root *rpl_tree = init_mi_tree(200, MI_OK_S, MI_OK_LEN);

	if (!rpl_tree)
		return 0;

	lock_get(wsconn_lock);
	for (h = 0; h < TCP_ID_HASH_SIZE; h++)
	{
		wsc = wsconn_hash[h];
		while(wsc)
		{
			if (wsc->con)
			{
				src_proto = (wsc->con->rcv.proto== PROTO_TCP)
						? "tcp" : "tls";
				memset(src_ip, 0, IP6_MAX_STR_SIZE + 1);
				ip_addr2sbuf(&wsc->con->rcv.src_ip, src_ip,
						IP6_MAX_STR_SIZE);

				dst_proto = (wsc->con->rcv.proto == PROTO_TCP)
						? "tcp" : "tls";
				memset(dst_ip, 0, IP6_MAX_STR_SIZE + 1);
				ip_addr2sbuf(&wsc->con->rcv.dst_ip, src_ip,
						IP6_MAX_STR_SIZE);

				interval = (int)time(NULL) - wsc->last_used;

				if (addf_mi_node_child(&rpl_tree->node, 0, 0, 0,
						"%d: %s:%s:%hu -> %s:%s:%hu "
						"(state: %s, "
						"last used %ds ago)",
						wsc->con->id,
						src_proto,
						strlen(src_ip) ? src_ip : "*",
						wsc->con->rcv.src_port,
						dst_proto,
						strlen(dst_ip) ? dst_ip : "*",
						wsc->con->rcv.dst_port,
						wsconn_state_str[wsc->state],
						interval) == 0)
					return 0;

				if (++connections == MAX_WS_CONNS_DUMP)
				{
					truncated = 1;
					break;
				}
			}

			wsc = wsc->next;
		}
	}
	lock_release(wsconn_lock);

	if (addf_mi_node_child(&rpl_tree->node, 0, 0, 0,
				"%d WebSocket connection%s found%s",
				connections, connections == 1 ? "" : "s",
				truncated == 1 ? "(truncated)" : "") == 0)
		return 0;

	return rpl_tree;
}
