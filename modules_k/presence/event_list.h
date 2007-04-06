/*
 * $Id: event_list.c 1953 2007-04-04 08:50:33Z anca_vamanu $
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
 *  2007-04-05  initial version (anca)
 */

#ifndef _PRES_EV_LST_
#define  _PRES_EV_LST_

#include "../../str.h"

#define WINFO_TYPE			1<< 0
#define PUBL_TYPE		    1<< 1

typedef struct ev
{
	str name;
	str* param;         // required param 
	/* to do: transform it in a list ( for multimple param)*/
	str stored_name;
	str content_type;
	int agg_body;
	int type;
	int req_auth;
	struct ev* wipeer; /* can be NULL or the name of teh winfo event */
	struct ev* next;
	
}ev_t;

typedef struct evlist
{
	int ev_count;
	ev_t* events;
}evlist_t;	

evlist_t* init_evlist();

int add_event(char* name, char* param, char* content_type, int agg_body);

ev_t* contains_event(str* name, str* param);

void destroy_evlist();

#endif
