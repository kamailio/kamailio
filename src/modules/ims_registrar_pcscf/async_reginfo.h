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

#ifndef ASYNC_REGINFO_H_
#define ASYNC_REGINFO_H_

#include "../../locking.h"
#include "sem.h"

#define REG_EVENT_UNKNOWN -1
#define REG_EVENT_SUBSCRIBE 0
#define REG_EVENT_PUBLISH 1

typedef struct _reginfo_event{
	int event;							/* event id */
	time_t registered;					/* time event was added to list - useful if we want to report on things that have taken too long to process */
	str publ_body;
	str publ_id;
	str publ_content_type;
	str subs_remote_target;
	str subs_watcher_uri;
	str subs_contact;
	str subs_outbound_proxy;
	int expires;
	int flag;
	int sourge_flag;
	int reg_info_event;
	str extra_headers;
	str pres_uri;
	struct _reginfo_event *next;
} reginfo_event_t;

typedef struct {
	int size;
	gen_lock_t *lock;
	reginfo_event_t *head;
	reginfo_event_t *tail;
	gen_sem_t *empty;
} reginfo_event_list_t;


int init_reginfo_event_list();
void destroy_reginfo_event_list();

reginfo_event_t* new_reginfo_event (int event, str *publ_body, str *publ_id, str *publ_content_type, str *subs_remote_target, str *subs_watcher_uri,
	str *subs_contact, str *subs_outbound_proxy, int expires, int flag, int source_flag, int reg_info_event, str *extra_headers, str *pres_uri);
void push_reginfo_event(reginfo_event_t* event);	/*add event to stack*/
reginfo_event_t* pop_reginfo_event();				/*pop next (head) event off list*/
void free_reginfo_event(reginfo_event_t*);			/*free memory allocated for event*/

void reginfo_event_process();


#endif /* ASYNC_REGINFO_H_ */
