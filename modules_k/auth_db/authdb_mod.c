/* 
 * $Id$
 *
 * Digest Authentication Module
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
 * 2005-05-31  general definition of AVPs in credentials now accepted - ID AVP,
 *             STRING AVP, AVP aliases (bogdan)
 * 2006-03-01 pseudo variables support for domain name (bogdan)
 */

#include <stdio.h>
#include <string.h>
#include "../../sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../mod_fix.h"
#include "../../mem/mem.h"
#include "../../modules/auth/api.h"
#include "aaa_avps.h"
#include "authorize.h"

MODULE_VERSION

#define TABLE_VERSION 6

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


static int auth_fixup(void** param, int param_no);

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
static str db_url           = {DEFAULT_RODB_URL, DEFAULT_RODB_URL_LEN};
str user_column             = {USER_COL, USER_COL_LEN};
str domain_column           = {DOMAIN_COL, DOMAIN_COL_LEN};
str pass_column             = {PASS_COL, PASS_COL_LEN};
str pass_column_2           = {PASS_COL_2, PASS_COL_2_LEN};


int calc_ha1                = 0;
int use_domain              = 0; /* Use also domain when looking up in table */

db1_con_t* auth_db_handle    = 0; /* database connection handle */
db_func_t auth_dbf;
auth_api_s_t auth_api;

char *credentials_list      = DEFAULT_CRED_LIST;
struct aaa_avp *credentials = 0; /* Parsed list of credentials to load */
int credentials_n           = 0; /* Number of credentials in the list */

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"www_authorize",      (cmd_function)www_authenticate,   2, auth_fixup, 0,
		REQUEST_ROUTE},
	{"www_authenticate",   (cmd_function)www_authenticate,   2, auth_fixup, 0,
		REQUEST_ROUTE},
	{"proxy_authorize",    (cmd_function)proxy_authenticate, 2, auth_fixup, 0,
		REQUEST_ROUTE},
	{"proxy_authenticate", (cmd_function)proxy_authenticate, 2, auth_fixup, 0,
		REQUEST_ROUTE},
	{"bind_auth_db",       (cmd_function)bind_auth_db,       0, 0, 0,
		0},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",            STR_PARAM, &db_url.s           },
	{"user_column",       STR_PARAM, &user_column.s      },
	{"domain_column",     STR_PARAM, &domain_column.s    },
	{"password_column",   STR_PARAM, &pass_column.s      },
	{"password_column_2", STR_PARAM, &pass_column_2.s    },
	{"calculate_ha1",     INT_PARAM, &calc_ha1           },
	{"use_domain",        INT_PARAM, &use_domain         },
	{"load_credentials",  STR_PARAM, &credentials_list   },
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"auth_db", 
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,          /* response function */
	destroy,    /* destroy function */
	child_init  /* child initialization function */
};


static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */
	
	auth_db_handle = auth_dbf.init(&db_url);
	if (auth_db_handle == 0){
		LM_ERR("unable to connect to the database\n");
		return -1;
	}

	return 0;
}


static int mod_init(void)
{
	bind_auth_s_t bind_auth;

	db_url.len = strlen(db_url.s);
	user_column.len = strlen(user_column.s);
	domain_column.len = strlen(domain_column.s);
	pass_column.len = strlen(pass_column.s);
	pass_column_2.len = strlen(pass_column_2.s);

	/* Find a database module */
	if (db_bind_mod(&db_url, &auth_dbf) < 0){
		LM_ERR("unable to bind to a database driver\n");
		return -1;
	}

	/* bind to auth module and import the API */
	bind_auth = (bind_auth_s_t)find_export("bind_auth_s", 0, 0);
	if (!bind_auth) {
		LM_ERR("unable to find bind_auth function. Check if you load"
				" the auth module.\n");
		return -2;
	}

	if (bind_auth(&auth_api) < 0) {
		LM_ERR("unable to bind auth module\n");
		return -3;
	}

	/* process additional list of credentials */
	if (parse_aaa_avps( credentials_list, &credentials, &credentials_n)!=0) {
		LM_ERR("failed to parse credentials\n");
		return -5;
	}

	return 0;
}


static void destroy(void)
{
	if (auth_db_handle) {
		auth_dbf.close(auth_db_handle);
		auth_db_handle = 0;
	}
	if (credentials) {
		free_aaa_avp_list(credentials);
		credentials = 0;
		credentials_n = 0;
	}
}


/*
 * Convert the char* parameters
 */
static int auth_fixup(void** param, int param_no)
{
	db1_con_t* dbh = NULL;
	str name;

	if(strlen((char*)*param)<=0) {
		LM_ERR("empty parameter %d not allowed\n", param_no);
		return -1;
	}

	if (param_no == 1) {
		return fixup_var_str_12(param, 1);
	} else if (param_no == 2) {
		name.s = (char*)*param;
		name.len = strlen(name.s);

		dbh = auth_dbf.init(&db_url);
		if (!dbh) {
			LM_ERR("unable to open database connection\n");
			return -1;
		}
		if(db_check_table_version(&auth_dbf, dbh, &name, TABLE_VERSION) < 0) {
			LM_ERR("error during table version check.\n");
			auth_dbf.close(dbh);
			return -1;
		}
		auth_dbf.close(dbh);
	}
	return 0;
}
