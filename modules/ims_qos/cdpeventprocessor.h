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

#ifndef CDPEVENTPROCESSOR
#define	CDPEVENTPROCESSOR

#include "../../locking.h"
#include "sem.h"
#include "rx_authdata.h"

typedef struct _cdp_cb_event{
	int event;							/* event id */
	time_t registered;					/* time event was added to list - useful if we want to report on things that have taken too long to process */
	rx_authsessiondata_t *session_data;	/* associated auth session data - can be null */
	str rx_session_id;
	struct _cdp_cb_event *next;
} cdp_cb_event_t;

typedef struct {
	gen_lock_t *lock;
	cdp_cb_event_t *head;
	cdp_cb_event_t *tail;
	gen_sem_t *empty;
	int size;
} cdp_cb_event_list_t;

int init_cdp_cb_event_list();
void destroy_cdp_cb_event_list();

cdp_cb_event_t* new_cdp_cb_event (int event, str *rx_session_id, rx_authsessiondata_t *session_data); 			/*create new event*/
void push_cdp_cb_event(cdp_cb_event_t* event);	/*add event to stack*/
cdp_cb_event_t* pop_cdp_cb_event();				/*pop next (head) event off list*/
void free_cdp_cb_event(cdp_cb_event_t*);		/*free memory allocated for event*/

void cdp_cb_event_process();

#endif
