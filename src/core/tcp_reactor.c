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

/**
 * @file
 * @brief mode 2 (full tcp reactor, ksr_tcp_main_threads == 2) - pool, dispatch
 * socket, and PROC_TCP_MAIN-side job/event handling.
 *
 * This file is a leaf module: it owns all mode-2-exclusive state (the
 * dispatch socketpair, the reactor thread pool, per-connection shield/job
 * bookkeeping) and exposes a small entry-point API (tcp_reactor.h) to
 * tcp_main.c and tcp_read.c. It does not touch tcp_main.c's private io_wait
 * set (io_h) or local timer (tcp_main_ltimer) directly - see the tcpmain_*
 * wrapper prototypes and the long comment next to them in tcp_conn.h for why.
 *
 * Threads: one io_wait thread (PROC_TCP_MAIN's main thread) + N pool threads.
 * Job model:
 *  - shield (io_wait): drop c from io_h + the timer and take the in-pool ref;
 *    the ref spans chained jobs; unshield/close releases it
 *  - F_CONN_POOL_RD/_WR: a read/write job owns c; at most one of each, they
 *    may run at the same time and share the SSL object and wbuf_q under
 *    write_lock, as in mode 0
 *  - F_CONN_POOL_CLOSE: a job failed or io_wait marked c bad; the last job
 *    to complete closes c
 *  - wsq: plaintext staged under write_lock, drained by a TCP_R_WRITE job
 *    into wbuf_q (tls_encode() there); all transports write this way
 *  - completion (io_wait, tcp_reactor_handle_done()): the last job out
 *    closes c, chains a staged write, or unshields
 *  - refs: hash + in-pool + transient (a worker's write request, a
 *    tls-event dispatch); whoever drops the last one frees c
 *  - TCP_R_RUN: a connectionless tcpx task, freed by its pool thread
 * Per-field owner or lock:
 *  - io_h, tcp_main_ltimer, c->flags: the io_wait thread
 *  - c->req: the running read job (c is shielded)
 *  - c->wbuf_q, c->wsq: c->write_lock; the completion's unlocked peeks are
 *    ordered by the done_lock handoff (push under done_lock + notify byte)
 *  - c->refcnt: atomic
 * Function headers name the calling thread: mode 2 [io_wait], [pool] (a pool
 * thread running a job), [io_wait|pool], [worker] (a SIP worker process),
 * [any], [init] (process start/stop, single-threaded).
 */

/* pthread_setname_np() needs _GNU_SOURCE on glibc; must be set before any
 * include (same requirement as tcp_cond.c). */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/socket.h>
#include <sys/time.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h> /* sigfillset/pthread_sigmask - pool threads */
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>

#include "dprint.h"
#include "worker_stats.h"
#include "ip_addr.h"
#include "globals.h"
#include "locking.h"
#include "mem/mem.h"
#include "mem/shm_mem.h"
#include "mem/pkg.h"
#include "pass_fd.h"  /* send_all() */
#include "tcp_conn.h" /* struct tcp_connection, tcpconn_ref/put, F_TCP_REQ_*,
						 * tcpconn_try_unhash()/_put_destroy()/etc. and the
						 * tcpmain_* io_h/timer wrappers */
#include "tcp_reactor.h"
#include "tcp_cond.h"
#include "tcp_int_send.h" /* _tcpconn_write_nb() */
#include "tcp_stats.h"
#include "tcp_ev.h"
#include "tcp_reactor_mem.h" /* tcp_reactor_pkg_lock_install() */
#include "tcp_mtops.h"		 /* tcpx_task_t, KSR_TCPX_MAIN_PIDX */
#include "tcp_server.h"		 /* ksr_tcp_reactor_get_dispatch_wfd() */
#include "tcp_read.h"		 /* tcp_read_req(), rd_conn_flags_t, RD_CONN_* */
#include "tcp_options.h"	 /* tcp_cfg */
#include "cfg/cfg_struct.h"	 /* cfg_get() */
#include "timer.h"
#include "timer_ticks.h"
#include "local_timer.h"
#ifdef CORE_TLS
#include "tls/tls_server.h" /* tls_encode() */
#else
#include "tls_hooks.h" /* tls_encode() via TLS_HOOKS */
#endif

#ifndef TCP_ASYNC
#error "mode 2 needs TCP_ASYNC (async write queues)"
#endif

/* mode 2 reactor: shared AF_UNIX SOCK_DGRAM socketpair.
 * [0] = read end (all workers recvfrom this fd after fork)
 * [1] = write end (tcp_main sends task pointers here) */
static int ksr_tcp_reactor_dsock[2] = {-1, -1};

/* mode 2 [any]: dispatch socketpair read end (workers receive tasks) */
int ksr_tcp_reactor_get_dispatch_rfd(void)
{
	return ksr_tcp_reactor_dsock[0];
}

/* mode 2 [any]: dispatch socketpair write end (pool threads send) */
int ksr_tcp_reactor_get_dispatch_wfd(void)
{
	return ksr_tcp_reactor_dsock[1];
}

/* =========================================================================
 * Job flow inside PROC_TCP_MAIN:
 *  - the io_wait thread owns every socket, io_h and the local timer
 *  - io_wait shields c and pushes a read or write job onto job_head, then
 *    signals the pool condvar
 *  - a pool thread pops the job, runs the read or write on c, and hands the
 *    job back on done_head plus one byte on the notify pipe
 *  - io_wait completes the job (tcp_reactor_handle_done()): close, chain a
 *    write, or unshield
 *  - a worker's write arrives as CONN_WRITE_REQ: the io_wait handler stages
 *    the plaintext on c->wsq and pushes the job; workers stay off the queues
 * ========================================================================= */

/* job kinds; the io_wait thread enqueues all three */
enum tcp_reactor_op
{
	TCP_R_READ = 1,	 /* read + reassemble conn, dispatch to workers */
	TCP_R_WRITE = 2, /* move conn->wsq into wbuf_q, flush wbuf_q */
	TCP_R_RUN = 3,	 /* run a tcpx_task_t; conn is NULL */
};

/* one pool job (pkg, allocated by the io_wait thread):
 *  - TCP_R_READ/_WRITE: conn + op; the write data stays on conn->wsq
 *  - the in-pool ref taken at shield covers every job on conn
 *  - the io_wait completion frees a READ/WRITE job
 *  - TCP_R_RUN: task + pidx, conn NULL; its pool thread runs task->exec()
 *    and frees the job (tcp_reactor_handle_tcpx_task_req()) */
struct tcp_reactor_job
{
	struct tcp_connection *conn;
	enum tcp_reactor_op op;
	/* READ: tcp_read_req() result (< 0 = close);
	 * WRITE: -1 close, 0 wbuf_q drained, 1 wbuf_q holds data */
	int resp;
	tcpx_task_t *task; /* TCP_R_RUN only */
	int pidx;		   /* TCP_R_RUN only: requesting process, for the result */
	struct tcp_reactor_job *next;
};

/* pool wakeup condvar:
 *  - the io_wait thread signals it after each job push
 *  - idle pool threads wait on it; tcp_reactor_pool_destroy() broadcasts stop
 *  - users: the threads of PROC_TCP_MAIN
 *  - tcp_cond_t is PROCESS_SHARED (+ ROBUST where supported), so it lives in
 *    shm, allocated in the main process before fork (tcp_reactor_dsock_init()) */
struct tcp_reactor_wake
{
	tcp_cond_t cond; /* PROCESS_SHARED + ROBUST mutex + cond */
};
/* mode 2 reactor pool wakeup condvar (shm) */
static struct tcp_reactor_wake *tcp_reactor_wake = NULL;

/* thread pool state, local to PROC_TCP_MAIN (pkg / process-private memory) */
struct tcp_reactor_pool
{
	pthread_t *threads;
	int threads_no;
	int stop;
	struct tcp_reactor_job *job_head; /* pending jobs, pushed by io_wait */
	struct tcp_reactor_job *job_tail;
	pthread_mutex_t done_lock;		   /* plain mutex: same process */
	struct tcp_reactor_job *done_head; /* finished jobs (pool threads) */
	struct tcp_reactor_job *done_tail;
	int notify_pipe[2]; /* [0] io_wait reads (io_h), [1] pool threads write */
};
static struct tcp_reactor_pool tcp_rpool;

/* mode 2 [io_wait]: read end of the pool's completion notify pipe */
int tcp_reactor_notify_fd(void)
{
	return tcp_rpool.notify_pipe[0];
}

/* number of reactor pool threads; configurable via the tcp_reactor_threads
 * cfg.y directive (assigns through the extern in globals.h), default 8. */
int ksr_tcp_reactor_threads = 8;

/* per-pool-thread index (0..N-1); -1 in the io_wait thread and elsewhere.
 * Tells a pool thread from the io_wait thread (both satisfy is_tcp_main()) */
static _Thread_local int tcp_reactor_thread_idx = -1;

/* mode 2 [any]: this pool thread's index, -1 on other threads */
int tcp_reactor_pool_thread_idx(void)
{
	return tcp_reactor_thread_idx;
}

/* dispatch socket framing:
 *  - AF_UNIX SOCK_DGRAM: one send() = one datagram = one task pointer
 *  - each worker recv() returns one whole datagram, so concurrent workers
 *    each get a complete pointer
 *  - the assert keeps the pointer within PIPE_BUF (sanity bound) */
_Static_assert(sizeof(uintptr_t) <= PIPE_BUF,
		"task pointer too wide for atomic dispatch write");

/* mode 2 [pool]: send a task pointer to the workers on the dispatch socket.
 *  - EINTR: retry
 *  - socket full (EAGAIN, workers behind): poll up to
 *    TCP_REACTOR_DISPATCH_POLL_TRIES x TCP_REACTOR_DISPATCH_POLL_MS for room,
 *    then drop the task (counts reactor_dispatch_drops)
 *  - other errors: fail
 * Returns 0 when sent, -1 on failure (the caller frees the task). */
#define TCP_REACTOR_DISPATCH_POLL_MS 100
#define TCP_REACTOR_DISPATCH_POLL_TRIES 5
static int tcp_reactor_dispatch_send(uintptr_t ptr)
{
	int wfd = ksr_tcp_reactor_get_dispatch_wfd();
	int tries = 0;
	ssize_t sent;
	struct pollfd pfd;

	for(;;) {
		sent = send(wfd, &ptr, sizeof(ptr), 0);
		if(sent == (ssize_t)sizeof(ptr))
			return 0;
		if(sent < 0 && errno == EINTR)
			continue; /* interrupted before anything was sent - retry */
		if(sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			if(tries == 0)
				counter_inc(ksr_cnt_rdispatch_waits);
			if(tries++ >= TCP_REACTOR_DISPATCH_POLL_TRIES) {
				LM_WARN("dispatch socket full: workers not draining after"
						" %d x %dms - dropping task\n",
						TCP_REACTOR_DISPATCH_POLL_TRIES,
						TCP_REACTOR_DISPATCH_POLL_MS);
				counter_inc(ksr_cnt_rdispatch_drops);
				return -1;
			}
			pfd.fd = wfd;
			pfd.events = POLLOUT;
			pfd.revents = 0;
			poll(&pfd, 1, TCP_REACTOR_DISPATCH_POLL_MS); /* wait for drain */
			continue;
		}
		LM_ERR("dispatch send failed (%s)\n",
				(sent < 0) ? strerror(errno) : "short write");
		return -1;
	}
}

/*
 * mode 2 [pool]: hand one reassembled message to a worker.
 *  - caller: the TCP_R_READ job, where modes 0/1 call receive_tcp_msg();
 *    WS calls it from ws_deliver_sip() in the same job
 *  - copies buf, rcv and flags into a shm tcp_reactor_task_t and sends the
 *    pointer on the dispatch socket
 *  - the worker picks the entry point from task->flags (plain SIP ->
 *    receive_msg(), F_TCP_REQ_HEP3 -> hep3_process_msg()) and frees the task
 * Returns 0 when the task is sent, -1 on error (task freed here).
 */
int tcp_reactor_dispatch_msg(char *buf, unsigned int len, unsigned int flags,
		struct receive_info *rcv)
{
	tcp_reactor_task_t *task;
	uintptr_t ptr;

	task = shm_malloc(sizeof(tcp_reactor_task_t) + len + 1);
	if(task == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	task->rcv = *rcv;
	task->msg_len = len;
	task->flags = flags;
	memcpy(task->msg_buf, buf, len);
	task->msg_buf[len] = '\0';

	ptr = (uintptr_t)task;
	if(tcp_reactor_dispatch_send(ptr) < 0) {
		shm_free(task);
		return -1;
	}
	return 0;
}

/* mode 2 [pool]: send a tls:connection-out event for c to a worker.
 *  - takes a transient ref on c for the hop
 *  - the worker runs the route and returns the ref via CONN_TLS_EVENT_DONE
 *    (tcp_reactor_handle_tls_event_done() drops it)
 * Returns 0 when sent, -1 on error (ref dropped here). */
int tcp_reactor_dispatch_tls_event(struct tcp_connection *c)
{
	tcp_reactor_task_t *task;
	uintptr_t ptr;

	/* transient ref: the worker reads c's cached TLS metadata. The calling
	 * read job holds the in-pool ref, so the error-path puts keep c alive */
	tcpconn_ref(c);

	task = shm_malloc(sizeof(tcp_reactor_task_t) + 1);
	if(task == NULL) {
		SHM_MEM_ERROR;
		tcpconn_put(c);
		return -1;
	}
	task->rcv = c->rcv;
	task->con = c;
	task->msg_len = 0;
	task->flags = F_TCP_REQ_TLS_EVENT;
	task->msg_buf[0] = '\0';

	ptr = (uintptr_t)task;
	if(tcp_reactor_dispatch_send(ptr) < 0) {
		shm_free(task);
		tcpconn_put(c);
		return -1;
	}
	return 0;
}

/* ===========================================================================
 * Write-queue staging, shield/unshield, and per-connection reactor helpers.
 * ========================================================================= */


/* mode 2 [io_wait|pool]: append buf (wire bytes) to c's wbuf_q.
 *  - direct = 0 (io_wait): queue all of buf, capped by tcpconn_wq_max
 *  - direct = 1 (pool write job): write to the socket first when wbuf_q is
 *    empty and c is connected, queue the unwritten rest
 *  - direct: a write error queues the rest too; the job's wbufq_run() then
 *    reports the error
 *  - direct chunks skip the per-conn cap: tcp_reactor_wsq_add() applied it
 * Returns 0, or -1 when _wbufq_add_cap() refuses (queue full / timeout). */
static int tcp_reactor_wbuf_put_locked(
		struct tcp_connection *c, const char *buf, unsigned len, int direct)
{
	int n = 0;

	if(direct && _wbufq_empty(c) && c->state != S_CONN_CONNECT) {
		n = _tcpconn_write_nb(c->s, c, buf, len);
		if(n < 0)
			n = 0;
		else if(n > 0 && c->state == S_CONN_ACCEPT) {
			TCP_STATS_ESTABLISHED(c->state);
			c->state = S_CONN_OK;
		}
		if((unsigned)n == len)
			return 0;
	}
	return _wbufq_add_cap(c, buf + n, len - n, !direct);
}

/* mode 2 [io_wait|pool]: append a plaintext payload to c's wbuf_q; the caller
 * holds c->write_lock.
 *  - TLS/WSS: tls_encode() it chunk by chunk, queue the records
 *  - TCP/WS: queue it as is
 *  - direct = 1: write-through first (tcp_reactor_wbuf_put_locked()); for the
 *    pool thread that owns c
 * Returns 0, -1 on error (c marked S_CONN_BAD, timeout forced). */
static int tcp_reactor_wbuf_add_locked(struct tcp_connection *c, char *buf,
		unsigned len, snd_flags_t send_flags, int direct)
{
#ifdef USE_TLS
	const char *t_buf, *rest_buf;
	unsigned t_len, rest_len;
	snd_flags_t t_send_flags;
	int wn;

	if(unlikely(c->type == PROTO_TLS || c->type == PROTO_WSS)) {
		t_buf = buf;
		t_len = len;
		do {
			t_send_flags = send_flags;
			wn = tls_encode(
					c, &t_buf, &t_len, &rest_buf, &rest_len, &t_send_flags);
			if(unlikely((wn < 0)
						|| (t_len
								&& (tcp_reactor_wbuf_put_locked(
											c, t_buf, t_len, direct)
										< 0)))) {
				c->state = S_CONN_BAD;
				c->timeout = get_ticks_raw(); /* force timeout */
				return -1;
			}
			/* tls_encode() clears SND_F_CON_CLOSE on all but the last chunk */
			tcpconn_set_send_flags(c, t_send_flags);
			t_buf = rest_buf;
			t_len = rest_len;
		} while(unlikely(rest_len && wn > 0));
		return 0;
	}
#endif /* USE_TLS */
	if(unlikely(
			   len && (tcp_reactor_wbuf_put_locked(c, buf, len, direct) < 0))) {
		c->state = S_CONN_BAD;
		c->timeout = get_ticks_raw(); /* force timeout */
		return -1;
	}
	tcpconn_set_send_flags(c, send_flags);
	return 0;
}

/* mode 2 [io_wait]: tcp_reactor_wbuf_add_locked() with direct = 0 under
 * c->write_lock, for callers outside the lock (send_put on io_wait, a new
 * outbound connection). Same 0/-1 return; the mode 0/1 counterpart is the TLS
 * branch of tcpconn_send_put() in tcp_main.c. */
static int tcp_reactor_wbuf_enqueue(struct tcp_connection *c, char *buf,
		unsigned len, snd_flags_t send_flags)
{
	int ret;

	lock_get(&c->write_lock);
	ret = tcp_reactor_wbuf_add_locked(c, buf, len, send_flags, 0);
	lock_release(&c->write_lock);
	return ret;
}

/* mode 2 [io_wait|pool]: watch POLLOUT on a hashed c whose wbuf_q holds data
 * (the mode 2 form of handle_ser_child()'s CONN_QUEUED_WRITE arm).
 *  - pool-owned c: return 0 untouched; the job completion re-arms POLLOUT
 *  - pull the local timer in to the write timeout when that comes first
 *  - add c->s to io_h with POLLOUT, or add POLLOUT to its POLLIN watch
 *  - io_watch failure: clear F_CONN_WRITE_W, return -1, keep c alive (the TLS
 *    read path uses c afterwards)
 * Returns 0 on success or when watched, -1 on io_watch failure. */
int tcp_reactor_enable_write_watch(struct tcp_connection *c)
{
	ticks_t t;

	/* first, before any c->flags write: a pool thread reaches here from a TLS
	 * or CRLF-pong write-back, and c->flags belongs to the io_wait thread */
	if(tcpconn_pool_busy(c))
		return 0;
	if(c->flags & F_CONN_WANTS_WR)
		return 0;
	c->flags |= F_CONN_WANTS_WR;
	t = get_ticks_raw();
	if(likely((c->flags & F_CONN_MAIN_TIMER)
			   && (TICKS_LT(c->wbuf_q.wr_timeout, c->timeout))
			   && TICKS_LT(t, c->wbuf_q.wr_timeout))) {
		tcpmain_local_timer_del(&c->timer);
		local_timer_reinit(&c->timer);
		tcpmain_local_timer_add(&c->timer, c->wbuf_q.wr_timeout - t, t);
	}
	if(!(c->flags & F_CONN_WRITE_W)) {
		c->flags |= F_CONN_WRITE_W;
		if(!(c->flags & F_CONN_READ_W)) {
			if(unlikely(tcpmain_io_watch_add_conn(c->s, POLLOUT, c) < 0)) {
				LM_CRIT("failed to enable write watch (reactor)\n");
				c->flags &= ~F_CONN_WRITE_W;
				return -1;
			}
		} else {
			if(unlikely(tcpmain_io_watch_chg(c->s, POLLIN | POLLOUT, -1) < 0)) {
				LM_CRIT("failed to change socket watch events (reactor)\n");
				c->flags &= ~F_CONN_WRITE_W;
				return -1;
			}
		}
	}
	return 0;
}

/* mode 2 [io_wait]: enable POLLOUT watching, destroying the connection on
 * io_watch failure. Used by the IPC write/connect handlers, which do not touch
 * c after this returns. Returns 0 on success, -1 on error (c unhashed and
 * destroyed). */
static int tcp_reactor_watch_write(struct tcp_connection *c)
{
	if(unlikely(tcp_reactor_enable_write_watch(c) < 0)) {
		if(likely(tcpconn_try_unhash(c))) {
			if((c->flags & F_CONN_READ_W) && (c->s != -1)) {
				tcpmain_io_watch_del(c->s, -1, 1);
				c->flags &= ~F_CONN_READ_W;
			}
			tcpconn_put_destroy(c);
		} else {
			BUG("unhashed connection watched for IO\n");
		}
		return -1;
	}
	return 0;
}

/* mode 2 [io_wait]: bound a partially read message by tcp_msg_read_timeout.
 *  - applies when the timeout is set, c is S_CONN_OK and a message start is
 *    recorded (c->req.tvrstart)
 *  - pulls c->timeout in to message start + ksr_tcp_msg_read_timeout
 *  - re-arms c->timer there, so the local timer of PROC_TCP_MAIN reaps the
 *    stalled read (ahead of the tcp_timer_check_connections() scan)
 * t = current tick.
 */
static void tcp_reactor_arm_read_timeout(struct tcp_connection *c, ticks_t t)
{
	struct timeval tvnow;
	long long elapsed_us, remaining_ms;
	ticks_t deadline;

	if(likely(ksr_tcp_msg_read_timeout <= 0 || c->state != S_CONN_OK
			   || c->req.tvrstart.tv_sec == 0))
		return;
	gettimeofday(&tvnow, NULL);
	elapsed_us = 1000000LL * (tvnow.tv_sec - c->req.tvrstart.tv_sec)
				 + (tvnow.tv_usec - c->req.tvrstart.tv_usec);
	remaining_ms = (1000000LL * ksr_tcp_msg_read_timeout - elapsed_us) / 1000LL;
	if(remaining_ms < 0)
		remaining_ms = 0;
	deadline = t + MS_TO_TICKS((ticks_t)remaining_ms);
	if(TICKS_LT(deadline, c->timeout))
		c->timeout = deadline;
	/* re-arm the local timer to (possibly) fire earlier at the read deadline */
	if(likely(c->flags & F_CONN_MAIN_TIMER)) {
		tcpmain_local_timer_del(&c->timer);
		local_timer_reinit(&c->timer);
		tcpmain_local_timer_add(&c->timer, c->timeout - t, t);
	}
}

/* mode 2: owner bit of a read/write job in c->flags */
#define TCP_REACTOR_OP_FLAG(op) \
	((op) == TCP_R_READ ? F_CONN_POOL_RD : F_CONN_POOL_WR)

/* mode 2 [io_wait]: append job to the pool job queue and wake a pool thread */
static void tcp_reactor_push_job(struct tcp_reactor_job *job)
{
	tcp_cond_lock(&tcp_reactor_wake->cond);
	job->next = NULL;
	if(tcp_rpool.job_tail != NULL)
		tcp_rpool.job_tail->next = job;
	else
		tcp_rpool.job_head = job;
	tcp_rpool.job_tail = job;
	tcp_cond_signal(&tcp_reactor_wake->cond);
	tcp_cond_unlock(&tcp_reactor_wake->cond);
}

/* mode 2 [io_wait]: enqueue a read/write job for c onto the pool job queue and
 * set its owner bit; the caller has shielded c. 0/-1 */
static int tcp_reactor_enqueue_job(
		struct tcp_connection *c, enum tcp_reactor_op op)
{
	struct tcp_reactor_job *job;

	job = pkg_malloc(sizeof(*job));
	if(unlikely(job == NULL)) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(job, 0, sizeof(*job));
	job->conn = c;
	job->op = op;
	c->flags |= TCP_REACTOR_OP_FLAG(op);
	tcp_reactor_push_job(job);
	return 0;
}

/* mode 2 [io_wait]: enqueue a TCP_R_RUN job carrying task; connectionless, so
 * it skips the shield and the ref. Returns 0, -1 on pkg OOM. */
static int tcp_reactor_enqueue_run(tcpx_task_t *task, int pidx)
{
	struct tcp_reactor_job *job;

	job = pkg_malloc(sizeof(*job));
	if(unlikely(job == NULL)) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(job, 0, sizeof(*job));
	job->op = TCP_R_RUN;
	job->task = task;
	job->pidx = pidx;
	tcp_reactor_push_job(job);
	return 0;
}

/* mode 2 [io_wait]: run a tcpx task (tcp_mtops.h) from another process on a
 * pool thread.
 *  - sender: e.g. the RPC process running tls.reload
 *  - exec() gets the requester's pidx; ksr_tcpx_thread_eresult(rtask, pidx)
 *    sends the result back to it
 */
void tcp_reactor_handle_tcpx_task_req(tcpx_task_t *task, int pidx)
{
	if(unlikely(tcp_reactor_enqueue_run(task, pidx) < 0)) {
		/* pkg OOM: run the task here on the io_wait thread; the sender waits
		 * in ksr_tcpx_task_result_recv() for its result */
		LM_ERR("failed to enqueue tcpx task %p - running inline\n",
				(void *)task);
		if(task->exec)
			task->exec(task->param, pidx);
	}
}

/* mode 2 [io_wait]: shield c - hand it to the pool.
 *  - pool-owned c: return 0 as is (the in-pool ref is held)
 *  - remove c->s from io_h (fd_i: its io_h index, -1 = look it up)
 *  - remove c from the local timer, clear the watch flags
 *  - take the in-pool ref; unshield or pool_close releases it, chained jobs
 *    share it
 *  - tcp_reactor_enqueue_job() then sets the job's owner bit
 * Returns 0, -1 on io_watch error. */
static int tcp_reactor_shield(struct tcp_connection *c, int fd_i)
{
	if(tcpconn_pool_busy(c))
		return 0;
	if((c->flags & (F_CONN_READ_W | F_CONN_WRITE_W)) && (c->s != -1)) {
		if(unlikely(tcpmain_io_watch_del(c->s, fd_i, 0) < 0)) {
			LM_ERR("reactor: io_watch_del (shield) failed for %p fd %d\n", c,
					c->s);
			return -1;
		}
	}
	if(c->flags & F_CONN_MAIN_TIMER) {
		tcpmain_local_timer_del(&c->timer);
		c->flags &= ~F_CONN_MAIN_TIMER;
	}
	c->flags &= ~(
			F_CONN_READ_W | F_CONN_WRITE_W | F_CONN_WANTS_RD | F_CONN_WANTS_WR);
	tcpconn_ref(c); /* in-pool ref: released at unshield/close */
	return 0;
}

/* mode 2 [io_wait]: mark c bad for closing.
 *  - set S_CONN_BAD and force the timeout
 *  - pool-owned c: also set F_CONN_POOL_CLOSE; a running job writes c->state
 *    (e.g. S_CONN_ACCEPT -> S_CONN_OK), the flag belongs to io_wait and keeps
 *    the close request for the job completion */
static void tcp_reactor_mark_bad(struct tcp_connection *c)
{
	c->state = S_CONN_BAD;
	c->timeout = get_ticks_raw(); /* force reaper */
	if(tcpconn_pool_busy(c))
		c->flags |= F_CONN_POOL_CLOSE;
}

/* mode 2 [io_wait]: for a pool-owned c:
 *  - mark it bad; the last job's completion closes it
 *  - drop the caller's ref; the in-pool ref keeps c alive */
void tcp_reactor_mark_bad_put(struct tcp_connection *c)
{
	tcp_reactor_mark_bad(c);
	if(unlikely(tcpconn_put(c)))
		tcpconn_destroy(c); /* defensive: the in-pool ref is held */
}

/* mode 2 [pool]: take (take = 1) or release a job's owner stamp on c.
 *  - c->rd_owner / c->wr_owner = pool thread index + 1, 0 = free
 *  - take on a set stamp = two jobs of one kind on c: BUG() */
static void tcp_reactor_stamp(struct tcp_connection *c, int op, int take)
{
	int *owner = (op == TCP_R_READ) ? &c->rd_owner : &c->wr_owner;

	if(take && unlikely(*owner != 0))
		BUG("conn %p: %s job on pool thread %d, already owned by %d\n", c,
				(op == TCP_R_READ) ? "read" : "write", tcp_reactor_thread_idx,
				*owner - 1);
	*owner = take ? tcp_reactor_thread_idx + 1 : 0;
}

/* mode 2 [io_wait]: start a write job for c's staged data while its read job
 * runs; the read's shield and in-pool ref cover it.
 * Returns 1 when the write job is queued, 0 when the read job is gone, a
 * write job or close is pending, c is bad or unhashed, wsq is empty, or OOM. */
static int tcp_reactor_write_beside_read(struct tcp_connection *c)
{
	if(!(c->flags & F_CONN_POOL_RD)
			|| (c->flags & (F_CONN_POOL_WR | F_CONN_POOL_CLOSE))
			|| c->state == S_CONN_BAD || !(c->flags & F_CONN_HASHED)
			|| c->wsq_head == NULL)
		return 0;
	if(unlikely(tcp_reactor_enqueue_job(c, TCP_R_WRITE) < 0))
		return 0; /* data waits in wsq; the read completion chains it */
	counter_inc(ksr_cnt_rrw_overlap);
	return 1;
}

/* mode 2 [any]: non-blocking check that fd has input (or EOF/error) now. 1/0 */
static int tcp_reactor_sock_readable(int fd)
{
	struct pollfd pfd;

	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	return (poll(&pfd, 1, 0) == 1);
}

/* mode 2 [io_wait]: start a read job for c's pending input while its write job
 * runs, so reads keep flowing under a steady stream of writes.
 * Returns 1 when the read job is queued, 0 when the write job is gone, a
 * read job or close is pending, c is bad or unhashed, input is empty, or OOM. */
static int tcp_reactor_read_beside_write(struct tcp_connection *c)
{
	if(!(c->flags & F_CONN_POOL_WR)
			|| (c->flags & (F_CONN_POOL_RD | F_CONN_POOL_CLOSE))
			|| c->state == S_CONN_BAD || !(c->flags & F_CONN_HASHED)
			|| !tcp_reactor_sock_readable(c->s))
		return 0;
	if(unlikely(tcp_reactor_enqueue_job(c, TCP_R_READ) < 0))
		return 0; /* the write completion re-arms POLLIN */
	counter_inc(ksr_cnt_rrw_overlap);
	return 1;
}

/* mode 2 [io_wait]: a write was staged on a pool-owned c: start a write job
 * beside its read, else count it as waiting behind the read
 * (reactor_writes_behind_read) */
static void tcp_reactor_staged_on_busy(struct tcp_connection *c)
{
	if(!tcp_reactor_write_beside_read(c) && (c->flags & F_CONN_POOL_RD)
			&& !(c->flags & F_CONN_POOL_WR))
		counter_inc(ksr_cnt_rwrites_behind_read);
}

/* mode 2 [any]: non-blocking check that fd accepts more data now. 1/0 */
static int tcp_reactor_sock_writable(int fd)
{
	struct pollfd pfd;

	pfd.fd = fd;
	pfd.events = POLLOUT;
	pfd.revents = 0;
	return (poll(&pfd, 1, 0) == 1) && (pfd.revents == POLLOUT);
}

/* mode 2 [io_wait|pool]: copy a plaintext chunk onto c->wsq (shm, under
 * c->write_lock); a TCP_R_WRITE job moves it into wbuf_q.
 *  - io_wait: worker writes (CONN_WRITE_REQ / CONN_CONNECT_REQ reuse)
 *  - pool: write-backs from the read path (WS pong/close, CRLF pong)
 * Returns 0, -1 on shm OOM or a full write queue (see the cap below). */
static int tcp_reactor_wsq_add(struct tcp_connection *c, const char *buf,
		unsigned len, snd_flags_t send_flags)
{
	struct tcp_wchunk *ch;
	unsigned int queued, staged;

	ch = shm_malloc(sizeof(*ch));
	if(unlikely(ch == NULL)) {
		SHM_MEM_ERROR;
		return -1;
	}
	ch->buf = shm_malloc(len ? len : 1);
	if(unlikely(ch->buf == NULL)) {
		SHM_MEM_ERROR;
		shm_free(ch);
		return -1;
	}
	if(len)
		memcpy(ch->buf, buf, len);
	ch->len = len;
	ch->send_flags = send_flags;
	ch->next = NULL;
	lock_get(&c->write_lock);
	/* write queue cap (tcpconn_wq_max), refuse when all three hold:
	 *  - wbuf_q holds data (an earlier write left bytes behind)
	 *  - wbuf_q + wsq + len exceeds the cap
	 *  - the socket is full now, i.e. the peer applies backpressure
	 * a wsq backlog waiting for its write job on a writable socket passes,
	 * as mode 0 caps on the socket-full state */
	if(unlikely(_wbufq_non_empty(c)
				&& (c->wbuf_q.queued + c->wsq_len + len
						> cfg_get(tcp, tcp_cfg, tcpconn_wq_max))
				&& !tcp_reactor_sock_writable(c->s))) {
		queued = c->wbuf_q.queued;
		staged = c->wsq_len;
		lock_release(&c->write_lock);
		LM_ERR("(%u bytes): write queue full (%u queued, %u staged) - conn "
			   "%p id %d state %d flags 0x%x\n",
				len, queued, staged, c, c->id, c->state, c->flags);
		TCP_EV_SENDQ_FULL(0, &c->rcv);
		TCP_STATS_SENDQ_FULL();
		shm_free(ch->buf);
		shm_free(ch);
		return -1;
	}
	if(c->wsq_tail != NULL)
		c->wsq_tail->next = ch;
	else
		c->wsq_head = ch;
	c->wsq_tail = ch;
	c->wsq_len += len;
	lock_release(&c->write_lock);
	return 0;
}

/* mode 2 [io_wait]: unshield c - hand it back to io_wait after its last job.
 *  - clear the pool owner bits
 *  - add c->s to io_h with POLLIN, plus POLLOUT when wbuf_q holds data
 *  - refresh c->timeout (con_lifetime) and re-add the local timer the shield
 *    removed; a partial read then gets the read deadline
 *  - the caller drops the in-pool ref
 * Returns 0, -1 on io_watch error (the caller closes c). */
static int tcp_reactor_unshield(struct tcp_connection *c)
{
	short events = POLLIN;
	ticks_t t;

	c->flags &= ~F_CONN_POOL_BUSY;
	c->flags |= F_CONN_READ_W | F_CONN_WANTS_RD;
	if(_wbufq_non_empty(c)) {
		events |= POLLOUT;
		c->flags |= F_CONN_WRITE_W | F_CONN_WANTS_WR;
	}
	if(unlikely(tcpmain_io_watch_add_conn(c->s, events, c) < 0)) {
		LM_ERR("reactor: failed to re-arm read watch for %p fd %d\n", c, c->s);
		c->flags &= ~(F_CONN_READ_W | F_CONN_WRITE_W);
		return -1;
	}
	t = get_ticks_raw();
	c->timeout = t + cfg_get(tcp, tcp_cfg, con_lifetime);
	/* re-arm the local timer disarmed by the shield (it was removed from the
	 * timer list, so reinit + add - no del). arm_read_timeout() below may then
	 * shorten it to the partial-read deadline if a message is mid-read. */
	if(!(c->flags & F_CONN_MAIN_TIMER)) {
		local_timer_reinit(&c->timer);
		tcpmain_local_timer_add(&c->timer, c->timeout - t, t);
		c->flags |= F_CONN_MAIN_TIMER;
	}
	tcp_reactor_arm_read_timeout(c, t);
	return 0;
}

/* mode 2 [io_wait]: close a shielded c after its last job (EOF, error, mark
 * bad); c is out of io_h and the timer.
 *  - callers: tcp_reactor_handle_done() (F_CONN_POOL_CLOSE) and
 *    tcp_reactor_unshield_or_chain() (bad/unhashed c, re-arm failure); both
 *    hold the in-pool ref
 *  - unhash and drop the hash ref
 *  - emit the tcpops close event
 *  - drop the in-pool ref last: the last ref frees c; with a transient ref
 *    outstanding (worker write request, tls-event dispatch) its holder frees
 *    c later */
static void tcp_reactor_pool_close(struct tcp_connection *c)
{
	if(unlikely(c->rd_owner || c->wr_owner))
		BUG("conn %p closed while owned (rd %d wr %d)\n", c, c->rd_owner - 1,
				c->wr_owner - 1);
	if(tcpconn_try_unhash(c))
		tcpconn_put(c);
	c->flags &= ~(F_CONN_POOL_BUSY | F_CONN_POOL_CLOSE | F_CONN_WANTS_RD
				  | F_CONN_WANTS_WR);

	tcp_emit_closed_event(c);
	tcpconn_put_destroy(c);
}

/* mode 2 [io_wait]: finish c's last pool job (or a failed enqueue), in order:
 *  - wsq holds data, wbuf_q is empty, c is healthy: chain a TCP_R_WRITE job;
 *    c stays shielded with the in-pool ref
 *  - c bad or unhashed: tcp_reactor_pool_close()
 *  - else unshield c and drop the in-pool ref; on re-arm failure close c */
static void tcp_reactor_unshield_or_chain(struct tcp_connection *c)
{
	/* wbuf_q holds data = socket full: unshield; the next POLLOUT in
	 * tcp_reactor_handle_tcpconn_ev() starts the write job for wsq */
	if(c->wsq_head != NULL && (c->flags & F_CONN_HASHED)
			&& c->state != S_CONN_BAD && !_wbufq_non_empty(c)) {
		if(likely(tcp_reactor_enqueue_job(c, TCP_R_WRITE) == 0)) {
			/* keep POOL_BUSY + in-pool ref; the write job completes */
			tcp_reactor_read_beside_write(c);
			return;
		}
		/* OOM: unshield; wsq waits for the next write request or POLLOUT */
	}
	/* close: pool_close() takes over the in-pool ref */
	if(unlikely(!(c->flags & F_CONN_HASHED) || (c->state == S_CONN_BAD))) {
		tcp_reactor_pool_close(c);
		return;
	}
	if(unlikely(tcp_reactor_unshield(c) < 0)) {
		tcp_reactor_pool_close(c);
		return;
	}
	/* back in io_h + the timer: drop the in-pool ref; the hash ref keeps c */
	if(unlikely(tcpconn_put(c)))
		tcpconn_destroy(c); /* defensive: the hash ref is held */
}

/* mode 2 write dispatch entry points, called from tcp_send() (tcp_main.c). */

/* mode 2 [any]: send a payload on c; PROC_TCP_MAIN performs every write (and
 * every tls_encode()).
 *  - c ref: the caller's ref (tcp_send()) moves into the write request; tcp
 *    main drops it (handle_ser_child()'s CONN_WRITE_REQ arm); error paths here
 *    drop it
 *  - worker: copy the payload into a shm write request, send it on
 *    unix_tcp_sock as CONN_WRITE_REQ
 *  - PROC_TCP_MAIN pool thread (a write-back from the read path, e.g. WS
 *    pong/close): stage on c->wsq; the read job's completion chains the write
 *  - PROC_TCP_MAIN io_wait thread: queue on wbuf_q, arm POLLOUT
 * Returns len when the payload is accepted, -1 on error. The write itself is
 * async: a later failure closes c and stays out of this return value. */
int tcp_reactor_send_put(struct tcp_connection *c, const char *buf,
		unsigned len, snd_flags_t send_flags)
{
	tcp_reactor_write_req_t *wreq;
	long response[2];

	if(unlikely(is_tcp_main())) {
		/* inside PROC_TCP_MAIN: skip the unix_tcp_sock self-send.
		 * is_tcp_main() holds for io_wait and pool threads alike; io_h belongs
		 * to io_wait, so split on tcp_reactor_thread_idx. Drop the caller's
		 * ref first */
		if(unlikely(tcpconn_put(c))) {
			tcpconn_destroy(c); /* was the last ref */
			return -1;
		}
		if(unlikely((c->state == S_CONN_BAD) || !(c->flags & F_CONN_HASHED))) {
			/* in the process of being destroyed => drop the write */
			return -1;
		}
		if(tcp_reactor_thread_idx >= 0) {
			/* pool thread, c shielded by this thread's read job (e.g. WS
			 * ws_send_crlf()/pong/close from ws_frame_receive()):
			 *  - stage on c->wsq (shm, under write_lock)
			 *  - the read job's completion chains a write job for it
			 *  - leave io_h and c->flags to io_wait: re-arming c->s here lets a
			 *    second read job run on c->req (torn WS frames) */
			if(unlikely(tcp_reactor_wsq_add(c, buf, len, send_flags) < 0))
				return -1;
			return (int)len;
		}
		/* io_wait thread: queue on wbuf_q and arm POLLOUT here; the io_wait
		 * thread owns io_h and c->flags */
		if(unlikely(tcp_reactor_wbuf_enqueue(c, (char *)buf, len, send_flags)
					< 0)) {
			return -1;
		}
		tcp_reactor_watch_write(c);
		return (int)len;
	}

	wreq = shm_malloc(sizeof(tcp_reactor_write_req_t));
	if(unlikely(wreq == NULL)) {
		SHM_MEM_ERROR;
		tcpconn_chld_put(c);
		return -1;
	}
	wreq->buf = shm_malloc(len);
	if(unlikely(wreq->buf == NULL)) {
		SHM_MEM_ERROR;
		shm_free(wreq);
		tcpconn_chld_put(c);
		return -1;
	}
	memcpy(wreq->buf, buf, len);
	wreq->len = len;
	wreq->conn = c; /* refcnt transferred to tcp_main */
	wreq->send_flags = send_flags;

	response[0] = (long)(uintptr_t)wreq;
	response[1] = CONN_WRITE_REQ;
	if(unlikely(send_all(unix_tcp_sock, response, sizeof(response)) <= 0)) {
		LM_ERR("failed to send write request to tcp main: %s (%d)\n",
				strerror(errno), errno);
		shm_free(wreq->buf);
		shm_free(wreq);
		tcpconn_chld_put(c);
		return -1;
	}
	return (int)len;
}

/* mode 2 [worker]: the worker lacks a connection to dst: ask PROC_TCP_MAIN to
 * open one and send buf on it.
 *  - copy dst, from and buf into a shm connect request, send it on
 *    unix_tcp_sock as CONN_CONNECT_REQ
 *  - tcp main opens the socket and creates the tcp_connection
 *    (tcp_reactor_handle_connect_req())
 * Returns len when the request is sent (the connect and write are async), -1
 * on error. */
int tcp_reactor_connect_put(struct dest_info *dst, union sockaddr_union *from,
		const char *buf, unsigned len)
{
	tcp_reactor_connect_req_t *creq;
	long response[2];

	if(unlikely(is_tcp_main())) {
		/* a connect from inside PROC_TCP_MAIN: refuse with a clear error.
		 * The unix_tcp_sock self-send is invalid here; the in-process senders
		 * (read-path pongs, HTTP 100-continue) reply on an existing connection
		 * via tcp_reactor_send_put() */
		LM_ERR("outbound connect requested from within PROC_TCP_MAIN to %s -"
			   " not supported in mode 2, dropping\n",
				su2a(&dst->to, sizeof(dst->to)));
		return -1;
	}

	creq = shm_malloc(sizeof(tcp_reactor_connect_req_t));
	if(unlikely(creq == NULL)) {
		SHM_MEM_ERROR;
		return -1;
	}
	creq->buf = shm_malloc(len);
	if(unlikely(creq->buf == NULL)) {
		SHM_MEM_ERROR;
		shm_free(creq);
		return -1;
	}
	memcpy(creq->buf, buf, len);
	creq->len = len;
	creq->dst = dst->to;
	if(from != NULL) {
		creq->from = *from;
		creq->have_from = 1;
	} else {
		creq->have_from = 0;
	}
	creq->proto = dst->proto;
	creq->id = dst->id;
	creq->try_local_port = (dst->send_sock) ? dst->send_sock->port_no : 0;
	creq->send_flags = dst->send_flags;

	response[0] = (long)(uintptr_t)creq;
	response[1] = CONN_CONNECT_REQ;
	if(unlikely(send_all(unix_tcp_sock, response, sizeof(response)) <= 0)) {
		LM_ERR("failed to send connect request to tcp main: %s (%d)\n",
				strerror(errno), errno);
		shm_free(creq->buf);
		shm_free(creq);
		return -1;
	}
	return (int)len;
}

/* mode 2 handle_ser_child() case bodies (CONN_TLS_EVENT_DONE/CONN_SCRIPT_CLOSE/
 * CONN_WRITE_REQ/CONN_CONNECT_REQ), called from tcp_main.c. */

/* mode 2 [io_wait]: CONN_TLS_EVENT_DONE arm of handle_ser_child(): a worker ran
 * the tls:connection-out route for tcpconn.
 *  - drop the transient ref of tcp_reactor_dispatch_tls_event()
 *  - the last ref frees tcpconn here, on the io_wait thread */
void tcp_reactor_handle_tls_event_done(struct tcp_connection *tcpconn)
{
	if(unlikely(tcpconn_put(tcpconn)))
		tcpconn_destroy(tcpconn);
}

/* mode 2 [io_wait]: CONN_SCRIPT_CLOSE arm of handle_ser_child(): a worker asks
 * to close the connection of a message that failed in the core
 * (tcp_script_mode = 0, tcp_read.c).
 *  - scid = connection id (rcv->proto_reserved1); a dispatched task carries
 *    the id, the worker holds no tcp_connection pointer
 *  - gone: nothing to do
 *  - pool-owned: mark bad, the last job's completion closes it
 *  - else unhash, unwatch, emit tcp:closed, free */
void tcp_reactor_handle_script_close(int scid)
{
	struct tcp_connection *tcpconn;

	tcpconn = tcpconn_get(scid, 0, 0, 0, 0); /* takes a ref */
	if(tcpconn == NULL) {
		LM_DBG("reactor: script-close for conn id %d: already gone\n", scid);
		return;
	}
	/* if a pool job currently owns it,
	 * do not unhash/free here - mark it bad and let the job completion
	 * close it */
	if(unlikely(tcpconn_pool_busy(tcpconn))) {
		tcp_reactor_mark_bad_put(tcpconn);
		return;
	}
	if(tcpconn_try_unhash(tcpconn))
		tcpconn_put(tcpconn);
	if(((tcpconn->flags & (F_CONN_WRITE_W | F_CONN_READ_W)))
			&& (tcpconn->s != -1)) {
		tcpmain_io_watch_del(tcpconn->s, -1, 1);
		tcpconn->flags &= ~(F_CONN_WRITE_W | F_CONN_READ_W);
	}
	/* reason stays unset -> reports tcp:closed (EOF), same as the
	 * mode 0/1 script-error close. */
	tcp_emit_closed_event(tcpconn);
	tcpconn_put_destroy(tcpconn); /* release the lookup ref */
}

/* mode 2 [io_wait]: stage a worker's write on c's wsq (buf: shm, freed here) and
 * start a write job when c is free and its socket has room; else the data waits
 * for the running job or for POLLOUT */
static void tcp_reactor_stage_write(struct tcp_connection *c, char *buf,
		unsigned len, snd_flags_t send_flags)
{
	int ret;

	ret = tcp_reactor_wsq_add(c, buf, len, send_flags);
	shm_free(buf);
	if(unlikely(ret < 0)) {
		tcp_reactor_mark_bad(c);
		return;
	}
	if(tcpconn_pool_busy(c)) {
		tcp_reactor_staged_on_busy(c);
		return;
	}
	/* socket full: the data waits in wsq for POLLOUT */
	if((c->flags & F_CONN_WRITE_W) && _wbufq_non_empty(c))
		return;
	if(unlikely(tcp_reactor_shield(c, -1) < 0)) {
		c->state = S_CONN_BAD;
		c->timeout = get_ticks_raw();
		return;
	}
	/* on failure: release the in-pool ref + un-shield; data waits in wsq */
	if(unlikely(tcp_reactor_enqueue_job(c, TCP_R_WRITE) < 0))
		tcp_reactor_unshield_or_chain(c);
}

/* mode 2 [io_wait]: CONN_WRITE_REQ arm of handle_ser_child(); wreq comes from
 * tcp_reactor_send_put() in a worker.
 *  - drop the worker's ref; c gone or closing: drop the write
 *  - else tcp_reactor_stage_write()
 *  - frees wreq and wreq->buf on every path */
void tcp_reactor_handle_write_req(tcp_reactor_write_req_t *wreq)
{
	struct tcp_connection *tcpconn;

	tcpconn = wreq->conn;
	/* release the worker's refcnt; if it was the last, conn is gone */
	if(unlikely(tcpconn_put(tcpconn))) {
		tcpconn_destroy(tcpconn);
		shm_free(wreq->buf);
		shm_free(wreq);
		return;
	}
	if(unlikely((tcpconn->state == S_CONN_BAD)
				|| !(tcpconn->flags & F_CONN_HASHED))) {
		/* in the process of being destroyed => drop the write */
		shm_free(wreq->buf);
		shm_free(wreq);
		return;
	}
	/* the write job tls_encode()s on its pool thread: the shield gives it the
	 * SSL object */
	tcp_reactor_stage_write(tcpconn, wreq->buf, wreq->len, wreq->send_flags);
	shm_free(wreq);
}

/* mode 2 [io_wait]: open, hash and watch a new outbound connection for creq
 * and queue its payload; frees creq */
static void tcp_reactor_connect_new(tcp_reactor_connect_req_t *creq,
		union sockaddr_union *cfrom, ticks_t con_lifetime)
{
	struct tcp_connection *cc;
	ticks_t t;

	/* open the connection (socket + non-blocking connect) here */
	cc = tcpconn_connect(&creq->dst, cfrom, creq->proto, &creq->send_flags);
	if(unlikely(cc == NULL)) {
		LM_ERR("failed to open outbound connection to %s\n",
				su2a(&creq->dst, sizeof(creq->dst)));
		shm_free(creq->buf);
		shm_free(creq);
		return;
	}
	atomic_set(&cc->refcnt, 1); /* owned solely by tcp_main (hash) */
	if(unlikely(tcpconn_add(cc) == 0)) {
		LM_ERR("failed to hash outbound connection %p\n", cc);
		tcp_safe_close(cc->s);
		cc->flags |= F_CONN_FD_CLOSED;
		_tcpconn_free(cc);
		shm_free(creq->buf);
		shm_free(creq);
		return;
	}
	tcpmain_note_new_conn(cc->type == PROTO_TLS);
	/* queue the payload; for TLS, tls_encode() buffers it until the
	 * handshake completes (the handshake is driven on the read/write
	 * events registered just below). */
	if(unlikely(tcp_reactor_wbuf_enqueue(
						cc, creq->buf, creq->len, creq->send_flags)
				< 0)) {
		tcpconn_try_unhash(cc);
		tcpconn_put_destroy(cc);
		shm_free(creq->buf);
		shm_free(creq);
		return;
	}
	shm_free(creq->buf);
	shm_free(creq);
	/* register in the reactor: watch read + write. The POLLOUT event
	 * completes the connect and drains the wbuf_q. */
	t = get_ticks_raw();
	cc->timeout = t + con_lifetime;
	tcpmain_local_timer_add(&cc->timer, con_lifetime, t);
	cc->flags |= F_CONN_MAIN_TIMER | F_CONN_READ_W | F_CONN_WANTS_RD
				 | F_CONN_WRITE_W | F_CONN_WANTS_WR;
	cc->flags &= ~F_CONN_FD_CLOSED;
	if(unlikely(tcpmain_io_watch_add_conn(cc->s, POLLIN | POLLOUT, cc) < 0)) {
		LM_CRIT("failed to add outbound connection to the fd list\n");
		cc->flags &= ~(F_CONN_WRITE_W | F_CONN_READ_W);
		tcpconn_try_unhash(cc);
		tcpconn_put_destroy(cc);
	}
}

/* mode 2 [io_wait]: CONN_CONNECT_REQ arm of handle_ser_child(); creq comes from
 * tcp_reactor_connect_put() in a worker.
 *  - look dst up again: another worker's request may have opened it since
 *  - found: stage the payload on it (tcp_reactor_stage_write())
 *  - else open a new connection (tcp_reactor_connect_new())
 *  - frees creq and creq->buf on every path */
void tcp_reactor_handle_connect_req(tcp_reactor_connect_req_t *creq)
{
	struct tcp_connection *cc;
	union sockaddr_union *cfrom;
	struct ip_addr cip;
	int cport;
	ticks_t con_lifetime;

	cfrom = creq->have_from ? &creq->from : NULL;
	su2ip_addr(&cip, &creq->dst);
	cport = su_getport(&creq->dst);
	con_lifetime = cfg_get(tcp, tcp_cfg, con_lifetime);
	/* dedup: another worker may have created a matching connection
	 * since this worker's lookup missed => reuse it if so */
	if(tcp_connection_match == TCPCONN_MATCH_STRICT
			|| (creq->send_flags.f & SND_F_FORCE_PROTO)) {
		cc = tcpconn_lookup(creq->id, &cip, cport, cfrom, creq->try_local_port,
				con_lifetime, creq->proto);
	} else {
		cc = tcpconn_get(creq->id, &cip, cport, cfrom, con_lifetime);
	}
	if(cc != NULL) {
		/* reuse: stage on wsq for a pool write job, as for CONN_WRITE_REQ;
		 * a pool job may own cc (mid-read) */
		if(unlikely(tcpconn_put(cc))) {
			tcpconn_destroy(cc);
			shm_free(creq->buf);
			shm_free(creq);
			return;
		}
		if(unlikely(
				   (cc->state == S_CONN_BAD) || !(cc->flags & F_CONN_HASHED))) {
			shm_free(creq->buf);
			shm_free(creq);
			return;
		}
		tcp_reactor_stage_write(cc, creq->buf, creq->len, creq->send_flags);
		shm_free(creq);
		return;
	}
	tcp_reactor_connect_new(creq, cfrom, con_lifetime);
}

/* mode 2 [io_wait]: an event on a pool-owned conn: drop its fd from the watch
 * set; the running job's completion re-adds it */
static void tcp_reactor_unwatch_busy(struct tcp_connection *tcpconn, int fd_i)
{
	if(tcpconn->s != -1) {
		if(unlikely(tcpmain_io_watch_del(tcpconn->s, fd_i, 0) < 0))
			LM_ERR("reactor: io_watch_del (busy re-arm) failed for %p "
				   "fd %d\n",
					tcpconn, tcpconn->s);
	}
	tcpconn->flags &= ~(F_CONN_READ_W | F_CONN_WRITE_W);
}

/* mode 2 [io_wait]: POLLOUT or an error on a write-watched conn: flush wbuf_q,
 * then drop the write watch once it is empty. -1: close the conn */
static int tcp_reactor_flush_wbuf(
		struct tcp_connection *tcpconn, short ev, int fd_i)
{
	int wempty_q = 0;

	if(unlikely((ev & (POLLERR | POLLHUP))
				|| (wbufq_run(tcpconn->s, tcpconn, &wempty_q) < 0))) {
		/* write error or peer gone: tear the connection down */
		return -1;
	}
	if(wempty_q) {
		tcpconn->flags &= ~F_CONN_WANTS_WR;
		if(!(tcpconn->flags & F_CONN_READ_W)) {
			if(unlikely(tcpmain_io_watch_chg(tcpconn->s, POLLIN, fd_i) == -1)) {
				LM_ERR("io_watch_chg (reactor wr) failed for %p fd "
					   "%d\n",
						tcpconn, tcpconn->s);
				return -1;
			}
		}
		tcpconn->flags &= ~F_CONN_WRITE_W;
		if(tcpconn_close_after_send(tcpconn))
			return -1;
	}
	return 0;
}

/* mode 2 [io_wait]: tear down an unshielded conn after an event handling
 * error */
static void tcp_reactor_ev_close(struct tcp_connection *tcpconn, int fd_i)
{
	/* unhash first, while F_CONN_MAIN_TIMER is still set, so the local
	 * timer is removed before the connection is freed (try_unhash only
	 * deletes the timer when the flag is set). Then stop watching the fd. */
	if(unlikely(!tcpconn_try_unhash(tcpconn)))
		LM_CRIT("unhashed connection %p\n", tcpconn);
	if((tcpconn->flags & (F_CONN_WRITE_W | F_CONN_READ_W))
			&& (tcpconn->s != -1)) {
		if(unlikely(tcpmain_io_watch_del(tcpconn->s, fd_i, 1) < 0)) {
			LM_ERR("io_watch_del (reactor) failed for %p fd %d\n", tcpconn,
					tcpconn->s);
		}
		tcpconn->flags &= ~(F_CONN_WRITE_W | F_CONN_READ_W);
	}
	tcpconn->flags &= ~(F_CONN_WANTS_RD | F_CONN_WANTS_WR);
	/* mode 2 inline read teardown (shield/enqueue not taken): same as
	 * tcp_reactor_pool_close - the reader detected an EOF/error/reset, so
	 * emit the tcpops close event here too. Fire-once guarded. */
	tcp_emit_closed_event(tcpconn);
	/* not shielded: only the hash ref is held - release it once here */
	tcpconn_put_destroy(tcpconn);
}

/* mode 2 [io_wait]: io event on tcpconn; handle_tcpconn_ev() calls it first in
 * mode 2 and returns its result (same convention as handle_io()).
 *  - mode 2 form of handle_tcpconn_ev(): PROC_TCP_MAIN keeps the fd, reads
 *    and reassembles on the pool, dispatches messages to workers
 *  - workers take tasks from the shared dispatch socket and the kernel spreads
 *    them (as udp_receiver_mode = 1); tcp_children[].busy stays unused
 * Steps, in order:
 *  - pool-owned tcpconn: unwatch the fd, return
 *  - POLLOUT with wsq data: shield, start a write job (+ a read beside)
 *  - POLLOUT/error on a write watch: flush wbuf_q here
 *  - POLLIN/error: shield, start a read job
 * Returns 0, -1 when tcpconn was closed here. */
int tcp_reactor_handle_tcpconn_ev(
		struct tcp_connection *tcpconn, short ev, int fd_i)
{
	/* pool-owned: the shield removed the fd, an event slipped through
	 * (e.g. a WS handshake con->type flip outruns the shield's io_watch_del):
	 *  - drop the fd from io_h; a second read job would share con->req
	 *  - the job completion re-adds it; level-triggered epoll re-fires for
	 *    buffered input */
	if(unlikely(tcpconn_pool_busy(tcpconn))) {
		tcp_reactor_unwatch_busy(tcpconn, fd_i);
		return 0;
	}

	/* writes staged while the socket was full: flush them all on the pool
	 * (TLS encodes there), with a read beside if input is pending */
	if((ev & POLLOUT) && !(ev & (POLLERR | POLLHUP))
			&& (tcpconn->flags & F_CONN_WRITE_W) && tcpconn->wsq_head != NULL) {
		if(unlikely(tcp_reactor_shield(tcpconn, fd_i) < 0))
			goto reactor_close;
		if(unlikely(tcp_reactor_enqueue_job(tcpconn, TCP_R_WRITE) < 0))
			tcp_reactor_unshield_or_chain(tcpconn);
		else
			tcp_reactor_read_beside_write(tcpconn);
		return 0;
	}
	/* write-watched, wsq empty: flush wbuf_q (wire bytes) here on io_wait */
	if((ev & (POLLOUT | POLLERR | POLLHUP)) && (tcpconn->flags & F_CONN_WRITE_W)
			&& tcp_reactor_flush_wbuf(tcpconn, ev, fd_i) < 0)
		goto reactor_close;

	if(ev & (POLLIN | POLLERR | POLLHUP)) {
		/* read on a pool thread, all four transports:
		 *  - the shield gives the job c, its SSL object and WS codec
		 *  - tls_encode()'s trampoline scratch buffer is per thread
		 *    (tcp_mtops.c)
		 *  - a WS upgrade starts as TCP/TLS and flips con->type to WS/WSS;
		 *    the shield covers both
		 * error/hangup events force EOF handling in the read */
		if(ev
				& (POLLHUP | POLLERR
#ifdef POLLRDHUP
						| POLLRDHUP
#endif /* POLLRDHUP */
						))
			tcpconn->flags |= F_CONN_FORCE_EOF;
		if(unlikely(tcp_reactor_shield(tcpconn, fd_i) < 0))
			goto reactor_close;
		if(unlikely(tcp_reactor_enqueue_job(tcpconn, TCP_R_READ) < 0)) {
			/* could not enqueue: release the in-pool ref and un-shield */
			tcp_reactor_unshield_or_chain(tcpconn);
		}
	}
	return 0;

reactor_close:
	tcp_reactor_ev_close(tcpconn, fd_i);
	return -1;
}

/* ========================================================================
 * mode 2: reactor thread pool inside PROC_TCP_MAIN (job model: file header)
 * ======================================================================== */

/* mode 2 [io_wait]: complete every job on done_head (one notify byte or many).
 *  - clear the job's owner bit; resp < 0 sets F_CONN_POOL_CLOSE
 *  - the other job still runs on c: start a new job for this direction when
 *    it has work, finish later
 *  - last job out: pool_close() on F_CONN_POOL_CLOSE, else
 *    unshield_or_chain()
 *  - io_h re-arm and close run here, on the io_wait thread */
static void tcp_reactor_handle_done(void)
{
	struct tcp_reactor_job *job, *next;
	struct tcp_connection *conn;
	int op, resp;

	pthread_mutex_lock(&tcp_rpool.done_lock);
	job = tcp_rpool.done_head;
	tcp_rpool.done_head = tcp_rpool.done_tail = NULL;
	pthread_mutex_unlock(&tcp_rpool.done_lock);

	while(job != NULL) {
		next = job->next;
		conn = job->conn;
		op = job->op;
		resp = job->resp;
		pkg_free(job);

		switch(op) {
			case TCP_R_READ:
			case TCP_R_WRITE:
				if(unlikely(!(conn->flags & TCP_REACTOR_OP_FLAG(op))))
					BUG("conn %p: op %d done without its owner bit (flags "
						"%x)\n",
							conn, op, conn->flags);
				conn->flags &= ~TCP_REACTOR_OP_FLAG(op);
				/* one in-pool ref per shield, released by close or unshield.
				 * resp < 0 (EOF, read or write error): close when the last
				 * job is out */
				if(unlikely(resp < 0))
					conn->flags |= F_CONN_POOL_CLOSE;
				if(tcpconn_pool_busy(conn)) {
					/* the other direction's job still runs: start a new
					 * job for this direction (wsq data / pending input) */
					if(op == TCP_R_WRITE)
						tcp_reactor_write_beside_read(conn);
					else
						tcp_reactor_read_beside_write(conn);
					break;
				}
				if(unlikely(conn->flags & F_CONN_POOL_CLOSE)) {
					tcp_reactor_pool_close(conn);
				} else {
					tcp_reactor_unshield_or_chain(conn);
				}
				break;
			default:
				break;
		}
		job = next;
	}
}

/* mode 2 [pool]: per-thread setup: index, OS thread name, blocked signals */
static void tcp_reactor_thread_init(int idx)
{
	sigset_t set;

	tcp_reactor_thread_idx = idx;
	KSR_POOL_MEMBER_ONCE(ksr_cnt_rthreads);

#if defined(HAVE_PTHREAD_SETNAME_NP_2ARG) \
		|| defined(HAVE_PTHREAD_SETNAME_NP_1ARG)
	{
		/* label this pool thread (comm is capped at 16 bytes incl. NUL) so
		 * profilers / top -H / gdb can tell the pool threads apart and from the
		 * io_wait thread. OS thread name only - kamailio's process table
		 * (kamcmd core.ps) is unaffected. Always names the calling thread, so
		 * macOS's 1-arg self-only form is sufficient. */
		char tname[16];
		snprintf(tname, sizeof(tname), "tcpr-pool-%d", tcp_reactor_thread_idx);
#if defined(HAVE_PTHREAD_SETNAME_NP_2ARG)
		pthread_setname_np(pthread_self(), tname);
#else
		pthread_setname_np(tname);
#endif
	}
#endif

	/* block signals - PROC_TCP_MAIN's main thread is the signal handler */
	sigfillset(&set);
	pthread_sigmask(SIG_BLOCK, &set, NULL);
}

/* mode 2 [pool]: wait for the next job and take it off the queue; NULL when
 * the pool stops with an empty queue */
static struct tcp_reactor_job *tcp_reactor_next_job(void)
{
	struct tcp_reactor_job *job;

	tcp_cond_lock(&tcp_reactor_wake->cond);
	while(!tcp_rpool.stop && tcp_rpool.job_head == NULL) {
		tcp_cond_wait(&tcp_reactor_wake->cond);
	}
	/* io_wait pushed the job; for READ/WRITE it shielded the conn first,
	 * so the conn is out of io_h and the timer and holds the in-pool ref */
	job = tcp_rpool.job_head;
	if(job != NULL) {
		tcp_rpool.job_head = job->next;
		if(tcp_rpool.job_head == NULL)
			tcp_rpool.job_tail = NULL;
	}
	tcp_cond_unlock(&tcp_reactor_wake->cond);
	return job;
}

/* mode 2 [pool]: TCP_R_READ job body; the result goes to job->resp */
static void tcp_reactor_run_read(struct tcp_reactor_job *job)
{
	/* read + reassemble on conn->s, all of TCP/TLS/WS/WSS:
	 *  - tcp_read_req() reads _tconfd(conn), which is conn->s in mode 2
	 *  - WS: the frame codec runs here and dispatches each reassembled
	 *    payload via tcp_reactor_dispatch_msg()
	 *  - io_watch and free belong to the io_wait completion */
	struct tcp_connection *conn = job->conn;
	rd_conn_flags_t read_flags;
	int n, resp;

	read_flags = (conn->flags & (F_CONN_EOF_SEEN | F_CONN_FORCE_EOF))
						 ? RD_CONN_FORCE_EOF
						 : 0;
	tcp_reactor_stamp(conn, TCP_R_READ, 1);
	do {
		resp = tcp_read_req(conn, &n, &read_flags);
	} while(resp >= 0 && (read_flags & RD_CONN_REPEAT_READ));
	if(resp < 0 && resp != CONN_EOF)
		conn->state = S_CONN_BAD;
	tcp_reactor_stamp(conn, TCP_R_READ, 0);
	job->resp = resp;
}

/* mode 2 [pool]: TCP_R_WRITE job body; the result goes to job->resp */
static void tcp_reactor_run_write(struct tcp_reactor_job *job)
{
	/* move conn->wsq into wbuf_q, then flush wbuf_q to the socket:
	 *  - TCP/WS: copy; TLS/WSS: tls_encode() via the direct-call
	 *    trampoline on this pool thread
	 *  - touch buffers and state under write_lock; c->flags belongs to
	 *    the io_wait completion */
	struct tcp_connection *conn = job->conn;
	struct tcp_wchunk *ch;
	int werr = 0, wempty = 0;

	tcp_reactor_stamp(conn, TCP_R_WRITE, 1);
	lock_get(&conn->write_lock);
	while((ch = conn->wsq_head) != NULL) {
		conn->wsq_head = ch->next;
		/* after an error the conn is dropped: discard the rest */
		if(!werr
				&& tcp_reactor_wbuf_add_locked(
						   conn, ch->buf, ch->len, ch->send_flags, 1)
						   < 0)
			werr = 1;
		shm_free(ch->buf);
		shm_free(ch);
	}
	conn->wsq_tail = NULL;
	conn->wsq_len = 0;
	lock_release(&conn->write_lock);
	if(!werr && wbufq_run(conn->s, conn, &wempty) < 0)
		werr = 1;
	tcp_reactor_stamp(conn, TCP_R_WRITE, 0);
	/* resp: <0 error or close-after-send with all data written;
	 * 1 = wbuf_q holds data for POLLOUT; 0 = fully drained */
	if(wempty && tcpconn_close_after_send(conn))
		werr = 1;
	job->resp = werr ? -1 : (wempty ? 0 : 1);
}

/* mode 2 [pool]: hand a completed job back to the io_wait thread */
static void tcp_reactor_job_done(struct tcp_reactor_job *job)
{
	char wake = 'x';

	pthread_mutex_lock(&tcp_rpool.done_lock);
	job->next = NULL;
	if(tcp_rpool.done_tail != NULL)
		tcp_rpool.done_tail->next = job;
	else
		tcp_rpool.done_head = job;
	tcp_rpool.done_tail = job;
	pthread_mutex_unlock(&tcp_rpool.done_lock);

	if(write(tcp_rpool.notify_pipe[1], &wake, 1) < 0) {
		if(errno != EAGAIN && errno != EWOULDBLOCK)
			LM_ERR("failed to notify reactor of completion: %s\n",
					strerror(errno));
	}
}

/* mode 2 [pool]: pool thread body; arg = thread index */
static void *tcp_reactor_thread_routine(void *arg)
{
	struct tcp_reactor_job *job;

	tcp_reactor_thread_init((int)(long)arg);
	while((job = tcp_reactor_next_job()) != NULL) {
		counter_inc(ksr_cnt_busy_rthreads);
		switch(job->op) {
			case TCP_R_READ:
				tcp_reactor_run_read(job);
				break;
			case TCP_R_WRITE:
				tcp_reactor_run_write(job);
				break;
			case TCP_R_RUN:
				LM_DBG("reactor: pool thread %d running tcpx task %p "
					   "(exec=%p)\n",
						tcp_reactor_thread_idx, (void *)job->task,
						job->task ? (void *)job->task->exec : NULL);
				if(job->task && job->task->exec)
					job->task->exec(job->task->param, job->pidx);
				pkg_free(job);
				counter_add(ksr_cnt_busy_rthreads, -1);
				continue;
			default:
				LM_ERR("unknown reactor job op %d\n", job->op);
				break;
		}
		counter_add(ksr_cnt_busy_rthreads, -1);
		tcp_reactor_job_done(job);
	}
	return NULL;
}

/* mode 2 [init]: create the notify pipe + spawn the pool. Runs in PROC_TCP_MAIN
 * after io_h is initialized. Returns 0 on success, -1 on error. */
int tcp_reactor_pool_init(void)
{
	int i;

	LM_INFO("reactor: using %d threads\n", ksr_tcp_reactor_threads);

	/* the io_wait thread keeps its name: as the main thread it names the process,
	 * which ps -C, pkill -x and killall match (pool threads name themselves) */
	if(pipe(tcp_rpool.notify_pipe) < 0) {
		LM_ERR("reactor notify pipe: %s\n", strerror(errno));
		return -1;
	}
	if(fcntl(tcp_rpool.notify_pipe[0], F_SETFL,
			   fcntl(tcp_rpool.notify_pipe[0], F_GETFL) | O_NONBLOCK)
					< 0
			|| fcntl(tcp_rpool.notify_pipe[1], F_SETFL,
					   fcntl(tcp_rpool.notify_pipe[1], F_GETFL) | O_NONBLOCK)
					   < 0) {
		LM_ERR("reactor notify pipe O_NONBLOCK: %s\n", strerror(errno));
		return -1;
	}
	/* the caller (tcp_main_loop()) adds notify_pipe[0] to io_h after this
	 * returns: io_h is private to tcp_main.c (tcp_conn.h) */
	if(pthread_mutex_init(&tcp_rpool.done_lock, NULL) != 0) {
		LM_ERR("failed to init reactor done_lock\n");
		return -1;
	}
	tcp_rpool.threads = pkg_malloc(sizeof(pthread_t) * ksr_tcp_reactor_threads);
	if(tcp_rpool.threads == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	tcp_rpool.threads_no = 0;
	tcp_rpool.stop = 0;
	/* serialize this process's pkg heap before any pool thread runs: from here
	 * on the io_wait thread and the pool threads share pkg concurrently */
	tcp_reactor_pkg_lock_install();
	for(i = 0; i < ksr_tcp_reactor_threads; i++) {
		if(pthread_create(&tcp_rpool.threads[i], NULL,
				   tcp_reactor_thread_routine, (void *)(long)i)
				!= 0) {
			LM_ERR("failed to create reactor pool thread %d/%d\n", i,
					ksr_tcp_reactor_threads);
			return -1; /* threads_no counts started threads for destroy/join */
		}
		tcp_rpool.threads_no++;
	}
	LM_INFO("reactor: started %d pool threads\n", tcp_rpool.threads_no);
	return 0;
}

/* mode 2 [init]: stop + join the pool. Safe to call when the pool was never
 * started. */
void tcp_reactor_pool_destroy(void)
{
	int i;

	if(tcp_rpool.threads == NULL)
		return;
	tcp_cond_lock(&tcp_reactor_wake->cond);
	tcp_rpool.stop = 1;
	tcp_cond_broadcast(&tcp_reactor_wake->cond);
	tcp_cond_unlock(&tcp_reactor_wake->cond);
	for(i = 0; i < tcp_rpool.threads_no; i++)
		pthread_join(tcp_rpool.threads[i], NULL);
	pkg_free(tcp_rpool.threads);
	tcp_rpool.threads = NULL;
	tcp_rpool.threads_no = 0;
}

/* mode 2 [io_wait]: drain the notify pipe and process all completed jobs
 * (coalesced). Called from tcp_main.c's handle_io() on F_TCP_REACTOR_NOTIFY
 * readiness - runs on the io_wait thread. */
void tcp_reactor_handle_notify(void)
{
	char nbuf[64];

	while(read(tcp_rpool.notify_pipe[0], nbuf, sizeof(nbuf)) > 0) {
	}
	tcp_reactor_handle_done();
}

/* mode 2 [init]: create the shared mode 2 objects, in the main process before
 * any TCP child (workers and PROC_TCP_MAIN) forks, so every child inherits
 * them:
 *  - dispatch socketpair (AF_UNIX SOCK_DGRAM, both ends non-blocking):
 *    PROC_TCP_MAIN sends task pointers on dsock[1], workers recv on dsock[0]
 *    and the kernel spreads the datagrams across them
 *  - the pool wakeup condvar in shm (tcp_reactor_wake)
 * The pool threads and the notify pipe start later, in PROC_TCP_MAIN.
 * Returns 0 (also outside mode 2), -1 on error. */
int tcp_reactor_dsock_init(void)
{
	if(ksr_tcp_main_threads != 2)
		return 0;
	if(socketpair(AF_UNIX, SOCK_DGRAM, 0, ksr_tcp_reactor_dsock) < 0) {
		LM_ERR("failed to create reactor dispatch socketpair: %s\n",
				strerror(errno));
		return -1;
	}
	if(fcntl(ksr_tcp_reactor_dsock[0], F_SETFL,
			   fcntl(ksr_tcp_reactor_dsock[0], F_GETFL) | O_NONBLOCK)
			< 0) {
		LM_ERR("failed to set O_NONBLOCK on reactor dsock[0]: %s\n",
				strerror(errno));
		return -1;
	}
	if(fcntl(ksr_tcp_reactor_dsock[1], F_SETFL,
			   fcntl(ksr_tcp_reactor_dsock[1], F_GETFL) | O_NONBLOCK)
			< 0) {
		LM_ERR("failed to set O_NONBLOCK on reactor dsock[1]: %s\n",
				strerror(errno));
		return -1;
	}
	LM_INFO("reactor: dispatch socketpair created (rfd=%d wfd=%d)\n",
			ksr_tcp_reactor_dsock[0], ksr_tcp_reactor_dsock[1]);

	/* pool wakeup condvar: shm, PROCESS_SHARED (struct tcp_reactor_wake) */
	tcp_reactor_wake = shm_malloc(sizeof(*tcp_reactor_wake));
	if(tcp_reactor_wake == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(tcp_reactor_wake, 0, sizeof(*tcp_reactor_wake));
	if(tcp_cond_init(&tcp_reactor_wake->cond) != 0) {
		LM_ERR("failed to init reactor pool condvar\n");
		shm_free(tcp_reactor_wake);
		tcp_reactor_wake = NULL;
		return -1;
	}
	LM_INFO("reactor: pool wakeup initialized\n");
	return 0;
}
