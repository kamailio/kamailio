/*
 *
 * Copyright (C) 2011 Mészáros Mihály
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
 * History:
 * --------
 *  2011-09-22  initial version (misi)
 */

/*!
 * \file
 * \brief SIP-router Presence :: ua-profile provisioning support
 * \ingroup presence
 * Module: \ref presence
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../parser/parse_content.h"
#include "../presence/event_list.h"
#include "presence_profile.h"


int profile_add_events(void)
{
    pres_ev_t event;
	
    /* constructing profile event */
    memset(&event, 0, sizeof(pres_ev_t));
    event.name.s = "ua-profile";
    event.name.len = 10;


    event.content_type.s = "text/xml";
    event.content_type.len = 8;

    event.default_expires= 3600;
    event.type = PUBL_TYPE;
    event.req_auth = 0;
    event.evs_publ_handl = 0;
    
    if (pres_add_event(&event) < 0) {
	LM_ERR("failed to add event \"ua-profile\"\n");
	return -1;
    }		
	
    return 0;
}
