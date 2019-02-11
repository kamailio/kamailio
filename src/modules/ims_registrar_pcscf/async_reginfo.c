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
#include "../pua/pua.h"
#include "../pua/send_publish.h"

#include "../pua/pua_bind.h"

extern pua_api_t pua;
extern int reginfo_queue_size_threshold;
reginfo_event_list_t *reginfo_event_list = 0;

int init_reginfo_event_list()
{
	if (reginfo_event_list)
		return 1;

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
	reginfo_event_list->size = 0;

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

reginfo_event_t* new_reginfo_event (int event, str *publ_body, str *publ_id, str *publ_content_type, str *subs_remote_target, str *subs_watcher_uri,
	str *subs_contact, str *subs_outbound_proxy, int expires, int flag, int source_flag, int reg_info_event, str *extra_headers, str *pres_uri)
{
	char *p;
	int len;
	reginfo_event_t *new_event;
    
	len = sizeof(reginfo_event_t);
	if(publ_body){
	    len += publ_body->len;
	}
	if(publ_id){
	    len += publ_id->len;
	}
	if(publ_content_type){
	    len += publ_content_type->len;
	}
	if(subs_remote_target){
	    len += subs_remote_target->len;
	}
	if(subs_watcher_uri){
	    len += subs_watcher_uri->len;
	}
	if(subs_contact){
	    len += subs_contact->len;
	}
	if(subs_outbound_proxy){
	    len += subs_outbound_proxy->len;
	}
	if(extra_headers){
	    len += extra_headers->len;
	}
	if(pres_uri){
	    len += pres_uri->len;
	}
	
	LM_DBG("Shm alloc'ing %d for new reg info event\n", len);
	new_event = (reginfo_event_t*) shm_malloc(len);
	
	if (!new_event) {
		LM_ERR("No more shm mem\n");
		return NULL;
	}
	memset(new_event, 0, len);
	
	p = (char*) (new_event + 1);

	if(publ_body) {
	    LM_DBG("publ_body [%.*s]\n", publ_body->len, publ_body->s);
	    new_event->publ_body.s = p;
	    new_event->publ_body.len = publ_body->len;
	    memcpy(p, publ_body->s, publ_body->len);
	    p += publ_body->len;
	}
	if(publ_id) {
	    LM_DBG("publ_id [%.*s]\n", publ_id->len, publ_id->s);
	    new_event->publ_id.s = p;
	    new_event->publ_id.len = publ_id->len;
	    memcpy(p, publ_id->s, publ_id->len);
	    p += publ_id->len;
	}
	if(publ_content_type) {
	    LM_DBG("publ_content_type [%.*s]\n", publ_content_type->len, publ_content_type->s);
	    new_event->publ_content_type.s = p;
	    new_event->publ_content_type.len = publ_content_type->len;
	    memcpy(p, publ_content_type->s, publ_content_type->len);
	    p += publ_content_type->len;
	}
	if(subs_remote_target) {
	    LM_DBG("subs_remote_target [%.*s]\n", subs_remote_target->len, subs_remote_target->s);
	    new_event->subs_remote_target.s = p;
	    new_event->subs_remote_target.len = subs_remote_target->len;
	    memcpy(p, subs_remote_target->s, subs_remote_target->len);
	    p += subs_remote_target->len;
	}
	if(subs_watcher_uri) {
	    LM_DBG("subs_watcher_uri [%.*s]\n", subs_watcher_uri->len, subs_watcher_uri->s);
	    new_event->subs_watcher_uri.s = p;
	    new_event->subs_watcher_uri.len = subs_watcher_uri->len;
	    memcpy(p, subs_watcher_uri->s, subs_watcher_uri->len);
	    p += subs_watcher_uri->len;
	}
	if(subs_contact) {
	    LM_DBG("subs_contact [%.*s]\n", subs_contact->len, subs_contact->s);
	    new_event->subs_contact.s = p;
	    new_event->subs_contact.len = subs_contact->len;
	    memcpy(p, subs_contact->s, subs_contact->len);
	    p += subs_contact->len;
	}
	if(subs_outbound_proxy) {
	    LM_DBG("subs_outbound_proxy [%.*s]\n", subs_outbound_proxy->len, subs_outbound_proxy->s);
	    new_event->subs_outbound_proxy.s = p;
	    new_event->subs_outbound_proxy.len = subs_outbound_proxy->len;
	    memcpy(p, subs_outbound_proxy->s, subs_outbound_proxy->len);
	    p += subs_outbound_proxy->len;
	}
	if(extra_headers) {
	    LM_DBG("extra_headers [%.*s]\n", extra_headers->len, extra_headers->s);
	    new_event->extra_headers.s = p;
	    new_event->extra_headers.len = extra_headers->len;
	    memcpy(p, extra_headers->s, extra_headers->len);
	    p += extra_headers->len;
	}
	if(pres_uri) {
	    LM_DBG("pres_uri [%.*s]\n", pres_uri->len, pres_uri->s);
	    new_event->pres_uri.s = p;
	    new_event->pres_uri.len = pres_uri->len;
	    memcpy(p, pres_uri->s, pres_uri->len);
	    p += pres_uri->len;
	}
		
	if (p != (((char*) new_event) + len)) {
	    LM_CRIT("buffer overflow\n");
	    shm_free(new_event);
	    return 0;
	}
	
	new_event->expires = expires;
	new_event->flag = flag;
	new_event->reg_info_event = reg_info_event;
	new_event->sourge_flag = source_flag;
	
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
	reginfo_event_list->size++;
	
	if(reginfo_queue_size_threshold > 0 && reginfo_event_list->size > reginfo_queue_size_threshold) {
	    LM_WARN("Reginfo queue is size [%d] and has exceed reginfo_queue_size_threshold of [%d]", reginfo_event_list->size, reginfo_queue_size_threshold);
	}
	
	sem_release(reginfo_event_list->empty);
	lock_release(reginfo_event_list->lock);
}

reginfo_event_t* pop_reginfo_event()
{
	reginfo_event_t *ev;

	// Make sure, it's initialized:
	init_reginfo_event_list();

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
	
	reginfo_event_list->size--;
	
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
    	publ_info_t publ;
	subs_info_t subs;
	reginfo_event_t *ev;
	for (;;) {
		LM_DBG("POPPING REGINFO EVENT\n");
	        ev = pop_reginfo_event();
	        LM_DBG("PROCESSING REGINFO EVENT with event [%d]\n", ev->event);

	        switch (ev->event) {
	        case REG_EVENT_PUBLISH:
	        	LM_DBG("Sending out-of-band publish with pres_uri [%.*s] publ_id [%.*s] publ_content_type [%.*s] extra_headers [%.*s]"
				"expires [%d] event [%d]\n",
				ev->pres_uri.len, ev->pres_uri.s, ev->publ_id.len, ev->publ_id.s, ev->publ_content_type.len, ev->publ_content_type.s,
				ev->extra_headers.len, ev->extra_headers.s, ev->expires, ev->reg_info_event);
			LM_DBG("publ_body [%.*s] \n",
				ev->publ_body.len, ev->publ_body.s);
			
			memset(&publ, 0, sizeof(publ_info_t));
			publ.pres_uri = &ev->pres_uri;
			publ.body = &ev->publ_body;
			publ.id = ev->publ_id;
			publ.content_type = ev->publ_content_type;
			publ.expires = ev->expires;

			/* make UPDATE_TYPE, as if this "publish dialog" is not found
			 by pua it will fallback to INSERT_TYPE anyway */
			publ.flag |= ev->flag;
			publ.source_flag |= ev->sourge_flag;
			publ.event |= ev->reg_info_event;
			publ.extra_headers = &ev->extra_headers;

			if (pua.send_publish(&publ) < 0) {
				LM_ERR("Error while sending publish\n");
			}
	        	break;
	        case REG_EVENT_SUBSCRIBE:
	        	memset(&subs, 0, sizeof(subs_info_t));

			subs.remote_target = &ev->subs_remote_target;
			subs.pres_uri= &ev->pres_uri;
			subs.watcher_uri= &ev->subs_watcher_uri;
			subs.expires = ev->expires;

			subs.source_flag= ev->sourge_flag;
			subs.event= ev->reg_info_event;
			subs.contact= &ev->subs_contact;
			subs.extra_headers = &ev->extra_headers;

			if(ev->subs_outbound_proxy.s) {
			    subs.outbound_proxy= &ev->subs_outbound_proxy;
			}

			subs.flag|= ev->flag;
			
			
			LM_DBG("Sending out-of-band subscribe with remote_target [%.*s] pres_uri [%.*s] subs_watcher_uri [%.*s] subs_contact [%.*s] extra_headers [%.*s] "
				"expires [%d] event [%d] flag [%d] source_flag [%d]\n",
				subs.remote_target->len, subs.remote_target->s, subs.pres_uri->len, subs.pres_uri->s, subs.watcher_uri->len, subs.watcher_uri->s,
				subs.contact->len, subs.contact->s, subs.extra_headers->len, subs.extra_headers->s, subs.expires, subs.event, subs.flag, subs.source_flag);
			if(subs.outbound_proxy) {
			    LM_DBG("subs_outbound_proxy [%.*s]\n",
				subs.outbound_proxy->len, subs.outbound_proxy->s);
			}
			

			if(pua.send_subscribe(&subs)< 0) {
				LM_ERR("while sending subscribe\n");
			}
			
	        	break;
	        default:
	        	LM_ERR("Unknown REG event.....ignoring\n");
	        	break;
	        }
	        free_reginfo_event(ev);
	}
}



