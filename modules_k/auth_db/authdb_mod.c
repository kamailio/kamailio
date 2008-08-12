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
#include "../../db/db.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../mod_fix.h"
#include "../../mem/mem.h"
#include "../auth/api.h"
#include "../sl/sl_api.h"
#include "aaa_avps.h"
#include "authorize.h"
#include "authdb_mod.h"

MODULE_VERSION


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

/** SL binds */
struct sl_binds slb;

#define DEFAULT_CRED_LIST "rpid"

/*
 * Module parameter variables
 */
str auth_db_db_url          = {DEFAULT_RODB_URL, DEFAULT_RODB_URL_LEN};

int calc_ha1                = 0;
int use_domain              = 0; /* Use also domain when looking up in table */

auth_api_t auth_api;

char *credentials_list      = DEFAULT_CRED_LIST;
struct aaa_avp *credentials = 0; /* Parsed list of credentials to load */
int credentials_n           = 0; /* Number of credentials in the list */

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"www_authorize",   (cmd_function)www_authorize,   2, auth_fixup, 0, REQUEST_ROUTE},
	{"proxy_authorize", (cmd_function)proxy_authorize, 2, auth_fixup, 0, REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"auth_db_db_url",          STR_PARAM, &auth_db_db_url.s         },
	{"subscriber_username_col", STR_PARAM, &subscriber_username_col.s},
	{"subscriber_domain_col",   STR_PARAM, &subscriber_domain_col.s  },
	{"password_column",         STR_PARAM, &subscriber_ha1_col.s     },
	{"password_column_2",       STR_PARAM, &subscriber_ha1b_col.s    },
	{"calculate_ha1",           INT_PARAM, &calc_ha1                 },
	{"use_domain",              INT_PARAM, &use_domain               },
	{"load_credentials",        STR_PARAM, &credentials_list         },
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
	return auth_db_db_open();
}


static int mod_init(void)
{
	bind_auth_t bind_auth;

	auth_db_db_url.len = strlen(auth_db_db_url.s);
	auth_db_db_vars();

	if (auth_db_db_init() < 0){
		LM_ERR("unable to init the database module\n");
		return -1;
	}

	/* bind to auth module and import the API */
	bind_auth = (bind_auth_t)find_export("bind_auth", 0, 0);
	if (!bind_auth) {
		LM_ERR("unable to find bind_auth function. Check if you load the auth module.\n");
		return -2;
	}

	if (bind_auth(&auth_api) < 0) {
		LM_ERR("unable to bind auth module\n");
		return -3;
	}

	/* load the SL API */
	if (load_sl_api(&slb)!=0) {
		LM_ERR("can't load SL API\n");
		return -1;
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
	auth_db_db_close();

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
	db_con_t* dbh = NULL;
	str name;

	if (param_no == 1) {
		return fixup_spve_null(param, 1);
	} else if (param_no == 2) {
		name.s = (char*)*param;
		name.len = strlen(name.s);

		dbh = auth_db_dbf.init(&auth_db_db_url);
		if (!dbh) {
			LM_ERR("unable to open database connection\n");
			return -1;
		}
	}
	auth_db_dbf.close(dbh);
	return 0;
}
