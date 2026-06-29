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

/* Allocate a tcp_reactor_task_t in shm, copy buf+rcv into it, and write
 * the pointer to the dispatch socketpair write end. Called from
 * PROC_TCP_MAIN in mode 2 wherever receive_tcp_msg() would be called. */
int tcp_reactor_dispatch_msg(
		char *buf, unsigned int len, struct receive_info *rcv);

#endif /* _TCP_REACTOR_H_ */
