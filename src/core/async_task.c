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
#include "parser/parse_param.h"


#include "async_task.h"

static async_wgroup_t *_async_wgroup_list = NULL;
static async_wgroup_t *_async_wgroup_crt = NULL;

int async_task_run(async_wgroup_t *awg, int idx);

/**
 *
 */
int async_task_workers_get(void)
{
	return (_async_wgroup_list)?_async_wgroup_list->workers:0;
}

/**
 *
 */
int async_task_workers_active(void)
{
	if(_async_wgroup_list==NULL || _async_wgroup_list->workers<=0)
		return 0;

	return 1;
}

/**
 *
 */
async_wgroup_t *async_task_workers_get_crt(void)
{
	return _async_wgroup_crt;
}

/**
 *
 */
int async_task_init_sockets(void)
{
	int val;
	async_wgroup_t *awg;

	for(awg=_async_wgroup_list; awg!=NULL; awg=awg->next) {
		if (socketpair(PF_UNIX, SOCK_DGRAM, 0, awg->sockets) < 0) {
			LM_ERR("opening tasks dgram socket pair\n");
			return -1;
		}

		if (awg->nonblock) {
			val = fcntl(awg->sockets[1], F_GETFL, 0);
			if(val<0) {
				LM_WARN("failed to get socket flags\n");
			} else {
				if(fcntl(awg->sockets[1], F_SETFL, val | O_NONBLOCK)<0) {
					LM_WARN("failed to set socket nonblock flag\n");
				}
			}
		}
	}

	LM_DBG("inter-process event notification sockets initialized\n");
	return 0;
}

/**
 *
 */
void async_task_close_sockets_child(void)
{
	async_wgroup_t *awg;

	LM_DBG("closing the notification socket used by children\n");

	for(awg=_async_wgroup_list; awg!=NULL; awg=awg->next) {
		close(awg->sockets[1]);
	}
}

/**
 *
 */
void async_task_close_sockets_parent(void)
{
	async_wgroup_t *awg;

	LM_DBG("closing the notification socket used by parent\n");

	for(awg=_async_wgroup_list; awg!=NULL; awg=awg->next) {
		close(awg->sockets[0]);
	}
}

/**
 *
 */
int async_task_init(void)
{
	int nrg = 0;
	async_wgroup_t *awg;

	LM_DBG("start initializing asynk task framework\n");
	if(_async_wgroup_list==NULL || _async_wgroup_list->workers<=0)
		return 0;

	/* overall number of processes */
	for(awg=_async_wgroup_list; awg!=NULL; awg=awg->next) {
		nrg += awg->workers;
	}

	/* advertise new processes to core */
	register_procs(nrg);

	/* advertise new processes to cfg framework */
	cfg_register_child(nrg);

	return 0;
}

/**
 *
 */
int async_task_initialized(void)
{
	if(_async_wgroup_list==NULL || _async_wgroup_list->workers<=0)
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
	char pname[64];
	async_wgroup_t *awg;

	if(_async_wgroup_list==NULL || _async_wgroup_list->workers<=0)
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
		/* no need to close the socket from sip workers */
		/* async_task_close_sockets_parent(); */
		return 0;
	}
	if (rank!=PROC_MAIN)
		return 0;

	for(awg=_async_wgroup_list; awg!=NULL; awg=awg->next) {
		snprintf(pname, 62, "Async Task Worker - %s",
				(awg->name.s)?awg->name.s:"unknown");
		for(i=0; i<awg->workers; i++) {
			pid=fork_process(PROC_RPC, pname, 1);
			if (pid<0) {
				return -1; /* error */
			}
			if(pid==0) {
				/* child */

				/* initialize the config framework */
				if (cfg_child_init()) {
					return -1;
				}
				/* main function for workers */
				if(async_task_run(awg, i+1)<0) {
					LM_ERR("failed to initialize task worker process: %d\n", i);
					return -1;
				}
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
	str gname = str_init("default");

	if(_async_wgroup_list!=NULL && _async_wgroup_list->workers>0) {
		LM_WARN("task workers already set\n");
		return 0;
	}
	if(n<=0)
		return 0;

	if(_async_wgroup_list==NULL) {
		_async_wgroup_list = (async_wgroup_t*)pkg_malloc(sizeof(async_wgroup_t)
				+ (gname.len+1)*sizeof(char));
		if(_async_wgroup_list==NULL) {
			LM_ERR("failed to create async wgroup\n");
			return -1;
		}
		memset(_async_wgroup_list, 0, sizeof(async_wgroup_t)
				+ (gname.len+1)*sizeof(char));
		_async_wgroup_list->name.s = (char*)_async_wgroup_list
				+ sizeof(async_wgroup_t);
		memcpy(_async_wgroup_list->name.s, gname.s, gname.len);
		_async_wgroup_list->name.len = gname.len;
	}
	_async_wgroup_list->workers = n;

	return 0;
}

/**
 *
 */
int async_task_set_nonblock(int n)
{
	if(n>0 && _async_wgroup_list!=NULL) {
		_async_wgroup_list->nonblock = 1;
	}

	return 0;
}

/**
 *
 */
int async_task_set_usleep(int n)
{
	int v = 0;

	if(_async_wgroup_list!=NULL) {
		v = _async_wgroup_list->usleep;
		_async_wgroup_list->usleep = n;
	}

	return v;
}

/**
 *
 */
int async_task_set_workers_group(char *data)
{
	str sval;
	param_t* params_list = NULL;
	param_hooks_t phooks;
	param_t *pit=NULL;
	async_wgroup_t awg;
	async_wgroup_t *newg;

	if(data==NULL) {
		return -1;
	}
	sval.s = data;
	sval.len = strlen(sval.s);

	if(sval.len<=0) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if(sval.s[sval.len-1]==';') {
		sval.len--;
	}
	if (parse_params(&sval, CLASS_ANY, &phooks, &params_list)<0) {
		return -1;
	}
	memset(&awg, 0, sizeof(async_wgroup_t));

	for (pit = params_list; pit; pit=pit->next) {
		if (pit->name.len==4
				&& strncasecmp(pit->name.s, "name", 4)==0) {
			awg.name = pit->body;
		} else if (pit->name.len==7
				&& strncasecmp(pit->name.s, "workers", 7)==0) {
			if (str2sint(&pit->body, &awg.workers) < 0) {
				LM_ERR("invalid workers value: %.*s\n", pit->body.len, pit->body.s);
				return -1;
			}
		} else if (pit->name.len==6
				&& strncasecmp(pit->name.s, "usleep", 6)==0) {
			if (str2sint(&pit->body, &awg.usleep) < 0) {
				LM_ERR("invalid usleep value: %.*s\n", pit->body.len, pit->body.s);
				return -1;
			}
		} else if (pit->name.len==8
				&& strncasecmp(pit->name.s, "nonblock", 8)==0) {
			if (str2sint(&pit->body, &awg.nonblock) < 0) {
				LM_ERR("invalid nonblock value: %.*s\n", pit->body.len, pit->body.s);
				return -1;
			}
		}
	}

	if(awg.name.len<=0) {
		LM_ERR("invalid name value: [%.*s]\n", sval.len, sval.s);
		return -1;
	}
	if (awg.workers<=0) {
		LM_ERR("invalid workers value: %d\n", awg.workers);
		return -1;
	}

	if(awg.name.len==7 && strncmp(awg.name.s, "default", 7)==0) {
		if(async_task_set_workers(awg.workers)<0) {
			LM_ERR("failed to create the default group\n");
			return -1;
		}
		async_task_set_nonblock(awg.nonblock);
		async_task_set_usleep(awg.usleep);
		return 0;
	}
	if(_async_wgroup_list==NULL) {
		if(async_task_set_workers(1)<0) {
			LM_ERR("failed to create the initial default group\n");
			return -1;
		}
	}

	newg = (async_wgroup_t*)pkg_malloc(sizeof(async_wgroup_t)
				+ (awg.name.len+1)*sizeof(char));
	if(newg==NULL) {
		LM_ERR("failed to create async wgroup [%.*s]\n", sval.len, sval.s);
		return -1;
	}
	memset(newg, 0, sizeof(async_wgroup_t)
				+ (awg.name.len+1)*sizeof(char));
	newg->name.s = (char*)newg + sizeof(async_wgroup_t);
	memcpy(newg->name.s, awg.name.s, awg.name.len);
	newg->name.len = awg.name.len;
	newg->workers = awg.workers;
	newg->nonblock = awg.nonblock;
	newg->usleep = awg.usleep;

	newg->next = _async_wgroup_list->next;
	_async_wgroup_list->next = newg;

	return 0;
}

/**
 *
 */
int async_task_push(async_task_t *task)
{
	int len;

	if(_async_wgroup_list==NULL || _async_wgroup_list->workers<=0) {
		LM_WARN("async task pushed, but no async workers - ignoring\n");
		return 0;
	}

	len = write(_async_wgroup_list->sockets[1], &task, sizeof(async_task_t*));
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
int async_task_group_push(str *gname, async_task_t *task)
{
	int len;
	async_wgroup_t *awg = NULL;

	if(_async_wgroup_list==NULL) {
		LM_WARN("async task pushed, but no async group - ignoring\n");
		return 0;
	}
	for(awg=_async_wgroup_list; awg!=NULL; awg=awg->next) {
		if(awg->name.len==gname->len
				&& memcmp(awg->name.s, gname->s, gname->len)==0) {
			break;
		}
	}
	if(awg==NULL) {
		LM_WARN("group [%.*s] not found - ignoring\n", gname->len, gname->s);
		return 0;
	}
	len = write(awg->sockets[1], &task, sizeof(async_task_t*));
	if(len<=0) {
		LM_ERR("failed to pass the task [%p] to group [%.*s]\n", task,
				gname->len, gname->s);
		return -1;
	}
	LM_DBG("task [%p] sent to group [%.*s]\n", task, gname->len, gname->s);
	return 0;
}

/**
 *
 */
int async_task_run(async_wgroup_t *awg, int idx)
{
	async_task_t *ptask;
	int received;

	LM_DBG("async task worker [%.*s] idx [%d] ready\n", awg->name.len,
			awg->name.s, idx);

	_async_wgroup_crt = awg;

	for( ; ; ) {
		if(unlikely(awg->usleep)) sleep_us(awg->usleep);
		if ((received = recvfrom(awg->sockets[0],
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
			LM_DBG("task executed [%p] (%p/%p)\n", (void*)ptask,
					(void*)ptask->exec, (void*)ptask->param);
			ptask->exec(ptask->param);
		} else {
			LM_DBG("task with no callback function - ignoring\n");
		}
		shm_free(ptask);
	}

	return 0;
}
