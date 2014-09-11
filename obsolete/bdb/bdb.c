/* $Id$
 *
 * Copyright (C) 2006-2007 Sippy Software, Inc. <sales@sippysoft.com>
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
 */


#include "bdb.h"

MODULE_VERSION

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"db_use_table",   (cmd_function)bdb_use_table,   2, 0, 0},
	{"db_init",        (cmd_function)bdb_init,        1, 0, 0},
	{"db_close",       (cmd_function)bdb_close,       2, 0, 0},
	{"db_query",       (cmd_function)bdb_query,       2, 0, 0},
	{"db_raw_query",   (cmd_function)bdb_raw_query,   2, 0, 0},
	{"db_free_result", (cmd_function)bdb_free_result, 2, 0, 0},
	{"db_insert",      (cmd_function)bdb_insert,      2, 0, 0},
	{"db_delete",      (cmd_function)bdb_delete,      2, 0, 0},
	{"db_update",      (cmd_function)bdb_update,      2, 0, 0},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"describe_table", PARAM_STRING|USE_FUNC_PARAM, (void*)bdb_describe_table},
	{0, 0, 0}
};


struct module_exports exports = {	
	"bdb",
	cmds,     /* Exported functions */
	0,        /* RPC method */
	params,   /* Exported parameters */
	mod_init, /* module initialization function */
	0,        /* response function*/
	mod_destroy,	  /* destroy function */
	0,        /* oncancel function */
	child_init         /* per-child init function */
};

bdb_table_p	bdb_tables = NULL;

static int mod_init(void)
{
#ifdef BDB_EXTRA_DEBUG
	LOG(L_NOTICE, "BDB:init...\n");
#endif

	return 0;
}

static int child_init(int rank)
{
#ifdef BDB_EXTRA_DEBUG
	LOG(L_NOTICE, "BDB:child_init: rank = %d\n", rank);
#endif

	return 0;
}

static void mod_destroy(void)
{
#ifdef BDB_EXTRA_DEBUG
	LOG(L_NOTICE, "BDB:destroy...\n");
#endif

	if (bdb_tables != NULL) {
		bdb_free_table_list(bdb_tables);
		bdb_tables = NULL;
	}
}
