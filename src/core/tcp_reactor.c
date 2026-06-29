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

#include <sys/socket.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "dprint.h"
#include "mem/shm_mem.h"
#include "tcp_reactor.h"
#include "tcp_server.h" /* ksr_tcp_reactor_get_dispatch_wfd() */

/*
 * Allocate a tcp_reactor_task_t in shm, copy the reassembled SIP message
 * and receive_info into it, then send the pointer to the workers via the
 * dispatch socketpair write end.
 *
 * Called from PROC_TCP_MAIN's io_wait loop (mode 2) at the point where
 * receive_tcp_msg() would normally be called in modes 0/1.
 *
 * The receiving worker calls receive_msg() then shm_free()s the task.
 */
int tcp_reactor_dispatch_msg(
		char *buf, unsigned int len, struct receive_info *rcv)
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
