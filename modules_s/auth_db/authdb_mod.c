/* 
 * $Id$
 *
 * Digest Authentication Module
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2003-02-26: checks and group moved to separate modules (janakj)
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
 * 2003-03-19  all mallocs/frees replaced w/ pkg_malloc/pkg_free (andrei)
 * 2003-04-05: default_uri #define used (jiri)
 * 2004-06-06  cleanup: static & auth_db_{init,bind,close.ver} used (andrei)
 */

#include <stdio.h>
#include <string.h>
#include "../../sr_module.h"
#include "../../db/db.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "authorize.h"
#include "../auth/aaa_avps.h"
#include "../auth/api.h"

MODULE_VERSION

#define TABLE_VERSION 3

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


/*
 * Pointer to reply function in stateless module
 */
int (*sl_reply)(struct sip_msg* _msg, char* _str1, char* _str2);


#define USER_COL "username"
#define USER_COL_LEN (sizeof(USER_COL) - 1)

#define DOMAIN_COL "domain"
#define DOMAIN_COL_LEN (sizeof(DOMAIN_COL) - 1)

#define PASS_COL "ha1"
#define PASS_COL_LEN (sizeof(PASS_COL) - 1)

#define PASS_COL_2 "ha1b"
#define PASS_COL_2_LEN (sizeof(PASS_COL_2) - 1)

#define DEFAULT_CRED_LIST "rpid"

/*
 * Module parameter variables
 */
static char* db_url         = DEFAULT_RODB_URL;
str user_column             = {USER_COL, USER_COL_LEN};
str domain_column           = {DOMAIN_COL, DOMAIN_COL_LEN};
str pass_column             = {PASS_COL, PASS_COL_LEN};
str pass_column_2           = {PASS_COL_2, PASS_COL_2_LEN};


int calc_ha1                = 0;
int use_domain              = 0;   /* Use also domain when looking up a table row */

db_con_t* auth_db_handle = 0;      /* database connection handle */
db_func_t auth_dbf;
auth_api_t auth_api;

str credentials_list        = {DEFAULT_CRED_LIST, sizeof(DEFAULT_CRED_LIST)};
str* credentials;          /* Parsed list of credentials to load */
int credentials_n;         /* Number of credentials in the list */

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
	{"db_url",            STR_PARAM, &db_url             },
	{"user_column",       STR_PARAM, &user_column.s      },
	{"domain_column",     STR_PARAM, &domain_column.s    },
	{"password_column",   STR_PARAM, &pass_column.s      },
	{"password_column_2", STR_PARAM, &pass_column_2.s    },
	{"calculate_ha1",     INT_PARAM, &calc_ha1           },
	{"use_domain",        INT_PARAM, &use_domain         },
	{"load_credentials",  STR_PARAM, &credentials_list.s },
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
	auth_db_handle = auth_dbf.init(db_url);
	if (auth_db_handle == 0){
		LOG(L_ERR, "auth_db:child_init: unable to connect to the database\n");
		return -1;
	}

	return 0;
}


static int mod_init(void)
{
	bind_auth_t bind_auth;

	DBG("auth_db module - initializing\n");

	user_column.len = strlen(user_column.s);
	domain_column.len = strlen(domain_column.s);
	pass_column.len = strlen(pass_column.s);
	pass_column_2.len = strlen(pass_column.s);

	credentials_list.len = strlen(credentials_list.s);

	     /* Find a database module */

	if (bind_dbmod(db_url, &auth_dbf) < 0){
		LOG(L_ERR, "auth_db:child_init: Unable to bind a database driver\n");
		return -2;
	}

        bind_auth = (bind_auth_t)find_export("bind_auth", 0, 0);
        if (!bind_auth) {
		LOG(L_ERR, "auth_db:mod_init: Unable to find bind_auth function\n");
	        return -1;
	}

	if (bind_auth(&auth_api) < 0) {
		LOG(L_ERR, "auth_db:child_init: Unable to bind auth module\n");
		return -3;
	}

	sl_reply = find_export("sl_send_reply", 2, 0);
	if (!sl_reply) {
		LOG(L_ERR, "auth_db:mod_init(): This module requires sl module\n");
		return -4;
	}

	if (aaa_avps_init(&credentials_list, &credentials, &credentials_n)) {
		return -1;
	}

	return 0;
}


static void destroy(void)
{
	if (auth_db_handle) {
		auth_dbf.close(auth_db_handle);
	}
}


/*
 * Convert char* parameter to str* parameter
 */
static int str_fixup(void** param, int param_no)
{
	db_con_t* dbh;
	str* s;
	int ver;
	str name;

	if (param_no == 1) {
		s = (str*)pkg_malloc(sizeof(str));
		if (!s) {
			LOG(L_ERR, "str_fixup(): No memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	} else if (param_no == 2) {
		name.s = (char*)*param;
		name.len = strlen(name.s);

		dbh = auth_dbf.init(db_url);
		if (!dbh) {
			LOG(L_ERR, "auth_db:str_fixup: Unable to open database connection\n");
			return -1;
		}
		ver = table_version(&auth_dbf, dbh, &name);
		auth_dbf.close(dbh);
		if (ver < 0) {
			LOG(L_ERR, "auth_db:str_fixup: Error while querying table version\n");
			return -1;
		} else if (ver < TABLE_VERSION) {
			LOG(L_ERR, "auth_db:str_fixup: Invalid table version (use ser_mysql.sh reinstall)\n");
			return -1;
		}
	}

	return 0;
}
