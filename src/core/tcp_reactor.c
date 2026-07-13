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

#include "dprint.h"
#include "mem/shm_mem.h"
#include "tcp_reactor.h"
#include "tcp_server.h" /* ksr_tcp_reactor_get_dispatch_wfd() */

/* The dispatch socket carries a single task pointer per datagram, and the
  * A write of at most PIPE_BUF bytes is guaranteed atomic */
_Static_assert(sizeof(uintptr_t) <= PIPE_BUF,
		"task pointer too wide for atomic dispatch write");

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
	ssize_t sent;

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
	sent = send(ksr_tcp_reactor_get_dispatch_wfd(), &ptr, sizeof(ptr), 0);
	if(sent != (ssize_t)sizeof(ptr)) {
		LM_ERR("failed to dispatch SIP task to workers (%s)\n",
				(sent < 0) ? strerror(errno) : "short write");
		shm_free(task);
		return -1;
	}
	return 0;
}
