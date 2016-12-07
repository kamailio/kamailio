/*
 * pua module - presence user agent module
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
 */


#include "pua_bind.h"
#include "../../dprint.h"
#include "hash.h"

int bind_pua(pua_api_t* api)
{
	if (!api) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	api->send_publish   =  send_publish; 
	api->send_subscribe =  send_subscribe;
	api->register_puacb =  register_puacb;
	api->is_dialog      =  is_dialog;
	api->get_record_id  =  get_record_id;
	api->add_event      =  add_pua_event;
	api->get_subs_list  =  get_subs_list;

	return 0;
}

