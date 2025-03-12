/*
 * presence_dfks module
 *
 * Presence Handling of "as-feature" events (handling x-as-feature-event+xml doc)
 *
 * Copyright (C) 2014 Maja Stanislawska <maja.stanislawska@yahoo.com>
 * Copyright (C) 2024 Victor Seva <vseva@sipwise.com>
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
#include "presence_dfks.h"

MODULE_VERSION

/* module functions */
static int mod_init(void);

/** API structures */
add_event_t pres_add_event;
sl_api_t slb;
presence_api_t pres;
pua_api_t pua;
libxml_api_t libxml_api;

/* clang-format off */
/* module exports */
struct module_exports exports = {
  "presence_dfks", /* module name */
  DEFAULT_DLFLAGS, /* dlopen flags */
  0,               /* exported functions */
  0,               /* exported parameters */
  0,               /* RPC method exports */
  0,               /* exported pseudo-variables */
  0,               /* response handling function */
  mod_init,        /* module initialization function */
  0,               /* per-child init function */
  0                /* module destroy function */
};
/* clang-format on */

/*
 * init module function
 */
static int mod_init(void)
{
	bind_presence_t bind_presence;
	bind_pua_t bind_pua;
	bind_libxml_t bind_libxml;

	/* bind the SL API */
	if(sl_load_api(&slb) != 0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	bind_presence = (bind_presence_t)find_export("bind_presence", 1, 0);
	if(!bind_presence) {
		LM_ERR("can't bind presence\n");
		return -1;
	}
	if(bind_presence(&pres) < 0) {
		LM_ERR("can't bind presence\n");
		return -1;
	}
	pres_add_event = pres.add_event;
	if(pres_add_event == NULL) {
		LM_ERR("could not import add_event function\n");
		return -1;
	}

	bind_pua = (bind_pua_t)find_export("bind_pua", 1, 0);
	if(!bind_pua) {
		LM_ERR("Can't bind pua\n");
		return -1;
	}
	if(bind_pua(&pua) < 0) {
		LM_ERR("mod_init Can't bind pua\n");
		return -1;
	}

	/* bind libxml wrapper functions */
	if((bind_libxml = (bind_libxml_t)find_export("bind_libxml_api", 1, 0))
			== NULL) {
		LM_ERR("can't import bind_libxml_api\n");
		return -1;
	}
	if(bind_libxml(&libxml_api) < 0) {
		LM_ERR("can not bind libxml api\n");
		return -1;
	}
	if(libxml_api.xmlNodeGetNodeByName == NULL) {
		LM_ERR("can not bind libxml api\n");
		return -1;
	}

	if(dfks_add_events() < 0) {
		LM_ERR("failed to add as-feature-event events\n");
		return -1;
	}

	return 0;
}
