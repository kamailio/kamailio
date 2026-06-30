/*
 * Copyright (C) 2025 S-P Chan
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
 * Workers recvfrom ksr_tcp_reactor_dsock[0], cast back, call
 * receive_msg(), then shm_free the task.
 */
typedef struct tcp_reactor_task
{
	struct receive_info rcv; /* copied from con->rcv at reassembly time */
	unsigned int msg_len;
	char msg_buf[0]; /* inline message buffer, null-terminated */
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
 * usable connection exists for a destination. The worker creates no struct and
 * opens no socket; tcp_main performs socket()/connect(), creates and hashes the
 * tcp_connection, queues the payload, and watches the fd. Allocated in shm by
 * the worker; passed as response[0] with command CONN_CONNECT_REQ. tcp_main
 * frees buf and the request struct.
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

/* Allocate a tcp_reactor_task_t in shm, copy buf+rcv into it, and write
 * the pointer to the dispatch socketpair write end. Called from
 * PROC_TCP_MAIN in mode 2 wherever receive_tcp_msg() would be called. */
int tcp_reactor_dispatch_msg(
		char *buf, unsigned int len, struct receive_info *rcv);

/* =========================================================================
 * Phase 2 - tcp_main_threads == 2 thread pool (OpenSIPS-style)
 *
 * The reactor (single io_wait thread in PROC_TCP_MAIN) owns epoll exclusively;
 * pool threads only run read/write callbacks on connections handed to them and
 * signal completion back over a notify pipe. Read/run jobs are enqueued by the
 * reactor thread (pkg memory); write jobs are submitted by TCP worker processes
 * by linking the connection onto the shm write queue and signalling the
 * PROCESS_SHARED condvar.
 * ========================================================================= */

/* job kinds (cf. OpenSIPS TCP_READ_JOB / TCP_WRITE_JOB / TCP_RUN_JOB) */
enum tcp_reactor_op
{
	TCP_R_READ = 1,	 /* run the read callback on conn (reactor-enqueued) */
	TCP_R_WRITE = 2, /* drain conn's write staging (worker-submitted) */
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

/* shared-memory write queue: a deduped list of CONNECTIONS (linked via
 * conn->wq_next), not jobs. Worker processes signal cond to submit; pool
 * threads in PROC_TCP_MAIN wait on it. Allocated in shm before fork. */
struct tcp_shared_write_queue
{
	tcp_cond_t cond; /* PROCESS_SHARED + ROBUST mutex + cond */
	struct tcp_connection *head;
	struct tcp_connection *tail;
};
extern struct tcp_shared_write_queue *tcp_reactor_wq;

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
