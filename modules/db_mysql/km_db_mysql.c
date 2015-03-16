/* 
 * MySQL module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */

/*! \file
 *  \brief DB_MYSQL :: Core
 *  \ingroup db_mysql
 *  Module: \ref db_mysql
 */

/*! \defgroup db_mysql DB_MYSQL :: the MySQL driver for Kamailio
 *  \brief The Kamailio database interface to the MySQL database
 *  - http://www.mysql.org
 *
 */

#include "../../sr_module.h"
#include "../../dprint.h"
#include "km_dbase.h"
#include "km_db_mysql.h"

#include <mysql/mysql.h>

unsigned int db_mysql_timeout_interval = 2;   /* Default is 6 seconds */
unsigned int db_mysql_auto_reconnect = 1;     /* Default is enabled   */
unsigned int db_mysql_insert_all_delayed = 0; /* Default is off */
unsigned int db_mysql_update_affected_found = 0; /* Default is off */

/* MODULE_VERSION */

/*! \brief
 * MySQL database module interface
 */
static kam_cmd_export_t cmds[] = {
	{"db_bind_api",         (cmd_function)db_mysql_bind_api,      0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

/*! \brief
 * Exported parameters
 */
static param_export_t params[] = {
/*	{"ping_interval",    INT_PARAM, &db_mysql_ping_interval}, */
	{"timeout_interval", INT_PARAM, &db_mysql_timeout_interval},
	{"auto_reconnect",   INT_PARAM, &db_mysql_auto_reconnect},
	{0, 0, 0}
};

struct kam_module_exports kam_exports = {	
	"db_mysql",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,          /*  module parameters */
	0,               /* exported statistics */
	0,               /* exported MI functions */
	0,               /* exported pseudo-variables */
	0,               /* extra processes */
	kam_mysql_mod_init,  /* module initialization function */
	0,               /* response function*/
	0,               /* destroy function */
	0                /* per-child init function */
};


int kam_mysql_mod_init(void)
{
	LM_DBG("MySQL client version is %s\n", mysql_get_client_info());
	return 0;
}

int db_mysql_bind_api(db_func_t *dbb)
{
	if(dbb==NULL)
		return -1;

	memset(dbb, 0, sizeof(db_func_t));

	dbb->use_table        = db_mysql_use_table;
	dbb->init             = db_mysql_init;
	dbb->close            = db_mysql_close;
	dbb->query            = db_mysql_query;
	dbb->fetch_result     = db_mysql_fetch_result;
	dbb->raw_query        = db_mysql_raw_query;
	dbb->free_result      = (db_free_result_f) db_mysql_free_result;
	dbb->insert           = db_mysql_insert;
	dbb->delete           = db_mysql_delete;
	dbb->update           = db_mysql_update;
	dbb->replace          = db_mysql_replace;
	dbb->last_inserted_id = db_mysql_last_inserted_id;
	dbb->insert_update    = db_mysql_insert_update;
	dbb->insert_delayed   = db_mysql_insert_delayed;
	dbb->affected_rows    = db_mysql_affected_rows;
	dbb->start_transaction= db_mysql_start_transaction;
	dbb->end_transaction  = db_mysql_end_transaction;
	dbb->abort_transaction= db_mysql_abort_transaction;
	dbb->raw_query_async  = db_mysql_raw_query_async;
	dbb->insert_async     = db_mysql_insert_async;

	return 0;
}
