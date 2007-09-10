/*
 * $Id: bind_presence.h 1979 2007-04-06 13:24:12Z anca_vamanu $
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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
 *  2007-04-17  initial version (anca)
 */

#ifndef _PRES_BIND_H_
#define _PRES_BIND_H_

#include "event_list.h"
#include "hash.h"

typedef int (*update_watchers_t)(str pres_uri, pres_ev_t* ev, str* rules_doc);

typedef struct presence_api {
	add_event_t add_event;
	contains_event_t contains_event;
	search_event_t search_event;
	get_event_list_t get_event_list;
	update_watchers_t update_watchers_status;
	/* subs hash table functions */
	new_shtable_t new_shtable;
	destroy_shtable_t destroy_shtable;
	insert_shtable_t insert_shtable;
	search_shtable_t search_shtable;
	delete_shtable_t delete_shtable;
	update_shtable_t update_shtable;
	mem_copy_subs_t  mem_copy_subs;
	update_db_subs_t update_db_subs;
	extract_sdialog_info_t extract_sdialog_info;
} presence_api_t;

int bind_presence(presence_api_t* api);

typedef int (*bind_presence_t)(presence_api_t* api);

#endif

