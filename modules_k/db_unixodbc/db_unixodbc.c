/* 
 * $Id$ 
 *
 * UNIXODBC module interface
 *
 * Copyright (C) 2005-2006 Marco Lorrai
 * Copyright (C) 2008 1&1 Internet AG
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * History:
 * --------
 *  2005-12-01  initial commit (chgen)
 */

#include "../../sr_module.h"
#include "dbase.h"
#include "db_unixodbc.h"

int ping_interval = 5 * 60; /* Default is 5 minutes */
int auto_reconnect = 1;     /* Default is enabled */
int use_escape_common = 0;  /* Enable common escaping */

MODULE_VERSION


/*
 * MySQL database module interface
 */
static cmd_export_t cmds[] = {
	{"db_use_table",   (cmd_function)db_unixodbc_use_table,      2, 0, 0, 0},
	{"db_init",        (cmd_function)db_unixodbc_init,        1, 0, 0, 0},
	{"db_close",       (cmd_function)db_unixodbc_close,       2, 0, 0, 0},
	{"db_query",       (cmd_function)db_unixodbc_query,       2, 0, 0, 0},
	{"db_raw_query",   (cmd_function)db_unixodbc_raw_query,   2, 0, 0, 0},
	{"db_free_result", (cmd_function)db_unixodbc_free_result, 2, 0, 0, 0},
	{"db_insert",      (cmd_function)db_unixodbc_insert,      2, 0, 0, 0},
	{"db_delete",      (cmd_function)db_unixodbc_delete,      2, 0, 0, 0},
	{"db_update",      (cmd_function)db_unixodbc_update,      2, 0, 0, 0},
	{"db_replace",     (cmd_function)db_unixodbc_replace,     2, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"ping_interval",     INT_PARAM, &ping_interval},
	{"auto_reconnect",    INT_PARAM, &auto_reconnect},
	{"use_escape_common", INT_PARAM, &use_escape_common},
	{0, 0, 0}
};


struct module_exports exports = {	
	"db_unixodbc",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,     /*  module parameters */
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	0,          /* module initialization function */
	0,          /* response function*/
	0,          /* destroy function */
	0           /* per-child init function */
};

