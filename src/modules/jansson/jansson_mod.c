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

#include "../../core/mod_fix.h"
#include "../../core/sr_module.h"
#include "../../core/kemi.h"

#include "jansson_funcs.h"
#include "jansson_utils.h"

MODULE_VERSION

/* module functions */
static int mod_init(void);
static int fixup_get_params(void** param, int param_no);
static int fixup_get_params_free(void** param, int param_no);
static int fixup_pv_get_params(void** param, int param_no);
static int fixup_pv_get_params_free(void** param, int param_no);
static int fixup_set_params(void** param, int param_no);
static int fixup_set_params_free(void** param, int param_no);
static int fixup_xencode(void** param, int param_no);
static int fixup_xencode_free(void** param, int param_no);


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
	{"jansson_pv_get", (cmd_function)janssonmod_pv_get, 3,
		fixup_pv_get_params, fixup_pv_get_params_free, ANY_ROUTE},
	{"jansson_array_size", (cmd_function)janssonmod_array_size, 3,
		fixup_get_params, fixup_get_params_free, ANY_ROUTE},
	{"jansson_set", (cmd_function)janssonmod_set_replace, 4,
		fixup_set_params, fixup_set_params_free, ANY_ROUTE},
	{"jansson_append", (cmd_function)janssonmod_set_append, 4,
		fixup_set_params, fixup_set_params_free, ANY_ROUTE},
	{"jansson_xdecode", (cmd_function)jansson_xdecode, 2,
		fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{"jansson_xencode", (cmd_function)jansson_xencode, 2,
		fixup_xencode, fixup_xencode_free, ANY_ROUTE},
	/* for backwards compatibility */
	{"jansson_get_field", (cmd_function)janssonmod_get_field, 3,
		fixup_get_params, fixup_get_params_free, ANY_ROUTE},
	/* non-script functions */
	{"jansson_to_val", (cmd_function)jansson_to_val, 0, 0, 0, 0},

	{0, 0, 0, 0, 0, 0}
};

struct module_exports exports = {
	"jansson",       /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	0,               /* param exports */
	0,               /* RPC method exports */
	0,               /* pseudo-variables exports */
	0,               /* response handling function */
	mod_init,        /* module init function */
	0,               /* per-child init function */
	0                /* module destroy function */
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

static int fixup_pv_get_params(void** param, int param_no)
{
	if (param_no == 1) {
		return fixup_spve_null(param, 1);
	}

	if (param_no == 2 || param_no == 3) {
		return fixup_pvar_null(param, 1);
	}

	ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

static int fixup_pv_get_params_free(void** param, int param_no)
{
	if (param_no == 1) {
		return fixup_free_spve_null(param, 1);
	}

	if (param_no == 2 || param_no == 3) {
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

static int fixup_xencode(void** param, int param_no)
{
	if (param_no == 1) {
		return fixup_spve_null(param, 1);
	}

	if (param_no == 2) {
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

static int fixup_xencode_free(void** param, int param_no)
{
	if (param_no == 1) {
		fixup_free_spve_null(param, 1);
		return 0;
	}

	if (param_no == 2) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/* just used for unit testing */
static int mod_init(void) {
	return 0;
}

/**
 *
 */
static int ki_jansson_get(sip_msg_t *msg, str *spath, str *sdoc, str *spv)
{
	pv_spec_t *pvs = NULL;

	pvs = pv_cache_get(spv);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%.*s]\n", spv->len, spv->s);
		return -1;
	}

	if(pvs->setf==NULL) {
		LM_ERR("read only output var [%.*s]\n", spv->len, spv->s);
		return -1;
	}

	return janssonmod_get_helper(msg, spath, sdoc, pvs);
}

/**
 *
 */
static sr_kemi_t sr_kemi_jansson_exports[] = {
	{ str_init("jansson"), str_init("get"),
		SR_KEMIP_INT, ki_jansson_get,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{{0, 0}, {0, 0}, 0, NULL, {0, 0, 0, 0, 0, 0}}
};

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_jansson_exports);
	return 0;
}
