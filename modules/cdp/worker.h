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

#ifndef __WORKER_H
#define __WORKER_H

#include "peer.h"
#include "diameter.h"
#include "utils.h"

/** function to be called on worker initialization */
typedef int (*worker_init_function)(int rank);

/** task element */ 
typedef struct _task_t {
	peer *p;			/**< peer that the message was received from */
	AAAMessage *msg;	/**< diameter message received */
} task_t;

/** task queue */
typedef struct {
	gen_lock_t *lock;	/**< lock for task queue operations */ 
	int start;			/**< start position in the queue array (index of oldest task) */
	int end;			/**< end position in the queue array (index of the youngest task) */
	int max;			/**< size of the queue array */
	task_t *queue;		/**< array holding the tasks */
	gen_sem_t *empty;	/**< id of semaphore for signaling an empty queue */
	gen_sem_t *full;	/**< id of semaphore for signaling an full queue */
} task_queue_t;

/** callback function to be called on message processing */
typedef int (*cdp_cb_f)(peer *p,AAAMessage *msg,void* ptr);

/** callback element for message processing */
typedef struct _cdp_cb_t{
	cdp_cb_f cb;				/**< callback function to be called on event */
	void **ptr;					/**< generic pointer to be passed to the callback */
	struct _cdp_cb_t *next; 	/**< next callback in the list */
	struct _cdp_cb_t *prev;		/**< previous callback in the list */
} cdp_cb_t;
	
/** list of callback elements for message processing */
typedef struct {
	cdp_cb_t *head;	/**< first element in the list */
	cdp_cb_t *tail; /**< last element in the list */
} cdp_cb_list_t;

void worker_init();
void worker_destroy();

int cb_add(cdp_cb_f cb,void *ptr);
void cb_remove(cdp_cb_t *cb);

int put_task(peer *p,AAAMessage *msg);
task_t take_task();


void worker_poison_queue();

void worker_process(int id);



#endif

