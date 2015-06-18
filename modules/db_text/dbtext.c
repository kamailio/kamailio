/*
 * DBText module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2014 Edvina AB, Olle E. Johansson
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
#include <unistd.h>

#include "../../sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../rpc_lookup.h"
#include "dbtext.h"
#include "dbt_lib.h"
#include "dbt_api.h"

MODULE_VERSION

static int mod_init(void);
static void destroy(void);

/*
 * Module parameter variables
 */
int db_mode = 0;  /* Database usage mode: 0 = cache, 1 = no cache */
int empty_string = 0;  /* Treat empty string as "" = 0, 1 = NULL */

int dbt_bind_api(db_func_t *dbb);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"db_bind_api",    (cmd_function)dbt_bind_api,   0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_mode", INT_PARAM, &db_mode},
	{"emptystring", INT_PARAM, &empty_string},
	{0, 0, 0}
};

static rpc_export_t rpc_methods[];

struct module_exports exports = {	
	"db_text",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,     /* Exported functions */
	params,   /* Exported parameters */
	0,        /* exported statistics */
	0,        /* exported MI functions */
	0,        /* exported pseudo-variables */
	0,        /* extra processes */
	mod_init, /* module initialization function */
	0,        /* response function*/
	destroy,  /* destroy function */
	0         /* per-child init function */
};

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if (rpc_register_array(rpc_methods)!=0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(db_api_init()<0)
		return -1;
	return 0;
}

static int mod_init(void)
{
	if(dbt_init_cache())
		return -1;
	/* return make_demo(); */
	
	return 0;
}

static void destroy(void)
{
	LM_DBG("destroy ...\n");
	dbt_cache_print(0);
	dbt_cache_destroy();
}



int dbt_bind_api(db_func_t *dbb)
{
	if(dbb==NULL)
		return -1;

	memset(dbb, 0, sizeof(db_func_t));

	dbb->use_table   = dbt_use_table;
	dbb->init        = dbt_init;
	dbb->close       = dbt_close;
	dbb->query       = (db_query_f)dbt_query;
	dbb->free_result = dbt_free_result;
	dbb->insert      = (db_insert_f)dbt_insert;
	dbb->delete      = (db_delete_f)dbt_delete; 
	dbb->update      = (db_update_f)dbt_update;
	dbb->affected_rows = (db_affected_rows_f) dbt_affected_rows;
	dbb->raw_query   = (db_raw_query_f) dbt_raw_query;
	dbb->cap         = DB_CAP_ALL | DB_CAP_AFFECTED_ROWS | DB_CAP_RAW_QUERY;

	return 0;
}

/* rpc function documentation */
static const char *rpc_dump_doc[2] = {
	"Write back to disk modified tables", 0
};

/* rpc function implementations */
static void rpc_dump(rpc_t *rpc, void *c) {
	if (0!=dbt_cache_print(0))
		rpc->rpl_printf(c, "Dump failed");
	else
		rpc->rpl_printf(c, "Dump OK");

	return;
}

static rpc_export_t rpc_methods[] = {
	{"db_text.dump", rpc_dump, rpc_dump_doc, 0},
	{0, 0, 0, 0}
};

