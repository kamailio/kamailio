/**
 * Copyright (C) 2025 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _TCP_MTOPS_
#define _TCP_MTOPS_

typedef void (*tcpx_cbe_f)(void *p, int pidx);

typedef struct tcpx_task
{
	tcpx_cbe_f exec;
	void *param;
} tcpx_task_t;

typedef struct tcpx_task_result
{
	int code;
	void *data;
} tcpx_task_result_t;

int ksr_tcpx_proc_list_init(void);
int ksr_tcpx_proc_list_prepare(void);
int ksr_tcpx_task_send(tcpx_task_t *task, int pidx);
int ksr_tcpx_task_result_recv(tcpx_task_result_t **rtask, int pidx);
int ksr_tcpx_thread_eresult(tcpx_task_result_t *rtask, int pidx);

#endif
