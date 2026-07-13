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

#include "ip_addr.h" /* struct receive_info, snd_flags_t */
#include "tcp_cond.h" /* tcp_cond_t (pulls in <pthread.h>) - mode 2 thread pool */

struct tcp_connection;

/*
 * Task dispatched from PROC_TCP_MAIN to a TCP worker (mode 2).
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
 * Write request sent from a TCP worker to PROC_TCP_MAIN (mode 2).
 *
 * Allocated in shared memory by the worker in tcp_send(); the pointer is
 * passed to tcp_main via pt[process_no].unix_sock as response[0] with
 * command CONN_WRITE_REQ. tcp_main queues buf into conn->wbuf_q, enables
 * POLLOUT watching, releases the worker's connection refcnt, and frees
 * both buf and the request struct.
 */
typedef struct tcp_reactor_write_req
{
	struct tcp_connection *conn; /* refcnt held by worker; tcp_main releases */
	char *buf;					 /* shm-allocated payload */
	unsigned int len;
	snd_flags_t send_flags;
} tcp_reactor_write_req_t;

/*
 * Connect request sent from a TCP worker to PROC_TCP_MAIN (mode 2) when no
 * struct tcp_connection exists
 * - the worker does not perform the connect.
 * - tcp_main performs socket()/connect(), creates the struct tcp_connection,
 *   queues the payload, and watches the fd
 * - allocated in shm by the worker; passed as response[0] with command
 * - tcp_main frees buf and the request struct.
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
 * PROC_TCP_MAIN in mode 2 wherever receive_tcp_msg() would be called. flags
 * carries the framing discriminator (e.g. F_TCP_REQ_HEP3) the worker uses to
 * pick the message entry point. */
int tcp_reactor_dispatch_msg(char *buf, unsigned int len, unsigned int flags,
		struct receive_info *rcv);

/* Dispatch a tls:connection-out event to a TCP worker (mode 2).
 * Takes a refcnt on c, allocates a F_TCP_REQ_TLS_EVENT
 * task in shm carrying c, and writes the pointer to the dispatch socket.
 * The worker runs the route and hands the refcnt back via CONN_TLS_EVENT_DONE.
 *
 * Returns 0 on success, -1 on error  */
int tcp_reactor_dispatch_tls_event(struct tcp_connection *c);

/* Reactor pool thread index of the calling thread: 0..N-1 on a PROC_TCP_MAIN
 * pool thread, -1 otherwise (io_wait/main thread, or any other process/mode).
 * Exposed so the pkg-allocator guard (tcp_reactor_mem.c) can flag pkg use on a
 * pool thread without the reactor thread-local leaking outside core. */
int tcp_reactor_pool_thread_idx(void);

/* =========================================================================
 * tcp_main_threads == 2
 *
 * PROC_TCP_MAIN/io_wait owns all sockets;
 * pool threads only run read/write callbacks on connections handed to them and
 * signal completion back over a notify pipe. All jobs (read/write/run) are
 * enqueued onto task_head by the io_wait thread - which first shields the conn -
 * and pool threads wait on the PROCESS_SHARED condvar. A write is triggered by a
 * TCP worker's CONN_WRITE_REQ, but the handler (on the io_wait thread) stages the
 * plaintext and enqueues the job; the worker never links the conn onto a queue.
 * ========================================================================= */

/* job kinds */
enum tcp_reactor_op
{
	TCP_R_READ = 1,	 /* run the read callback on conn (reactor-enqueued) */
	TCP_R_WRITE = 2, /* drain conn's write staging (io_wait-enqueued) */
	TCP_R_RUN = 3	 /* run an arbitrary fn(data) on a pool thread */
};

/* A job references only the connection (+op); it never carries a payload -
 * write data lives on the connection's wsq staging list. The job holds a
 * refcount on conn (taken at enqueue, released in completion). Read/run jobs
 * are pkg-allocated by the reactor thread. */
struct tcp_reactor_job
{
	struct tcp_connection *conn;
	enum tcp_reactor_op op;
	int (*run)(void *data); /* TCP_R_RUN only */
	void *data;				/* TCP_R_RUN only */
	int resp;				/* callback result (e.g. wbuf still pending) */
	int ret;				/* completion disposition */
	struct tcp_reactor_job *next;
};

/* Wakeup condvar for the reactor pool: the io_wait thread signals it to hand
 * jobs to pool threads, which wait on it. A PROCESS_SHARED + (possibly)ROBUST condvar in
 * shm, allocated before fork; the sharing is only between the io_wait thread
 * and the pool threads inside PROC_TCP_MAIN. */
struct tcp_reactor_wake
{
	tcp_cond_t cond; /* PROCESS_SHARED + ROBUST mutex + cond */
};
extern struct tcp_reactor_wake *tcp_reactor_wake;

/* thread pool state, local to PROC_TCP_MAIN (pkg / process-private memory) */
struct tcp_reactor_pool
{
	pthread_t *threads;
	int threads_no;
	int stop;
	struct tcp_reactor_job *task_head; /* read/run jobs (reactor-enqueued) */
	struct tcp_reactor_job *task_tail;
	pthread_mutex_t done_lock; /* plain mutex: same process only */
	struct tcp_reactor_job *done_head;
	struct tcp_reactor_job *done_tail;
	int notify_pipe[2]; /* [0] watched by reactor, [1] written by threads */
};
extern struct tcp_reactor_pool tcp_rpool;

#endif /* _TCP_REACTOR_H_ */
