/*
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 * This is the core presence module, used in combination with other modules.
 *
 * \ingroup presence
 */

#include <stdio.h>
#include <stdlib.h>
#include "../../dprint.h"
#include "../../sr_module.h"
#include "presence.h"
#include "bind_presence.h"
#include "notify.h"
#include "publish.h"
#include "subscribe.h"

int bind_presence(presence_api_t* api)
{
	if (!api) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}
	
	api->add_event = add_event;
	api->contains_event= contains_event;
	api->search_event= search_event;
	api->get_event_list= get_event_list;
	api->update_watchers_status= update_watchers_status;
	api->new_shtable= new_shtable;
	api->destroy_shtable= destroy_shtable;
	api->insert_shtable= insert_shtable;
	api->search_shtable= search_shtable;
	api->delete_shtable= delete_shtable;
	api->update_shtable= update_shtable;
	api->mem_copy_subs= mem_copy_subs;
	api->update_db_subs_timer= update_db_subs_timer;
	api->extract_sdialog_info= extract_sdialog_info;
	api->get_sphere= get_sphere;
	api->get_presentity= get_p_notify_body;
	api->free_presentity= free_notify_body;
	api->pres_auth_status= pres_auth_status;
	api->handle_publish= handle_publish;
	api->handle_subscribe0= handle_subscribe0;
	api->handle_subscribe= handle_subscribe;
	return 0;
}


