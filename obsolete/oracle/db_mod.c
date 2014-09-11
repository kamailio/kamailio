/* 
 * Module interface
 *
 * Copyright (C) 2005 RingCentral Inc.
 * Created by Dmitry Semyonov <dsemyonov@ringcentral.com>
 *
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

#include "../../sr_module.h"
#include "dbase.h"


MODULE_VERSION


/*
 * Oracle database module interface
 */
static cmd_export_t cmds[] = {
	{"db_use_table",   (cmd_function)db_use_table,   2, 0, 0},
	{"db_init",        (cmd_function)db_init,        1, 0, 0},
	{"db_close",       (cmd_function)db_close,       2, 0, 0},
	{"db_query",       (cmd_function)db_query,       2, 0, 0},
	{"db_raw_query",   (cmd_function)db_raw_query,   2, 0, 0},
	{"db_free_result", (cmd_function)db_free_result, 2, 0, 0},
	{"db_insert",      (cmd_function)db_insert,      2, 0, 0},
	{"db_delete",      (cmd_function)db_delete,      2, 0, 0},
	{"db_update",      (cmd_function)db_update,      2, 0, 0},
	{0, 0, 0, 0, 0}
};



struct module_exports exports = {	
	"oracle",
	cmds,

	0,        /*  module paramers */

	0,        /* module initialization function */
	0,        /* response function*/
	0,        /* destroy function */
	0,        /* oncancel function */
	0         /* per-child init function */
};
