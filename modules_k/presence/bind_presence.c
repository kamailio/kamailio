/*
 * $Id: bind_presence.c 1979 2007-04-06 13:24:12Z anca_vamanu $
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
 *  2007-04-17  initial version (anca)
 */
#include "bind_presence.h"
#include "../../dprint.h"
#include "../../sr_module.h"

int bind_presence(event_api_t* api)
{
	if (!api) {
		LOG(L_ERR, "NOTIFIER:bind_notifier: Invalid parameter value\n");
		return -1;
	}
	
	api->add_event = add_event;
	api->contains_event= contains_event;

	return 0;
}


