/* 
 * $Id$ 
 *
 * Digest Authentication - Radius support
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 * 2003-03-09: Based on auth_mod.c from radius_auth (janakj)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <radiusclient.h>
#include "../../sr_module.h"
#include "../../error.h"
#include "../../dprint.h"
#include "authrad_mod.h"
#include "authorize.h"

pre_auth_f pre_auth_func = 0;   /* Pre authorization function from auth module */
post_auth_f post_auth_func = 0; /* Post authorization function from auth module */

static int mod_init(void);                        /* Module initialization function */
static int str_fixup(void** param, int param_no); /* char* -> str* */


/*
 * Module parameter variables
 */
char* radius_config = "/usr/local/etc/radiusclient/radiusclient.conf";


/*
 * Module interface
 */
struct module_exports exports = {
	"auth_radius", 
	(char*[]) { 
		"radius_www_authorize",
		"radius_proxy_authorize"
	},
	(cmd_function[]) {
		radius_www_authorize,
		radius_proxy_authorize,
	},
	(int[]) {1, 1},
	(fixup_function[]) {
		str_fixup, str_fixup
	},
	2,
	(char*[]) {
		"radius_config"       /* Radius client config file */
	},                            /* Module parameter names */
	(modparam_t[]) {
		STR_PARAM
	},                            /* Module parameter types */
	(void*[]) {
		&radius_config
	},          /* Module parameter variable pointers */
	1,          /* Number of module paramers */
	mod_init,   /* module initialization function */
	0,          /* response function */
	0,          /* destroy function */
	0,          /* oncancel function */
	0           /* child initialization function */
};


/*
 * Module initialization function
 */
static int mod_init(void)
{
	DBG("auth_radius - Initializing\n");

	if (rc_read_config(radius_config) != 0) {
		LOG(L_ERR, "auth_radius: Error opening configuration file \n");
		return -1;
	}
    
	if (rc_read_dictionary(rc_conf_str("dictionary")) != 0) {
		LOG(L_ERR, "auth_radius: Error opening dictionary file \n");
		return -2;
	}

	pre_auth_func = (pre_auth_f)find_export("~pre_auth", 0);
	post_auth_func = (post_auth_f)find_export("~post_auth", 0);

	if (!(pre_auth_func && post_auth_func)) {
		LOG(L_ERR, "auth_radius: This module requires auth module\n");
		return -3;
	}

	return 0;
}


/*
 * Convert char* parameter to str* parameter
 */
static int str_fixup(void** param, int param_no)
{
	str* s;

	if (param_no == 1) {
		s = (str*)malloc(sizeof(str));
		if (!s) {
			LOG(L_ERR, "auth_radius: str_fixup(): No memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}

	return 0;
}
