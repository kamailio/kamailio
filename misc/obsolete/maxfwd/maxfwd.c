/*
 * $Id$
 *
 * maxfwd module
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*
 * History:
 * --------
 *  2003-03-11  updated to the new module interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-19  all mallocs/frees replaced w/ pkg_malloc/pkg_free (andrei)
 *  2004-08-15  max value of max-fwd header is configurable via max_limit
 *              module param (bogdan)
 *  2008-02-26  support for cfg API (tma)
 */

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "mf_funcs.h"
#include "../../cfg/cfg.h"

MODULE_VERSION

#define MODULE_NAME "maxfwd"

struct cfg_group_maxfwd {
	int max_limit;
};

static struct cfg_group_maxfwd default_maxfwd_cfg = {
	max_limit:16
};

static void *maxfwd_cfg = &default_maxfwd_cfg;

static cfg_def_t maxfwd_cfg_def[] = {
        {"max_limit", CFG_VAR_INT, 0, 255, 0, 0, "Max. maxfwd limit"},
        {0, 0, 0, 0, 0, 0}
};
				

static int process_maxfwd_header(struct sip_msg* msg, char* str, char* str2);
static int check_lowlimit(struct sip_msg* msg, char* str, char* str2);
static int mod_init(void);

static cmd_export_t cmds[]={
	{MODULE_NAME"_process", process_maxfwd_header, 1, fixup_var_int_1, REQUEST_ROUTE},
	{MODULE_NAME"_at_least", check_lowlimit, 1, fixup_var_int_1, REQUEST_ROUTE},
	/* backward compatability only */
	{"mf_process_maxfwd_header", process_maxfwd_header, 1, fixup_var_int_1, REQUEST_ROUTE},
	{"process_maxfwd", process_maxfwd_header, 1,
		fixup_var_int_1, REQUEST_ROUTE},
	{"mf_lowlimit", check_lowlimit, 1, fixup_var_int_1, REQUEST_ROUTE},
	{0,0,0,0,0}
};

static param_export_t params[]={
	{"max_limit",    PARAM_INT,  &default_maxfwd_cfg.max_limit},
	{0,0,0}
};


#ifdef STATIC_MAXFWD
struct module_exports MODULE_NAME##_exports = {
#else
struct module_exports exports= {
#endif
	MODULE_NAME,
	cmds,
	0,       /* RPC methods */
	params,
	mod_init,
	(response_function) 0,
	(destroy_function) 0,
	0,
	0  /* per-child init function */
};



static int mod_init(void) {
	DBG(MODULE_NAME": initializing\n");
	/* declare the configuration */
	if (cfg_declare(MODULE_NAME, maxfwd_cfg_def, &default_maxfwd_cfg, cfg_sizeof(maxfwd), &maxfwd_cfg)) {
		ERR(MODULE_NAME": mod_init: failed to declare the configuration\n");
		return E_UNSPEC;
	}								 	
	return E_OK;
}

static int process_maxfwd_header(struct sip_msg* msg, char* str1, char* str2) {
	int val, tmp;
	str mf_value;
	int max_limit;

	val = is_maxfwd_present(msg, &mf_value);
	switch (val) {
		case -1:
			if (get_int_fparam(&tmp, msg, (fparam_t*) str1) < 0) return -1;
			if (tmp < 0 || tmp > 255) {
				ERR(MODULE_NAME": number (%d) beyond range <0,255>\n", tmp);
				return -1;
			}
			if (tmp == 0) return 0;
			max_limit = cfg_get(maxfwd, maxfwd_cfg, max_limit);
			if ( max_limit && tmp > max_limit) {
				ERR(MODULE_NAME": default value (%d) greater than max.limit (%d)\n", tmp, max_limit);
				return -1;
			}
			add_maxfwd_header(msg, tmp);
			break;
		case -2:
			break;
		case 0:
			return -1;
		default:
			max_limit = cfg_get(maxfwd, maxfwd_cfg, max_limit);
			if (max_limit && val > max_limit){
				DBG(MODULE_NAME": process_maxfwd_header: "
					"value %d decreased to %d\n", val, max_limit);
				val = max_limit+1;
			}
			if ( decrement_maxfwd(msg, val, &mf_value)!=1 )
				ERR(MODULE_NAME": process_maxfwd_header: "
					"decrement failed\n");
	}
	return 1;
}

/* check if the current Max Forwards value is below/above a certain threshold */
static int check_lowlimit(struct sip_msg* msg, char* str1, char* str2) {
	int val, lowlimit;
	str mf_value;

	val = is_maxfwd_present(msg, &mf_value);
	switch (val) {
		case -2: /* parsing error */
			return -1;
		case -1: /* header not present */
			return 1;
		default:
			if (get_int_fparam(&lowlimit, msg, (fparam_t*) str1) < 0) return -1;
			DBG(MODULE_NAME": check_low_limit(%d): current=%d\n", lowlimit, val);
			return ((val >= 0) && (val < lowlimit))?-1:1;
	}
}
