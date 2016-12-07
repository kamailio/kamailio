/*
 * add "conference" event to presence module - mariusbucur
 * 
 * Copyright (C) 2010 Marius Bucur
 * Copyright (C) 2007 Juha Heinanen
 * Copyright (C) 2008 Klaus Darilion, IPCom
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
 * 2010-07-12  initial version (mariusbucur)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../parser/parse_content.h"
#include "../presence/event_list.h"
#include "presence_conference.h"
#include "notify_body.h"

int conference_add_events(void)
{
    pres_ev_t event;
	
    /* constructing "conference" event and add it to the list of events packages supported */
    memset(&event, 0, sizeof(pres_ev_t));
    event.name.s = "conference";
    event.name.len = 10;

    event.content_type.s = "application/conference-info+xml";
    event.content_type.len = 31;

    event.default_expires= 3600;
    event.type = PUBL_TYPE;
    event.req_auth = 0;
    event.evs_publ_handl = 0;

    /* aggregate XML body and free() fuction */
    event.agg_nbody = conf_agg_nbody;
    event.free_body = free_xml_body;

    /* modify XML body for each watcher to set the correct "version" */
    event.aux_body_processing = conf_body_setversion;

	
    if (pres_add_event(&event) < 0) {
		LM_ERR("failed to add event \"conference\"\n");
		return -1;
    }
	
    return 0;
}

