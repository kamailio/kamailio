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
#include "../../lib/kcore/faked_msg.h"
#include "../../lib/kcore/kstats_wrapper.h"
#include "../../lib/kmi/tree.h"
#include "../../mem/mem.h"
#include "ws_conn.h"
#include "ws_mod.h"

/* Maximum number of connections to display when using the ws.dump MI command */
#define MAX_WS_CONNS_DUMP	50

ws_connection_t **wsconn_id_hash = NULL;
#define wsconn_listadd	tcpconn_listadd
#define wsconn_listrm	tcpconn_listrm

gen_lock_t *wsconn_lock = NULL;
#define WSCONN_LOCK	lock_get(wsconn_lock)
#define WSCONN_UNLOCK	lock_release(wsconn_lock)

gen_lock_t *wsstat_lock = NULL;

ws_connection_used_list_t *wsconn_used_list = NULL;

stat_var *ws_current_connections;
stat_var *ws_max_concurrent_connections;

char *wsconn_state_str[] =
{
	"CONNECTING",	/* WS_S_CONNECTING */
	"OPEN",		/* WS_S_OPEN */
	"CLOSING",	/* WS_S_CLOSING */
	"CLOSED"	/* WS_S_CLOSED */
};

/* MI command status text */
static str str_status_empty_param = str_init("Empty display order parameter");
static str str_status_bad_param = str_init("Bad display order parameter");
static str str_status_too_many_params = str_init("Too many parameters");

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

	wsconn_id_hash =
		(ws_connection_t **) shm_malloc(TCP_ID_HASH_SIZE *
						sizeof(ws_connection_t*));
	if (wsconn_id_hash == NULL)
	{
		LM_ERR("allocating WebSocket hash-table\n");
		goto error;
	}
	memset((void *) wsconn_id_hash, 0,
		TCP_ID_HASH_SIZE * sizeof(ws_connection_t *));

	wsconn_used_list = (ws_connection_used_list_t *) shm_malloc(
					sizeof(ws_connection_used_list_t));
	if (wsconn_used_list == NULL)
	{
		LM_ERR("allocating WebSocket used list\n");
		goto error;
	}
	memset((void *) wsconn_used_list, 0, sizeof(ws_connection_used_list_t));

	return 0;

error:
	if (wsconn_lock) lock_dealloc((void *) wsconn_lock);
	if (wsstat_lock) lock_dealloc((void *) wsstat_lock);
	wsconn_lock = wsstat_lock = NULL;

	if (wsconn_id_hash) shm_free(wsconn_id_hash);
	if (wsconn_used_list) shm_free(wsconn_used_list);
	wsconn_id_hash = NULL;
	wsconn_used_list = NULL;

	return -1;
}

static inline void _wsconn_rm(ws_connection_t *wsc)
{
	wsconn_listrm(wsconn_id_hash[wsc->id_hash], wsc, id_next, id_prev);
	shm_free(wsc);
	wsc = NULL;
	update_stat(ws_current_connections, -1);
}

void wsconn_destroy(void)
{
	int h;

	if (wsconn_used_list)
	{
		shm_free(wsconn_used_list);
		wsconn_used_list = NULL;
	}

	if (wsconn_id_hash)
	{
		WSCONN_UNLOCK;
		WSCONN_LOCK;
		for (h = 0; h < TCP_ID_HASH_SIZE; h++)
		{
			ws_connection_t *wsc = wsconn_id_hash[h];
			while (wsc)
			{
				ws_connection_t *next = wsc->id_next;
				_wsconn_rm(wsc);
				wsc = next;
			}
		}
		WSCONN_UNLOCK;

		shm_free(wsconn_id_hash);
		wsconn_id_hash = NULL;
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

int wsconn_add(struct receive_info rcv, unsigned int sub_protocol)
{
	int cur_cons, max_cons;
	int id = rcv.proto_reserved1;
	int id_hash = tcp_id_hash(id);
	ws_connection_t *wsc;

	/* Allocate and fill in new WebSocket connection */
	wsc = shm_malloc(sizeof(ws_connection_t));
	if (wsc == NULL)
	{
		LM_ERR("allocating shared memory\n");
		return -1;
	}
	memset(wsc, 0, sizeof(ws_connection_t));
	wsc->id = id;
	wsc->id_hash = id_hash;
	wsc->state = WS_S_OPEN;
	wsc->rcv = rcv;
	wsc->sub_protocol = sub_protocol;

	WSCONN_LOCK;
	/* Add to WebSocket connection table */
	wsconn_listadd(wsconn_id_hash[wsc->id_hash], wsc, id_next, id_prev);

	/* Add to the end of the WebSocket used list */
	wsc->last_used = (int)time(NULL);
	if (wsconn_used_list->head == NULL)
		wsconn_used_list->head = wsconn_used_list->tail = wsc;
	else
	{
		wsc->used_prev = wsconn_used_list->tail;
		wsconn_used_list->tail->used_next = wsc;
		wsconn_used_list->tail = wsc;
	}
	WSCONN_UNLOCK;

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

static void wsconn_run_route(ws_connection_t *wsc)
{
	int rt, backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;

	LM_DBG("wsconn_run_route event_route[websocket:closed]\n");

	rt = route_get(&event_rt, "websocket:closed");
	if (rt < 0 || event_rt.rlist[rt] == NULL)
	{
		LM_DBG("route does not exist");
		return;
	}

	if (faked_msg_init() < 0)
	{
		LM_ERR("faked_msg_init() failed\n");
		return;
	}
	fmsg = faked_msg_next();
	fmsg->rcv = wsc->rcv;

	backup_rt = get_route_type();
	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ctx);
	run_top_route(event_rt.rlist[rt], fmsg, 0);
	set_route_type(backup_rt);
}

int wsconn_rm(ws_connection_t *wsc, ws_conn_eventroute_t run_event_route)
{
	if (!wsc)
	{
		LM_ERR("wsconn_rm: null pointer\n");
		return -1;
	}

	if (run_event_route == WSCONN_EVENTROUTE_YES)
		wsconn_run_route(wsc);

	WSCONN_LOCK;
	/* Remove from the WebSocket used list */
	if (wsconn_used_list->head == wsc)
		wsconn_used_list->head = wsc->used_next;
	if (wsconn_used_list->tail == wsc)
		wsconn_used_list->tail = wsc->used_prev;
	if (wsc->used_prev)
		wsc->used_prev->used_next = wsc->used_next;
	if (wsc->used_next)
		wsc->used_next->used_prev = wsc->used_prev;

	_wsconn_rm(wsc);
	WSCONN_UNLOCK;

	return 0;
}

int wsconn_update(ws_connection_t *wsc)
{
	if (!wsc)
	{
		LM_ERR("wsconn_update: null pointer\n");
		return -1;
	}

	WSCONN_LOCK;
	wsc->last_used = (int) time(NULL);
	if (wsconn_used_list->tail == wsc)
		/* Already at the end of the list */
		goto end;
	if (wsconn_used_list->head == wsc)
		wsconn_used_list->head = wsc->used_next;
	if (wsc->used_prev)
		wsc->used_prev->used_next = wsc->used_next;
	if (wsc->used_next)
		wsc->used_next->used_prev = wsc->used_prev;
	wsc->used_prev = wsconn_used_list->tail;
	wsc->used_next = NULL;
	wsconn_used_list->tail->used_next = wsc;
	wsconn_used_list->tail = wsc;

end:
	WSCONN_UNLOCK;
	return 0;
}

void wsconn_close_now(ws_connection_t *wsc)
{
	struct tcp_connection *con = tcpconn_get(wsc->id, 0, 0, 0, 0);

	if (wsconn_rm(wsc, WSCONN_EVENTROUTE_YES) < 0)
		LM_ERR("removing WebSocket connection\n");

	if (con == NULL)
	{
		LM_ERR("getting TCP/TLS connection\n");
		return;
	}

	con->send_flags.f |= SND_F_CON_CLOSE;
	con->state = S_CONN_BAD;
	con->timeout = get_ticks_raw();
}

ws_connection_t *wsconn_get(int id)
{
	int id_hash = tcp_id_hash(id);
	ws_connection_t *wsc;

	WSCONN_LOCK;
	for (wsc = wsconn_id_hash[id_hash]; wsc; wsc = wsc->id_next)
	{
		if (wsc->id == id)
		{
			WSCONN_UNLOCK;
			return wsc;
		}
	}
	WSCONN_UNLOCK;

	return NULL;
}

static int add_node(struct mi_root *tree, ws_connection_t *wsc)
{
	int interval;
	char *src_proto, *dst_proto, *pong;
	char src_ip[IP6_MAX_STR_SIZE + 1], dst_ip[IP6_MAX_STR_SIZE + 1];
	struct tcp_connection *con = tcpconn_get(wsc->id, 0, 0, 0, 0);

	if (con)
	{
		src_proto = (con->rcv.proto== PROTO_WS) ? "ws" : "wss";
		memset(src_ip, 0, IP6_MAX_STR_SIZE + 1);
		ip_addr2sbuf(&con->rcv.src_ip, src_ip, IP6_MAX_STR_SIZE);

		dst_proto = (con->rcv.proto == PROTO_WS) ? "ws" : "wss";
		memset(dst_ip, 0, IP6_MAX_STR_SIZE + 1);
		ip_addr2sbuf(&con->rcv.dst_ip, dst_ip, IP6_MAX_STR_SIZE);

		pong = wsc->awaiting_pong ? "awaiting Pong, " : "";

		interval = (int)time(NULL) - wsc->last_used;

		if (addf_mi_node_child(&tree->node, 0, 0, 0,
					"%d: %s:%s:%hu -> %s:%s:%hu (state: %s"
					", %slast used %ds ago)",
					wsc->id,
					src_proto,
					strlen(src_ip) ? src_ip : "*",
					con->rcv.src_port,
					dst_proto,
					strlen(dst_ip) ? dst_ip : "*",
					con->rcv.dst_port,
					wsconn_state_str[wsc->state],
					pong,
					interval) == 0)
			return -1;

		return 1;
	}
	else
		return 0;
}

struct mi_root *ws_mi_dump(struct mi_root *cmd, void *param)
{
	int h, connections = 0, truncated = 0, order = 0, found = 0;
	ws_connection_t *wsc;
	struct mi_node *node = NULL;
	struct mi_root *rpl_tree = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	
	if (!rpl_tree)
		return 0;

	node = cmd->node.kids;
	if (node != NULL)
	{
		if (node->value.s == NULL || node->value.len == 0)
		{
			LM_WARN("empty display order parameter\n");
			return init_mi_tree(400, str_status_empty_param.s,
						str_status_empty_param.len);
		}
		strlower(&node->value);
		if (strncmp(node->value.s, "id_hash", 7) == 0)
			order = 0;
		else if (strncmp(node->value.s, "used_desc", 9) == 0)
			order = 1;
		else if (strncmp(node->value.s, "used_asc", 8) == 0)
			order = 2;
		else
		{
			LM_WARN("bad display order parameter\n");
			return init_mi_tree(400, str_status_bad_param.s,
						str_status_bad_param.len);
		}

		if (node->next != NULL)
		{
			LM_WARN("too many parameters\n");
			return init_mi_tree(400, str_status_too_many_params.s,
						str_status_too_many_params.len);
		}
	}

	WSCONN_LOCK;
	if (order == 0)
	{
		for (h = 0; h < TCP_ID_HASH_SIZE; h++)
		{
			wsc = wsconn_id_hash[h];
			while(wsc)
			{
				if ((found = add_node(rpl_tree, wsc)) < 0)
					return 0;


				connections += found;
				if (connections >= MAX_WS_CONNS_DUMP)
				{
					truncated = 1;
					break;
				}

				wsc = wsc->id_next;
			}

			if (truncated == 1)
				break;
		}
	}
	else if (order == 1)
	{
		wsc = wsconn_used_list->head;
		while (wsc)
		{
			if ((found = add_node(rpl_tree, wsc)) < 0)
				return 0;

			connections += found;
			if (connections >= MAX_WS_CONNS_DUMP)
			{
				truncated = 1;
				break;
			}

			wsc = wsc->used_next;
		}
	}
	else
	{
		wsc = wsconn_used_list->tail;
		while (wsc)
		{
			if ((found = add_node(rpl_tree, wsc)) < 0)
				return 0;

			connections += found;
			if (connections >= MAX_WS_CONNS_DUMP)
			{
				truncated = 1;
				break;
			}

			wsc = wsc->used_prev;
		}
	}
	WSCONN_UNLOCK;

	if (addf_mi_node_child(&rpl_tree->node, 0, 0, 0,
				"%d WebSocket connection%s found%s",
				connections, connections == 1 ? "" : "s",
				truncated == 1 ? "(truncated)" : "") == 0)
		return 0;

	return rpl_tree;
}
