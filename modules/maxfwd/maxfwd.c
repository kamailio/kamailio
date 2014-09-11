/*
 * $Id$
 *
 * MAXFWD module
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *  2003-03-11  updated to the new module interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-19  all mallocs/frees replaced w/ pkg_malloc/pkg_free (andrei)
 *  2004-08-15  max value of max-fwd header is configurable via max_limit
 *              module param (bogdan)
 *  2005-09-15  max_limit param cannot be disabled anymore (according to RFC)
 *              (bogdan)
 *  2005-11-03  is_maxfwd_lt() function added; MF value saved in 
 *              msg->maxforwards->parsed (bogdan)
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../cfg/cfg.h"
#include "mf_funcs.h"
#include "api.h"

MODULE_VERSION

struct cfg_group_maxfwd {
	int max_limit;
};

static struct cfg_group_maxfwd default_maxfwd_cfg = {
	.max_limit=70
};

static void *maxfwd_cfg = &default_maxfwd_cfg;

static cfg_def_t maxfwd_cfg_def[] = {
        {"max_limit", CFG_VAR_INT, 0, 255, 0, 0, "Max. maxfwd limit"},
        {0, 0, 0, 0, 0, 0}
};

static int w_process_maxfwd_header(struct sip_msg* msg,char* str,char* str2);
static int is_maxfwd_lt(struct sip_msg *msg, char *slimit, char *foo);
static int mod_init(void);

int bind_maxfwd(maxfwd_api_t *api);

static cmd_export_t cmds[]={
	{"maxfwd_process", (cmd_function)w_process_maxfwd_header, 1,
		fixup_var_int_1, 0, REQUEST_ROUTE},
	{"mf_process_maxfwd_header", (cmd_function)w_process_maxfwd_header, 1,
		fixup_var_int_1, 0, REQUEST_ROUTE},
	{"process_maxfwd", (cmd_function)w_process_maxfwd_header, 1,
		fixup_var_int_1, 0, REQUEST_ROUTE},

	{"is_maxfwd_lt", (cmd_function)is_maxfwd_lt, 1,
		fixup_var_int_1, 0, REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"maxfwd_at_least", (cmd_function)is_maxfwd_lt, 1,
		fixup_var_int_1, 0, REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"mf_lowlimit", (cmd_function)is_maxfwd_lt, 1,
		fixup_var_int_1, 0, REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},

	{"bind_maxfwd",  (cmd_function)bind_maxfwd,  0,
		0, 0, 0},
	{0,0,0,0,0,0}
};

static param_export_t params[]={
	{"max_limit",    INT_PARAM,  &default_maxfwd_cfg.max_limit},
	{0,0,0}
};



struct module_exports exports= {
	"maxfwd",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,
	0,
	0,
	0           /* per-child init function */
};



static int mod_init(void)
{
	if (cfg_declare("maxfwd", maxfwd_cfg_def, &default_maxfwd_cfg,
				cfg_sizeof(maxfwd), &maxfwd_cfg)) {
		LM_ERR("failed to declare the configuration\n");
		return E_CFG;
	}
	return 0;
}




/**
 * process max forward header
 */
int process_maxfwd_header(struct sip_msg *msg, int limit)
{
	int val;
	str mf_value = {0};
	int max_limit;

	if(limit<0 || limit>255) {
		LM_ERR("invalid param value: %d\n", limit);
		return -1;
	}
	max_limit = cfg_get(maxfwd, maxfwd_cfg, max_limit);

	val=is_maxfwd_present(msg, &mf_value);
	switch (val) {
		/* header not found */
		case -1:
			if (add_maxfwd_header(msg, (unsigned int)limit)!=0)
				goto error;
			return 2;
		/* error */
		case -2:
			goto error;
		/* found */
		case 0:
			return -1;
		default:
			if (val>max_limit){
				LM_DBG("value %d decreased to %d\n", val, max_limit);
				val = max_limit+1;
			}
			if ( decrement_maxfwd(msg, val, &mf_value)!=0 ) {
				LM_ERR("decrement failed!\n");
				goto error;
			}
	}

	return 1;
error:
	return -2;
}

/**
 *
 */
static int w_process_maxfwd_header(struct sip_msg* msg, char* str1, char* str2)
{
	int mfval;
	if (get_int_fparam(&mfval, msg, (fparam_t*) str1) < 0) {
		LM_ERR("could not get param value\n");
		return -1;
	}
	return process_maxfwd_header(msg, mfval);
}


/**
 *
 */
static int is_maxfwd_lt(struct sip_msg *msg, char *slimit, char *foo)
{
	str mf_value;
	int limit;
	int val;

	limit = (int)(long)slimit;
	if (get_int_fparam(&limit, msg, (fparam_t*) slimit) < 0) {
		LM_ERR("could not get param value\n");
		return -1;
	}
	if(limit<0 || limit>255) {
		LM_ERR("invalid param value: %d\n", limit);
		return -1;
	}
	val = is_maxfwd_present( msg, &mf_value);
	LM_DBG("value = %d \n",val);

	if ( val<0 ) {
		/* error or not found */
		return val-1;
	} else if ( val>=limit ) {
		/* greater or equal than/to limit */
		return -1;
	}

	return 1;
}

/**
 * @brief bind functions to MAXFWD API structure
 */
int bind_maxfwd(maxfwd_api_t *api)
{
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	api->process_maxfwd = process_maxfwd_header;

	return 0;
}
