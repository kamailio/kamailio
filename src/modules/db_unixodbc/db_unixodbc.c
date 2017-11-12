/*
 * UNIXODBC module interface
 *
 * Copyright (C) 2005-2006 Marco Lorrai
 * Copyright (C) 2008 1&1 Internet AG
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
 *
 */

#include "../../core/sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../lib/srdb1/db_query.h"
#include "dbase.h"
#include "db_unixodbc.h"

int ping_interval = 5 * 60; /* Default is 5 minutes */
int auto_reconnect = 1;     /* Default is enabled */
int use_escape_common = 0;  /* Enable common escaping */
int replace_query = 1;      /* Enable ODBC replace query */

char *db_unixodbc_tquote = NULL;

MODULE_VERSION

int db_unixodbc_bind_api(db_func_t *dbb);
int unixodbc_mod_init(void);

/*
 * MySQL database module interface
 */
static cmd_export_t cmds[] = {
	{"db_bind_api",    (cmd_function)db_unixodbc_bind_api,    0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"ping_interval",     INT_PARAM, &ping_interval},
	{"auto_reconnect",    INT_PARAM, &auto_reconnect},
	{"use_escape_common", INT_PARAM, &use_escape_common},
	{"replace_query",     INT_PARAM, &replace_query},
	{"quote_char",        PARAM_STRING, &db_unixodbc_tquote},
	{0, 0, 0}
};


struct module_exports exports = {
	"db_unixodbc",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,             /*  module parameters */
	0,                  /* exported statistics */
	0,                  /* exported MI functions */
	0,                  /* exported pseudo-variables */
	0,                  /* extra processes */
	unixodbc_mod_init,  /* module initialization function */
	0,                  /* response function*/
	0,                  /* destroy function */
	0                   /* per-child init function */
};

int db_unixodbc_bind_api(db_func_t *dbb)
{
	if(dbb==NULL)
		return -1;

	memset(dbb, 0, sizeof(db_func_t));

	dbb->use_table        = db_unixodbc_use_table;
	dbb->init             = db_unixodbc_init;
	dbb->close            = db_unixodbc_close;
	dbb->query            = db_unixodbc_query;
	dbb->fetch_result     = db_unixodbc_fetch_result;
	dbb->raw_query        = db_unixodbc_raw_query;
	dbb->free_result      = db_unixodbc_free_result;
	dbb->insert           = db_unixodbc_insert;
	dbb->delete           = db_unixodbc_delete;
	dbb->update           = db_unixodbc_update;
	if (replace_query)
		dbb->replace      = db_unixodbc_replace;
	else
		dbb->replace      = db_unixodbc_update_or_insert;
	dbb->raw_query_async  = db_unixodbc_raw_query_async;
	dbb->insert_async     = db_unixodbc_insert_async;
 
	return 0;
}

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if(db_api_init()<0)
		return -1;
	return 0;
}

int unixodbc_mod_init(void)
{
	return 0;
}

