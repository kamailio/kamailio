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

#include <sys/socket.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>

#include "dprint.h"
#include "mem/shm_mem.h"
#include "tcp_conn.h" /* struct tcp_connection, tcpconn_ref/put, F_TCP_REQ_* */
#include "tcp_reactor.h"
#include "tcp_server.h" /* ksr_tcp_reactor_get_dispatch_wfd() */

/* The dispatch socket carries a single task pointer per datagram, and the
  * A write of at most PIPE_BUF bytes is guaranteed atomic */
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

	/* Keep the connection alive across the hop to the worker; the worker only
	 * reads c's shm-cached TLS metadata and hands the refcnt back to
	 * PROC_TCP_MAIN (CONN_TLS_EVENT_DONE), which drops it. We run on a
	 * PROC_TCP_MAIN thread with the connection still owned there (the read job
	 * that reached us holds it), so our extra ref is never the last one - the
	 * error-path tcpconn_put() below therefore cannot reach zero, and no
	 * cross-process destroy is needed here. */
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
