/*
 * Copyright (C) 2026 S-P Chan
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _TCP_REACTOR_H_
#define _TCP_REACTOR_H_

#include "ip_addr.h"   /* struct receive_info, snd_flags_t */
#include "tcp_mtops.h" /* tcpx_task_t - carried by TCP_R_RUN jobs */

/* TCP reactor implementation for tcp_main_threads = 2 mode */

struct tcp_connection;

/*
 * Task dispatched from PROC_TCP_MAIN to a TCP worker:
 *  - one shm block, message inline:
 *    shm_malloc(sizeof(tcp_reactor_task_t) + msg_len + 1)
 *  - a pool thread sends the pointer (uintptr_t) on ksr_tcp_reactor_dsock[1]
 *  - a worker receives it on ksr_tcp_reactor_dsock[0], runs the entry point
 *    flags select (plain SIP -> receive_msg(), F_TCP_REQ_HEP3 ->
 *    hep3_process_msg(), F_TCP_REQ_TLS_EVENT -> tls:connection-out route),
 *    then shm_free()s the task
 */
typedef struct tcp_reactor_task
{
	struct receive_info rcv;	/* copy of con->rcv at reassembly time */
	struct tcp_connection *con; /* F_TCP_REQ_TLS_EVENT: the connection of the
								 * route, holding a transient ref; the worker
								 * returns it via CONN_TLS_EVENT_DONE.
								 * message tasks: NULL */
	unsigned int msg_len;
	unsigned int flags; /* worker entry point (F_TCP_REQ_HEP3, ...); the
						 * message itself carries the rest */
	char msg_buf[0];	/* inline message buffer, null-terminated */
} tcp_reactor_task_t;

/*
 * Write request from a TCP worker to PROC_TCP_MAIN:
 *  - the worker allocates it in shm (tcp_reactor_send_put()) and sends the
 *    pointer on its unix_tcp_sock as response[0], command CONN_WRITE_REQ
 *  - tcp main (tcp_reactor_handle_write_req()) drops the worker's conn ref,
 *    stages buf on conn->wsq for a pool write job, frees buf and the request
 */
typedef struct tcp_reactor_write_req
{
	struct tcp_connection *conn; /* refcnt held by worker; tcp_main releases */
	char *buf;					 /* shm-allocated payload */
	unsigned int len;
	snd_flags_t send_flags;
} tcp_reactor_write_req_t;

/*
 * Connect request from a TCP worker to PROC_TCP_MAIN, for a dst lacking a
 * struct tcp_connection:
 *  - the worker allocates it in shm (tcp_reactor_connect_put()) and sends the
 *    pointer on its unix_tcp_sock as response[0], command CONN_CONNECT_REQ
 *  - tcp main (tcp_reactor_handle_connect_req()) looks dst up again and
 *    reuses a connection another worker opened meanwhile, else connects,
 *    creates the connection and queues the payload
 *  - for TLS the worker snapshots its tls xavp (tls_hook.connect_info): tcp
 *    main has no access to the worker's xavps
 *  - tcp main frees buf and the request
 */
typedef struct tcp_reactor_connect_req
{
	union sockaddr_union dst;  /* destination (dst->to) */
	union sockaddr_union from; /* bind source (valid only if have_from) */
	int have_from;
	int proto;			/* PROTO_TCP / PROTO_TLS / PROTO_WS / PROTO_WSS */
	int id;				/* dst->id, for the lookup in tcp main */
	int try_local_port; /* send_sock local port, for strict matching */
	snd_flags_t send_flags;
	char *buf; /* shm payload (plaintext for TLS) */
	unsigned int len;
	ksr_coninfo_t cinfo; /* tls server_name/server_id (shm), for the new conn */
} tcp_reactor_connect_req_t;

/* mode 2 [pool]: hand one reassembled message to a worker, where modes 0/1
 * call receive_tcp_msg():
 *  - copy buf, rcv and flags into a shm tcp_reactor_task_t
 *  - send the pointer on the dispatch socket
 *  - flags selects the worker entry point (e.g. F_TCP_REQ_HEP3)
 * Returns 0 when sent, -1 on error. */
int tcp_reactor_dispatch_msg(char *buf, unsigned int len, unsigned int flags,
		struct receive_info *rcv);

/* mode 2 [pool]: run the tls:connection-out route for c in a worker:
 *  - take a transient ref on c, carried by an F_TCP_REQ_TLS_EVENT task
 *  - the worker returns the ref via CONN_TLS_EVENT_DONE
 * Returns 0 when sent, -1 on error (ref dropped). */
int tcp_reactor_dispatch_tls_event(struct tcp_connection *c);

/* mode 2 [any]: pool thread index of the calling thread:
 *  - 0..N-1 on a PROC_TCP_MAIN pool thread
 *  - -1 on the io_wait thread, in other processes and in modes 0/1
 * tcp_main.c uses it to tell a pool thread from the io_wait thread. */
int tcp_reactor_pool_thread_idx(void);

/* mode 2 write entry points, called from tcp_main.c:
 *  - tcp_reactor_send_put() (tcp_send()): send on an existing connection;
 *    a worker ships a CONN_WRITE_REQ, PROC_TCP_MAIN threads queue in process
 *  - tcp_reactor_connect_put() (tcp_send()): a worker asks tcp main to open
 *    a new connection (CONN_CONNECT_REQ)
 *  - tcp_reactor_enable_write_watch(): arm POLLOUT on a hashed connection;
 *    also the TLS handshake flush in tcpconn_send_unsafe() */
int tcp_reactor_send_put(struct tcp_connection *c, const char *buf,
		unsigned len, snd_flags_t send_flags);
int tcp_reactor_connect_put(struct dest_info *dst, union sockaddr_union *from,
		const char *buf, unsigned len);
int tcp_reactor_enable_write_watch(struct tcp_connection *c);

/* mode 2 hooks, one per integration point in tcp_main.c:
 *  - handle_tcpconn_ev(): tcp_reactor_handle_tcpconn_ev()
 *  - handle_ser_child(): the CONN_* request handlers, the tcpx task request,
 *    mark_bad_put()
 *  - handle_io() / tcp_main_loop(): notify fd and handler, pool init/destroy
 *  - tcp_init_children(), before fork: tcp_reactor_dsock_init()
 * the pool, job and shield machinery stays file-static in tcp_reactor.c */
int tcp_reactor_handle_tcpconn_ev(
		struct tcp_connection *tcpconn, short ev, int fd_i);
void tcp_reactor_handle_write_req(tcp_reactor_write_req_t *wreq);
void tcp_reactor_handle_connect_req(tcp_reactor_connect_req_t *creq);
void tcp_reactor_handle_tls_event_done(struct tcp_connection *tcpconn);
void tcp_reactor_handle_script_close(int scid);
void tcp_reactor_mark_bad_put(struct tcp_connection *c);
void tcp_reactor_handle_tcpx_task_req(tcpx_task_t *task, int pidx);
void tcp_reactor_handle_notify(void);
int tcp_reactor_notify_fd(void);
int tcp_reactor_pool_init(void);
void tcp_reactor_pool_destroy(void);
int tcp_reactor_dsock_init(void);

#endif /* _TCP_REACTOR_H_ */
