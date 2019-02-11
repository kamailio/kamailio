/*
 * PUA_JSON module interface
 *
 * Copyright (C) 2018 VoIPxSWITCH
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 */

#include <stdio.h>
#include <string.h>

#include "../json/api.h"
#include "../presence/bind_presence.h"
#include "pua_json_mod.h"

MODULE_VERSION

static param_export_t params[] = {
		{"pua_include_entity", INT_PARAM, &pua_include_entity},
		{0, 0, 0}};

static cmd_export_t cmds[] = {
		{"pua_json_publish", (cmd_function)pua_json_publish, 1, 0, 0,
				ANY_ROUTE},
		{0, 0, 0, 0, 0, 0}};

struct module_exports exports = {
		"pua_json",			/* module name */
		DEFAULT_DLFLAGS,	/* dlopen flags */
		cmds,				/* exported functions */
		params,				/* exported parameters */
		0,					/* RPC method exports */
		0,					/* exported pseudo-variables */
		0,					/* response handling function */
		mod_init,			/* module initialization function */
		0,					/* per-child init function */
		0					/* module destroy function */
};

static int mod_init(void)
{
	if(json_load_api(&json_api) < 0) {
		LM_ERR("cannot bind to JSON API\n");
		return -1;
	}
	if(presence_load_api(&presence_api) < 0) {
		LM_ERR("cannot bind to PRESENCE API\n");
		return -1;
	}

	return 0;
}
