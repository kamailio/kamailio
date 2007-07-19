/*
 * $Id: presence.c 1953 2007-04-04 08:50:33Z anca_vamanu $
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2007-04-04  initial version (anca)
 */

#include <stdlib.h>
#include<stdio.h>
#include <string.h>
#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h" 
#include "event_list.h"

int add_event(ev_t* event)
{
	ev_t* ev= NULL;
	int size;
	char* sep= NULL;
	str wipeer_name;
	char buf[50];
	int ret_code= 1;

	if(event->name.s== NULL || event->name.len== 0)
	{
		LOG(L_ERR, "PRESENCE: add_event: NULL event name\n");
		return -1;
	}

	if(event->content_type.s== NULL || event->content_type.len== 0)
	{
		LOG(L_ERR, "PRESENCE: add_event: NULL content_type param\n");
		return -1;
	}

	if(contains_event(&event->name, event->param))
	{
		DBG("PRESENCE: add_event: Found prevoius record for event\n");
		ret_code= -1;
		goto done;
	}	
	size= sizeof(ev_t)+ (event->name.len+ event->content_type.len)*sizeof(char);

	if(event->param)
		size+= sizeof(str)+ ( 2*event->param->len + event->name.len+ 2)* sizeof(char);

	ev= (ev_t*)shm_malloc(size);
	if(ev== NULL)
	{
		LOG(L_ERR, "PRESENCE: add_event: ERROR while allocating memory\n");
		ret_code= -1;
		goto done;
	}
	memset(ev, 0, size);

	size= sizeof(ev_t);
	ev->name.s= (char*)ev+ size;
	ev->name.len= event->name.len;
	memcpy(ev->name.s, event->name.s, event->name.len);
	size+= event->name.len;

	if(event->param)
	{	
		ev->param= (str*)((char*)ev+ size);
		size+= sizeof(str);
		ev->param->s= (char*)ev+ size;
		memcpy(ev->param->s, event->param->s, event->param->len);
		ev->param->len= event->param->len;
		size+= event->param->len;
	
		ev->stored_name.s= (char*)ev+ size;
		memcpy(ev->stored_name.s, event->name.s, event->name.len);
		ev->stored_name.len= event->name.len;
		memcpy(ev->stored_name.s+ ev->stored_name.len, ";", 1);
		ev->stored_name.len+= 1;
		memcpy(ev->stored_name.s+ ev->stored_name.len, event->param->s, event->param->len);
		ev->stored_name.len+= event->param->len;
		size+= ev->stored_name.len;
	}
	else
	{
		ev->stored_name.s= ev->name.s;
		ev->stored_name.len= ev->name.len;
	}	

	ev->content_type.s= (char*)ev+ size;
	ev->content_type.len= event->content_type.len;
	memcpy(ev->content_type.s, event->content_type.s, event->content_type.len);
	size+= ev->content_type.len;
	
	sep= strchr(event->name.s, '.');

	if(sep && strncmp(sep+1, "winfo", 5)== 0)
	{	
		ev->type= WINFO_TYPE;
		wipeer_name.s= event->name.s;
		wipeer_name.len= sep - event->name.s;
		ev->wipeer= contains_event(&wipeer_name, ev->param );
	}
	else
	{	
		ev->type= PUBL_TYPE;
		wipeer_name.s= buf;
		memcpy(wipeer_name.s, event->name.s, event->name.len);
		wipeer_name.len= event->name.len;
		memcpy(wipeer_name.s+ wipeer_name.len, ".winfo", 6);
		wipeer_name.len+= 6;
		ev->wipeer= contains_event(&wipeer_name, ev->param);
	}
	
	if(ev->wipeer)	
		ev->wipeer->wipeer= ev;

	ev->req_auth= event->req_auth;
	ev->agg_nbody= event->agg_nbody;
	ev->apply_auth_nbody= event->apply_auth_nbody;
	ev->is_watcher_allowed= event->is_watcher_allowed;
	ev->evs_publ_handl= event->evs_publ_handl;
	ev->etag_not_new= event->etag_not_new;
	ev->free_body= event->free_body;
	ev->next= EvList->events;
	EvList->events= ev;
	EvList->ev_count++;

	DBG("PRESENCE: add_event: succesfully added event: %.*s - len= %d\n", 
			ev->stored_name.len, ev->stored_name.s, ev->stored_name.len);
done:
	
	return ret_code; 
}

evlist_t* init_evlist()
{
	evlist_t*  list= NULL;

	list= (evlist_t*)shm_malloc(sizeof(evlist_t));
	if(list== NULL)
	{
		LOG(L_ERR, "PRESENCE: init_evlist: ERROR no more share memory\n");
		return NULL;
	}
	list->ev_count= 0;
	list->events= NULL;
	
	return list;
}	

ev_t* contains_event(str* name, str* param)
{
	ev_t* event;
	event= EvList->events;

	while(event)
	{
		if(event->name.len== name->len &&
				strncmp(event->name.s, name->s, name->len)== 0)
		{
			// verify if there is a restrictive params and if it equals the given one
			if(!event->param)
				return event;
			
			if(!param)
				return NULL;

			if(event->param->len== param->len &&
					strncmp(event->param->s, param->s, param->len)== 0)
				return event;
	
		}
		
		event= event->next;
	}	

	return NULL;	
}

void destroy_evlist()
{
    ev_t* e1, *e2;
    if (EvList) 
	{
		e1= EvList->events;
		while(e1)
	    {
			e2= e1->next;
			shm_free(e1);
			e1= e2;
	    }	
		shm_free(EvList);
    }
}

