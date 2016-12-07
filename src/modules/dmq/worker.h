/**
 * dmq module - distributed message queue
 *
 * Copyright (C) 2011 Bucur Marius - Ovidiu
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _DMQ_WORKER_H_
#define _DMQ_WORKER_H_

#include "peer.h"
#include "../../locking.h"
#include "../../atomic_ops.h"
#include "../../parser/msg_parser.h"


typedef struct dmq_job {
	peer_callback_t f;
	struct sip_msg* msg;
	dmq_peer_t* orig_peer;
	struct dmq_job* next;
	struct dmq_job* prev;
} dmq_job_t;

typedef struct job_queue {
	atomic_t count;
	struct dmq_job* back;
	struct dmq_job* front;
	gen_lock_t lock;
} job_queue_t;

struct dmq_worker {
	job_queue_t* queue;
	int jobs_processed;
	gen_lock_t lock;
	int pid;
};

typedef struct dmq_worker dmq_worker_t;

void init_worker(dmq_worker_t* worker);
int add_dmq_job(struct sip_msg*, dmq_peer_t*);
void worker_loop(int id);

job_queue_t* alloc_job_queue();
void destroy_job_queue(job_queue_t* queue);
int job_queue_push(job_queue_t* queue, dmq_job_t* job);
dmq_job_t* job_queue_pop(job_queue_t* queue);
int job_queue_size(job_queue_t* queue);

#endif

