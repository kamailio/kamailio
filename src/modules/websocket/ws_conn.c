/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 *
 */

#include "../../core/locking.h"
#include "../../core/str.h"
#include "../../core/tcp_conn.h"
#include "../../core/fmsg.h"
#include "../../core/counters.h"
#include "../../core/kemi.h"
#include "../../core/mem/mem.h"
#include "../../core/events.h"
#include "ws_conn.h"
#include "websocket.h"

/* Maximum number of connections to display when using the ws.dump command */
#define MAX_WS_CONNS_DUMP 50

extern int ws_verbose_list;
extern str ws_event_callback;
extern int ws_keepalive_processes;
extern int ws_rm_delay_interval;

ws_connection_t **wsconn_id_hash = NULL;
#define wsconn_listadd tcpconn_listadd
#define wsconn_listrm tcpconn_listrm

gen_lock_t *wsconn_lock = NULL;
#define WSCONN_LOCK lock_get(wsconn_lock)
#define WSCONN_UNLOCK lock_release(wsconn_lock)

#define wsconn_ref(c) atomic_inc(&((c)->refcnt))
#define wsconn_unref(c) atomic_dec_and_test(&((c)->refcnt))

gen_lock_t *wsstat_lock = NULL;

ws_connection_list_t *wsconn_used_list = NULL;

stat_var *ws_current_connections;
stat_var *ws_max_concurrent_connections;
stat_var *ws_sip_current_connections;
stat_var *ws_sip_max_concurrent_connections;
stat_var *ws_msrp_current_connections;
stat_var *ws_msrp_max_concurrent_connections;

char *wsconn_state_str[] = {
		"CONNECTING", /* WS_S_CONNECTING */
		"OPEN",		  /* WS_S_OPEN */
		"CLOSING",	/* WS_S_CLOSING */
		"CLOSED"	  /* WS_S_CLOSED */
};

/* RPC command status text */
static str str_status_bad_param = str_init("Bad display order parameter");

int wsconn_init(void)
{
	wsconn_lock = lock_alloc();
	if(wsconn_lock == NULL) {
		LM_ERR("allocating lock\n");
		goto error;
	}
	if(lock_init(wsconn_lock) == 0) {
		LM_ERR("initialising lock\n");
		goto error;
	}

	wsstat_lock = lock_alloc();
	if(wsstat_lock == NULL) {
		LM_ERR("allocating lock\n");
		goto error;
	}
	if(lock_init(wsstat_lock) == NULL) {
		LM_ERR("initialising lock\n");
		goto error;
	}

	wsconn_id_hash = (ws_connection_t **)shm_malloc(
			TCP_ID_HASH_SIZE * sizeof(ws_connection_t *));
	if(wsconn_id_hash == NULL) {
		LM_ERR("allocating WebSocket hash-table\n");
		goto error;
	}
	memset((void *)wsconn_id_hash, 0,
			TCP_ID_HASH_SIZE * sizeof(ws_connection_t *));

	wsconn_used_list = (ws_connection_list_t *)shm_malloc(
			sizeof(ws_connection_list_t));
	if(wsconn_used_list == NULL) {
		LM_ERR("allocating WebSocket used list\n");
		goto error;
	}
	memset((void *)wsconn_used_list, 0, sizeof(ws_connection_list_t));

	return 0;

error:
	if(wsconn_lock)
		lock_dealloc((void *)wsconn_lock);
	if(wsstat_lock)
		lock_dealloc((void *)wsstat_lock);
	wsconn_lock = wsstat_lock = NULL;

	if(wsconn_id_hash)
		shm_free(wsconn_id_hash);
	if(wsconn_used_list)
		shm_free(wsconn_used_list);
	wsconn_id_hash = NULL;
	wsconn_used_list = NULL;

	return -1;
}

static inline void _wsconn_rm(ws_connection_t *wsc)
{
	wsconn_listrm(wsconn_id_hash[wsc->id_hash], wsc, id_next, id_prev);

	update_stat(ws_current_connections, -1);
	if(wsc->sub_protocol == SUB_PROTOCOL_SIP)
		update_stat(ws_sip_current_connections, -1);
	else if(wsc->sub_protocol == SUB_PROTOCOL_MSRP)
		update_stat(ws_msrp_current_connections, -1);

	shm_free(wsc);
}

void wsconn_destroy(void)
{
	int h;

	if(wsconn_used_list) {
		shm_free(wsconn_used_list);
		wsconn_used_list = NULL;
	}

	if(wsconn_id_hash) {
		WSCONN_UNLOCK;
		WSCONN_LOCK;
		for(h = 0; h < TCP_ID_HASH_SIZE; h++) {
			ws_connection_t *wsc = wsconn_id_hash[h];
			while(wsc) {
				ws_connection_t *next = wsc->id_next;
				_wsconn_rm(wsc);
				wsc = next;
			}
		}
		WSCONN_UNLOCK;

		shm_free(wsconn_id_hash);
		wsconn_id_hash = NULL;
	}

	if(wsconn_lock) {
		lock_destroy(wsconn_lock);
		lock_dealloc((void *)wsconn_lock);
		wsconn_lock = NULL;
	}

	if(wsstat_lock) {
		lock_destroy(wsstat_lock);
		lock_dealloc((void *)wsstat_lock);
		wsstat_lock = NULL;
	}
}

int wsconn_add(struct receive_info *rcv, unsigned int sub_protocol)
{
	int cur_cons, max_cons;
	int id = rcv->proto_reserved1;
	int id_hash = tcp_id_hash(id);
	ws_connection_t *wsc;

	LM_DBG("connection id [%d]\n", id);

	/* Allocate and fill in new WebSocket connection */
	wsc = shm_malloc(sizeof(ws_connection_t) + BUF_SIZE + 1);
	if(wsc == NULL) {
		LM_ERR("allocating shared memory\n");
		return -1;
	}
	memset(wsc, 0, sizeof(ws_connection_t) + BUF_SIZE + 1);
	wsc->id = id;
	wsc->id_hash = id_hash;
	wsc->state = WS_S_OPEN;
	wsc->rcv = *rcv;
	wsc->sub_protocol = sub_protocol;
	wsc->run_event = 0;
	wsc->frag_buf.s = ((char *)wsc) + sizeof(ws_connection_t);
	atomic_set(&wsc->refcnt, 0);

	LM_DBG("new wsc => [%p], ref => [%d]\n", wsc,
			atomic_get(&wsc->refcnt));

	WSCONN_LOCK;
	/* Add to WebSocket connection table */
	wsconn_listadd(wsconn_id_hash[wsc->id_hash], wsc, id_next, id_prev);

	/* Add to the end of the WebSocket used list */
	wsc->last_used = (int)time(NULL);
	if(wsconn_used_list->head == NULL)
		wsconn_used_list->head = wsconn_used_list->tail = wsc;
	else {
		wsc->used_prev = wsconn_used_list->tail;
		wsconn_used_list->tail->used_next = wsc;
		wsconn_used_list->tail = wsc;
	}
	wsconn_ref(wsc);

	WSCONN_UNLOCK;

	LM_DBG("added to conn_table wsc => [%p], ref => [%d]\n", wsc,
			atomic_get(&wsc->refcnt));

	/* Update connection statistics */
	lock_get(wsstat_lock);

	update_stat(ws_current_connections, 1);
	cur_cons = get_stat_val(ws_current_connections);
	max_cons = get_stat_val(ws_max_concurrent_connections);
	if(max_cons < cur_cons)
		update_stat(ws_max_concurrent_connections, cur_cons - max_cons);

	if(wsc->sub_protocol == SUB_PROTOCOL_SIP) {
		update_stat(ws_sip_current_connections, 1);
		cur_cons = get_stat_val(ws_sip_current_connections);
		max_cons = get_stat_val(ws_sip_max_concurrent_connections);
		if(max_cons < cur_cons)
			update_stat(ws_sip_max_concurrent_connections, cur_cons - max_cons);
	} else if(wsc->sub_protocol == SUB_PROTOCOL_MSRP) {
		update_stat(ws_msrp_current_connections, 1);
		cur_cons = get_stat_val(ws_msrp_current_connections);
		max_cons = get_stat_val(ws_msrp_max_concurrent_connections);
		if(max_cons < cur_cons)
			update_stat(
					ws_msrp_max_concurrent_connections, cur_cons - max_cons);
	}

	lock_release(wsstat_lock);

	return 0;
}

static void wsconn_run_close_callback(ws_connection_t *wsc)
{
	sr_event_param_t evp = {0};
	wsc->rcv.proto_reserved1 = wsc->id;
	evp.rcv = &wsc->rcv;
	sr_event_exec(SREV_TCP_WS_CLOSE, &evp);
}

static void wsconn_run_route(ws_connection_t *wsc)
{
	int rt, backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;
	sr_kemi_eng_t *keng = NULL;
	str evrtname = str_init("websocket:closed");

	LM_DBG("wsconn_run_route event_route[websocket:closed]\n");

	rt = route_lookup(&event_rt, evrtname.s);
	if(rt < 0 || event_rt.rlist[rt] == NULL) {
		if(ws_event_callback.len <= 0 || ws_event_callback.s == NULL) {
			LM_DBG("event route does not exist");
			return;
		}
		keng = sr_kemi_eng_get();
		if(keng == NULL) {
			LM_DBG("event route callback engine does not exist");
			return;
		} else {
			rt = -1;
		}
	}

	if(faked_msg_init() < 0) {
		LM_ERR("faked_msg_init() failed\n");
		return;
	}

	fmsg = faked_msg_next();
	wsc->rcv.proto_reserved1 = wsc->id;
	fmsg->rcv = wsc->rcv;

	backup_rt = get_route_type();
	set_route_type(EVENT_ROUTE);
	init_run_actions_ctx(&ctx);
	if(rt < 0) {
		/* kemi script event route callback */
		if(keng && sr_kemi_route(keng,fmsg, EVENT_ROUTE, &ws_event_callback,
					&evrtname) < 0) {
			LM_ERR("error running event route kemi callback\n");
		}
	} else {
		/* native cfg event route */
		run_top_route(event_rt.rlist[rt], fmsg, 0);
	}
	set_route_type(backup_rt);
}

static void wsconn_dtor(ws_connection_t *wsc)
{
	if(!wsc)
		return;

	LM_DBG("wsconn_dtor for [%p] refcnt [%d]\n", wsc, atomic_get(&wsc->refcnt));

	if(wsc->run_event)
		wsconn_run_route(wsc);

	wsconn_run_close_callback(wsc);

	shm_free(wsc);

	LM_DBG("wsconn_dtor for [%p] destroyed\n", wsc);
}

int wsconn_rm(ws_connection_t *wsc, ws_conn_eventroute_t run_event_route)
{
	LM_DBG("wsconn_rm for [%p] refcnt [%d]\n", wsc, atomic_get(&wsc->refcnt));

	if(run_event_route == WSCONN_EVENTROUTE_YES)
		wsc->run_event = 1;

	return wsconn_put(wsc);
}

int wsconn_update(ws_connection_t *wsc)
{
	if(!wsc) {
		LM_ERR("wsconn_update: null pointer\n");
		return -1;
	}

	WSCONN_LOCK;
	wsc->last_used = (int)time(NULL);
	if(wsconn_used_list->tail == wsc)
		/* Already at the end of the list */
		goto end;
	if(wsconn_used_list->head == wsc)
		wsconn_used_list->head = wsc->used_next;
	if(wsc->used_prev)
		wsc->used_prev->used_next = wsc->used_next;
	if(wsc->used_next)
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

	if(wsconn_rm(wsc, WSCONN_EVENTROUTE_YES) < 0)
		LM_ERR("removing WebSocket connection\n");

	if(con == NULL) {
		LM_ERR("getting TCP/TLS connection\n");
		return;
	}

	tcpconn_put(con);
	con->send_flags.f |= SND_F_CON_CLOSE;
	con->state = S_CONN_BAD;
	con->timeout = get_ticks_raw();
}

void wsconn_detach_connection(ws_connection_t *wsc)
{
	/* Remove from the WebSocket used list */
	if(wsconn_used_list->head == wsc)
		wsconn_used_list->head = wsc->used_next;
	if(wsconn_used_list->tail == wsc)
		wsconn_used_list->tail = wsc->used_prev;
	if(wsc->used_prev)
		wsc->used_prev->used_next = wsc->used_next;
	if(wsc->used_next)
		wsc->used_next->used_prev = wsc->used_prev;

	/* remove from wsconn_id_hash */
	wsconn_listrm(wsconn_id_hash[wsc->id_hash], wsc, id_next, id_prev);

	/* stat */
	update_stat(ws_current_connections, -1);
	if(wsc->sub_protocol == SUB_PROTOCOL_SIP)
		update_stat(ws_sip_current_connections, -1);
	else if(wsc->sub_protocol == SUB_PROTOCOL_MSRP)
		update_stat(ws_msrp_current_connections, -1);
}

/* mode controls if lock needs to be aquired */
int wsconn_put_mode(ws_connection_t *wsc, int mode)
{
	if(!wsc)
		return -1;

	LM_DBG("wsconn_put start for [%p] refcnt [%d]\n", wsc,
			atomic_get(&wsc->refcnt));

	if(mode) {
		WSCONN_LOCK;
	}
	if(wsc->state == WS_S_REMOVING) {
		goto done;
	}
	/* refcnt == 0*/
	if(wsconn_unref(wsc)) {
		wsc->state = WS_S_REMOVING;
		wsc->rmticks = get_ticks();
	}
	LM_DBG("wsconn_put end for [%p] refcnt [%d]\n", wsc,
			atomic_get(&wsc->refcnt));

done:
	if(mode) {
		WSCONN_UNLOCK;
	}

	return 0;
}

/* must be called with unlocked WSCONN_LOCK */
int wsconn_put(ws_connection_t *wsc)
{
	return wsconn_put_mode(wsc, 1);
}

ws_connection_t *wsconn_get(int id)
{
	int id_hash = tcp_id_hash(id);
	ws_connection_t *wsc;

	LM_DBG("wsconn_get for id [%d]\n", id);

	WSCONN_LOCK;
	for(wsc = wsconn_id_hash[id_hash]; wsc; wsc = wsc->id_next) {
		if(wsc->id == id) {
			wsconn_ref(wsc);
			LM_DBG("wsconn_get returns wsc [%p] refcnt [%d]\n", wsc,
					atomic_get(&wsc->refcnt));

			WSCONN_UNLOCK;

			return wsc;
		}
	}
	WSCONN_UNLOCK;

	return NULL;
}

int wsconn_put_id(int id)
{
	int id_hash = tcp_id_hash(id);
	ws_connection_t *wsc;

	LM_DBG("wsconn put id [%d]\n", id);

	WSCONN_LOCK;
	for(wsc = wsconn_id_hash[id_hash]; wsc; wsc = wsc->id_next) {
		if(wsc->id == id) {
			LM_DBG("wsc [%p] refcnt [%d]\n", wsc,
					atomic_get(&wsc->refcnt));
			wsconn_put_mode(wsc, 0);

			WSCONN_UNLOCK;

			return 1;
		}
	}
	WSCONN_UNLOCK;

	return 0;
}

ws_connection_t **wsconn_get_list(void)
{
	ws_connection_t **list = NULL;
	ws_connection_t *wsc = NULL;
	size_t list_size = 0;
	size_t list_len = 0;
	size_t i = 0;

	if(ws_verbose_list)
		LM_DBG("wsconn get list - starting\n");

	WSCONN_LOCK;

	/* get the number of used connections */
	wsc = wsconn_used_list->head;
	while(wsc) {
		if(ws_verbose_list)
			LM_DBG("counter wsc [%p] prev => [%p] next => [%p]\n", wsc,
					wsc->used_prev, wsc->used_next);
		list_len++;
		wsc = wsc->used_next;
	}

	if(!list_len)
		goto end;

	/* allocate a NULL terminated list of wsconn pointers */
	list_size = (list_len + 1) * sizeof(ws_connection_t *);
	list = pkg_malloc(list_size);
	if(!list)
		goto end;

	memset(list, 0, list_size);

	/* copy */
	wsc = wsconn_used_list->head;
	for(i = 0; i < list_len; i++) {
		if(!wsc) {
			LM_ERR("Wrong list length\n");
			break;
		}

		list[i] = wsc;
		wsconn_ref(wsc);
		if(ws_verbose_list)
			LM_DBG("wsc [%p] id [%d] ref++\n", wsc, wsc->id);

		wsc = wsc->used_next;
	}
	list[i] = NULL; /* explicit NULL termination */

end:
	WSCONN_UNLOCK;

	if(ws_verbose_list)
		LM_DBG("wsconn_get_list returns list [%p]"
			   " with [%d] members\n",
				list, (int)list_len);

	return list;
}

int wsconn_put_list(ws_connection_t **list_head)
{
	ws_connection_t **list = NULL;
	ws_connection_t *wsc = NULL;

	LM_DBG("wsconn_put_list [%p]\n", list_head);

	if(!list_head)
		return -1;

	list = list_head;
	wsc = *list_head;
	while(wsc) {
		wsconn_put(wsc);
		wsc = *(++list);
	}

	pkg_free(list_head);

	return 0;
}


ws_connection_id_t *wsconn_get_list_ids(int idx)
{
	ws_connection_id_t *list = NULL;
	ws_connection_t *wsc = NULL;
	size_t list_size = 0;
	size_t list_len = 0;
	size_t i = 0;

	if(ws_verbose_list)
		LM_DBG("wsconn get list ids - starting\n");

	WSCONN_LOCK;

	/* get the number of used connections */
	wsc = wsconn_used_list->head;
	while(wsc) {
		if(wsc->id % ws_keepalive_processes == idx) {
			if(ws_verbose_list) {
				LM_DBG("counter wsc [%p] prev => [%p] next => [%p] (%d/%d)\n",
						wsc, wsc->used_prev, wsc->used_next, wsc->id, idx);
			}
			list_len++;
		}
		wsc = wsc->used_next;
	}

	if(!list_len)
		goto end;

	/* allocate a NULL terminated list of wsconn pointers */
	list_size = (list_len + 1) * sizeof(ws_connection_id_t);
	list = pkg_malloc(list_size);
	if(!list)
		goto end;

	memset(list, 0, list_size);

	/* copy */
	wsc = wsconn_used_list->head;
	for(i = 0; i < list_len; i++) {
		if(!wsc) {
			LM_ERR("Wrong list length\n");
			break;
		}

		if(wsc->id % ws_keepalive_processes == idx) {
			list[i].id = wsc->id;
			wsconn_ref(wsc);
			if(ws_verbose_list) {
				LM_DBG("wsc [%p] id [%d] (%d) - ref++\n", wsc, wsc->id, idx);
			}
		}
		wsc = wsc->used_next;
	}
	list[i].id = -1; /* explicit -1 termination */

end:
	WSCONN_UNLOCK;

	if(ws_verbose_list) {
		LM_DBG("wsconn get list id returns list [%p]"
			   " with [%d] members (%d)\n",
				list, (int)list_len, idx);
	}

	return list;
}

int wsconn_put_list_ids(ws_connection_id_t *list_head)
{
	ws_connection_id_t *list = NULL;
	int i;

	LM_DBG("wsconn put list id [%p]\n", list_head);

	if(!list_head)
		return -1;

	list = list_head;
	for(i=0; list[i].id!=-1; i++) {
		wsconn_put_id(list[i].id);
	}

	pkg_free(list_head);

	return 0;
}

void ws_timer(unsigned int ticks, void *param)
{
	ws_connection_list_t rmlist;
	ws_connection_t *wsc;
	ws_connection_t *next;
	struct tcp_connection *con = NULL;
	ticks_t nticks;
	int h;

	rmlist.head = NULL;
	nticks = get_ticks();

	WSCONN_LOCK;
	for(h = 0; h < TCP_ID_HASH_SIZE; h++) {
		wsc = wsconn_id_hash[h];
		while(wsc) {
			next = wsc->id_next;
			if(wsc->state == WS_S_REMOVING
					&& wsc->rmticks <= nticks - ws_rm_delay_interval) {
				wsconn_detach_connection(wsc);
				wsc->id_next = rmlist.head;
				rmlist.head = wsc;
			} else if(wsc->state != WS_S_REMOVING) {
				con = tcpconn_get(wsc->id, 0, 0, 0, 0);
				if(con == NULL) {
					LM_DBG("ws structure without active tcp connection\n");
					wsc->state = WS_S_REMOVING;
					wsc->rmticks = get_ticks();
				} else {
					tcpconn_put(con);
				}
			}
			wsc = next;
		}
	}
	WSCONN_UNLOCK;

	for(wsc = rmlist.head; wsc; ) {
		next = wsc->id_next;
		wsconn_dtor(wsc);
		wsc = next;
	}
}

static int ws_rpc_add_node(
		rpc_t *rpc, void *ctx, void *ih, ws_connection_t *wsc)
{
	int interval;
	char *src_proto, *dst_proto, *pong, *sub_protocol;
	char src_ip[IP6_MAX_STR_SIZE + 1], dst_ip[IP6_MAX_STR_SIZE + 1];
	struct tcp_connection *con = tcpconn_get(wsc->id, 0, 0, 0, 0);
	char rplbuf[512];

	if(con) {
		src_proto = (con->rcv.proto == PROTO_WS) ? "ws" : "wss";
		memset(src_ip, 0, IP6_MAX_STR_SIZE + 1);
		ip_addr2sbuf(&con->rcv.src_ip, src_ip, IP6_MAX_STR_SIZE);

		dst_proto = (con->rcv.proto == PROTO_WS) ? "ws" : "wss";
		memset(dst_ip, 0, IP6_MAX_STR_SIZE + 1);
		ip_addr2sbuf(&con->rcv.dst_ip, dst_ip, IP6_MAX_STR_SIZE);

		pong = wsc->awaiting_pong ? "awaiting Pong, " : "";

		interval = (int)time(NULL) - wsc->last_used;
		if(wsc->sub_protocol == SUB_PROTOCOL_SIP)
			sub_protocol = "sip";
		else if(wsc->sub_protocol == SUB_PROTOCOL_MSRP)
			sub_protocol = "msrp";
		else
			sub_protocol = "**UNKNOWN**";

		if(snprintf(rplbuf, 512, "%d: %s:%s:%hu -> %s:%s:%hu (state: %s"
								 ", %s last used %ds ago"
								 ", sub-protocol: %s)",
				   wsc->id, src_proto, strlen(src_ip) ? src_ip : "*",
				   con->rcv.src_port, dst_proto, strlen(dst_ip) ? dst_ip : "*",
				   con->rcv.dst_port, wsconn_state_str[wsc->state], pong,
				   interval, sub_protocol)
				< 0) {
			tcpconn_put(con);
			rpc->fault(ctx, 500, "Failed to print connection details");
			return -1;
		}
		if(rpc->array_add(ih, "s", rplbuf) < 0) {
			tcpconn_put(con);
			rpc->fault(ctx, 500, "Failed to add to response");
			return -1;
		}

		tcpconn_put(con);
		return 1;
	} else
		return 0;
}

void ws_rpc_dump(rpc_t *rpc, void *ctx)
{
	int h, connections = 0, truncated = 0, order = 0, found = 0;
	ws_connection_t *wsc;
	str sorder = {0};
	void *th;
	void *ih;
	void *dh;

	if(rpc->scan(ctx, "*S", &sorder) == 1) {
		if(sorder.len == 7 && strncasecmp(sorder.s, "id_hash", 7) == 0) {
			order = 0;
		} else if(sorder.len == 9
				  && strncasecmp(sorder.s, "used_desc", 9) == 0) {
			order = 1;
		} else if(sorder.len == 8
				  && strncasecmp(sorder.s, "used_asc", 8) == 0) {
			order = 2;
		} else {
			LM_WARN("bad display order parameter\n");
			rpc->fault(ctx, 400, str_status_bad_param.s);
			return;
		}
	}

	/* add root node */
	if(rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}
	if(rpc->struct_add(th, "[", "connections", &ih) < 0) {
		rpc->fault(ctx, 500, "Internal error connections structure");
		return;
	}

	WSCONN_LOCK;
	if(order == 0) {
		for(h = 0; h < TCP_ID_HASH_SIZE; h++) {
			wsc = wsconn_id_hash[h];
			while(wsc) {
				if((found = ws_rpc_add_node(rpc, ctx, ih, wsc)) < 0) {
					WSCONN_UNLOCK;
					return;
				}


				connections += found;
				if(connections >= MAX_WS_CONNS_DUMP) {
					truncated = 1;
					break;
				}

				wsc = wsc->id_next;
			}

			if(truncated == 1)
				break;
		}
	} else if(order == 1) {
		wsc = wsconn_used_list->head;
		while(wsc) {
			if((found = ws_rpc_add_node(rpc, ctx, ih, wsc)) < 0) {
				WSCONN_UNLOCK;
				return;
			}

			connections += found;
			if(connections >= MAX_WS_CONNS_DUMP) {
				truncated = 1;
				break;
			}

			wsc = wsc->used_next;
		}
	} else {
		wsc = wsconn_used_list->tail;
		while(wsc) {
			if((found = ws_rpc_add_node(rpc, ctx, ih, wsc)) < 0) {
				WSCONN_UNLOCK;
				return;
			}

			connections += found;
			if(connections >= MAX_WS_CONNS_DUMP) {
				truncated = 1;
				break;
			}

			wsc = wsc->used_prev;
		}
	}
	WSCONN_UNLOCK;

	if(rpc->struct_add(th, "{", "info", &dh) < 0) {
		rpc->fault(ctx, 500, "Internal error info structure");
		return;
	}
	if(rpc->struct_add(dh, "ds", "wscounter", connections, "truncated",
			   (truncated == 1) ? "yes" : "no")
			< 0) {
		rpc->fault(ctx, 500, "Internal error adding info structure");
		return;
	}

	return;
}
