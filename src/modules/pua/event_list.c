/*
 * pua module - presence user agent module
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../str.h"
#include "send_publish.h"
#include "../../mem/shm_mem.h"
#include "event_list.h"

pua_event_t* init_pua_evlist(void)
{
	pua_event_t* list= NULL;

	list= (pua_event_t*)shm_malloc(sizeof(pua_event_t));
	if(list== NULL)
	{
		LM_ERR("no more share memory\n");
		return NULL;
	}
	list->next= NULL;
	
	return list;

}

int add_pua_event(int ev_flag, char* name, char* content_type,
		evs_process_body_t* process_body)
{
		
	pua_event_t* event= NULL;
	int size;
	int name_len;
	int ctype_len= 0;
	str str_name;	

	name_len= strlen(name);
	str_name.s= name;
	str_name.len= name_len;

	if(contains_pua_event(&str_name))
	{
		LM_DBG("Event already exists\n");
		return 0;
	}
	if(content_type)
		ctype_len= strlen(content_type);

	size= sizeof(pua_event_t)+ (name_len+ ctype_len)* sizeof(char);

	event= (pua_event_t*)shm_malloc(size);
	if(event== NULL)
	{
		LM_ERR("No more share memory\n");
		return -1;
	}	
	memset(event, 0, size);
	size= sizeof(pua_event_t);

	event->name.s= (char*)event+ size;
	memcpy(event->name.s, name, name_len);
	event->name.len= name_len;
	size+= name_len;
			
	if(content_type)
	{
		event->content_type.s= (char*)event+ size;
		memcpy(event->content_type.s, content_type, ctype_len);
		event->content_type.len= ctype_len;
		size+= ctype_len;		
	}

	event->process_body= process_body;
	event->ev_flag= ev_flag;

	event->next= pua_evlist->next;
	pua_evlist->next= event;

	return 0;
}	

pua_event_t* contains_pua_event(str* name)
{
	pua_event_t* event;
	event= pua_evlist->next;

	while(event)
	{
		if(event->name.len== name->len &&
				strncmp(event->name.s, name->s, name->len)== 0)
		{
			return event;	
		}
		event= event->next;
	}	

	return NULL;	
}

pua_event_t* get_event(int ev_flag)
{
	pua_event_t* event;
	event= pua_evlist->next;

	while(event)
	{
		if(event->ev_flag== ev_flag)
		{
			return event;	
		}
		event= event->next;
	}	
	return NULL;	
}


void destroy_pua_evlist(void)
{
	pua_event_t* e1, *e2;

	if(pua_evlist)
	{
		e1= pua_evlist->next;
		while(e1)
		{
			e2= e1->next;
			shm_free(e1);
			e1= e2;
		}	
		shm_free(pua_evlist);
	}	

}	
