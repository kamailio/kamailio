/* 
 * $Id$ 
 *
 * Digest Authentication Module
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
 * --------
 * 2003-02-26: checks and group moved to separate modules (janakj)
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
 */

#include <stdio.h>
#include <string.h>
#include "../../sr_module.h"
#include "../../db/db.h"
#include "../../dprint.h"
#include "../../error.h"
#include "authorize.h"
#include "../auth/api.h"


/*
 * Module destroy function prototype
 */
static void destroy(void);


/*
 * Module child-init function prototype
 */
static int child_init(int rank);


/*
 * Module initialization function prototype
 */
static int mod_init(void);


static int str_fixup(void** param, int param_no);


pre_auth_f pre_auth_func = 0;
post_auth_f post_auth_func = 0;

/*
 * Pointer to reply function in stateless module
 */
int (*sl_reply)(struct sip_msg* _msg, char* _str1, char* _str2);

/*
 * Module parameter variables
 */
char* db_url           = "sql://serro:47serro11@localhost/ser";
char* username_column  = "username";
char* domain_column    = "domain";
char* pass_column      = "ha1";
char* pass_column_2    = "ha1b";
int   calc_ha1         = 0;

db_con_t* db_handle;   /* Database connection handle */


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"www_authorize",   www_authorize,   2, str_fixup, REQUEST_ROUTE},
	{"proxy_authorize", proxy_authorize, 2, str_fixup, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url", STR_PARAM, &db_url},
	{"username_column",   STR_PARAM, &username_column},
	{"domain_column",     STR_PARAM, &domain_column  },
	{"password_column",   STR_PARAM, &pass_column    },
	{"password_column_2", STR_PARAM, &pass_column_2  },
	{"calculate_ha1",     INT_PARAM, &calc_ha1       },
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"auth_db", 
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	mod_init,   /* module initialization function */
	0,          /* response function */
	destroy,    /* destroy function */
	0,          /* oncancel function */
	child_init  /* child initialization function */
};


static int child_init(int rank)
{
	if (db_url == 0) {
		LOG(L_ERR, "auth:init_child(): Use db_url parameter\n");
		return -1;
	}
	db_handle = db_init(db_url);
	if (!db_handle) {
		LOG(L_ERR, "auth:init_child(): Unable to connect database\n");
		return -1;
	}
	return 0;

}


static int mod_init(void)
{
	printf("auth module - initializing\n");
	
	     /* Find a database module */
	if (bind_dbmod()) {
		LOG(L_ERR, "mod_init(): Unable to bind database module\n");
		return -1;
	}

	pre_auth_func = (pre_auth_f)find_export("~pre_auth", 0);
	post_auth_func = (post_auth_f)find_export("~post_auth", 0);

	if (!(pre_auth_func && post_auth_func)) {
		LOG(L_ERR, "auth_db:mod_init(): This module requires auth module\n");
		return -2;
	}

	sl_reply = find_export("sl_send_reply", 2);
	if (!sl_reply) {
		LOG(L_ERR, "auth_db:mod_init(): This module requires sl module\n");
		return -2;
	}

	return 0;
}



static void destroy(void)
{
	db_close(db_handle);
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
			LOG(L_ERR, "str_fixup(): No memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}

	return 0;
}
