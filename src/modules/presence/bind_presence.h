/*
 * presence module - presence server implementation
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
/*! \file
 * \brief Kamailio Presence :: Kamailio generic presence module
 *
 * \ingroup presence
 */


#ifndef _PRES_BIND_H_
#define _PRES_BIND_H_

#include "event_list.h"
#include "hash.h"
#include "presentity.h"
#include "../../sr_module.h"

typedef int (*update_watchers_t)(str pres_uri, pres_ev_t* ev, str* rules_doc);
typedef str* (*pres_get_presentity_t)(str pres_uri, pres_ev_t *ev, str *etag, str *contact);
typedef void (*pres_free_presentity_t)(str *presentity, pres_ev_t *ev);
typedef int (*pres_auth_status_t)(struct sip_msg* msg, str watcher_uri, str presentity_uri);
typedef int (*pres_handle_publish_t)(struct sip_msg* msg, char *str1, char* str2);
typedef int (*pres_handle_subscribe0_t)(struct sip_msg* msg);
typedef int (*pres_handle_subscribe_t)(struct sip_msg* msg, str watcher_user, str watcher_domain);

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
	update_db_subs_t update_db_subs_timer;
	extract_sdialog_info_t extract_sdialog_info;
	pres_get_sphere_t get_sphere;
	pres_get_presentity_t get_presentity;
	pres_free_presentity_t free_presentity;
	pres_auth_status_t pres_auth_status;
	pres_handle_publish_t handle_publish;
	pres_handle_subscribe0_t handle_subscribe0;
	pres_handle_subscribe_t handle_subscribe;
} presence_api_t;

int bind_presence(presence_api_t* api);

typedef int (*bind_presence_t)(presence_api_t* api);

inline static int presence_load_api(presence_api_t *api)
{
	bind_presence_t bind_presence_exports;
	if (!(bind_presence_exports = (bind_presence_t)find_export("bind_presence", 1, 0)))
	{
		LM_ERR("Failed to import bind_presence\n");
		return -1;
	}
	return bind_presence_exports(api);
}

#endif

