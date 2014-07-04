/*
 * $Id$
 *
 * SQlite module interface
 *
 * Copyright (C) 2010 Timo Teräs
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
 */

#include <sys/time.h>
#include <sqlite3.h>

#include "../../sr_module.h"
#include "../../lib/srdb1/db_query.h"
#include "../../lib/srdb1/db.h"
#include "dbase.h"

MODULE_VERSION

static int sqlite_bind_api(db_func_t *dbb)
{
	if(dbb==NULL)
		return -1;

	memset(dbb, 0, sizeof(db_func_t));

	dbb->use_table		= db_sqlite_use_table;
	dbb->init		= db_sqlite_init;
	dbb->close		= db_sqlite_close;
	dbb->free_result	= db_sqlite_free_result;
	dbb->query		= db_sqlite_query;
	dbb->insert		= db_sqlite_insert;
	dbb->delete		= db_sqlite_delete;
	dbb->update		= db_sqlite_update;
	dbb->raw_query		= db_sqlite_raw_query;

	return 0;
}

static cmd_export_t cmds[] = {
	{"db_bind_api", (cmd_function)sqlite_bind_api, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if(db_api_init()<0)
		return -1;
	return 0;
}

static int sqlite_mod_init(void)
{
	sqlite3_initialize();

	LM_INFO("SQlite library version %s (compiled using %s)\n",
		sqlite3_libversion(),
		SQLITE_VERSION);
	return 0;
}


static void sqlite_mod_destroy(void)
{
	LM_INFO("SQlite terminate\n");

	sqlite3_shutdown();
}

struct module_exports exports = {
	"db_sqlite",
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,			/* module commands */
	0,			/* module parameters */
	0,			/* exported statistics */
	0,			/* exported MI functions */
	0,			/* exported pseudo-variables */
	0,			/* extra processes */
	sqlite_mod_init,	/* module initialization function */
	0,			/* response function*/
	sqlite_mod_destroy,	/* destroy function */
	0			/* per-child init function */
};
