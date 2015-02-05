/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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
 * 
 */

#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "utils.h"
#include "globals.h"
#include "config.h"

#include "worker.h"
#include "diameter_api.h"

#include "../../cfg/cfg_struct.h"
#include "cdp_stats.h"

/* defined in ../diameter_peer.c */
int dp_add_pid(pid_t pid);
void dp_del_pid(pid_t pid);

extern dp_config *config; /**< Configuration for this diameter peer 	*/
extern struct cdp_counters_h cdp_cnts_h;

task_queue_t *tasks; /**< queue of tasks */

cdp_cb_list_t *callbacks; /**< list of callbacks for message processing */

extern unsigned int workerq_latency_threshold; /**<max delay for putting task into worker queue */
extern unsigned int workerq_length_threshold_percentage;	/**< default threshold for worker queue length, percentage of max queue length */
/**
 * Initializes the worker structures, like the task queue.
 */
void worker_init() {
    tasks = shm_malloc(sizeof (task_queue_t));

    tasks->lock = lock_alloc();
    tasks->lock = lock_init(tasks->lock);

    sem_new(tasks->empty, 0);

    sem_new(tasks->full, 1);

    tasks->start = 0;
    tasks->end = 0;
    tasks->max = config->queue_length;
    tasks->queue = shm_malloc(tasks->max * sizeof (task_t));
    if (!tasks->queue) {
        LOG_NO_MEM("shm", tasks->max * sizeof (task_t));
        goto out_of_memory;
    }
    memset(tasks->queue, 0, tasks->max * sizeof (task_t));

    callbacks = shm_malloc(sizeof (cdp_cb_list_t));
    if (!callbacks) goto out_of_memory;
    callbacks->head = 0;
    callbacks->tail = 0;
    return;
out_of_memory:
    if (tasks) {
        if (tasks->lock) {
            lock_destroy(tasks->lock);
            lock_dealloc(&(tasks->lock));
        }
        sem_free(tasks->full);
        sem_free(tasks->empty);
        if (tasks->queue) shm_free(tasks->queue);
        shm_free(tasks);
    }
    if (callbacks) shm_free(callbacks);
}

/**
 * Destroys the worker structures.
 */
void worker_destroy() {
    int i, sval = 0;
    if (callbacks) {
        while (callbacks->head)
            cb_remove(callbacks->head);
        shm_free(callbacks);
    }

    // to deny runing the poison queue again
    config->workers = 0;
    if (tasks) {
        lock_get(tasks->lock);
        for (i = 0; i < tasks->max; i++) {
            if (tasks->queue[i].msg) AAAFreeMessage(&(tasks->queue[i].msg));
            tasks->queue[i].msg = 0;
            tasks->queue[i].p = 0;
        }
        lock_release(tasks->lock);

        LM_INFO("Unlocking workers waiting on empty queue...\n");
        for (i = 0; i < config->workers; i++)
            sem_release(tasks->empty);
        LM_INFO("Unlocking workers waiting on full queue...\n");
        i = 0;
        while (sem_getvalue(tasks->full, &sval) == 0)
            if (sval <= 0) {
                sem_release(tasks->full);
                i = 1;
            } else break;
        sleep(i);

        lock_get(tasks->lock);
        shm_free(tasks->queue);
        lock_destroy(tasks->lock);
        lock_dealloc((void*) tasks->lock);

        //lock_release(tasks->empty);
        sem_free(tasks->full);
        sem_free(tasks->empty);

        shm_free(tasks);
    }
}

/*unsafe*/
int cb_add(cdp_cb_f cb, void *ptr) {
    cdp_cb_t *x;
    x = shm_malloc(sizeof (cdp_cb_t));
    if (!x) {
        LOG_NO_MEM("shm", sizeof (cdp_cb_t));
        return 0;
    }
    x->cb = cb;
    x->ptr = shm_malloc(sizeof (void*));
    if (!x->ptr) {
        LOG_NO_MEM("shm", sizeof (void*));
        return 0;
    }
    *(x->ptr) = ptr;
    x->next = 0;
    x->prev = callbacks->tail;
    if (callbacks->tail) callbacks->tail->next = x;
    callbacks->tail = x;
    if (!callbacks->head) callbacks->head = x;
    return 1;
}

/*unsafe*/
void cb_remove(cdp_cb_t *cb) {
    cdp_cb_t *x;
    x = callbacks->head;
    while (x && x != cb) x = x->next;
    if (!x) return;
    if (x->prev) x->prev->next = x->next;
    else callbacks->head = x->next;
    if (x->next) x->next->prev = x->prev;
    else callbacks->tail = x->prev;

    if (x->ptr) shm_free(x->ptr);
    shm_free(x);
}

/**
 * Adds a message as a task to the task queue.
 * This blocks if the task queue is full, until there is space.
 * @param p - the peer that the message was received from
 * @param msg - the message
 * @returns 1 on success, 0 on failure (eg. shutdown in progress)
 */
int put_task(peer *p, AAAMessage *msg) {

    struct timeval start, stop;
    int num_tasks, length_percentage;
    
    long elapsed_useconds=0, elapsed_seconds=0, elapsed_millis=0;
    lock_get(tasks->lock);

    gettimeofday(&start, NULL);
    while ((tasks->end + 1) % tasks->max == tasks->start) {
        lock_release(tasks->lock);

        if (*shutdownx) {
            sem_release(tasks->full);
            return 0;
        }

        sem_get(tasks->full);

        if (*shutdownx) {
            sem_release(tasks->full);
            return 0;
        }

        lock_get(tasks->lock);
    }
    
    counter_inc(cdp_cnts_h.queuelength);

    gettimeofday(&stop, NULL);
    elapsed_useconds = stop.tv_usec - start.tv_usec;
    elapsed_seconds = stop.tv_sec - start.tv_sec;

    elapsed_useconds = elapsed_seconds*1000000 + elapsed_useconds;
    elapsed_millis = elapsed_useconds/1000;
    if (elapsed_millis > workerq_latency_threshold) {
        LM_ERR("took too long to put task into task queue > %d - [%ld]\n", workerq_latency_threshold, elapsed_millis);
    }

    tasks->queue[tasks->end].p = p;
    tasks->queue[tasks->end].msg = msg;
    tasks->end = (tasks->end + 1) % tasks->max;
    if (sem_release(tasks->empty) < 0)
        LM_WARN("Error releasing tasks->empty semaphore > %s!\n", strerror(errno));
    lock_release(tasks->lock);

    if(workerq_length_threshold_percentage > 0) {
        num_tasks = tasks->end - tasks->start;
	length_percentage = num_tasks/tasks->max*100;
	if(length_percentage > workerq_length_threshold_percentage) {
	    LM_WARN("Queue length has exceeded length threshold percentage [%i] and is length [%i]", length_percentage, num_tasks);
	}
    }
    //int num_tasks = tasks->end - tasks->start;
    //LM_ERR("Added task to task queue.  Queue length [%i]", num_tasks);


    return 1;
}

/**
 * Remove and return the first task from the queue (FIFO).
 * This blocks until there is something in the queue.
 * @returns the first task from the queue or an empty task on error (eg. shutdown in progress)
 */
task_t take_task() {
    task_t t = {0, 0};
    lock_get(tasks->lock);
    while (tasks->start == tasks->end) {
        lock_release(tasks->lock);
        if (*shutdownx) {
            sem_release(tasks->empty);
            return t;
        }
        sem_get(tasks->empty);
        if (*shutdownx) {
            sem_release(tasks->empty);
            return t;
        }

        lock_get(tasks->lock);
    }

    counter_add(cdp_cnts_h.queuelength, -1);
    t = tasks->queue[tasks->start];
    tasks->queue[tasks->start].msg = 0;
    tasks->start = (tasks->start + 1) % tasks->max;
    if (sem_release(tasks->full) < 0)
        LM_WARN("Error releasing tasks->full semaphore > %s!\n", strerror(errno));
    lock_release(tasks->lock);

    //int num_tasks = tasks->end - tasks->start;
    //LM_ERR("Taken task from task queue.  Queue length [%i]", num_tasks);


    return t;
}

/**
 * Poisons the worker queue.
 * Actually it just releases the task queue locks so that the workers get to evaluate
 * if a shutdown is in process and exit.
 */
void worker_poison_queue() {
    int i;
    if (config->workers && tasks)
        for (i = 0; i < config->workers; i++)
            if (sem_release(tasks->empty) < 0)
                LM_WARN("Error releasing tasks->empty semaphore > %s!\n", strerror(errno));
}

/**
 * This is the main worker process.
 * Takes tasks from the queue in a loop and processes them by calling the registered callbacks.
 * @param id - id of the worker
 * @returns never, exits on shutdown.
 */
void worker_process(int id) {
    task_t t;
    cdp_cb_t *cb;
    int r;
    LM_INFO("[%d] Worker process started...\n", id);
    /* init the application level for this child */
    while (1) {
        if (shutdownx && (*shutdownx)) break;
	cfg_update();
        t = take_task();
        if (!t.msg) {
            if (shutdownx && (*shutdownx)) break;
            LM_INFO("[%d] got empty task Q(%d/%d)\n", id, tasks->start, tasks->end);
            continue;
        }
        LM_DBG("worker_process(): [%d] got task Q(%d/%d)\n", id, tasks->start, tasks->end);
        r = is_req(t.msg);
        for (cb = callbacks->head; cb; cb = cb->next)
            (*(cb->cb))(t.p, t.msg, *(cb->ptr));

        if (r) {
            AAAFreeMessage(&(t.msg));
        } else {
            /* will be freed by the user in upper api */
            /*AAAFreeMessage(&(t.msg));*/
        }
    }
    worker_poison_queue();
    LM_INFO("[%d]... Worker process finished\n", id);
#ifdef CDP_FOR_SER
#else
#ifdef PKG_MALLOC
    LM_DBG("Worker[%d] Memory status (pkg):\n", id);
    //pkg_status();
#ifdef pkg_sums
    pkg_sums();
#endif
#endif
    dp_del_pid(getpid());
#endif
    exit(0);
}

