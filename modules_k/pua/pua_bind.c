/*
 * $Id$
 *
 * pua module - presence user agent module
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
 */


#include "pua_bind.h"
#include "../../dprint.h"

int bind_pua(pua_api_t* api)
{
	if (!api) {
		LOG(L_ERR, "PUA:bind_pua: Invalid parameter value\n");
		return -1;
	}

	api->send_publish = (send_publish_t )find_export
		("send_publish", 1, 0);
	if (api->send_publish == 0)
	{
		LOG(L_ERR, "PUA:bind_pua: Can't bind send_publish\n");
		return -1;
	}

	api->send_subscribe = ( send_subscribe_t)find_export
		("send_subscribe", 1, 0);
	if (api->send_subscribe == 0)
	{
		LOG(L_ERR, "PUA:bind_pua: Can't bind send_subscribe\n");
		return -1;
	}
	api->register_puacb = ( register_puacb_t)find_export
		("register_puacb", 1, 0);
	if (api->register_puacb == 0)
	{
		LOG(L_ERR, "PUA:bind_pua: Can't bind send_subscribe\n");
		return -1;
	}
	api->is_dialog= (query_dialog_t)find_export("pua_is_dialog", 1, 0);
	if(api->is_dialog== 0)
	{
		LOG(L_ERR, "PUA:bind_pua: Can't bind pua_is_dialog\n");
		return -1;
	}
	api->add_event= (add_pua_event_t)find_export("add_pua_event", 4, 0);
	if(api->add_event== 0)
	{
		LOG(L_ERR, "PUA:bind_pua: Can't bind add_pua_event\n");
		return -1;
	}

	return 0;
}

