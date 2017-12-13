/*
 * presence_dialoginfo module - Presence Handling of dialog events
 *
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/str.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/mem/mem.h"
#include "../presence/bind_presence.h"
#include "add_events.h"
#include "presence_dialoginfo.h"

MODULE_VERSION

/* module functions */
static int mod_init(void);

/* module variables */
add_event_t pres_add_event;

/* module parameters */
int force_single_dialog = 0;
int force_dummy_dialog = 0;

/* module exported commands */
static cmd_export_t cmds[] =
{
	{0,	0, 0, 0, 0, 0}
};

/* module exported paramaters */
static param_export_t params[] = {
	{ "force_single_dialog", INT_PARAM, &force_single_dialog },
	{ "force_dummy_dialog", INT_PARAM, &force_dummy_dialog },
	{0, 0, 0}
};

/* module exports */
struct module_exports exports= {
	"presence_dialoginfo",		/* module name */
	DEFAULT_DLFLAGS,			/* dlopen flags */
	cmds,						/* exported functions */
	params,						/* exported parameters */
	0,							/* exported statistics */
	0,							/* exported MI functions */
	0,							/* exported pseudo-variables */
	0,							/* extra processes */
	mod_init,					/* module initialization function */
	0,							/* response handling function */
	0,							/* destroy function */
	0							/* per-child init function */
};

/*
 * init module function
 */
static int mod_init(void)
{
	presence_api_t pres;
	bind_presence_t bind_presence;

	bind_presence= (bind_presence_t)find_export("bind_presence", 1,0);
	if (!bind_presence) {
		LM_ERR("can't bind presence\n");
		return -1;
	}
	if (bind_presence(&pres) < 0) {
		LM_ERR("can't bind pua\n");
		return -1;
	}

	pres_add_event = pres.add_event;
	if (pres_add_event == NULL) {
		LM_ERR("could not import add_event\n");
		return -1;
	}
	if(dlginfo_add_events() < 0) {
		LM_ERR("failed to add dialog-info events\n");
		return -1;
	}

	return 0;
}
