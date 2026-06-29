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
#include <stdarg.h>

#include <pthread.h>

#include "dprint.h"
#include "pt.h"
#include "mem/shm.h"
#include "globals.h"

#include "tcp_mtops.h"

/*
 *
 */
typedef struct ksr_tcpx_proc
{
	int pidx;		  /* index of the process */
	int rank;		  /* rank of the process */
	unsigned int pid; /* pid of the process */
	int sndsock[2];	  /* send to thread */
	int rcvsock[2];	  /* receive from thread */
	pthread_t ethread;
	unsigned char wrbuf[TLS_WR_MBUF_SZ]; /* per process write buffer */
} ksr_tcpx_proc_t;

/**
 *
 */
static ksr_tcpx_proc_t *_ksr_tcpx_proc_list = NULL;

/**
 *
 */
static int _ksr_tcpx_proc_list_size = 0;

/* mode 2: tcp_main-local write buffer and synchronous result slot.
 * Used by the direct-call path (KSR_TCPX_MAIN_PIDX) instead of the
 * per-process socketpair relay used in mode 1. */
extern int is_tcp_main(void);
static unsigned char ksr_tcp_main_wrbuf[TLS_WR_MBUF_SZ];
static tcpx_task_result_t *ksr_tcp_main_eresult = NULL;

/**
 * Set a unique id per thread
 */

_Thread_local int thread_etask_pidx = -1;
static void *ksr_tcpx_thread_etask(void *param)
{
	int pidx = 0;
	tcpx_task_t *ptask = NULL;
	int received;

	pidx = (int)(long)param;
	thread_etask_pidx = pidx;

	while(1) {
		if((received = recvfrom(_ksr_tcpx_proc_list[pidx].sndsock[0], &ptask,
					sizeof(tcpx_task_t *), 0, NULL, 0))
				< 0) {
			LM_ERR("failed to received task (%d: %s)\n", errno,
					strerror(errno));
			continue;
		}
		if(received != sizeof(tcpx_task_t *)) {
			LM_ERR("invalid task size %d\n", received);
			continue;
		}
		if(ptask->exec != NULL) {
			LM_DBG("task executed [%p] (%p/%p)\n", (void *)ptask,
					(void *)ptask->exec, (void *)ptask->param);
			ptask->exec(ptask->param, pidx);
		} else {
			LM_DBG("task with no callback function - ignoring\n");
		}
	}

	return NULL;
}

/**
 *
 */
int ksr_tcpx_thread_eresult(tcpx_task_result_t *rtask, int pidx)
{
	int len;

	if(pidx == KSR_TCPX_MAIN_PIDX) {
		/* mode 2 direct-call path: store result for synchronous retrieval */
		ksr_tcp_main_eresult = rtask;
		return 0;
	}

	len = write(_ksr_tcpx_proc_list[pidx].rcvsock[1], &rtask,
			sizeof(tcpx_task_result_t *));
	if(len <= 0) {
		LM_ERR("failed to send the task result [%p] to pidx [%d]\n", rtask,
				pidx);
		return -1;
	}
	LM_DBG("task result [%p] sent to pidx [%d]\n", rtask, pidx);
	return 0;
}

/**
 *
 */
int ksr_tcpx_task_send(tcpx_task_t *task, int pidx)
{
	int len;

	if(ksr_tcp_main_threads == 2) {
		if(!is_tcp_main()) {
			/* worker calling trampoline in mode 2 — socketpair not allocated */
			LM_ERR("tcpx task send from non-main process unsupported in"
				   " mode 2\n");
			return -1;
		}
		/* already in PROC_TCP_MAIN: call exec() directly, no relay needed */
		if(task->exec)
			task->exec(task->param, KSR_TCPX_MAIN_PIDX);
		return 0;
	}

	len = write(
			_ksr_tcpx_proc_list[pidx].sndsock[1], &task, sizeof(tcpx_task_t *));
	if(len <= 0) {
		LM_ERR("failed to send the task [%p] to pidx [%d]\n", task, pidx);
		return -1;
	}
	LM_DBG("task [%p] sent to pidx [%d]\n", task, pidx);
	return 0;
}

/**
 *
 */
int ksr_tcpx_task_result_recv(tcpx_task_result_t **rtask, int pidx)
{
	int received;

	if(ksr_tcp_main_threads == 2) {
		/* mode 2: exec() ran synchronously; result already in the slot */
		*rtask = ksr_tcp_main_eresult;
		ksr_tcp_main_eresult = NULL;
		return 0;
	}

	if((received = recvfrom(_ksr_tcpx_proc_list[pidx].rcvsock[0], rtask,
				sizeof(tcpx_task_result_t *), 0, NULL, 0))
			< 0) {
		LM_ERR("failed to received task result (%d: %s)\n", errno,
				strerror(errno));
		return -1;
	}
	if(received != sizeof(tcpx_task_result_t *)) {
		LM_ERR("invalid task result size %d\n", received);
		return -1;
	}

	return 0;
}

/**
 *
 */
int ksr_tcpx_proc_list_init(void)
{
	int i;

	if(_ksr_tcpx_proc_list != NULL) {
		return 0;
	}

	if(ksr_tcp_main_threads == 2) {
		/* mode 2: no per-process trampoline threads or socketpairs needed */
		return 0;
	}

	_ksr_tcpx_proc_list_size = get_max_procs();

	if(_ksr_tcpx_proc_list_size <= 0) {
		LM_ERR("no instance processes\n");
		return -1;
	}
	if(_ksr_tcpx_proc_list != NULL)
		return -1;
	_ksr_tcpx_proc_list = (ksr_tcpx_proc_t *)shm_malloc(
			_ksr_tcpx_proc_list_size * sizeof(ksr_tcpx_proc_t));
	if(_ksr_tcpx_proc_list == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(_ksr_tcpx_proc_list, 0,
			_ksr_tcpx_proc_list_size * sizeof(ksr_tcpx_proc_t));

	for(i = 0; i < _ksr_tcpx_proc_list_size; i++) {
		if(socketpair(PF_UNIX, SOCK_DGRAM, 0, _ksr_tcpx_proc_list[i].sndsock)
				< 0) {
			LM_ERR("opening tasks dgram snd socket pair\n");
			goto error;
		}
		if(socketpair(PF_UNIX, SOCK_DGRAM, 0, _ksr_tcpx_proc_list[i].rcvsock)
				< 0) {
			LM_ERR("opening tasks dgram rcv socket pair\n");
			goto error;
		}
	}

	return 0;

error:
	shm_free(_ksr_tcpx_proc_list);
	_ksr_tcpx_proc_list = NULL;
	_ksr_tcpx_proc_list_size = 0;
	return -1;
}

/**
 *
 */
int ksr_tcpx_proc_list_prepare(void)
{
	int i;

	if(ksr_tcp_main_threads == 2) {
		/* mode 2: direct-call path (step 4) replaces per-process threads */
		return 0;
	}

	for(i = 0; i < _ksr_tcpx_proc_list_size; i++) {
		if(pthread_create(&_ksr_tcpx_proc_list[i].ethread, NULL,
				   ksr_tcpx_thread_etask, (void *)(long)i)) {
			LM_ERR("failed to start all worker threads\n");
			goto error;
		}
	}

	return 0;

error:
	shm_free(_ksr_tcpx_proc_list);
	_ksr_tcpx_proc_list = 0;
	_ksr_tcpx_proc_list_size = 0;
	return -1;
}

/**
 *
 */
unsigned char *ksr_tcpx_thread_wrbuf(int pidx)
{
	if(pidx == KSR_TCPX_MAIN_PIDX)
		return ksr_tcp_main_wrbuf;
	if(_ksr_tcpx_proc_list == NULL)
		return NULL;
	if(pidx >= _ksr_tcpx_proc_list_size)
		return NULL;

	return _ksr_tcpx_proc_list[pidx].wrbuf;
}

/**
 *
 */
int ksr_tcpx_proc_list_rank(int rank)
{
	if(_ksr_tcpx_proc_list == NULL)
		return -1;
	if(process_no >= _ksr_tcpx_proc_list_size)
		return -1;
	_ksr_tcpx_proc_list[process_no].pidx = process_no;
	_ksr_tcpx_proc_list[process_no].pid = (unsigned int)my_pid();
	_ksr_tcpx_proc_list[process_no].rank = rank;

	return 0;
}

/**
 *
 */
int ksr_tcpx_proc_list_destroy(void)
{
	if(_ksr_tcpx_proc_list == NULL)
		return -1;

	shm_free(_ksr_tcpx_proc_list);
	_ksr_tcpx_proc_list = NULL;
	_ksr_tcpx_proc_list_size = 0;
	return 0;
}
