/*
 * presence_reginfo module - Presence Handling of reg events
 *
 * Copyright (C) 2011 Carsten Bock, carsten@ng-voice.com
 * http://www.ng-voice.com
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
#include "presence_reginfo.h"

MODULE_VERSION

/* module functions */
static int mod_init(void);

/* module variables */
add_event_t pres_add_event;

/* module parameters */
int pres_reginfo_aggregate_presentities = 0;
unsigned int pres_reginfo_default_expires = 3600;

/* module exported parameters */
static param_export_t params[] = {
	{ "default_expires", INT_PARAM, &pres_reginfo_default_expires },
	{ "aggregate_presentities", INT_PARAM, &pres_reginfo_aggregate_presentities },
	{0, 0, 0}
};

/* module exports */
/* clang-format off */
struct module_exports exports= {
    "presence_reginfo",	/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	0,					/* exported functions */
	params,				/* exported parameters */
	0,				    /* RPC method exports */
	0,					/* exported pseudo-variables */
	0,					/* response handling function */
	mod_init,			/* module initialization function */
	0,					/* per-child init function */
	0					/* module destroy function */
};
/* clang-format on */

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
		LM_ERR("can't bind presence\n");
		return -1;
	}

	if (pres_reginfo_aggregate_presentities != 0
			&& pres_reginfo_aggregate_presentities != 1) {
		LM_ERR("invalid aggregate_presentities param value, should be 0 or 1\n");
		return -1;
	}

	pres_add_event = pres.add_event;
	if (pres_add_event == NULL) {
		LM_ERR("could not import add_event\n");
		return -1;
	}
	if(reginfo_add_events() < 0) {
		LM_ERR("failed to add reginfo-info events\n");
		return -1;
	}

	return 0;
}
