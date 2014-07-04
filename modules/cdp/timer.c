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
#include <unistd.h>

#include "utils.h"
#include "globals.h"
#include "worker.h"

#include "timer.h"

#include "../../cfg/cfg_struct.h"


/* defined in ../diameter_peer.c */
int dp_add_pid(pid_t pid);
void dp_del_pid(pid_t pid);


timer_cb_list_t *timers=0;	/**< list of timers */
gen_lock_t *timers_lock=0;	/**< lock for the list of timers */

/** how many seconds to sleep on each timer iteration */
#define TIMER_RESOLUTION 1

/**
 * Loop that checks every #TIMER_RESOLUTION seconds if some timer expired.
 * On expires, the callback is called. The callback should return rapidly
 * in order to avoid blocking the timer process. If the timer is "one_time",
 * then it is removed from the timers list.
 * @returns on shutdown
 */
void timer_loop()
{
	time_t now;
	timer_cb_t *i;
	callback_f cb=0;
	void *ptr=0;
	int interval=0;
	
	while(1){
		if (shutdownx && *shutdownx) break;
		now = time(0);
		cfg_update();
	
		do {
			cb = 0;
			lock_get(timers_lock);
				i = timers->head;
				while(i && i->expires>now) i = i->next;
				if (i){
					cb = i->cb;
					ptr = *(i->ptr);
					if (i->one_time){
						if (i->prev) i->prev->next = i->next;
						else timers->head = i->next;
						if (i->next) i->next->prev = i->prev;
						else timers->tail = i->next;
						shm_free(i);
						i=0;
					}
				}
			lock_release(timers_lock);
	
			if (cb) { 
				interval = cb(now,ptr);
				if (i){
					lock_get(timers_lock);
						i->expires = now + interval;
					lock_release(timers_lock);
				}
			}
	
		} while(cb);
				
		sleep(TIMER_RESOLUTION);
	}
}

/**
 * Adds a timer to the timer list.
 * @param expires_in - time until expiration in seconds
 * @param one_time - if after expiration it should be removed or kept in the timers list
 * @param cb - callback function to be called on expiration
 * @param ptr - generic pointer to pass to the callback on expiration
 * @returns 1 on success or 0 on failure
 */
int add_timer(int expires_in,int one_time,callback_f cb,void *ptr)
{
	timer_cb_t *n;
	if (expires_in==0){
		LM_ERR("add_timer(): Minimum expiration time is 1 second!\n");
		return 0;
	}
	n = shm_malloc(sizeof(timer_cb_t));
	if (!n){
		LOG_NO_MEM("shm",sizeof(timer_cb_t));
		return 0;
	}
	n->ptr = shm_malloc(sizeof(void*));
	if (!n){
		LOG_NO_MEM("shm",sizeof(void*));
		shm_free(n);
		return 0;
	}
	n->expires = expires_in + time(0);
	n->one_time = one_time;
	//n->interval = expires_in;
	n->cb = cb;
	*(n->ptr) = ptr;

	lock_get(timers_lock);
		n->prev = timers->tail;
		n->next = 0;
		if (!timers->head) timers->head = n;
		if (timers->tail) timers->tail->next = n;
		timers->tail = n;
	lock_release(timers_lock);
	return 1;
}

/**
 * Init the timer structures
 */
void timer_cdp_init()
{
	timers = shm_malloc(sizeof(timer_cb_list_t));
	timers->head=0;
	timers->tail=0;
	timers_lock = lock_alloc();
	timers_lock = lock_init(timers_lock);	
}

/**
 * Destroy the timer structures
 */
void timer_cdp_destroy()
{
	timer_cb_t *n,*i;
/*	lock_get(timers_lock);*/
	i = timers->head;
	while(i){
		n = i->next;
		if (i->ptr) shm_free(i->ptr);
		shm_free(i);
		i = n;
	}
	shm_free(timers);
	lock_destroy(timers_lock);
	lock_dealloc((void*)timers_lock);
}

/**
 * Timer Process function.
 * It calls timer_loop().
 * @param returns - whether on shutdown this function should return or exit
 * @returns if returns is set then on shutdown, else never and on shutdown it exits
 */
void timer_process(int returns)
{
	LM_INFO("Timer process starting up...\n");
		
	timer_loop();
	
	LM_INFO("... Timer process finished\n");
	if (!returns) {
#ifdef CDP_FOR_SER
#else
#ifdef PKG_MALLOC
	#ifdef PKG_MALLOC
		LM_DBG("Timer Memory status (pkg):\n");
		//pkg_status();
		#ifdef pkg_sums
			pkg_sums();
		#endif 
	#endif
#endif
		dp_del_pid(getpid());		
#endif		
		
		exit(0);
	}
}


