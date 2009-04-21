/*
 * $Id$
 *
 * Domain module
 *
 * Copyright (C) 2002-2008 Juha Heinanen
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
 * -------
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
 * 2003-04-05: default_uri #define used (jiri)
 * 2003-04-06: db connection closed in mod_init (janakj)
 * 2004-06-06: updated to the new DB api, cleanup: static dbf & handler,
 *             calls to domain_db_{bind,init,close,ver} (andrei)
 * 2006-01-22: added is_domain_local(variable) function (dan)
 *
 */


#include "domain_mod.h"
#include <stdio.h>
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../sr_module.h"
#include "../../pvar.h"
#include "../../mod_fix.h"
#include "domain.h"
#include "mi.h"
#include "hash.h"
#include "api.h"

/*
 * Module management function prototypes
 */
static int mod_init(void);
static void destroy(void);
static int child_init(int rank);
static int mi_child_init(void);

MODULE_VERSION

/*
 * Version of domain table required by the module,
 * increment this value if you change the table in
 * an backwards incompatible way
 */
#define TABLE_VERSION 1

#define DOMAIN_TABLE "domain"
#define DOMAIN_TABLE_LEN (sizeof(DOMAIN_TABLE) - 1)

#define DOMAIN_COL "domain"
#define DOMAIN_COL_LEN (sizeof(DOMAIN_COL) - 1)

/*
 * Module parameter variables
 */
static str db_url = {DEFAULT_RODB_URL, DEFAULT_RODB_URL_LEN};
int db_mode = 0;			/* Database usage mode: 0 = no cache, 1 = cache */
str domain_table = {DOMAIN_TABLE, DOMAIN_TABLE_LEN}; /* Name of domain table */
str domain_col = {DOMAIN_COL, DOMAIN_COL_LEN};       /* Name of domain column */

/*
 * Other module variables
 */
struct domain_list ***hash_table = 0;	/* Pointer to current hash table pointer */
struct domain_list **hash_table_1 = 0;	/* Pointer to hash table 1 */
struct domain_list **hash_table_2 = 0;	/* Pointer to hash table 2 */


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"is_from_local", (cmd_function)is_from_local, 0, 0, 0,
	 REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{"is_uri_host_local", (cmd_function)is_uri_host_local, 0, 0, 0,
	 REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{"is_domain_local", (cmd_function)w_is_domain_local, 1, fixup_pvar_null,
	 fixup_free_pvar_null,
	 REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{"bind_domain", (cmd_function)bind_domain, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",         STR_PARAM, &db_url.s      },
	{"db_mode",        INT_PARAM, &db_mode       },
	{"domain_table",   STR_PARAM, &domain_table.s},
	{"domain_col",     STR_PARAM, &domain_col.s  },
	{0, 0, 0}
};


/*
 * Exported MI functions
 */
static mi_export_t mi_cmds[] = {
	{ MI_DOMAIN_RELOAD, mi_domain_reload, MI_NO_INPUT_FLAG, 0, mi_child_init },
	{ MI_DOMAIN_DUMP,   mi_domain_dump,   MI_NO_INPUT_FLAG, 0, 0             },
	{ 0, 0, 0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"domain",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,      /* Exported functions */
	params,    /* Exported parameters */
	0,         /* exported statistics */
	mi_cmds,   /* exported MI functions */
	0,         /* exported pseudo-variables */
	0,         /* extra processes */
	mod_init,  /* module initialization function */
	0,         /* response function*/
	destroy,   /* destroy function */
	child_init /* per-child init function */
};


static int mod_init(void)
{
	int i;

	LM_DBG("Initializing\n");
	
	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	db_url.len = strlen(db_url.s);
	domain_table.len = strlen(domain_table.s);
	domain_col.len = strlen(domain_col.s);

	/* Check if database module has been loaded */
	if (domain_db_bind(&db_url) < 0)  return -1;

	/* Check if cache needs to be loaded from domain table */
	if (db_mode != 0) {

		if (domain_db_init(&db_url)<0) return -1;

		/* Check table version */
		if (domain_db_ver(&domain_table, TABLE_VERSION) < 0) {
		    LM_ERR("error during check of domain table version\n");
		    goto error;
		}

		/* Initializing hash tables and hash table variable */
		hash_table_1 = (struct domain_list **)shm_malloc
			(sizeof(struct domain_list *) * DOM_HASH_SIZE);
		if (hash_table_1 == 0) {
			LM_ERR("No memory for hash table\n");
			goto error;
		}

		hash_table_2 = (struct domain_list **)shm_malloc
			(sizeof(struct domain_list *) * DOM_HASH_SIZE);
		if (hash_table_2 == 0) {
			LM_ERR("No memory for hash table\n");
			goto error;
		}
		for (i = 0; i < DOM_HASH_SIZE; i++) {
			hash_table_1[i] = hash_table_2[i] = (struct domain_list *)0;
		}

		hash_table = (struct domain_list ***)shm_malloc
			(sizeof(struct domain_list *));
		*hash_table = hash_table_1;

		if (reload_domain_table() == -1) {
			LM_ERR("Domain table reload failed\n");
			goto error;
		}

		domain_db_close();
	}

	return 0;
error:
	domain_db_close();
	return -1;
}


static int child_init(int rank)
{
	/* Check if database is needed by child */
	if ( db_mode==0 && rank>0 ) {
		if (domain_db_init(&db_url)<0) {
			LM_ERR("Unable to connect to the database\n");
			return -1;
		}
	}
	return 0;
}


static int mi_child_init(void)
{
	return domain_db_init(&db_url);
}


static void destroy(void)
{
	/* Destroy is called from the main process only,
	 * there is no need to close database here because
	 * it is closed in mod_init already
	 */
	if (hash_table) {
		shm_free(hash_table);
		hash_table = 0;
	}
	if (hash_table_1) {
		hash_table_free(hash_table_1);
		shm_free(hash_table_1);
		hash_table_1 = 0;
	}
	if (hash_table_2) {
		hash_table_free(hash_table_2);
		shm_free(hash_table_2);
		hash_table_2 = 0;
	}
}
