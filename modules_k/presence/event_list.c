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
#include "../../parser/parse_event.h" 
#include "../../mem/shm_mem.h" 
#include "../../mem/mem.h" 
#include "event_list.h"
#include "hash.h"

#define MAX_EVNAME_SIZE 20

int search_event_params(event_t* ev, event_t* searched_ev);

event_t* shm_copy_event(event_t* e)
{
	event_t* ev= NULL;
	param_t* p1, *p2;
	int size;

	ev= (event_t*)shm_malloc(sizeof(event_t));
	if(ev== NULL)
	{
		ERR_MEM(SHARE_MEM);
	}
	memset(ev, 0, sizeof(event_t));

	ev->text.s= (char*)shm_malloc(e->text.len* sizeof(char));
	if(ev->text.s== NULL)
	{
		ERR_MEM(SHARE_MEM);
	}
	memcpy(ev->text.s, e->text.s, e->text.len);
	ev->text.len= e->text.len;

	p1= e->params;
	while(p1)
	{
		size= sizeof(param_t)+ (p1->name.len+ p1->body.len)* sizeof(char);
		p2= (param_t*)shm_malloc(size);
		if(p2== NULL)
		{
			ERR_MEM(SHARE_MEM);
		}
		memset(p2, 0, size);

		size= sizeof(param_t);
		CONT_COPY(p2, p2->name, p1->name);
		if(p1->body.s && p1->body.len)
			CONT_COPY(p2, p2->body, p1->body);
		p2->next= ev->params;
		ev->params= p2;
		p1= p1->next;
	}
	ev->parsed= e->parsed;

	return ev;

error:
	shm_free_event(ev);
	return NULL;
}

void shm_free_event(event_t* ev)
{
	if(ev== NULL)
		return;
	
	if(ev->text.s)
		shm_free(ev->text.s);

	free_event_params(ev->params, SHM_MEM_TYPE);

	shm_free(ev);
}


int add_event(pres_ev_t* event)
{
	pres_ev_t* ev= NULL;
	event_t parsed_event;
	str wipeer_name;
	char* sep;
	char buf[50];
	int not_in_list= 0;

	memset(&parsed_event, 0, sizeof(event_t));

	if(event->name.s== NULL || event->name.len== 0)
	{
		LM_ERR("NULL event name\n");
		return -1;
	}

	if(event->content_type.s== NULL || event->content_type.len== 0)
	{
		LM_ERR("NULL content_type param\n");
		return -1;
	}
	
	ev= contains_event(&event->name, &parsed_event);
	if(ev== NULL)
	{
		not_in_list= 1;
		ev= (pres_ev_t*)shm_malloc(sizeof(pres_ev_t));
		if(ev== NULL)
		{
			free_event_params(parsed_event.params, PKG_MEM_TYPE);
			ERR_MEM(SHARE_MEM);
		}
		memset(ev, 0, sizeof(pres_ev_t));
		ev->name.s= (char*)shm_malloc(event->name.len* sizeof(char));
		if(ev->name.s== NULL)
		{
			free_event_params(parsed_event.params, PKG_MEM_TYPE);
			ERR_MEM(SHARE_MEM);
		}
		memcpy(ev->name.s, event->name.s, event->name.len);
		ev->name.len= event->name.len;

		ev->evp= shm_copy_event(&parsed_event);
		if(ev->evp== NULL)
		{
			LM_ERR("copying event_t structure\n");
			free_event_params(parsed_event.params, PKG_MEM_TYPE);
			goto error;
		}
		free_event_params(parsed_event.params, PKG_MEM_TYPE);
	}
	else
	{
		free_event_params(parsed_event.params, PKG_MEM_TYPE);
		if(ev->content_type.s)
		{
			LM_DBG("Event already registered\n");
			return 0;
		}
	}

	ev->content_type.s=(char*)shm_malloc(event->content_type.len* sizeof(char)) ;
	if(ev->content_type.s== NULL)
	{
		ERR_MEM(SHARE_MEM);
	}	
	ev->content_type.len= event->content_type.len;
	memcpy(ev->content_type.s, event->content_type.s, event->content_type.len);

	sep= strchr(event->name.s, '.');
	if(sep && strncmp(sep+1, "winfo", 5)== 0)
	{	
		ev->type= WINFO_TYPE;
		wipeer_name.s= event->name.s;
		wipeer_name.len= sep - event->name.s;
		ev->wipeer= contains_event(&wipeer_name, NULL);
	}
	else
	{	
		ev->type= PUBL_TYPE;
		wipeer_name.s= buf;
		memcpy(wipeer_name.s, event->name.s, event->name.len);
		wipeer_name.len= event->name.len;
		memcpy(wipeer_name.s+ wipeer_name.len, ".winfo", 6);
		wipeer_name.len+= 6;
		ev->wipeer= contains_event(&wipeer_name, NULL);
	}
	
	if(ev->wipeer)	
		ev->wipeer->wipeer= ev;

	if(event->req_auth && 
		( event->get_auth_status==0 ||event->get_rules_doc== 0))
	{
		LM_ERR("bad event structure\n");
		goto error;
	}
	ev->req_auth= event->req_auth;
	ev->agg_nbody= event->agg_nbody;
	ev->apply_auth_nbody= event->apply_auth_nbody;
	ev->get_auth_status= event->get_auth_status;
	ev->get_rules_doc= event->get_rules_doc;
	ev->evs_publ_handl= event->evs_publ_handl;
	ev->etag_not_new= event->etag_not_new;
	ev->free_body= event->free_body;
	ev->default_expires= event->default_expires;

	if(not_in_list)
	{
		ev->next= EvList->events;
		EvList->events= ev;
	}
	EvList->ev_count++;
	
	LM_DBG("succesfully added event: %.*s - len= %d\n",ev->name.len,
			ev->name.s, ev->name.len);
	return 0;
error:
	if(ev && not_in_list)
	{
		free_pres_event(ev);	
	}
	return -1;
}

void free_pres_event(pres_ev_t* ev)
{
	if(ev== NULL)
		return;

	if(ev->name.s)
		shm_free(ev->name.s);
	if(ev->content_type.s)
		shm_free(ev->content_type.s);
	shm_free_event(ev->evp);
	shm_free(ev);

}

evlist_t* init_evlist(void)
{
	evlist_t*  list= NULL;

	list= (evlist_t*)shm_malloc(sizeof(evlist_t));
	if(list== NULL)
	{
		LM_ERR("no more share memory\n");
		return NULL;
	}
	list->ev_count= 0;
	list->events= NULL;
	
	return list;
}	

pres_ev_t* contains_event(str* sname, event_t* parsed_event)
{
	event_t event;
	pres_ev_t* e;
	
	memset(&event, 0, sizeof(event_t));
	if(event_parser(sname->s, sname->len, &event)< 0)
	{
		LM_ERR("parsing event\n");
		return NULL;
	}
	if(parsed_event)
		*parsed_event= event;
	else
	{
		free_event_params(event.params, PKG_MEM_TYPE);
	}
	e= search_event(&event);

	return e;
}

void free_event_params(param_t* params, int mem_type)
{
	param_t* t1, *t2;
	t2= t1= params;

	while(t1)
	{
		t2= t1->next;
		if(mem_type == SHM_MEM_TYPE)
			shm_free(t1);
		else
			pkg_free(t1);
		t1= t2;
	}
	
}

pres_ev_t* search_event(event_t* event)
{
	pres_ev_t* pres_ev;
	pres_ev= EvList->events;

	LM_DBG("start event= [%.*s]\n", event->text.len, event->text.s);

	while(pres_ev)
	{
		if(pres_ev->evp->parsed== event->parsed)
		{
			if(event->params== NULL && pres_ev->evp->params== NULL)
			{
				return pres_ev;
			}
	
			/* search all parameters in event in ev */
			if(search_event_params(event, pres_ev->evp)< 0)
				goto cont;
			
			/* search all parameters in ev in event */
			if(search_event_params(pres_ev->evp, event)< 0)
				goto cont;

			return pres_ev;
		}
cont:		pres_ev= pres_ev->next;
	}
	return NULL;

}

int search_event_params(event_t* ev, event_t* searched_ev)
{
	param_t* ps, *p;
	int found;

	ps= ev->params;

	while(ps)
	{
		p= searched_ev->params;
		found= 0;
	
		while(p)
		{
			if(p->name.len== ps->name.len && 
				strncmp(p->name.s,ps->name.s, ps->name.len)== 0)
				if((p->body.s== 0 && ps->body.s== 0) ||
					(p->body.len== ps->body.len && 
					strncmp(p->body.s,ps->body.s,ps->body.len)== 0))
				{
					found= 1;
					break;
				}
				p= p->next;
		}
		if(found== 0)
			return -1;
		ps= ps->next;
	}

	return 1;

}
int get_event_list(str** ev_list)
{	
	pres_ev_t* ev= EvList->events;
	int i;
	str* list;
	*ev_list= NULL;
	
	if(EvList->ev_count== 0)
		return 0;
	
	list= (str*)pkg_malloc(sizeof(str));
	if(list== NULL)
	{
		LM_ERR("No more memory\n");
		return -1;
	}
	memset(list, 0, sizeof(str));
	list->s= (char*)pkg_malloc(EvList->ev_count* MAX_EVNAME_SIZE);
	if(list->s== NULL)
	{
		LM_ERR("No more memory\n");
		pkg_free(list);
		return -1;
	}
	list->s[0]= '\0';
	
	for(i= 0; i< EvList->ev_count; i++)
	{
		if(i> 0)
		{
			memcpy(list->s+ list->len, ", ", 2);
			list->len+= 2;
		}	
		memcpy(list->s+ list->len, ev->name.s, ev->name.len );
		list->len+= ev->name.len ;
		ev= ev->next;
	}
	
	*ev_list= list;
	return 0;
}

void destroy_evlist(void)
{
    pres_ev_t* e1, *e2;
    if (EvList) 
	{
		e1= EvList->events;
		while(e1)
	    {
			e2= e1->next;
			free_pres_event(e1);
			e1= e2;
	    }	
		shm_free(EvList);
    }
}

