/* 
 * $Id$
 *
 * ALIAS_DB Module
 *
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of a module for Kamailio, a free SIP server.
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
 * History:
 * --------
 * 2004-09-01: first version (ramona)
 */


#include <stdio.h>
#include <string.h>
#include "../../sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include "../../ut.h"
#include "../../mod_fix.h"

#include "alookup.h"
#include "api.h"

MODULE_VERSION


/* Module destroy function prototype */
static void destroy(void);


/* Module child-init function prototype */
static int child_init(int rank);


/* Module initialization function prototype */
static int mod_init(void);

static int w_alias_db_lookup(struct sip_msg* _msg, char* _table, char* _str2);

/* Module parameter variables */
static str db_url       = str_init(DEFAULT_RODB_URL);
str user_column         = str_init("username");
str domain_column       = str_init("domain");
str alias_user_column   = str_init("alias_username");
str alias_domain_column = str_init("alias_domain");
str domain_prefix       = {NULL, 0};
int use_domain          = 0;
int ald_append_branches = 0;

db1_con_t* db_handle;   /* Database connection handle */
db_func_t adbf;  /* DB functions */

/* Exported functions */
static cmd_export_t cmds[] = {
	{"alias_db_lookup", (cmd_function)w_alias_db_lookup, 1, fixup_spve_null, 0,
		REQUEST_ROUTE|FAILURE_ROUTE},
	{"bind_alias_db",   (cmd_function)bind_alias_db, 1, 0, 0,
		0},
	{0, 0, 0, 0, 0, 0}
};


/* Exported parameters */
static param_export_t params[] = {
	{"db_url",              PARAM_STR, &db_url        },
	{"user_column",         PARAM_STR, &user_column   },
	{"domain_column",       PARAM_STR, &domain_column },
	{"alias_user_column",   PARAM_STR, &alias_user_column   },
	{"alias_domain_column", PARAM_STR, &alias_domain_column },
	{"use_domain",          INT_PARAM, &use_domain      },
	{"domain_prefix",       PARAM_STR, &domain_prefix },
	{"append_branches",     INT_PARAM, &ald_append_branches   },
	{0, 0, 0}
};


/* Module interface */
struct module_exports exports = {
	"alias_db",
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


/**
 *
 */
static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	db_handle = adbf.init(&db_url);
	if (!db_handle)
	{
		LM_ERR("unable to connect database\n");
		return -1;
	}
	return 0;

}


/**
 *
 */
static int mod_init(void)
{
  /* Find a database module */
	if (db_bind_mod(&db_url, &adbf))
	{
		LM_ERR("unable to bind database module\n");
		return -1;
	}
	if (!DB_CAPABILITY(adbf, DB_CAP_QUERY))
	{
		LM_CRIT("database modules does not "
			"provide all functions needed by avpops module\n");
		return -1;
	}

	return 0;
}


/**
 *
 */
static void destroy(void)
{
	if (db_handle) {
		adbf.close(db_handle);
		db_handle = 0;
	}
}

static int w_alias_db_lookup(struct sip_msg* _msg, char* _table, char* _str2)
{
        str table_s;

        if(_table==NULL || fixup_get_svalue(_msg, (gparam_p)_table, &table_s)!=0)
        {
                LM_ERR("invalid table parameter\n");
                return -1;
        }

        return alias_db_lookup(_msg, table_s);
}

int bind_alias_db(struct alias_db_binds *pxb)
{
        if (pxb == NULL)
        {
                LM_WARN("bind_alias_db: Cannot load alias_db API into a NULL pointer\n");
                return -1;
        }

        pxb->alias_db_lookup = alias_db_lookup;
        return 0;
}
