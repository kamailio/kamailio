/**
 *
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <string.h>

#include "../../core/mod_fix.h"
#include "../../core/sr_module.h"

#include "api.h"
#include "json_funcs.h"
#include "json_trans.h"

MODULE_VERSION

static int fixup_get_field(void** param, int param_no);
static int fixup_get_field_free(void** param, int param_no);
str tr_json_escape_str = str_init("%");
char tr_json_escape_char = '%';

/* Exported functions */
static tr_export_t mod_trans[] = {
		{{"json", sizeof("json") - 1}, json_tr_parse}, {{0, 0}, 0}};

static cmd_export_t cmds[] = {
		{"json_get_field", (cmd_function)json_get_field, 3, fixup_get_field,
				fixup_get_field_free, ANY_ROUTE},
		{"json_get_string", (cmd_function)json_get_string, 3, fixup_get_field,
				fixup_get_field_free, ANY_ROUTE},
		{"bind_json", (cmd_function)bind_json, 0, 0, 0, ANY_ROUTE},
		{0, 0, 0, 0, 0, 0}};

static param_export_t params[] = {
		{"json_escape_char", PARAM_STR, &tr_json_escape_str}, {0, 0, 0}};

struct module_exports exports = {
	"json",          /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	0,               /* pseudo-variables exports */
	0,               /* response handling function */
	0,               /* module init function */
	0,               /* per-child init function */
	0                /* module destroy function */
};

int _json_extract_field(struct json_object *json_obj, char *json_name, str *val) {
	json_extract_field(json_name, val);
	return 0;
}

/**
 *
 */
int bind_json(json_api_t *api) {
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	api->json_parse = json_parse;
	api->get_object = json_get_object;
	api->extract_field = _json_extract_field;
	return 0;
}

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if(json_tr_init_buffers() < 0) {
		LM_ERR("failed to initialize transformations buffers\n");
		return -1;
	}
	return register_trans_mod(path, mod_trans);
}

static int fixup_get_field(void** param, int param_no)
{
  if (param_no == 1 || param_no == 2) {
		return fixup_spve_null(param, 1);
	}

	if (param_no == 3) {
		if (fixup_pvar_null(param, 1) != 0) {
		    LM_ERR("failed to fixup result pvar\n");
		    return -1;
		}
		if (((pv_spec_t *)(*param))->setf == NULL) {
		    LM_ERR("result pvar is not writeble\n");
		    return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

static int fixup_get_field_free(void** param, int param_no)
{
	if (param_no == 1 || param_no == 2) {
		fixup_free_spve_null(param, 1);
		return 0;
	}

	if (param_no == 3) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}
