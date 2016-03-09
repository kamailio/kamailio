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

#include "transaction.h"

#include "timer.h"
#include "globals.h"
#include "cdp_stats.h"

extern struct cdp_counters_h cdp_cnts_h;
cdp_trans_list_t *trans_list=0;		/**< list of transactions */

/**
 * Initializes the transaction structure.
 * Also adds a timer callback for checking the transaction statuses
 * @returns 1 if success or 0 on error
 */
int cdp_trans_init()
{
	trans_list = shm_malloc(sizeof(cdp_trans_list_t));
	if (!trans_list){
		LOG_NO_MEM("shm",sizeof(cdp_trans_list_t));
		return 0;
	}
	trans_list->head = 0;
	trans_list->tail = 0;
	trans_list->lock = lock_alloc();
	trans_list->lock = lock_init(trans_list->lock);

	add_timer(1,0,cdp_trans_timer,0);
	return 1;
}

int cdp_trans_destroy()
{
	cdp_trans_t *t=0;
	if (trans_list){
		lock_get(trans_list->lock);
		while(trans_list->head){
			t = trans_list->head;
			trans_list->head = t->next;
			cdp_free_trans(t);
		}
		lock_destroy(trans_list->lock);
		lock_dealloc((void*)trans_list->lock);
		shm_free(trans_list);
		trans_list = 0;
	}

	return 1;
}
/**
 * Create and add a transaction to the transaction list.
 * @param msg - the message that this related to
 * @param cb - callback to be called on response or time-out
 * @param ptr - generic pointer to pass to the callback on call
 * @param timeout - timeout time in seconds
 * @param auto_drop - whether to auto drop the transaction on event, or let the application do it later
 * @returns the created cdp_trans_t* or NULL on error
 */
inline cdp_trans_t* cdp_add_trans(AAAMessage *msg,AAATransactionCallback_f *cb, void *ptr,int timeout,int auto_drop)
{
	cdp_trans_t *x;
	x = shm_malloc(sizeof(cdp_trans_t));
	if (!x) {
		LOG_NO_MEM("shm",sizeof(cdp_trans_t));
		return 0;
	}
	x->ptr = shm_malloc(sizeof(void*));
	if (!x->ptr) {
		LOG_NO_MEM("shm",sizeof(void*));
		shm_free(x);
		return 0;
	}

	gettimeofday(&x->started, NULL);
	x->endtoendid = msg->endtoendId;
	x->hopbyhopid = msg->hopbyhopId;
	x->cb = cb;
	*(x->ptr) = ptr;
	x->expires = timeout + time(0);
	x->auto_drop = auto_drop;
	x->next = 0;

	lock_get(trans_list->lock);
	x->prev = trans_list->tail;
	if (trans_list->tail) trans_list->tail->next = x;
	trans_list->tail = x;
	if (!trans_list->head) trans_list->head = x;
	lock_release(trans_list->lock);
	return x;
}

/**
 * Remove from the list and deallocate a transaction.
 * @param msg - the message that relates to that particular transaction
 */
inline void del_trans(AAAMessage *msg)
{
	cdp_trans_t *x;
	lock_get(trans_list->lock);
	x = trans_list->head;
	while(x&& x->endtoendid!=msg->endtoendId && x->hopbyhopid!=msg->hopbyhopId) x = x->next;
	if (x){
		if (x->prev) x->prev->next = x->next;
		else trans_list->head = x->next;
		if (x->next) x->next->prev = x->prev;
		else trans_list->tail = x->prev;
		cdp_free_trans(x);
	}
	lock_release(trans_list->lock);
}

/**
 * Return and remove the transaction from the transaction list.
 * @param msg - the message that this transaction relates to
 * @returns the cdp_trans_t* if found or NULL if not
 */
inline cdp_trans_t* cdp_take_trans(AAAMessage *msg)
{
	cdp_trans_t *x;
	lock_get(trans_list->lock);
	x = trans_list->head;
	while(x&& x->endtoendid!=msg->endtoendId && x->hopbyhopid!=msg->hopbyhopId) x = x->next;
	if (x){
		if (x->prev) x->prev->next = x->next;
		else trans_list->head = x->next;
		if (x->next) x->next->prev = x->prev;
		else trans_list->tail = x->prev;
	}
	lock_release(trans_list->lock);
	return x;
}

/**
 * Deallocate the memory taken by a transaction.
 * @param x - the transaction to deallocate
 */
inline void cdp_free_trans(cdp_trans_t *x)
{
	if (x->ptr) shm_free(x->ptr);
	shm_free(x);
}

/**
 * Timer callback for checking the transaction status.
 * @param now - time of call
 * @param ptr - generic pointer, passed to the transactional callbacks
 */
int cdp_trans_timer(time_t now, void* ptr)
{
	cdp_trans_t *x,*n;
	LM_DBG("trans_timer(): taking care of diameter transactions...\n");
	lock_get(trans_list->lock);
	x = trans_list->head;
	while(x)
	{
		if (now>x->expires){
            counter_inc(cdp_cnts_h.timeout);		//Transaction has timed out waiting for response
	    
			x->ans = 0;
			if (x->cb){
				(x->cb)(1,*(x->ptr),0, (now - x->expires));
			}
			n = x->next;

			if (x->prev) x->prev->next = x->next;
			else trans_list->head = x->next;
			if (x->next) x->next->prev = x->prev;
			else trans_list->tail = x->prev;
			if (x->auto_drop) cdp_free_trans(x);

			x = n;
		} else
			x = x->next;
	}
	lock_release(trans_list->lock);
	return 1;
}



/* TRANSACTIONS */

/**
* Create a AAATransaction for the given request.
* @param app_id - id of the request's application
* @param cmd_code - request's code
* @returns the AAATransaction*
*/
AAATransaction *AAACreateTransaction(AAAApplicationId app_id,AAACommandCode cmd_code)
{
	AAATransaction *t;
	t = shm_malloc(sizeof(AAATransaction));
	if (!t) return 0;
	memset(t,0,sizeof(AAATransaction));
	t->application_id=app_id;
	t->command_code=cmd_code;
	return t;
}

/**
* Free the memory allocated for the AAATransaction.
* @param trans - the AAATransaction to be deallocated
* @returns 1 on success, 0 on failure
*/
int AAADropTransaction(AAATransaction *trans)
{
	if (!trans) return 0;
	shm_free(trans);
	return 1;
}
