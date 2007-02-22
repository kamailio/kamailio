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
#include "authdb_mod.h"

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


/*
 * Pointer to reply function in stateless module
 */
sl_api_t sl;


#define USERNAME_COL "auth_username"
#define DID_COL "did"
#define REALM_COL "realm"
#define PASS_COL "ha1"
#define PASS_COL_2 "ha1b"
#define DEFAULT_CRED_LIST "uid"
#define FLAGS_COL "flags"

/*
 * Module parameter variables
 */
static char* db_url         = DEFAULT_RODB_URL;

str username_column = STR_STATIC_INIT(USERNAME_COL);
str did_column      = STR_STATIC_INIT(DID_COL);
str realm_column    = STR_STATIC_INIT(REALM_COL);
str pass_column     = STR_STATIC_INIT(PASS_COL);
str pass_column_2   = STR_STATIC_INIT(PASS_COL_2);
str flags_column    = STR_STATIC_INIT(FLAGS_COL);

int calc_ha1 = 0;
int use_did = 1;

db_con_t* auth_db_handle = 0;      /* database connection handle */
db_func_t auth_dbf;
auth_api_t auth_api;

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
    {"flags_column",      PARAM_STR,    &flags_column    },
    {"calculate_ha1",     PARAM_INT,    &calc_ha1        },
    {"load_credentials",  PARAM_STR,    &credentials_list},
    {"use_did",           PARAM_INT,    &use_did         },
    {0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
    "auth_db",
    cmds,       /* Exported functions */
    0,          /* RPC methods */
    params,     /* Exported parameters */
    mod_init,   /* module initialization function */
    0,          /* response function */
    destroy,    /* destroy function */
    0,          /* oncancel function */
    child_init  /* child initialization function */
};


static int child_init(int rank)
{
	if (rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */
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
static int authdb_fixup(void** param, int param_no)
{
    db_con_t* dbh;
    int ver;
    fparam_t* p;
    
    if (param_no == 1) {
	return fixup_var_str_12(param, param_no);
    } else if (param_no == 2) {
	if (fixup_var_str_12(param, param_no) < 0) return -1;
	p = (fparam_t*)(*param);
	if (p->type == FPARAM_STR) {
	    dbh = auth_dbf.init(db_url);
	    if (!dbh) {
		ERR("Unable to open database connection\n");
		return -1;
	    }
	    ver = table_version(&auth_dbf, dbh, &p->v.str);
	    auth_dbf.close(dbh);
	    if (ver < 0) {
		ERR("Error while querying table version\n");
		return -1;
	    } else if (ver < TABLE_VERSION) {
		ERR("Invalid table version (use ser_mysql.sh reinstall)\n");
		return -1;
	    }
	} else {
	    DBG("Not checking table version, parameter is attribute or select\n");
	}
    }
    
    return 0;
}
