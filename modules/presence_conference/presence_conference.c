/*
 * presence_conference module - Presence Handling of "conference" events (handling conference-info+xml doc)
 *
 * Copyright (C) 2010 Marius Bucur
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
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../parser/msg_parser.h"
#include "../../mem/mem.h"
#include "../presence/bind_presence.h"
#include "add_events.h"
#include "presence_conference.h"

MODULE_VERSION

/* module functions */
static int mod_init(void);

/* module variables */
add_event_t pres_add_event;

/* module parameters */
int use_partial_states = 0;

/* module mi command functions */
static struct mi_root* conference_reset(struct mi_root* cmd, void* param);

/* module exported commands */
static cmd_export_t cmds[] =
{
    {0,	0, 0, 0, 0, 0}
};

/* module exported paramaters */
static param_export_t params[] = {
	{ "use_partial_states", INT_PARAM, &use_partial_states },
	{0, 0, 0}
};

/* module mi commands */
static mi_export_t mi_cmds[] = {
	{ "conference_reset", conference_reset, 0, 0, 0},
	{ 0, 0, 0, 0, 0 }
};

/* presence api bind structure */
presence_api_t pres;

/* module exports */
struct module_exports exports= {
    "presence_conference",		/* module name */
    DEFAULT_DLFLAGS,			/* dlopen flags */
    cmds,				/* exported functions */
    params,				/* exported parameters */
    0,					/* exported statistics */
    mi_cmds,				/* exported MI functions */
    0,					/* exported pseudo-variables */
    0,					/* extra processes */
    mod_init,				/* module initialization function */
    0,					/* response handling function */
    0,					/* destroy function */
    0					/* per-child init function */
};
	
/*
 * init module function
 */
static int mod_init(void)
{
	bind_presence_t bind_presence;

	bind_presence= (bind_presence_t)find_export("bind_presence", 1,0);
	if (!bind_presence) {
		LM_ERR("cannot find bind_presence\n");
		return -1;
	}
	if (bind_presence(&pres) < 0) {
		LM_ERR("cannot bind to presence module\n");
		return -1;
	}

	pres_add_event = pres.add_event;
	if (pres_add_event == NULL) {
		LM_ERR("could not import add_event function\n");
		return -1;
	}
	if(conference_add_events() < 0) {
		LM_ERR("failed to add conference-info events\n");
		return -1;		
	}	
    
    return 0;
}


/* module mi command functions */

//presence_conference reset mi command
static struct mi_root* conference_reset(struct mi_root* cmd, void* param)
{
	LM_ERR("Conference received mi command [reason %*.s] [code %d]...\n", cmd->reason.len, cmd->reason.s, cmd->code);
	//reset subscriptions to the event package for some presentity (conference)
	return cmd;
}
