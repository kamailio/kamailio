/* 
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <string.h>
#include "../../sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../mod_fix.h"
#include "../../trim.h"
#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"
#include "../../modules/auth/api.h"
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


static int w_is_subscriber(sip_msg_t *msg, char *_uri, char* _table,
		char *_flags);
static int auth_fixup(void** param, int param_no);
static int auth_check_fixup(void** param, int param_no);
int parse_aaa_pvs(char *definition, pv_elem_t **pv_def, int *cnt);

#define USER_COL "username"
#define DOMAIN_COL "domain"
#define PASS_COL "ha1"
#define PASS_COL_2 "ha1b"

/*
 * Module parameter variables
 */
static str db_url           = str_init(DEFAULT_RODB_URL);
str user_column             = str_init(USER_COL);
str domain_column           = str_init(DOMAIN_COL);
str pass_column             = str_init(PASS_COL);
str pass_column_2           = str_init(PASS_COL_2);

static int version_table_check = 1;

int calc_ha1                = 0;
int use_domain              = 0; /* Use also domain when looking up in table */

db1_con_t* auth_db_handle    = 0; /* database connection handle */
db_func_t auth_dbf;
auth_api_s_t auth_api;

char *credentials_list      = 0;
pv_elem_t *credentials      = 0; /* Parsed list of credentials to load */
int credentials_n           = 0; /* Number of credentials in the list */

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"www_authorize",      (cmd_function)www_authenticate,   2, auth_fixup, 0,
		REQUEST_ROUTE},
	{"www_authenticate",   (cmd_function)www_authenticate,   2, auth_fixup, 0,
		REQUEST_ROUTE},
	{"www_authenticate",   (cmd_function)www_authenticate2,  3, auth_fixup, 0,
REQUEST_ROUTE},
	{"proxy_authorize",    (cmd_function)proxy_authenticate, 2, auth_fixup, 0,
		REQUEST_ROUTE},
	{"proxy_authenticate", (cmd_function)proxy_authenticate, 2, auth_fixup, 0,
		REQUEST_ROUTE},
	{"auth_check",         (cmd_function)auth_check,         3, auth_check_fixup, 0,
		REQUEST_ROUTE},
	{"is_subscriber",      (cmd_function)w_is_subscriber,    3, auth_check_fixup, 0,
		ANY_ROUTE},
	{"bind_auth_db",       (cmd_function)bind_auth_db,       0, 0, 0,
		0},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",            PARAM_STR, &db_url            },
	{"user_column",       PARAM_STR, &user_column       },
	{"domain_column",     PARAM_STR, &domain_column     },
	{"password_column",   PARAM_STR, &pass_column       },
	{"password_column_2", PARAM_STR, &pass_column_2     },
	{"calculate_ha1",     INT_PARAM, &calc_ha1            },
	{"use_domain",        INT_PARAM, &use_domain          },
	{"load_credentials",  PARAM_STRING, &credentials_list    },
	{"version_table",     INT_PARAM, &version_table_check },
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
	if (parse_aaa_pvs(credentials_list, &credentials, &credentials_n) != 0) {
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
		pv_elem_free_all(credentials);
		credentials = 0;
		credentials_n = 0;
	}
}


/**
 * check if the subscriber identified by _uri has a valid record in
 * database table _table
 */
static int w_is_subscriber(sip_msg_t *msg, char *_uri, char* _table,
		char *_flags)
{
	str suri;
	str stable;
	int iflags;
	int ret;
	sip_uri_t puri;

	if(msg==NULL || _uri==NULL || _table==NULL || _flags==NULL) {
		LM_ERR("invalid parameters\n");
		return AUTH_ERROR;
	}

	if (get_str_fparam(&suri, msg, (fparam_t*)_uri) < 0) {
		LM_ERR("failed to get uri value\n");
		return -1;
	}

	if (suri.len==0) {
		LM_ERR("invalid uri parameter - empty value\n");
		return -1;
	}
	if(parse_uri(suri.s, suri.len, &puri)<0){
		LM_ERR("invalid uri parameter format\n");
		return -1;
	}

	if (get_str_fparam(&stable, msg, (fparam_t*)_table) < 0) {
		LM_ERR("failed to get table value\n");
		return -1;
	}

	if (stable.len==0) {
		LM_ERR("invalid table parameter - empty value\n");
		return -1;
	}

	if(fixup_get_ivalue(msg, (gparam_p)_flags, &iflags)!=0)
	{
		LM_ERR("invalid flags parameter\n");
		return -1;
	}

	LM_DBG("uri [%.*s] table [%.*s] flags [%d]\n", suri.len, suri.s,
			stable.len,  stable.s, iflags);
	ret = fetch_credentials(msg, &puri.user,
				(iflags&AUTH_DB_SUBS_USE_DOMAIN)?&puri.host:NULL,
				&stable, iflags);

	if(ret>=0)
		return 1;
	return ret;
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

	if (param_no == 1 || param_no == 3) {
		return fixup_var_str_12(param, 1);
	} else if (param_no == 2) {
		name.s = (char*)*param;
		name.len = strlen(name.s);

		dbh = auth_dbf.init(&db_url);
		if (!dbh) {
			LM_ERR("unable to open database connection\n");
			return -1;
		}
		if(version_table_check!=0
				&& db_check_table_version(&auth_dbf, dbh, &name,
					TABLE_VERSION) < 0) {
			LM_ERR("error during table version check.\n");
			auth_dbf.close(dbh);
			return -1;
		}
		auth_dbf.close(dbh);
	}
	return 0;
}

/*
 * Convert cfg parameters to run-time structures
 */
static int auth_check_fixup(void** param, int param_no)
{
	if(strlen((char*)*param)<=0) {
		LM_ERR("empty parameter %d not allowed\n", param_no);
		return -1;
	}
	if (param_no == 1) {
		return fixup_var_str_12(param, 1);
	}
	if (param_no == 2) {
		return fixup_var_str_12(param, 2);
	}
	if (param_no == 3) {
		return fixup_igp_null(param, 1);
	}
	return 0;
}

/*
 * Parse extra credentials list
 */
int parse_aaa_pvs(char *definition, pv_elem_t **pv_def, int *cnt)
{
	pv_elem_t *pve;
	str pv;
	char *p;
	char *end;
	char *sep;

	p = definition;
	if (p==0 || *p==0)
		return 0;

	*pv_def = 0;
	*cnt = 0;

	/* get element by element */
	while ( (end=strchr(p,';'))!=0 || (end=p+strlen(p))!=p ) {
		/* new pv_elem_t */
		if ( (pve=(pv_elem_t*)pkg_malloc(sizeof(pv_elem_t)))==0 ) {
			LM_ERR("no more pkg mem\n");
			goto error;
		}
		memset( pve, 0, sizeof(pv_elem_t));

		/* definition is between p and e */
		/* search backwards because PV definition may contain '=' characters */
		for (sep = end; sep >= p && *sep != '='; sep--); 
		if (sep > p) {
			/* pv=column style */
			/* set column name */
			pve->text.s = sep + 1;
			pve->text.len = end - pve->text.s;
			trim(&pve->text);
			if (pve->text.len == 0)
				goto parse_error;
			/* set pv spec */
			pv.s = p;
			pv.len = sep - p;
			trim(&pv);
			if (pv.len == 0)
				goto parse_error;
		} else {
			/* no pv, only column name */
			pve->text.s = p;
			pve->text.len = end - pve->text.s;
			trim(&pve->text);
			if (pve->text.len == 0)
				goto parse_error;
			/* create an avp definition for the spec parser */
			pv.s = (char*)pkg_malloc(pve->text.len + 7);
			if (pv.s == NULL) {
				LM_ERR("no more pkg mem\n");
				goto parse_error;
			}
			pv.len = snprintf(pv.s, pve->text.len + 7, "$avp(%.*s)",
			                  pve->text.len, pve->text.s);
		}

		/* create a pv spec */
		LM_DBG("column: %.*s  pv: %.*s\n", pve->text.len, pve->text.s, pv.len, pv.s);
		pve->spec = pv_spec_lookup(&pv, NULL);
		if(pve->spec==NULL || pve->spec->setf == NULL) {
			LM_ERR("PV is not writeable: %.*s\n", pv.len, pv.s);
			goto parse_error;
		}

		/* link the element */
		pve->next = *pv_def;
		*pv_def = pve;
		(*cnt)++;
		pve = 0;
		/* go to the end */
		p = end;
		if (*p==';')
			p++;
		if (*p==0)
			break;
	}

	return 0;
parse_error:
	LM_ERR("parse failed in \"%s\" at pos %d(%s)\n",
		definition, (int)(long)(p-definition),p);
error:
	pkg_free( pve );
	pv_elem_free_all( *pv_def );
	*pv_def = 0;
	*cnt = 0;
	return -1;
}
