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
 * Task dispatched from PROC_TCP_MAIN to a TCP worker.
 *
 * Allocated in shared memory with the message buffer inline:
 *   shm_malloc(sizeof(tcp_reactor_task_t) + msg_len + 1)
 *
 * The pointer is written to ksr_tcp_reactor_dsock[1] as a uintptr_t.
 * Workers recvfrom ksr_tcp_reactor_dsock[0], cast back, dispatch on
 * flags (plain SIP -> receive_msg(), F_TCP_REQ_HEP3 -> hep3_process_msg()),
 * then shm_free the task.
 */
typedef struct tcp_reactor_task
{
	struct receive_info rcv;	/* copied from con->rcv at reassembly time */
	struct tcp_connection *con; /* F_TCP_REQ_TLS_EVENT only: the connection
								 * whose tls:connection-out route the worker runs;
								 * NULL for message tasks. A dispatch refcnt is
								 * held on it until the worker hands it back via
								 * CONN_TLS_EVENT_DONE. */
	unsigned int msg_len;
	unsigned int flags; /* framing discriminator (F_TCP_REQ_HEP3, ...); the
						 * message buffer is self-describing, so this only
						 * selects the worker entry point, it carries no state */
	char msg_buf[0];	/* inline message buffer, null-terminated */
} tcp_reactor_task_t;

/*
 * Write request sent from a TCP worker to PROC_TCP_MAIN.
 *
 * Allocated in shared memory by the worker in tcp_send(); the pointer is
 * passed to tcp_main via pt[process_no].unix_sock as response[0] with
 * command CONN_WRITE_REQ. tcp_main stages buf on conn->wsq for a pool write
 * job, releases the worker's connection refcnt, and frees both buf and the
 * request struct.
 */
typedef struct tcp_reactor_write_req
{
	struct tcp_connection *conn; /* refcnt held by worker; tcp_main releases */
	char *buf;					 /* shm-allocated payload */
	unsigned int len;
	snd_flags_t send_flags;
} tcp_reactor_write_req_t;

/*
 * Connect request sent from a TCP worker to PROC_TCP_MAIN, which owns the
 * connect, when the worker lacks a struct tcp_connection for dst.
 * Allocated in shm by the worker and passed as response[0] with command
 * CONN_CONNECT_REQ. tcp_main reuses a matching connection another worker
 * opened meanwhile, or does socket()/connect(), creates the connection and
 * queues the payload; it frees buf and the request struct.
 */
typedef struct tcp_reactor_connect_req
{
	union sockaddr_union dst;  /* destination (dst->to) */
	union sockaddr_union from; /* bind source (valid only if have_from) */
	int have_from;
	int proto;			/* PROTO_TCP / PROTO_TLS / PROTO_WS / PROTO_WSS */
	int id;				/* dst->id, for the dedup re-check */
	int try_local_port; /* send_sock local port, for strict matching */
	snd_flags_t send_flags;
	char *buf; /* shm payload (plaintext for TLS) */
	unsigned int len;
} tcp_reactor_connect_req_t;

/* Allocate a tcp_reactor_task_t in shm, copy buf+rcv+flags into it, and write
 * the pointer to the dispatch socketpair write end. Called from
 * PROC_TCP_MAIN wherever receive_tcp_msg() would be called. flags
 * carries the framing discriminator (e.g. F_TCP_REQ_HEP3) the worker uses to
 * pick the message entry point. */
int tcp_reactor_dispatch_msg(char *buf, unsigned int len, unsigned int flags,
		struct receive_info *rcv);

/* Dispatch a tls:connection-out event to a TCP worker.
 * Takes a refcnt on c, allocates a F_TCP_REQ_TLS_EVENT
 * task in shm carrying c, and writes the pointer to the dispatch socket.
 * The worker runs the route and hands the refcnt back via CONN_TLS_EVENT_DONE.
 *
 * Returns 0 on success, -1 on error  */
int tcp_reactor_dispatch_tls_event(struct tcp_connection *c);

/* Reactor pool thread index of the calling thread: 0..N-1 on a PROC_TCP_MAIN
 * pool thread, -1 otherwise (io_wait/main thread, or any other process/mode).
 * Used by tcp_main.c to tell a pool thread from the io_wait thread. */
int tcp_reactor_pool_thread_idx(void);

/* Mode-2 write entry points, called from tcp_send()/tcpconn_send_unsafe() in
 * tcp_main.c. tcp_reactor_send_put()/_connect_put() ship a write/connect
 * request from a worker to PROC_TCP_MAIN (or, when already running inside
 * PROC_TCP_MAIN, stage the payload directly); tcp_reactor_enable_write_watch()
 * arms POLLOUT on an already-hashed connection - also used directly by the
 * tcpconn_send_unsafe() TLS handshake-flush path. */
int tcp_reactor_send_put(struct tcp_connection *c, const char *buf,
		unsigned len, snd_flags_t send_flags);
int tcp_reactor_connect_put(struct dest_info *dst, union sockaddr_union *from,
		const char *buf, unsigned len);
int tcp_reactor_enable_write_watch(struct tcp_connection *c);

/* Mode-2 entry points called from tcp_main.c - one per integration point in
 * the io_wait dispatch (handle_tcpconn_ev(), handle_ser_child(),
 * tcp_main_loop(), tcp_init_children()). All of tcp_reactor.c's own pool/job/
 * shield machinery is file-static; these are the only doors into it. */
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
