#include "dmq.h"
#include "peer.h"
#include "worker.h"

void worker_loop(int id) {
	dmq_worker_t* worker = &workers[id];
	dmq_job_t* current_job;
	peer_reponse_t peer_response;
	int ret_value;
	for(;;) {
		LM_DBG("dmq_worker [%d %d] getting lock\n", id, my_pid());
		lock_get(&worker->lock);
		LM_DBG("dmq_worker [%d %d] lock acquired\n", id, my_pid());
		/* multiple lock_release calls might be performed, so remove from queue until empty */
		do {
			current_job = job_queue_pop(worker->queue);
			/* job_queue_pop might return NULL if queue is empty */
			if(current_job) {
				ret_value = current_job->f(current_job->msg, &peer_response);
				if(ret_value < 0) {
					LM_ERR("running job failed\n");
				}
				shm_free(current_job);
				worker->jobs_processed++;
			}
		} while(job_queue_size(worker->queue) > 0);
	}
}

int add_dmq_job(struct sip_msg* msg, dmq_peer_t* peer) {
	int i, found_available = 0;
	dmq_job_t new_job;
	dmq_worker_t* worker;
	new_job.f = peer->callback;
	new_job.msg = msg;
	new_job.orig_peer = peer;
	if(!num_workers) {
		LM_ERR("error in add_dmq_job no workers spawned\n");
		return -1;
	}
	/* initialize the worker with the first one */
	worker = workers;
	/* search for an available worker, or, if not possible, for the least busy one */
	for(i = 0; i < num_workers; i++) {
		if(job_queue_size(workers[i].queue) == 0) {
			worker = &workers[i];
			found_available = 1;
			break;
		} else if(job_queue_size(workers[i].queue) < job_queue_size(worker->queue)) {
			worker = &workers[i];
		}
	}
	if(!found_available) {
		LM_DBG("no available worker found, passing job to the least busy one [%d %d]\n",
		       worker->pid, job_queue_size(worker->queue));
	}
	job_queue_push(worker->queue, &new_job);
	lock_release(&worker->lock);
	return 0;
}

void init_worker(dmq_worker_t* worker) {
	memset(worker, 0, sizeof(*worker));
	lock_init(&worker->lock);
	// acquire the lock for the first time - so that dmq_worker_loop blocks
	lock_get(&worker->lock);
	worker->queue = alloc_job_queue();
}

job_queue_t* alloc_job_queue() {
	job_queue_t* queue = shm_malloc(sizeof(job_queue_t));
	atomic_set(&queue->count, 0);
	queue->front = NULL;
	queue->back = NULL;
	lock_init(&queue->lock);
	return queue;
}

void destroy_job_queue(job_queue_t* queue) {
	shm_free(queue);
}

int job_queue_size(job_queue_t* queue) {
	return atomic_get(&queue->count);
}

void job_queue_push(job_queue_t* queue, dmq_job_t* job) {
	/* we need to copy the dmq_job into a newly created dmq_job in shm */
	dmq_job_t* newjob = shm_malloc(sizeof(dmq_job_t));
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
}
dmq_job_t* job_queue_pop(job_queue_t* queue) {
	dmq_job_t* front;
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