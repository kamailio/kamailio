/*
 * dmq module - distributed message queue
 *
 * Copyright (C) 2011 Bucur Marius - Ovidiu
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 *
 */

#include "dmq.h"
#include "peer.h"
#include "message.h"
#include "worker.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/mod_fix.h"
#include "../../core/sip_msg_clone.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/cfg/cfg_struct.h"


/**
 * @brief dmq worker loop
 */
void worker_loop(int id)
{
	dmq_worker_t *worker;
	dmq_job_t *current_job;
	peer_reponse_t peer_response;
	int ret_value;
	int not_parsed;
	dmq_node_t *dmq_node = NULL;

	worker = &dmq_workers[id];
	for(;;) {
		if(dmq_worker_usleep <= 0) {
			LM_DBG("dmq_worker [%d %d] getting lock\n", id, my_pid());
			lock_get(&worker->lock);
			LM_DBG("dmq_worker [%d %d] lock acquired\n", id, my_pid());
		} else {
			sleep_us(dmq_worker_usleep);
		}
		cfg_update();

		/* remove from queue until empty */
		while(job_queue_size(worker->queue) > 0) {
			/* fill the response with 0's */
			memset(&peer_response, 0, sizeof(peer_response));
			current_job = job_queue_pop(worker->queue);
			/* job_queue_pop might return NULL if queue is empty */
			if(current_job) {
				/* extract the from uri */
				if(current_job->msg->from->parsed) {
					not_parsed = 0;
				} else {
					not_parsed = 1;
				}
				if(parse_from_header(current_job->msg) < 0) {
					LM_ERR("bad sip message or missing From hdr\n");
				} else {
					dmq_node = find_dmq_node_uri(dmq_node_list,
							&((struct to_body *)current_job->msg->from->parsed)
									 ->uri);
				}

				ret_value = current_job->f(
						current_job->msg, &peer_response, dmq_node);
				if(ret_value < 0) {
					LM_ERR("running job failed\n");
					goto nextjob;
				}
				LM_DBG("running job executed\n");
				/* add the body to the reply */
				if(peer_response.body.s) {
					if(set_reply_body(current_job->msg, &peer_response.body,
							   &peer_response.content_type)
							< 0) {
						LM_ERR("error adding lumps\n");
						goto nextjob;
					}
				}
				/* send the reply */
				if(peer_response.resp_code > 0 && peer_response.reason.s != NULL
						&& peer_response.reason.len > 0) {
					if(_dmq_slb.freply(current_job->msg,
							   peer_response.resp_code, &peer_response.reason)
							< 0) {
						LM_ERR("error sending reply\n");
					} else {
						LM_DBG("done sending reply\n");
					}
				} else {
					LM_WARN("no reply sent\n");
				}
				worker->jobs_processed++;
				LM_DBG("jobs_processed:%d\n", worker->jobs_processed);

			nextjob:
				/* if body given, free the lumps and free the body */
				if(peer_response.body.s) {
					del_nonshm_lump_rpl(&current_job->msg->reply_lump);
					pkg_free(peer_response.body.s);
				}
				if((current_job->msg->from->parsed) && (not_parsed)) {
					free_to(current_job->msg->from->parsed);
				}

				shm_free(current_job->msg);
				shm_free(current_job);
			}
		}
	}
}

/**
 * @brief add a dmq job
 */
int add_dmq_job(struct sip_msg *msg, dmq_peer_t *peer)
{
	int i, found_available = 0;
	dmq_job_t new_job = {0};
	dmq_worker_t *worker;
	struct sip_msg *cloned_msg = NULL;
	int cloned_msg_len;

	/* Pre-parse headers so they are included in our clone. Parsing later
	 * will result in linking pkg structures to shm msg, eventually leading
	 * to memory errors. */
	if(parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse headers\n");
		return -1;
	}

	cloned_msg = sip_msg_shm_clone(msg, &cloned_msg_len, 1);
	if(!cloned_msg) {
		LM_ERR("error cloning sip message\n");
		return -1;
	}

	new_job.f = peer->callback;
	new_job.msg = cloned_msg;
	new_job.orig_peer = peer;
	if(!dmq_num_workers) {
		LM_ERR("error in add_dmq_job: no workers spawned\n");
		goto error;
	}
	if(!dmq_workers[0].queue) {
		LM_ERR("workers not (yet) initialized\n");
		goto error;
	}
	/* initialize the worker with the first one */
	worker = dmq_workers;
	/* search for an available worker, or, if not possible,
	 * for the least busy one */
	for(i = 0; i < dmq_num_workers; i++) {
		if(job_queue_size(dmq_workers[i].queue) == 0) {
			worker = &dmq_workers[i];
			found_available = 1;
			break;
		} else if(job_queue_size(dmq_workers[i].queue)
				  < job_queue_size(worker->queue)) {
			worker = &dmq_workers[i];
		}
	}
	if(!found_available) {
		LM_DBG("no available worker found, passing job"
			   " to the least busy one [%d %d]\n",
				worker->pid, job_queue_size(worker->queue));
	}
	if(job_queue_push(worker->queue, &new_job) < 0) {
		goto error;
	}
	if(dmq_worker_usleep <= 0) {
		lock_release(&worker->lock);
		LM_DBG("dmq_worker [%d %d] lock released\n", i, worker->pid);
	}
	return 0;
error:
	if(cloned_msg != NULL) {
		shm_free(cloned_msg);
	}
	return -1;
}

/**
 * @brief init dmq worker
 */
int init_worker(dmq_worker_t *worker)
{
	memset(worker, 0, sizeof(*worker));
	if(dmq_worker_usleep <= 0) {
		lock_init(&worker->lock);
		// acquire the lock for the first time - so that dmq_worker_loop blocks
		lock_get(&worker->lock);
	}
	worker->queue = alloc_job_queue();
	if(worker->queue == NULL) {
		LM_ERR("queue could not be initialized\n");
		return -1;
	}
	return 0;
}

/**
 * @brief allog dmq job queue
 */
job_queue_t *alloc_job_queue()
{
	job_queue_t *queue;

	queue = shm_malloc(sizeof(job_queue_t));
	if(queue == NULL) {
		SHM_MEM_ERROR;
		return NULL;
	}
	memset(queue, 0, sizeof(job_queue_t));
	atomic_set(&queue->count, 0);
	lock_init(&queue->lock);
	return queue;
}

/**
 * @ brief destroy job queue
 */
void destroy_job_queue(job_queue_t *queue)
{
	if(queue != NULL)
		shm_free(queue);
}

/**
 * @brief return job queue size
 */
int job_queue_size(job_queue_t *queue)
{
	return atomic_get(&queue->count);
}

/**
 * @brief push to job queue
 */
int job_queue_push(job_queue_t *queue, dmq_job_t *job)
{
	/* we need to copy the dmq_job into a newly created dmq_job in shm */
	dmq_job_t *newjob;

	newjob = shm_malloc(sizeof(dmq_job_t));
	if(newjob == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}

	*newjob = *job;

	lock_get(&queue->lock);
	newjob->prev = NULL;
	newjob->next = queue->back;
	if(queue->back) {
		queue->back->prev = newjob;
	}
	queue->back = newjob;
	if(!queue->front) {
		queue->front = newjob;
	}
	atomic_inc(&queue->count);
	lock_release(&queue->lock);
	return 0;
}

/**
 * @brief pop from job queue
 */
dmq_job_t *job_queue_pop(job_queue_t *queue)
{
	dmq_job_t *front;
	lock_get(&queue->lock);
	if(!queue->front) {
		lock_release(&queue->lock);
		return NULL;
	}
	front = queue->front;
	if(front->prev) {
		queue->front = front->prev;
		front->prev->next = NULL;
	} else {
		queue->front = NULL;
		queue->back = NULL;
	}
	atomic_dec(&queue->count);
	lock_release(&queue->lock);
	return front;
}
