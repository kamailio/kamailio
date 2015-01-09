/**
 * Copyright (C) 2011 Flowroute LLC (flowroute.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <string.h>

#include "../../mod_fix.h"
#include "../../sr_module.h"

#include "jansson_funcs.h"
#include "jansson_utils.h"

MODULE_VERSION

/* module functions */
static int mod_init(void);
static int fixup_get_params(void** param, int param_no);
static int fixup_get_params_free(void** param, int param_no);
static int fixup_set_params(void** param, int param_no);
static int fixup_set_params_free(void** param, int param_no);


int janssonmod_set_replace(struct sip_msg* msg, char* type_in, char* path_in,
		char* value_in, char* result){
	return janssonmod_set(0, msg, type_in, path_in, value_in, result);
}

int janssonmod_set_append(struct sip_msg* msg, char* type_in, char* path_in,
		char* value_in, char* result) {
	return janssonmod_set(1, msg, type_in, path_in, value_in, result);
}
int janssonmod_get_field(struct sip_msg* msg, char* jansson_in, char* path_in,
		char* result) {
	return janssonmod_get(msg, path_in, jansson_in, result);
}

/* Exported functions */
static cmd_export_t cmds[]={
	{"jansson_get", (cmd_function)janssonmod_get, 3,
		fixup_get_params, fixup_get_params_free, ANY_ROUTE},
	{"jansson_array_size", (cmd_function)janssonmod_array_size, 3,
		fixup_get_params, fixup_get_params_free, ANY_ROUTE},
	{"jansson_set", (cmd_function)janssonmod_set_replace, 4,
		fixup_set_params, fixup_set_params_free, ANY_ROUTE},
	{"jansson_append", (cmd_function)janssonmod_set_append, 4,
		fixup_set_params, fixup_set_params_free, ANY_ROUTE},
	/* for backwards compatibility */
	{"jansson_get_field", (cmd_function)janssonmod_get_field, 3,
		fixup_get_params, fixup_get_params_free, ANY_ROUTE},
	/* non-script functions */
	{"jansson_to_val", (cmd_function)jansson_to_val, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

struct module_exports exports = {
		"jansson",
		DEFAULT_DLFLAGS,	/* dlopen flags */
		cmds,				/* Exported functions */
		0,					/* Exported parameters */
		0,					/* exported statistics */
		0,					/* exported MI functions */
		0,					/* exported pseudo-variables */
		0,					/* extra processes */
		mod_init,			/* module initialization function */
		0,					/* response function*/
		0,					/* destroy function */
		0					/* per-child init function */
};


static int fixup_get_params(void** param, int param_no)
{
	if (param_no <= 2) {
		return fixup_spve_null(param, 1);
	}

	if (param_no == 3) {
		return fixup_pvar_null(param, 1);
	}

	ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

static int fixup_get_params_free(void** param, int param_no)
{
	if (param_no <= 2) {
		return fixup_free_spve_null(param, 1);
	}

	if (param_no == 3) {
		return fixup_free_pvar_null(param, 1);
	}

	ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

static int fixup_set_params(void** param, int param_no)
{
	if(param_no <= 3) {
		return fixup_spve_null(param, 1);
	}

	if (param_no == 4) {
		return fixup_pvar_null(param, 1);
	}

	ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

static int fixup_set_params_free(void** param, int param_no)
{
	if (param_no <= 3) {
		return fixup_free_spve_null(param, 1);
	}

	if (param_no == 4) {
		return fixup_free_pvar_null(param, 1);
	}

	ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/* just used for unit testing */
static int mod_init(void) {
	return 0;
}
