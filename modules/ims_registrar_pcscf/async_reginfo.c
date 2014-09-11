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
#include "async_reginfo.h"

reginfo_event_list_t *reginfo_event_list = 0;

int init_reginfo_event_list()
{
	reginfo_event_list = shm_malloc(sizeof(reginfo_event_list_t));
	if (!reginfo_event_list) {
		LM_ERR("No more SHM mem\n");
		return 0;
	}
	memset(reginfo_event_list, 0, sizeof(reginfo_event_list_t));
	reginfo_event_list->lock = lock_alloc();
	if (!reginfo_event_list->lock) {
		LM_ERR("failed to create reginfo event list lock\n");
		return 0;
	}
	reginfo_event_list->lock = lock_init(reginfo_event_list->lock);

	sem_new(reginfo_event_list->empty, 0); //pre-locked - as we assume list is empty at start

	return 1;
}
void destroy_reginfo_event_list()
{
	reginfo_event_t *ev, *tmp;

	lock_get(reginfo_event_list->lock);
	ev = reginfo_event_list->head;
	while (ev) {
		tmp = ev->next;
		free_reginfo_event(ev);
		ev = tmp;
	}
	lock_destroy(reginfo_event_list->lock);
	lock_dealloc(reginfo_event_list->lock);
	shm_free(reginfo_event_list);
}

reginfo_event_t* new_reginfo_event (int event)
{
	reginfo_event_t *new_event = shm_malloc(sizeof(reginfo_event_t));
	if (!new_event) {
		LM_ERR("No more shm mem\n");
		return NULL;
	}
	new_event->registered = time(NULL);
	new_event->event = event;
	new_event->next = 0;

	return new_event;
}

void push_reginfo_event(reginfo_event_t* event)
{
	lock_get(reginfo_event_list->lock);
	if (reginfo_event_list->head == 0) { //empty list
		reginfo_event_list->head = reginfo_event_list->tail = event;
	} else {
		reginfo_event_list->tail->next = event;
		reginfo_event_list->tail = event;
	}
	sem_release(reginfo_event_list->empty);
	lock_release(reginfo_event_list->lock);
}

reginfo_event_t* pop_reginfo_event()
{
	reginfo_event_t *ev;

	lock_get(reginfo_event_list->lock);
	while (reginfo_event_list->head == 0) {
		lock_release(reginfo_event_list->lock);
		sem_get(reginfo_event_list->empty);
		lock_get(reginfo_event_list->lock);
	}

	ev = reginfo_event_list->head;
	reginfo_event_list->head = ev->next;

	if (ev == reginfo_event_list->tail) { //list now empty
		reginfo_event_list->tail = 0;
	}
	ev->next = 0; //make sure whoever gets this cant access our list
	lock_release(reginfo_event_list->lock);

	return ev;
}

void free_reginfo_event(reginfo_event_t* ev)
{
	if (ev) {
		LM_DBG("Freeing reginfo event structure\n");
		shm_free(ev);
	}
}

void reginfo_event_process()
{
	reginfo_event_t *ev;
	for (;;) {
			LM_DBG("POPPING REGINFO EVENT\n");
	        ev = pop_reginfo_event();
	        LM_DBG("PROCESSING REGINFO EVENT with event [%d]\n", ev->event);

	        switch (ev->event) {
	        case REG_EVENT_PUBLISH:
	        	LM_DBG("Sending out-of-band publish\n");
	        	break;
	        case REG_EVENT_SUBSCRIBE:
	        	LM_DBG("Sending out-of-band subscribe\n");
	        	break;
	        default:
	        	LM_ERR("Unknown REG event.....ignoring\n");
	        	break;
	        }
	        free_reginfo_event(ev);
	}
}



