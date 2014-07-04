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

#ifndef __TIMERCDP_H
#define __TIMERCDP_H

#include "worker.h"

/** callback function for timer event */
typedef int (*callback_f)(time_t now,void *ptr);

/** timer element */
typedef struct _timer_cb_t{
	time_t expires;		/**< time of expiration */
	int one_time;		/**< if to trigger the event just one_time and then remove */
	callback_f cb;		/**< callback function to be called on timer expiration */
	void **ptr;			/**< generic parameter to call the callback with		*/
	
	struct _timer_cb_t *next;/**< next timer in the timer list */
	struct _timer_cb_t *prev;/**< previous timer in the timer list */	
} timer_cb_t;

/** timer list */
typedef struct {
	timer_cb_t *head;	/**< first element in the timer list */
	timer_cb_t *tail;	/**< last element in the timer list */
} timer_cb_list_t;

int add_timer(int expires_in,int one_time,callback_f cb,void *ptr);

void timer_cdp_init();

void timer_cdp_destroy();


void timer_process(int returns);



#endif

