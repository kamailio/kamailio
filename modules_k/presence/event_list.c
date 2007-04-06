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
#include "presence.h"

int add_event(char* name, char* param, char* content_type, int agg_body)
{
	ev_t* ev= NULL;
	int size;
	str str_name;
	str* str_param= NULL;
	char* sep= NULL;
	str wipeer_name;
	char buf[50];
	int ret_code= 1;

	str_name.s= name;
	str_name.len= strlen(name);

	if(param)
	{
		str_param= (str*)pkg_malloc(sizeof(str));
		if(str_param== NULL)
		{
			LOG(L_ERR, "PRESENCE: add_event: ERROR No more memory\n");
			return -1;
		}
		str_param->s= param;
		str_param->len= strlen(param);
	}

	if(contains_event(&str_name, str_param))
	{
		DBG("PRESENCE: add_event: Found prevoius record for event\n");
		ret_code= -1;
		goto done;
	}	
	size= sizeof(ev_t)+ (str_name.len+ strlen(content_type))
		*sizeof(char);

	if(param)
		size+= sizeof(str)+ ( 2*strlen(param) + str_name.len+ 1)* sizeof(char);

	ev= (ev_t*)shm_malloc(size);
	if(ev== NULL)
	{
		LOG(L_ERR, "PRESENCE: add_event: ERROR while allocating memory\n");
		ret_code= -1;
		goto done;
	}
	memset(ev, 0, sizeof(ev_t));

	size= sizeof(ev_t);
	ev->name.s= (char*)ev+ size;
	ev->name.len= str_name.len;
	memcpy(ev->name.s, name, str_name.len);
	size+= str_name.len;

	if(str_param)
	{	
		ev->param= (str*)((char*)ev+ size);
		size+= sizeof(str);
		ev->param->s= (char*)ev+ size;
		memcpy(ev->param->s, str_param->s, str_param->len);
		ev->param->len= str_param->len;
		size+= str_param->len;

		ev->stored_name.s= (char*)ev+ size;
		memcpy(ev->stored_name.s, name, str_name.len);
		ev->stored_name.len= str_name.len;
		memcpy(ev->stored_name.s+ ev->stored_name.len, ";", 1);
		ev->stored_name.len+= 1;
		memcpy(ev->stored_name.s+ ev->stored_name.len, str_param->s, str_param->len);
		ev->stored_name.len+= str_param->len;
		size+= ev->stored_name.len;
	}
	else
	{
		ev->stored_name.s= ev->name.s;
		ev->stored_name.len= ev->name.len;
	}	

	ev->content_type.s= (char*)ev+ size;
	ev->content_type.len= strlen(content_type);
	memcpy(ev->content_type.s, content_type, ev->content_type.len);
	size+= ev->content_type.len;
	
	ev->agg_body= agg_body;
	
	sep= strchr(name, '.');
	if(sep)
	{
		if(strncmp(sep+1, "winfo", 5)== 0)
		{	
			ev->type= WINFO_TYPE;
			wipeer_name.s= name;
			wipeer_name.len= sep - name;
			
			ev->wipeer= contains_event(&wipeer_name, ev->param );
		}
		else
		{	
			ev->type= PUBL_TYPE;
			wipeer_name.s= buf;
			wipeer_name.len= sprintf(wipeer_name.s, "%s", name);
			memcpy(wipeer_name.s+ wipeer_name.len, ".winfo", 5);
			wipeer_name.len+= 5;
			ev->wipeer= contains_event(&wipeer_name, ev->param);
		}

	}	
	else
		ev->type= PUBL_TYPE;

	ev->next= EvList->events;
	EvList->events= ev;
	EvList->ev_count++;

done:
	if(str_param)
		pkg_free(str_param);
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
	e1= EvList->events;
	while(e1)
	{
		e2= e1->next;
		shm_free(e1);
		e1= e2;
	}	
	shm_free(EvList);
}	
