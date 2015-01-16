/*
 * Copyright (C) 2014 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*!
* \file
* \brief Kamailio core :: Asynchronus tasks
* \ingroup core
* Module: \ref core
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#include "dprint.h"
#include "sr_module.h"
#include "ut.h"
#include "pt.h"
#include "cfg/cfg_struct.h"


#include "async_task.h"

static int _async_task_workers = 0;
static int _async_task_sockets[2];

int async_task_run(int idx);

/**
 *
 */
int async_task_init_sockets(void)
{
	if (socketpair(PF_UNIX, SOCK_DGRAM, 0, _async_task_sockets) < 0) {
		LM_ERR("opening tasks dgram socket pair\n");
		return -1;
	}
	LM_DBG("inter-process event notification sockets initialized\n");
	return 0;
}

/**
 *
 */
void async_task_close_sockets_child(void)
{
	LM_DBG("closing the notification socket used by children\n");
	close(_async_task_sockets[1]);
}

/**
 *
 */
void async_task_close_sockets_parent(void)
{
	LM_DBG("closing the notification socket used by parent\n");
	close(_async_task_sockets[0]);
}

/**
 *
 */
int async_task_init(void)
{
	LM_DBG("start initializing asynk task framework\n");
	if(_async_task_workers<=0)
		return 0;

	/* advertise new processes to core */
	register_procs(_async_task_workers);

	/* advertise new processes to cfg framework */
	cfg_register_child(_async_task_workers);

	return 0;
}

/**
 *
 */
int async_task_initialized(void)
{
	if(_async_task_workers<=0)
		return 0;
	return 1;
}

/**
 *
 */
int async_task_child_init(int rank)
{
	int pid;
	int i;

	if(_async_task_workers<=0)
		return 0;

	LM_DBG("child initializing asynk task framework\n");

	if (rank==PROC_INIT) {
		if(async_task_init_sockets()<0) {
			LM_ERR("failed to initialize tasks sockets\n");
			return -1;
		}
		return 0;
	}

	if(rank>0) {
		async_task_close_sockets_parent();
		return 0;
	}
	if (rank!=PROC_MAIN)
		return 0;

	for(i=0; i<_async_task_workers; i++) {
		pid=fork_process(PROC_RPC, "Async Task Worker", 1);
		if (pid<0)
			return -1; /* error */
		if(pid==0) {
			/* child */

			/* initialize the config framework */
			if (cfg_child_init())
				return -1;
			/* main function for workers */
			if(async_task_run(i+1)<0) {
				LM_ERR("failed to initialize task worker process: %d\n", i);
				return -1;
			}
		}
	}

	return 0;
}

/**
 *
 */
int async_task_set_workers(int n)
{
	if(_async_task_workers>0) {
		LM_WARN("task workers already set\n");
		return 0;
	}
	if(n<=0)
		return 0;

	_async_task_workers = n;

	return 0;
}

/**
 *
 */
int async_task_push(async_task_t *task)
{
	int len;

	if(_async_task_workers<=0)
		return 0;

	len = write(_async_task_sockets[1], &task, sizeof(async_task_t*));
	if(len<=0) {
		LM_ERR("failed to pass the task to asynk workers\n");
		return -1;
	}
	LM_DBG("task sent [%p]\n", task);
	return 0;
}

/**
 *
 */
int async_task_run(int idx)
{
	async_task_t *ptask;
	int received;

	LM_DBG("async task worker %d ready\n", idx);

	for( ; ; ) {
		if ((received = recvfrom(_async_task_sockets[0],
							&ptask, sizeof(async_task_t*),
							0, NULL, 0)) < 0) {
			LM_ERR("failed to received task (%d: %s)\n", errno, strerror(errno));
			continue;
		}
		if(received != sizeof(async_task_t*)) {
			LM_ERR("invalid task size %d\n", received);
			continue;
		}
		if(ptask->exec!=NULL) {
			LM_DBG("task executed [%p] (%p/%p)\n", ptask,
					ptask->exec, ptask->param);
			ptask->exec(ptask->param);
		}
		shm_free(ptask);
	}

	return 0;
}
