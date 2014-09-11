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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "../../lib/srdb2/db.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "../../modules/auth/api.h"
#include "authorize.h"
#include "aaa_avps.h"
#include "uid_auth_db_mod.h"

MODULE_VERSION

#define TABLE_VERSION 7

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


static int authdb_fixup(void** param, int param_no);



#define USERNAME_COL "auth_username"
#define DID_COL "did"
#define REALM_COL "realm"
#define PASS_COL "ha1"
#define PASS_COL_2 "ha1b"
#define PLAIN_PASS_COL "password"
#define DEFAULT_CRED_LIST "uid"
#define FLAGS_COL "flags"

/*
 * Module parameter variables
 */
static char* db_url         = DEFAULT_RODB_URL;

str username_column         = STR_STATIC_INIT(USERNAME_COL);
str did_column              = STR_STATIC_INIT(DID_COL);
str realm_column            = STR_STATIC_INIT(REALM_COL);
str pass_column             = STR_STATIC_INIT(PASS_COL);
str pass_column_2           = STR_STATIC_INIT(PASS_COL_2);
str flags_column            = STR_STATIC_INIT(FLAGS_COL);
str plain_password_column   = STR_STATIC_INIT(PLAIN_PASS_COL);

int calc_ha1                = 0;
int use_did                 = 0;
int check_all               = 0;

db_ctx_t* auth_db_handle = 0;      /* database connection handle */
auth_api_s_t auth_api;

str credentials_list        = STR_STATIC_INIT(DEFAULT_CRED_LIST);

str* credentials;          /* Parsed list of credentials to load */
int credentials_n;         /* Number of credentials in the list */


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
    {"www_authenticate",   www_authenticate,    2, authdb_fixup, REQUEST_ROUTE},
    {"www_authorize",      www_authenticate,    2, authdb_fixup, REQUEST_ROUTE},
    {"proxy_authenticate", proxy_authenticate,  2, authdb_fixup, REQUEST_ROUTE},
    {"proxy_authorize",    proxy_authenticate,  2, authdb_fixup, REQUEST_ROUTE},
    {0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
    {"db_url",            PARAM_STRING, &db_url          },
    {"username_column",   PARAM_STR,    &username_column },
    {"did_column",        PARAM_STR,    &did_column      },
    {"realm_column",      PARAM_STR,    &realm_column    },
    {"password_column",   PARAM_STR,    &pass_column     },
    {"password_column_2", PARAM_STR,    &pass_column_2   },
    {"plain_password_column",   PARAM_STR,    &plain_password_column },
    {"flags_column",      PARAM_STR,    &flags_column    },
    {"calculate_ha1",     PARAM_INT,    &calc_ha1        },
    {"load_credentials",  PARAM_STR,    &credentials_list},
    {"use_did",           PARAM_INT,    &use_did         },
    {"check_all_ha1",     PARAM_INT,    &check_all       },
    {0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
    "uid_auth_db",
    cmds,       /* Exported functions */
    0,          /* RPC methods */
    params,     /* Exported parameters */
    mod_init,   /* module initialization function */
    0,          /* response function */
    destroy,    /* destroy function */
    0,          /* oncancel function */
    child_init  /* child initialization function */
};

static authdb_table_info_t *registered_tables = NULL;

static int generate_queries(authdb_table_info_t *info)
{
	db_fld_t match_with_did[] = {
		{ .name = username_column.s, .type = DB_STR }, 
		{ .name = realm_column.s, .type = DB_STR }, 
		{ .name = did_column.s, .type = DB_STR }, 
		{ .name = NULL }
	};
	db_fld_t match_without_did[] = {
		{ .name = username_column.s, .type = DB_STR }, 
		{ .name = realm_column.s, .type = DB_STR }, 
		{ .name = NULL }
	};
	db_fld_t *result_cols = NULL;
	int len, i;

	len = sizeof(*result_cols) * (credentials_n + 3);
	result_cols = pkg_malloc(len);
	if (!result_cols) {
		ERR("can't allocate pkg mem\n");
		return -1;
	}
	memset(result_cols, 0, len);

	result_cols[0].name = pass_column.s;
	result_cols[0].type = DB_CSTR;
	
	result_cols[1].name = flags_column.s;
	result_cols[1].type = DB_INT;
	for (i = 0; i < credentials_n; i++) {
		result_cols[2 + i].name = credentials[i].s;
		result_cols[2 + i].type = DB_STR;
	}
	result_cols[2 + i].name = NULL;

	if (use_did) {
		info->query_pass = db_cmd(DB_GET, auth_db_handle, info->table.s, 
				result_cols, match_with_did, NULL);
		result_cols[0].name = pass_column_2.s;
		info->query_pass2 = db_cmd(DB_GET, auth_db_handle, info->table.s, 
				result_cols, match_with_did, NULL);
		result_cols[0].name = plain_password_column.s;
		info->query_password = db_cmd(DB_GET, auth_db_handle, info->table.s, 
				result_cols, match_with_did, NULL);
	}
	else {
		info->query_pass = db_cmd(DB_GET, auth_db_handle, info->table.s, 
				result_cols, match_without_did, NULL);
		result_cols[0].name = pass_column_2.s;
		info->query_pass2 = db_cmd(DB_GET, auth_db_handle, info->table.s, 
				result_cols, match_without_did, NULL);
		result_cols[0].name = plain_password_column.s;
		info->query_password = db_cmd(DB_GET, auth_db_handle, info->table.s, 
				result_cols, match_without_did, NULL);
	}

	pkg_free(result_cols);
	if (info->query_pass && info->query_pass2 && info->query_password) return 0;
	else return -1;
}

static int child_init(int rank)
{
	authdb_table_info_t *i;

	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	auth_db_handle = db_ctx("auth_db");
	if (!auth_db_handle) goto err;
	if (db_add_db(auth_db_handle, db_url) < 0) goto err;
	if (db_connect(auth_db_handle) < 0) goto err;

	/* initializing queries */
	i = registered_tables;
	while (i) {
		if (generate_queries(i) < 0) {
			ERR("can't prepare queries\n");
			return -1;
		}
		i = i->next;
	}
    
	return 0;

err:

	if (auth_db_handle) {
		auth_db_handle = NULL;
		db_ctx_free(auth_db_handle);
	}

	ERR("Error while initializing database layer\n");
	return -1;
}


static int mod_init(void)
{
    bind_auth_s_t bind_auth;
    
    DBG("auth_db module - initializing\n");
    
    bind_auth = (bind_auth_s_t)find_export("bind_auth_s", 0, 0);
    if (!bind_auth) {
	LOG(L_ERR, "auth_db:mod_init: Unable to find bind_auth function\n");
	return -1;
    }
    if (bind_auth(&auth_api) < 0) {
	LOG(L_ERR, "auth_db:child_init: Unable to bind auth module\n");
	return -3;
    }
    
    if (aaa_avps_init(&credentials_list, &credentials, &credentials_n)) {
	return -1;
    }
    
    return 0;
}


static void destroy(void)
{
    if (auth_db_handle) {
		db_ctx_free(auth_db_handle);
		auth_db_handle = NULL;
    }
}

static int str_case_equals(const str *a, const str *b)
{
	/* ugly hack: taken from libcds */
	int i;
	
	if (!a) {
		if (!b) return 0;
		else return (b->len == 0) ? 0 : 1;
	}
	if (!b) return (a->len == 0) ? 0 : 1;
	if (a->len != b->len) return 1;
	
	for (i = 0; i < a->len; i++) 
		if (a->s[i] != b->s[i]) return 1;
	return 0;
}

static authdb_table_info_t *find_table_info(str *table)
{
	authdb_table_info_t *i = registered_tables;
	
	/* sequential search is OK because it is called only in child init */
	while (i) { 
		if (str_case_equals(&i->table, table) == 0) return i;
		i = i->next;
	}
	return NULL;
}

static authdb_table_info_t *register_table(str *table)
{
	authdb_table_info_t *info;

	info = find_table_info(table);
	if (info) return info; /* queries for this table already exist */

	info = (authdb_table_info_t*)pkg_malloc(sizeof(authdb_table_info_t) + table->len + 1);
	if (!info) {
		ERR("can't allocate pkg mem\n");
		return NULL;
	}

	info->table.s = info->buf;
	info->table.len = table->len;
	memcpy(info->table.s, table->s, table->len);
	info->table.s[table->len] = 0;

	/* append to the begining (we don't care about order) */
	info->next = registered_tables;
	registered_tables = info;

	return info;
}
/*
 * Convert char* parameter to str* parameter
 */
static int authdb_fixup(void** param, int param_no)
{
	fparam_t* p;

	if (param_no == 1) {
		return fixup_var_str_12(param, param_no);
	} else if (param_no == 2) {
		if (fixup_var_str_12(param, param_no) < 0) return -1;
		p = (fparam_t*)(*param);
		if (p->type == FPARAM_STR) {
			*param = register_table(&p->v.str);
			if (!*param) {
				ERR("can't register table %.*s\n", p->v.str.len, p->v.str.s);
				return -1;
			}
		} else {
			ERR("Non-string value of table with credentials is not allowed.\n");
			/* TODO: allow this too */
			return -1;
		}
	}

    return 0;
}
