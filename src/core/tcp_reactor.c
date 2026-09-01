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

/* mode 2 reactor: shared AF_UNIX SOCK_DGRAM socketpair.
 * [0] = read end (all workers recvfrom this fd after fork)
 * [1] = write end (tcp_main sends task pointers here) */
static int ksr_tcp_reactor_dsock[2] = {-1, -1};

int ksr_tcp_reactor_get_dispatch_rfd(void)
{
	return ksr_tcp_reactor_dsock[0];
}

int ksr_tcp_reactor_get_dispatch_wfd(void)
{
	return ksr_tcp_reactor_dsock[1];
}

/* mode 2 reactor pool wakeup condvar (shm) */
struct tcp_reactor_wake *tcp_reactor_wake = NULL;
struct tcp_reactor_pool tcp_rpool;

/* number of reactor pool threads; configurable via the tcp_reactor_threads
 * cfg.y directive (assigns through the extern in globals.h), default 8. */
int ksr_tcp_reactor_threads = 8;

/* per-pool-thread index (0..N-1); -1 in the io_wait/main thread and any other
 * thread. Set at pool-thread start (tcp_reactor_thread_routine()). Two uses:
 * selecting the per-thread TLS encode scratch buffer (see tcp_mtops.c) so
 * concurrent pool encodes never collide, and letting tcp_reactor_send_put()/
 * tcpconn_send_unsafe() tell a pool thread from the io_wait thread (both
 * satisfy is_tcp_main()) via tcp_reactor_pool_thread_idx(). */
static _Thread_local int tcp_reactor_thread_idx = -1;

int tcp_reactor_pool_thread_idx(void)
{
	return tcp_reactor_thread_idx;
}

/* The dispatch socket carries one task pointer per datagram. A write of at
 * most PIPE_BUF bytes is guaranteed atomic, so the pointer is never torn
 * across concurrent worker recvs (see the _Static_assert below). */
_Static_assert(sizeof(uintptr_t) <= PIPE_BUF,
		"task pointer too wide for atomic dispatch write");

/* Hand a task pointer to the workers over the dispatch socketpair.
 *
 * The socket is non-blocking: poll briefly for the buffer to drain
 * and retry, giving up only on a hard error or if the workers
 * stay wedged past the bounded wait.
 * Returns 0 on success, -1 on failure
 * (task not sent - the caller must free it). */
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
			if(tries++ >= TCP_REACTOR_DISPATCH_POLL_TRIES) {
				LM_WARN("dispatch socket full: workers not draining after"
						" %d x %dms - dropping task\n",
						TCP_REACTOR_DISPATCH_POLL_TRIES,
						TCP_REACTOR_DISPATCH_POLL_MS);
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
 * Allocate a tcp_reactor_task_t in shm, copy the reassembled SIP message
 * and receive_info into it, then send the pointer to the workers via the
 * dispatch socketpair write end.
 *
 * Called on a PROC_TCP_MAIN reactor pool thread - the read/reassembly runs in
 * the TCP_R_READ job, not on the io_wait thread - at the point where
 * receive_tcp_msg() would be called inline in modes 0/1. WS delivers here too
 * (ws_deliver_sip), likewise from a pool thread.
 *
 * The receiving worker dispatches on task->flags (plain SIP -> receive_msg(),
 * F_TCP_REQ_HEP3 -> hep3_process_msg()) then shm_free()s the task.
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

int tcp_reactor_dispatch_tls_event(struct tcp_connection *c)
{
	tcp_reactor_task_t *task;
	uintptr_t ptr;

	/* Keep c alive for the hop to the worker: it reads c's cached TLS metadata
	 * and hands the refcnt back via CONN_TLS_EVENT_DONE. The read job that
	 * reached us already holds a ref, so this extra one is never the last -
	 * the error-path tcpconn_put() below can't free c. */
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


/* mode 2: append an outgoing payload to c's write queue (wbuf_q), encrypting it
 * first for TLS/WSS (tls_encode) or copying it verbatim for plain TCP.
 **/
static int tcp_reactor_wbuf_add_locked(struct tcp_connection *c, char *buf,
		unsigned len, snd_flags_t send_flags)
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
						|| (t_len && (_wbufq_add(c, t_buf, t_len) < 0)))) {
				c->state = S_CONN_BAD;
				c->timeout = get_ticks_raw(); /* force timeout */
				return -1;
			}
			t_buf = rest_buf;
			t_len = rest_len;
		} while(unlikely(rest_len && wn > 0));
		return 0;
	}
#endif /* USE_TLS */
	if(unlikely(len && (_wbufq_add(c, buf, len) < 0))) {
		c->state = S_CONN_BAD;
		c->timeout = get_ticks_raw(); /* force timeout */
		return -1;
	}
	tcpconn_set_send_flags(c, send_flags);
	return 0;
}

/* mode 2: the locking wrapper around tcp_reactor_wbuf_add_locked() for callers
 * that do NOT already hold c->write_lock - it takes the lock, appends (encoding
 * for TLS), and releases it. Same 0/-1 return. Mirrors the TLS branch of the
 * legacy tcpconn_send_put(). */
static int tcp_reactor_wbuf_enqueue(struct tcp_connection *c, char *buf,
		unsigned len, snd_flags_t send_flags)
{
	int ret;

	lock_get(&c->write_lock);
	ret = tcp_reactor_wbuf_add_locked(c, buf, len, send_flags);
	lock_release(&c->write_lock);
	return ret;
}

/* mode 2: enable POLLOUT watching on an already-hashed connection (mirrors the
 * CONN_QUEUED_WRITE logic). Non-destructive: on io_watch failure it clears the
 * write-watch flag and returns -1 WITHOUT freeing c, so callers that keep using
 * c afterwards (e.g. the TLS read path) stay safe. Returns 0 on success. */
int tcp_reactor_enable_write_watch(struct tcp_connection *c)
{
	ticks_t t;

	/* mode 2: if the conn is currently owned by a pool job
	 * (F_CONN_POOL_BUSY) it is in neither io_h nor the local timer, and is
	 * being touched by a pool thread. Return immediately WITHOUT touching
	 * c->flags or io_h - a pool thread (this may be called from its TLS /
	 * CRLF-pong write-back) must not race the io_wait thread on c->flags. The
	 * data is already in wbuf_q and the job completion re-arms POLLOUT from it.
	 * This check MUST come before any c->flags write below. */
	if(c->flags & F_CONN_POOL_BUSY)
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

/* mode 2: enable POLLOUT watching, destroying the connection on io_watch
 * failure. Used by the IPC write/connect handlers, which do not touch c after
 * this returns. Returns 0 on success, -1 on error (c unhashed and destroyed). */
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

/* mode 2: modifies the existing per-connection timer:
 * - allows PROC_TCP_MAIN to reap a partial read, instead of
 *   waiting for the slower cross-process tcp_timer_check_connections() sca
 * - pulls c->timeout in to the partial-read deadline (message start +
 *   ksr_tcp_msg_read_timeout) and re-arms c->timer to fire there
 * Caller passes t = current tick.
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

/* mode 2: enqueue a read/write job for c onto the pool task queue. Caller
 * (io_wait thread) has already shielded the conn. Returns 0/-1. */
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
	tcp_cond_lock(&tcp_reactor_wake->cond);
	job->next = NULL;
	if(tcp_rpool.task_tail != NULL)
		tcp_rpool.task_tail->next = job;
	else
		tcp_rpool.task_head = job;
	tcp_rpool.task_tail = job;
	tcp_cond_signal(&tcp_reactor_wake->cond);
	tcp_cond_unlock(&tcp_reactor_wake->cond);
	return 0;
}

/* mode 2: enqueue a connectionless TCP_R_RUN job carrying task onto the pool
 * queue. No shield/refcount dance concerns. Returns 0/-1. */
static int tcp_reactor_enqueue_run(tcpx_task_t *task)
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
	tcp_cond_lock(&tcp_reactor_wake->cond);
	job->next = NULL;
	if(tcp_rpool.task_tail != NULL)
		tcp_rpool.task_tail->next = job;
	else
		tcp_rpool.task_head = job;
	tcp_rpool.task_tail = job;
	tcp_cond_signal(&tcp_reactor_wake->cond);
	tcp_cond_unlock(&tcp_reactor_wake->cond);
	return 0;
}

/* mode 2 + mtops: used to send tasks to the thread pool from external
 * processes like jsonrpc with tls.reload
 */
void tcp_reactor_handle_tcpx_task_req(tcpx_task_t *task)
{
	if(unlikely(tcp_reactor_enqueue_run(task) < 0)) {
		/* Enqueue failed (pkg OOM) - the sender is blocked in
		 * ksr_tcpx_task_result_recv() so dropping
		 * the task here would hang it forever. Run it inline instead. */
		LM_ERR("failed to enqueue tcpx task %p - running inline\n",
				(void *)task);
		if(task->exec)
			task->exec(task->param, KSR_TCPX_MAIN_PIDX);
	}
}

/* mode 2: take exclusive pool ownership of a connection. Runs in the
 * io_wait thread. Removes the conn from io_h and the local timer, clears its
 * watch flags, sets F_CONN_POOL_BUSY, and takes the single "in-pool" refcount
 * that is held until the conn is unshielded or closed (it spans chained jobs).
 * No-op (returns 0) if already busy. Returns -1 on io_watch error. */
static int tcp_reactor_shield(struct tcp_connection *c, int fd_i)
{
	if(c->flags & F_CONN_POOL_BUSY)
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
	c->flags |= F_CONN_POOL_BUSY;
	tcpconn_ref(c); /* in-pool ref: released at unshield/close */
	return 0;
}

/* mode 2: stage a plaintext write chunk onto c's per-connection
 * write staging list (shm). Taken by a pool TCP_R_WRITE job. Safe to call from
 * the io_wait thread (CONN_WRITE_REQ handler). Returns 0/-1. */
static int tcp_reactor_wsq_add(struct tcp_connection *c, const char *buf,
		unsigned len, snd_flags_t send_flags)
{
	struct tcp_wchunk *ch;

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
	if(c->wsq_tail != NULL)
		c->wsq_tail->next = ch;
	else
		c->wsq_head = ch;
	c->wsq_tail = ch;
	lock_release(&c->write_lock);
	return 0;
}

/* mode 2: re-arm a connection in the reactor after its pool job(s)
 * finished (unshield). Runs in the io_wait thread. Clears F_CONN_POOL_BUSY,
 * adds POLLIN plus POLLOUT if a write accumulated, and re-arms the local timer
 * the shield removed. Returns 0 on success, -1 on io_watch error (caller
 * should then close the connection). */
static int tcp_reactor_read_rearm(struct tcp_connection *c)
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

/* mode 2: tear down a connection owned by a pool job (EOF/error). The conn
 * is shielded (out of io_h + timer already); runs on the io_wait thread.
 *
 * Both callers (handle_done on resp < 0, unshield_or_chain's close/re-arm-
 * failure paths) enter with the in-pool ref still held. read_close drops the
 * hash ref first, then the in-pool ref last - so if a transient ref is still
 * outstanding (e.g. the tls:connection-out dispatch ref), tcpconn_put_destroy()
 * lands above zero and that holder does the final free, instead of this call
 * freeing a still-referenced conn. */
static void tcp_reactor_read_close(struct tcp_connection *c)
{
	if(tcpconn_try_unhash(c))
		tcpconn_put(c);
	c->flags &= ~(F_CONN_POOL_BUSY | F_CONN_WANTS_RD | F_CONN_WANTS_WR);

	tcp_emit_closed_event(c);
	tcpconn_put_destroy(c);
}

/* mode 2: pool job completion on a healthy conn (io_wait thread).
 * If a write was staged during the job, chain a TCP_R_WRITE job and keep
 * ownership (POOL_BUSY + in-pool ref stay). Otherwise release the in-pool ref
 * and hand the conn back to the reactor (un-shield: re-arm io_h + timer). */
static void tcp_reactor_unshield_or_chain(struct tcp_connection *c)
{
	if(c->wsq_head != NULL && (c->flags & F_CONN_HASHED)
			&& c->state != S_CONN_BAD) {
		if(likely(tcp_reactor_enqueue_job(c, TCP_R_WRITE) == 0))
			return; /* keep POOL_BUSY + in-pool ref; the write job completes */
		/* enqueue failed: fall through to un-shield; the staged data waits in
		 * wsq until the next write trigger */
	}
	/* Close paths: hand the still-held in-pool ref to read_close(), which drops
	 * it last (see its comment) - safe even if a transient ref (e.g. the
	 * tls:connection-out dispatch ref) is still outstanding. */
	if(unlikely(!(c->flags & F_CONN_HASHED) || (c->state == S_CONN_BAD))) {
		tcp_reactor_read_close(c);
		return;
	}
	if(unlikely(tcp_reactor_read_rearm(c) < 0)) {
		tcp_reactor_read_close(c);
		return;
	}
	/* healthy: re-armed into io_h + the local timer, so release the in-pool
	 * ref. The hash ref (and any still-outstanding transient holder) keeps the
	 * conn alive. */
	if(unlikely(tcpconn_put(c)))
		tcpconn_destroy(c); /* only if this was somehow the last ref */
}

/* mode 2 write dispatch entry points, called from tcp_send() (tcp_main.c). */

/* mode 2: a TCP worker hands an outgoing payload to PROC_TCP_MAIN, which owns
 * all writes. TLS is handled in PROC_TCP_MAIN and not at the worker.
 *
 * Ownership: the caller's refcnt on c is transferred into the write request and
 * dropped by tcp_main once the request is handled (see the CONN_WRITE_REQ arm of
 * handle_ser_child()); on any error here we drop it ourselves so c is not leaked.
 *
 * Returns len as soon as the payload is accepted for sending - NOT when it
 * reaches the wire. The send is async (handed to PROC_TCP_MAIN, queued in
 * wbuf_q, written on POLLOUT), so the SIP caller sees success immediately; a
 * later send failure is handled by tearing down the connection, and never comes
 * back through this return value.
 *
 * Fast path: if we are already inside PROC_TCP_MAIN (a core CRLF pong / HTTP 100
 * sent via tcp_send() from the read path) there is no worker->main hop - queue
 * on wbuf_q and arm POLLOUT directly, mirroring the CONN_WRITE_REQ handler. */
int tcp_reactor_send_put(struct tcp_connection *c, const char *buf,
		unsigned len, snd_flags_t send_flags)
{
	tcp_reactor_write_req_t *wreq;
	long response[2];

	if(unlikely(is_tcp_main())) {
		/* Called from within PROC_TCP_MAIN itself - no unix_tcp_sock self-send.
		 * BUT is_tcp_main() is true for the whole process, i.e. BOTH the io_wait
		 * thread AND the reactor pool threads. Only the io_wait thread owns io_h,
		 * so the two cases must be handled differently. Release the refcnt the
		 * caller (tcp_send) took on c either way. */
		if(unlikely(tcpconn_put(c))) {
			tcpconn_destroy(c); /* was the last ref */
			return -1;
		}
		if(unlikely((c->state == S_CONN_BAD) || !(c->flags & F_CONN_HASHED))) {
			/* in the process of being destroyed => drop the write */
			return -1;
		}
		if(tcp_reactor_thread_idx >= 0) {
			/* On a POOL thread: e.g. WS ws_send_crlf()/pong/close issued inline
			 * from the read path (ws_frame_receive) while this thread owns the
			 * shielded conn. We must NOT touch io_h or c's watch flags here -
			 * that races the io_wait thread and can re-arm the fd while a read
			 * job owns it, letting a second thread run on the same con->req
			 * (torn WS frames / double-unmask -> garbage to workers). Stage the
			 * payload on the conn's wsq (shm, under write_lock); the read job's
			 * completion (tcp_reactor_unshield_or_chain) drains it via a chained
			 * write job. */
			if(unlikely(tcp_reactor_wsq_add(c, buf, len, send_flags) < 0))
				return -1;
			return (int)len;
		}
		/* io_wait thread (e.g. a core CRLF keepalive pong or HTTP/1.1
		 * 100-continue sent via tcp_send() from the reactor's own read path):
		 * safe to queue the payload and arm POLLOUT directly, mirroring the
		 * CONN_WRITE_REQ handler. */
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

/* mode 2: no usable connection exists - ask PROC_TCP_MAIN to open a new
 * outbound connection and send on it. The worker opens no socket and creates no
 * tcp_connection. Returns len on success (optimistic), -1 on error. */
int tcp_reactor_connect_put(struct dest_info *dst, union sockaddr_union *from,
		const char *buf, unsigned len)
{
	tcp_reactor_connect_req_t *creq;
	long response[2];

	if(unlikely(is_tcp_main())) {
		/* A new outbound connection requested by code running inside
		 * PROC_TCP_MAIN itself. The unix_tcp_sock self-send below is invalid
		 * here. The known in-tcp_main senders (core read-path CRLF pong /
		 * HTTP 100-continue) always target an already-established connection
		 * (handled by tcp_reactor_send_put), so this path is not expected to be
		 * reached; fail loudly rather than emit a confusing "send on 0" error. */
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

/* mode 2: a worker finished a dispatched tls:connection-out event
 * (tcp_reactor_dispatch_tls_event). tcpconn is the connection; drop the
 * dispatch refcnt taken there. Doing it here, in PROC_TCP_MAIN, keeps
 * connection destruction on the owner. Runs on the io_wait thread
 * (handle_ser_child's CONN_TLS_EVENT_DONE arm). */
void tcp_reactor_handle_tls_event_done(struct tcp_connection *tcpconn)
{
	if(unlikely(tcpconn_put(tcpconn)))
		tcpconn_destroy(tcpconn);
}

/* mode 2: close connection on core errors (used with tcp_script_mode=0).
 * scid is the connection id (rcv->proto_reserved1) - the worker never has a
 * tcp_connection pointer for a dispatched task. Runs on the io_wait thread
 * (handle_ser_child's CONN_SCRIPT_CLOSE arm). */
void tcp_reactor_handle_script_close(int scid)
{
	struct tcp_connection *tcpconn;

	tcpconn = tcpconn_get(scid, 0, 0, 0, 0); /* takes a ref */
	if(tcpconn == NULL) {
		LM_DBG("mode2: script-close for conn id %d: already gone\n", scid);
		return;
	}
	/* if a pool job currently owns it,
	 * do not unhash/free here - mark it bad and let the job completion
	 * close it */
	if(unlikely(tcpconn->flags & F_CONN_POOL_BUSY)) {
		tcpconn->state = S_CONN_BAD;
		tcpconn->timeout = get_ticks_raw();
		if(unlikely(tcpconn_put(tcpconn)))
			tcpconn_destroy(tcpconn); /* can't happen while busy */
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

/* mode 2: worker queued a write; wreq is the write request (shm-allocated by
 * the worker in tcp_reactor_send_put()). Queues the payload into the wbuf_q
 * (encrypting it first for TLS) and enables write watching, or (PROTO_TCP/
 * TLS/WSS) stages it on wsq and shields+enqueues a TCP_R_WRITE pool job.
 * Runs on the io_wait thread (handle_ser_child's CONN_WRITE_REQ arm). Frees
 * wreq and wreq->buf on every path. */
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
	/* Writes are offloaded to a pool thread. Stage the plaintext, then -
	 * if the conn is not already owned by a pool job - shield it and
	 * enqueue a write job; for TLS the job tls_encode()s each staged chunk
	 * before flushing wbuf_q (the conn is shielded, so that pool thread
	 * owns the SSL object exclusively). If the conn is busy, the data
	 * waits in wsq and the running job's completion chains a write job.
	 * Plain WS keeps the inline copy+watch path below (no TLS); WSS is
	 * staged here too so its tls_encode runs on the owning pool thread. */
	if(likely(tcpconn->type == PROTO_TCP || tcpconn->type == PROTO_TLS
			   || tcpconn->type == PROTO_WSS)) {
		if(unlikely(tcp_reactor_wsq_add(
							tcpconn, wreq->buf, wreq->len, wreq->send_flags)
					< 0)) {
			tcpconn->state = S_CONN_BAD;
			tcpconn->timeout = get_ticks_raw(); /* force reaper */
			shm_free(wreq->buf);
			shm_free(wreq);
			return;
		}
		shm_free(wreq->buf);
		shm_free(wreq);
		if(!(tcpconn->flags & F_CONN_POOL_BUSY)) {
			if(unlikely(tcp_reactor_shield(tcpconn, -1) < 0)) {
				tcpconn->state = S_CONN_BAD;
				tcpconn->timeout = get_ticks_raw();
				return;
			}
			if(unlikely(tcp_reactor_enqueue_job(tcpconn, TCP_R_WRITE) < 0)) {
				/* release the in-pool ref + un-shield; data waits in wsq */
				tcp_reactor_unshield_or_chain(tcpconn);
			}
		}
		return;
	}
	if(unlikely(tcp_reactor_wbuf_enqueue(
						tcpconn, wreq->buf, wreq->len, wreq->send_flags)
				< 0)) {
		shm_free(wreq->buf);
		shm_free(wreq);
		return;
	}
	shm_free(wreq->buf);
	shm_free(wreq);
	tcp_reactor_watch_write(tcpconn);
}

/* mode 2: worker has no usable connection - open a new outbound one here in
 * PROC_TCP_MAIN, or reuse a matching one another worker created meanwhile.
 * creq is the connect request (shm-allocated by the worker in
 * tcp_reactor_connect_put()). Runs on the io_wait thread (handle_ser_child's
 * CONN_CONNECT_REQ arm). Frees creq and creq->buf on every path. */
void tcp_reactor_handle_connect_req(tcp_reactor_connect_req_t *creq)
{
	struct tcp_connection *cc;
	union sockaddr_union *cfrom;
	struct ip_addr cip;
	int cport;
	ticks_t t;
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
		/* reuse: queue the payload onto the existing connection. The
		 * reused conn may already be owned by a pool job (mid-read):
		 * PROTO_TCP/TLS - stage the plaintext on wsq and let a pool write
		 *     job do the TLS encode + flush
		 * WS/WSS - keep the inline encode+watch path. */
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
		if(likely(cc->type == PROTO_TCP || cc->type == PROTO_TLS
				   || cc->type == PROTO_WSS)) {
			if(unlikely(tcp_reactor_wsq_add(
								cc, creq->buf, creq->len, creq->send_flags)
						< 0)) {
				cc->state = S_CONN_BAD;
				cc->timeout = get_ticks_raw(); /* force reaper */
				shm_free(creq->buf);
				shm_free(creq);
				return;
			}
			shm_free(creq->buf);
			shm_free(creq);
			if(!(cc->flags & F_CONN_POOL_BUSY)) {
				if(unlikely(tcp_reactor_shield(cc, -1) < 0)) {
					cc->state = S_CONN_BAD;
					cc->timeout = get_ticks_raw();
					return;
				}
				if(unlikely(tcp_reactor_enqueue_job(cc, TCP_R_WRITE) < 0)) {
					/* release in-pool ref + un-shield; data waits in wsq */
					tcp_reactor_unshield_or_chain(cc);
				}
			}
			return;
		}
		if(likely(tcp_reactor_wbuf_enqueue(
						  cc, creq->buf, creq->len, creq->send_flags)
				   == 0))
			tcp_reactor_watch_write(cc);
		shm_free(creq->buf);
		shm_free(creq);
		return;
	}
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

/* mode 2: full read/write handling for a connection's io event, called from
 * handle_tcpconn_ev() as its very first action when ksr_tcp_main_threads==2 -
 * tcp_main.c owns the fd permanently there (no fd-passing, no send2child), so
 * this is effectively a second implementation of handle_tcpconn_ev() for that
 * mode, kept here because the shield/enqueue/unshield dance and its io_h/timer
 * bookkeeping is reactor-exclusive. Runs on the io_wait thread. Same return
 * convention as handle_tcpconn_ev()/handle_io(). */
int tcp_reactor_handle_tcpconn_ev(
		struct tcp_connection *tcpconn, short ev, int fd_i)
{
	/* mode 2: tcp_main owns the fd permanently, reads and
	 * assembles SIP messages (no fd-passing, no send2child).
	 *
	 * Worker busy-tracking (tcp_children[].busy) is skipped: use
	 * kernel socket load balancing like udp_receiver_mode = 1
	 */
	rd_conn_flags_t read_flags;
	int n;
	int resp;

	/* Defensive: a shielded conn (F_CONN_POOL_BUSY) should already be out of
	 * io_h, so this event shouldn't fire. It can if the fd stayed armed while
	 * busy (e.g. a WS handshake con->type flip in the worker outrunning the
	 * shield's io_watch_del). Enqueuing a second read job would race two pool
	 * threads on the same con->req buffer, so just drop the fd from the watch
	 * set; the in-flight job's completion re-adds it, and level-triggered
	 * epoll re-fires for any buffered data. */
	if(unlikely(tcpconn->flags & F_CONN_POOL_BUSY)) {
		if(tcpconn->s != -1) {
			if(unlikely(tcpmain_io_watch_del(tcpconn->s, fd_i, 0) < 0))
				LM_ERR("reactor: io_watch_del (busy re-arm) failed for %p "
					   "fd %d\n",
						tcpconn, tcpconn->s);
		}
		tcpconn->flags &= ~(F_CONN_READ_W | F_CONN_WRITE_W);
		return 0;
	}

#ifdef TCP_ASYNC
	/* drain pending writes first if the fd is write-watched */
	if((ev & (POLLOUT | POLLERR | POLLHUP))
			&& (tcpconn->flags & F_CONN_WRITE_W)) {
		int wempty_q = 0;
		if(unlikely((ev & (POLLERR | POLLHUP))
					|| (wbufq_run(tcpconn->s, tcpconn, &wempty_q) < 0))) {
			/* write error or peer gone: tear the connection down */
			goto reactor_close;
		}
		if(wempty_q) {
			tcpconn->flags &= ~F_CONN_WANTS_WR;
			if(!(tcpconn->flags & F_CONN_READ_W)) {
				if(unlikely(tcpmain_io_watch_chg(tcpconn->s, POLLIN, fd_i)
							== -1)) {
					LM_ERR("io_watch_chg (reactor wr) failed for %p fd "
						   "%d\n",
							tcpconn, tcpconn->s);
					goto reactor_close;
				}
			}
			tcpconn->flags &= ~F_CONN_WRITE_W;
			if(tcpconn_close_after_send(tcpconn))
				goto reactor_close;
		}
	}
#endif /* TCP_ASYNC */

	if(ev & (POLLIN | POLLERR | POLLHUP)) {
#ifdef TCP_ASYNC
		/* All four transports read on a pool thread. The shield gives that
		 * thread exclusive ownership of the conn (F_CONN_POOL_BUSY), so the
		 * SSL object and the WS codec are each touched by one thread at a
		 * time (tls_encode()'s trampoline scratch buffer is per-thread too,
		 * see tcp_mtops.c). The pre-upgrade handshake runs as PROTO_TCP/
		 * PROTO_TLS, already covered, and stays covered after the con->type
		 * flip to WS/WSS. */
		if(likely(tcpconn->type == PROTO_TCP || tcpconn->type == PROTO_TLS
				   || tcpconn->type == PROTO_WS
				   || tcpconn->type == PROTO_WSS)) {
			/* shield the conn from the (level-triggered) reactor + timer so
			 * a pool thread can own it exclusively, then enqueue the read */
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
			return 0;
		}
#endif /* TCP_ASYNC */
		/* !TCP_ASYNC only: under TCP_ASYNC the four-transport offload above
		 * returns, so this synchronous-read fall-through is the non-async
		 * build's live path (unreachable in a normal TCP_ASYNC build). */
		/* fold any error/EOF event into a forced EOF so a final buffered
		 * message is still drained before the connection is closed */
		read_flags = ((
#ifdef POLLRDHUP
							  (ev & POLLRDHUP) |
#endif /* POLLRDHUP */
							  (ev & (POLLHUP | POLLERR))
							  | (tcpconn->flags
									  & (F_CONN_EOF_SEEN | F_CONN_FORCE_EOF)))
							 && !(ev & POLLPRI))
							 ? RD_CONN_FORCE_EOF
							 : 0;
	repeat_read:
		resp = tcp_read_req(tcpconn, &n, &read_flags);
		/* tcp_read_req() dispatches complete messages to workers via
		 * tcp_reactor_dispatch_msg(); for TLS it may run OpenSSL through
		 * the direct-call trampoline (KSR_TCPX_MAIN_PIDX). */
		if(unlikely(resp < 0)) {
			if(resp != CONN_EOF)
				tcpconn->state = S_CONN_BAD;
			goto reactor_close;
		}
		if(unlikely(read_flags & RD_CONN_REPEAT_READ))
			goto repeat_read;
		/* partial or complete read: keep the fd, refresh the idle timeout.
		 * If a message is still mid-read, also bound the timeout to the
		 * message-read deadline so a stalled partial read is reaped. */
		{
			ticks_t tnow = get_ticks_raw();
			tcpconn->timeout = tnow + cfg_get(tcp, tcp_cfg, con_lifetime);
#ifdef TCP_ASYNC
			tcp_reactor_arm_read_timeout(tcpconn, tnow);
#endif /* TCP_ASYNC */
		}
	}
	return 0;

reactor_close:
	/* unhash first, while F_CONN_MAIN_TIMER is still set, so the local
	 * timer is removed before the connection is freed (try_unhash only
	 * deletes the timer when the flag is set). Then stop watching the fd. */
	if(tcpconn_try_unhash(tcpconn))
		tcpconn_put(tcpconn);
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
	 * tcp_reactor_read_close - the reader detected an EOF/error/reset, so
	 * emit the tcpops close event here too. Fire-once guarded. */
	tcp_emit_closed_event(tcpconn);
	tcpconn_put_destroy(tcpconn);
	return -1;
}

/* ========================================================================
 * mode 2: reactor thread pool inside PROC_TCP_MAIN - all reads on the pool;
 * all writes except plain-WS on the pool
 *
 * Threads: one io_wait thread (this process's main thread) + N pool threads.
 *
 * Ownership / serialization:
 *  - epoll (io_h) and the local timer (tcp_main_ltimer): io_wait thread ONLY.
 *    Pool threads never call io_watch_*() or local_timer_*().
 *  - F_CONN_POOL_BUSY = "a pool job owns this conn": set by the io_wait thread
 *    when it shields the conn (removes it from io_h + the timer) and enqueues a
 *    read/write job; cleared by the io_wait completion. While set, the conn has
 *    at most ONE owner (one read OR write job at a time; chained jobs are
 *    sequential), so its read/write callbacks never run concurrently with each
 *    other or with the reactor.
 *  - A single "in-pool" refcount is taken at shield and released at
 *    unshield/close; it spans chained jobs and pins the conn so it cannot be
 *    freed while a pool thread uses it.
 *
 * Per-field synchronization:
 *  - c->flags        : io_wait thread ONLY (pool jobs never write c->flags;
 *                      reads/buffers/state only). No race.
 *  - c->req          : the read job exclusively while it runs (conn shielded).
 *  - c->wbuf_q,c->wsq: c->write_lock (gen_lock). The completion's unlocked
 *                      peeks (_wbufq_non_empty / wsq_head) are safe via the
 *                      done_lock happens-before edge.
 *  - c->refcnt       : atomic.
 *  - handoff pool->io_wait: push to done_head under done_lock + a notify byte;
 *                      io_wait reads done_head under done_lock - a
 *                      release/acquire barrier, so completion sees the job's
 *                      final writes.
 **/

/* tcp_reactor_thread_idx is defined near the top (needed earlier by
 * tcp_reactor_send_put's pool-thread detection). */

/* runs in the io_wait thread on notify_pipe readiness: process all completed
 * jobs (coalesced). Only the io_wait thread touches io_h, so all re-arm / close
 * happens here, never in a pool thread. */
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
#ifdef TCP_ASYNC
			case TCP_R_READ:
			case TCP_R_WRITE:
				/* The conn carries the single in-pool refcount (taken at
				 * shield, released by close / unshield - not per job).
				 * resp < 0 => EOF/error/write-error => tear down; else either
				 * chain a staged write or hand the conn back to the reactor. */
				if(unlikely(resp < 0)) {
					tcp_reactor_read_close(conn);
				} else {
					tcp_reactor_unshield_or_chain(conn);
				}
				break;
#endif /* TCP_ASYNC */
			default:
				break;
		}
		job = next;
	}
}

/* pool thread body. arg = thread index. */
static void *tcp_reactor_thread_routine(void *arg)
{
	struct tcp_reactor_job *job;
	struct tcp_connection *conn;
	sigset_t set;
	char wake = 'x';

	tcp_reactor_thread_idx = (int)(long)arg;

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

	while(1) {
		job = NULL;
		tcp_cond_lock(&tcp_reactor_wake->cond);
		while(!tcp_rpool.stop && tcp_rpool.task_head == NULL) {
			tcp_cond_wait(&tcp_reactor_wake->cond);
		}
		if(tcp_rpool.stop && tcp_rpool.task_head == NULL) {
			tcp_cond_unlock(&tcp_reactor_wake->cond);
			break;
		}
		/* All read/write/run jobs are enqueued onto task_head by the io_wait
		 * thread, which has already shielded the conn (F_CONN_POOL_BUSY) and
		 * taken the in-pool refcount, so a pool thread only ever touches a
		 * connection already out of the reactor's epoll and timer. */
		job = tcp_rpool.task_head;
		if(job != NULL) {
			tcp_rpool.task_head = job->next;
			if(tcp_rpool.task_head == NULL)
				tcp_rpool.task_tail = NULL;
		}
		tcp_cond_unlock(&tcp_reactor_wake->cond);

		if(job == NULL)
			continue; /* spurious wakeup */

		switch(job->op) {
			case TCP_R_READ: {
				/* read + reassemble on conn->s (all of PROTO_TCP/TLS/WS/WSS).
				 * tcp_read_req() reads via _tconfd(conn)==conn->s; for WS the frame
				 * codec runs here under the shield and dispatches the defragmented
				 * payload to workers via tcp_reactor_dispatch_msg(). No io_watch /
				 * no free here - the io_wait thread completes. */
				rd_conn_flags_t read_flags;
				int n, resp;
				conn = job->conn;
				read_flags =
						(conn->flags & (F_CONN_EOF_SEEN | F_CONN_FORCE_EOF))
								? RD_CONN_FORCE_EOF
								: 0;
				do {
					resp = tcp_read_req(conn, &n, &read_flags);
				} while(resp >= 0 && (read_flags & RD_CONN_REPEAT_READ));
				if(resp < 0 && resp != CONN_EOF)
					conn->state = S_CONN_BAD;
				job->resp = resp;
				break;
			}
			case TCP_R_WRITE: {
				/* drain the connection's plaintext staging list into wbuf_q
				 * (PROTO_TCP: plain copy; PROTO_TLS: tls_encode via the
				 * direct-call trampoline on this pool thread) and flush wbuf_q to
				 * the socket. No c->flags writes here (the io_wait completion owns
				 * those); only buffers/state under write_lock. */
				struct tcp_wchunk *ch;
				int werr = 0, wempty = 0;
				conn = job->conn;
				lock_get(&conn->write_lock);
				while((ch = conn->wsq_head) != NULL) {
					conn->wsq_head = ch->next;
					if(tcp_reactor_wbuf_add_locked(
							   conn, ch->buf, ch->len, ch->send_flags)
							< 0)
						werr = 1;
					shm_free(ch->buf);
					shm_free(ch);
				}
				conn->wsq_tail = NULL;
				lock_release(&conn->write_lock);
				if(!werr && wbufq_run(conn->s, conn, &wempty) < 0)
					werr = 1;
				/* resp: <0 error; 1 = wbuf_q still has data (needs POLLOUT,
				 * handled inline after un-shield); 0 = fully drained */
				job->resp = werr ? -1 : (wempty ? 0 : 1);
				break;
			}
			case TCP_R_RUN:
				LM_WARN("tcpr-pool-%d: running tcpx task %p (exec=%p)\n",
						tcp_reactor_thread_idx, (void *)job->task,
						job->task ? (void *)job->task->exec : NULL);
				if(job->task && job->task->exec)
					job->task->exec(job->task->param, KSR_TCPX_MAIN_PIDX);
				pkg_free(job);
				continue;
			default:
				LM_ERR("unknown reactor job op %d\n", job->op);
				break;
		}

		/* hand the completed job back to the reactor thread */
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
	return NULL;
}

/* create the notify pipe + spawn the pool. Runs in PROC_TCP_MAIN after io_h is
 * initialized. Returns 0 on success, -1 on error. */
int tcp_reactor_pool_init(void)
{
	int i;

	LM_INFO("TCP reactor: using %d threads\n", ksr_tcp_reactor_threads);

	/* pool_init() runs on PROC_TCP_MAIN's io_wait/main thread; name it here (pool
	 * threads name themselves in tcp_reactor_thread_routine). OS thread comm only,
	 * so kamcmd core.ps still reports the usual process description. */
#if defined(HAVE_PTHREAD_SETNAME_NP_2ARG)
	pthread_setname_np(pthread_self(), "tcpr-iowait");
#elif defined(HAVE_PTHREAD_SETNAME_NP_1ARG)
	pthread_setname_np("tcpr-iowait");
#endif

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
	/* NOT registered in io_h here - tcp_reactor_pool_init() runs on
	 * PROC_TCP_MAIN's io_wait thread, but io_h itself stays private to
	 * tcp_main.c (see tcp_conn.h); the caller (tcp_main_loop()) does the
	 * io_watch_add() for this pipe right after this function returns. */
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
	LM_INFO("tcp reactor mode 2: started %d pool threads\n",
			tcp_rpool.threads_no);
	return 0;
}

/* stop + join the pool. Safe to call when the pool was never started. */
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

/* mode 2: drain the notify pipe and process all completed jobs (coalesced).
 * Called from tcp_main.c's handle_io() on F_TCP_REACTOR_NOTIFY readiness -
 * runs on the io_wait thread. */
void tcp_reactor_handle_notify(void)
{
	char nbuf[64];

	while(read(tcp_rpool.notify_pipe[0], nbuf, sizeof(nbuf)) > 0) {
	}
	tcp_reactor_handle_done();
}

/* mode 2: create the reactor dispatch socketpair (AF_UNIX SOCK_DGRAM).
 * MUST be called in the main process before any TCP child (workers AND
 * PROC_TCP_MAIN) is forked, so all of them inherit the shared fds: workers
 * recvfrom dsock[0] (the kernel load-balances across them) and PROC_TCP_MAIN
 * sends reassembled SIP tasks on dsock[1]. Both ends are non-blocking.
 * Returns 0 on success (or when not in mode 2), -1 on error. */
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
	LM_INFO("tcp reactor dispatch socketpair created (rfd=%d wfd=%d)\n",
			ksr_tcp_reactor_dsock[0], ksr_tcp_reactor_dsock[1]);

	/* reactor pool wakeup: its condvar must live in shm and be initialised
	 * before fork so PROC_TCP_MAIN's io_wait thread and pool threads share the
	 * same PROCESS_SHARED condvar. The pool threads + notify pipe (tcp_rpool)
	 * are created later, inside PROC_TCP_MAIN. */
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
	LM_INFO("tcp reactor pool wakeup initialized\n");
	return 0;
}
