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
#include "../auth/api.h"
#include "aaa_avps.h"

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


pre_auth_f pre_auth_func = 0;
post_auth_f post_auth_func = 0;

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

#define AVPS_COL_INT "domn"
#define AVPS_COL_INT_LEN (sizeof(AVPS_COL_INT) - 1)

#define AVPS_COL_STR "uuid|rpid"
#define AVPS_COL_STR_LEN (sizeof(AVPS_COL_STR) - 1)


/*
 * Module parameter variables
 */
static str db_url           = {DEFAULT_RODB_URL, DEFAULT_RODB_URL_LEN};
str user_column      = {USER_COL, USER_COL_LEN};
str domain_column    = {DOMAIN_COL, DOMAIN_COL_LEN};
str pass_column      = {PASS_COL, PASS_COL_LEN};
str pass_column_2    = {PASS_COL_2, PASS_COL_2_LEN};
static str avps_column_int  = {AVPS_COL_INT, AVPS_COL_INT_LEN};
static str avps_column_str  = {AVPS_COL_STR, AVPS_COL_STR_LEN};
str *avps_int        = NULL;
str *avps_str        = NULL;
int avps_int_n       = 0;
int avps_str_n       = 0;
int calc_ha1         = 0;
int use_domain       = 1;    /* Use also domain when looking up a table row */



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
	{"db_url",            STR_PARAM, &db_url.s         },
	{"user_column",       STR_PARAM, &user_column.s    },
	{"domain_column",     STR_PARAM, &domain_column.s  },
	{"password_column",   STR_PARAM, &pass_column.s    },
	{"password_column_2", STR_PARAM, &pass_column_2.s  },
	{"avps_column_int",   STR_PARAM, &avps_column_int.s},
	{"avps_column_str",   STR_PARAM, &avps_column_str.s},
	{"calculate_ha1",     INT_PARAM, &calc_ha1         },
	{"use_domain",        INT_PARAM, &use_domain       },
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
	     /* Close connection opened in mod_init */
	return auth_db_init(db_url.s);
}


static int mod_init(void)
{

	DBG("auth_db module - initializing\n");

	db_url.len = strlen(db_url.s);
	user_column.len = strlen(user_column.s);
	domain_column.len = strlen(domain_column.s);
	pass_column.len = strlen(pass_column.s);
	pass_column_2.len = strlen(pass_column.s);

	if (aaa_avps_init(&avps_column_int, &avps_column_str, &avps_int,
	    &avps_str, &avps_int_n, &avps_str_n) == -1)
		return -1;

	     /* Find a database module */
	if (auth_db_bind(db_url.s)<0)
		return -2;

	pre_auth_func = (pre_auth_f)find_export("pre_auth", 0, 0);
	post_auth_func = (post_auth_f)find_export("post_auth", 0, 0);

	if (!(pre_auth_func && post_auth_func)) {
		LOG(L_ERR, "auth_db:mod_init(): This module requires auth module\n");
		return -3;
	}

	sl_reply = find_export("sl_send_reply", 2, 0);
	if (!sl_reply) {
		LOG(L_ERR, "auth_db:mod_init(): This module requires sl module\n");
		return -4;
	}

	return 0;
}



static void destroy(void)
{
	auth_db_close();
}


/*
 * Convert char* parameter to str* parameter
 */
static int str_fixup(void** param, int param_no)
{
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

		ver=auth_db_ver(db_url.s, &name);
		if (ver < 0) {
			LOG(L_ERR, "auth_db:str_fixup(): Error while querying table version\n");
			return -1;
		} else if (ver < TABLE_VERSION) {
			LOG(L_ERR, "auth_db:str_fixup(): Invalid table version (use ser_mysql.sh reinstall)\n");
			return -1;
		}
	}

	return 0;
}
