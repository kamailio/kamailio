/*
 * $Id$
 *
 * DBText module interface
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
 */

/**
 * DBText module interface
 *  
 * 2003-01-30 created by Daniel
 * 2003-03-11 New module interface (janakj)
 * 2003-03-16 flags export parameter added (janakj)
 * 
 */

#include <stdio.h>
#include <unistd.h>

#include "../../sr_module.h"
#include "dbtext.h"
#include "dbt_lib.h"
#include "dbt_api.h"

MODULE_VERSION

static int mod_init(void);
static void destroy(void);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"db_use_table",   (cmd_function)dbt_use_table,  2, 0, 0},
	{"db_init",        (cmd_function)dbt_init,       1, 0, 0},
	{"db_close",       (cmd_function)dbt_close,      2, 0, 0},
	{"db_query",       (cmd_function)dbt_query,      2, 0, 0},
	{"db_raw_query",   (cmd_function)dbt_raw_query,  2, 0, 0},
	{"db_free_result", (cmd_function)dbt_free_query, 2, 0, 0},
	{"db_insert",     (cmd_function)dbt_insert,     2, 0, 0},
	{"db_delete",     (cmd_function)dbt_delete,     2, 0, 0},
	{"db_update",     (cmd_function)dbt_update,     2, 0, 0},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{0, 0, 0}
};


struct module_exports exports = {	
	"dbtext",
	cmds,     /* Exported functions */
	0,        /* RPC method */
	params,   /* Exported parameters */
	mod_init, /* module initialization function */
	0,        /* response function*/
	destroy,  /* destroy function */
	0,        /* oncancel function */
	0         /* per-child init function */
};


static int mod_init(void)
{
	if(dbt_init_cache())
		return -1;
	/*return make_demo(); */
	
	return 0;
}

static void destroy(void)
{
	DBG("DBT:destroy ...\n");
	dbt_cache_print(0);
	dbt_cache_destroy();
}

