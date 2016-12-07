/*
 * $Id$
 *
 * CASSANDRA module interface
 *
 * Copyright (C) 2012 1&1 Internet AG
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
 * History:
 * --------
 * 2012-01  first version (Anca Vamanu)
 */

#include <stdio.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../dprint.h"

#include "dbcassa_base.h"
#include "dbcassa_table.h"

unsigned int cassa_conn_timeout= 1000;
unsigned int cassa_send_timeout= 2000;
unsigned int cassa_recv_timeout= 4000;
unsigned int cassa_retries= 1;
unsigned int cassa_auto_reconnect = 1;

static int cassa_mod_init(void);
static void mod_destroy(void);
int db_cassa_bind_api(db_func_t *dbb);
str dbcassa_schema_path={0, 0};

MODULE_VERSION

/*
 *  database module interface
 */
static cmd_export_t cmds[] = {
	{"db_bind_api",  (cmd_function)db_cassa_bind_api, 0, 0, 0},
	{0, 0, 0, 0, 0}
};


static param_export_t params[] = {
	{"schema_path",      PARAM_STR,  &dbcassa_schema_path.s},
	{"connect_timeout",  PARAM_INT,  &cassa_conn_timeout},
	{"send_timeout",     PARAM_INT,  &cassa_send_timeout},
	{"receive_timeout",  PARAM_INT,  &cassa_recv_timeout},
	{"retries",          PARAM_INT,  &cassa_retries},
	{"auto_reconnect",   INT_PARAM,  &cassa_auto_reconnect},
	{0, 0, 0}
};


struct module_exports exports = {
	"db_cassandra",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,          /* module parameters */
	0,               /* exported statistics */
	0,               /* exported MI functions */
	0,               /* exported pseudo-variables */
	0,               /* extra processes */
	cassa_mod_init,  /* module initialization function */
	0,               /* response function*/
	mod_destroy,     /* destroy function */
	0                /* per-child init function */
};

static int cassa_mod_init(void)
{
	if(!dbcassa_schema_path.s) {
		LM_ERR("Set the schema_path parameter to the path of the directory"
				" where the table schemas are found (they must be described in cassa special format)\n");
		return -1;
	}
	dbcassa_schema_path.len = strlen(dbcassa_schema_path.s);

	return dbcassa_read_table_schemas();
}

db1_con_t *db_cassa_init(const str* _url)
{
	return db_do_init(_url,  (void* (*)()) db_cassa_new_connection);
}


/*!
 * \brief Close database when the database is no longer needed
 * \param _h closed connection, as returned from db_cassa_init
 * \note free all memory and resources
 */
void db_cassa_close(db1_con_t* _h)
{
	db_do_close(_h, (void (*)()) db_cassa_free_connection);
}

/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_cassa_use_table(db1_con_t* _h, const str* _t)
{
	return db_use_table(_h, _t);
}



int db_cassa_bind_api(db_func_t *dbb)
{
	if(dbb==NULL)
		return -1;

	memset(dbb, 0, sizeof(db_func_t));

	dbb->use_table        = db_cassa_use_table;
	dbb->init             = db_cassa_init;
	dbb->close            = db_cassa_close;
	dbb->query            = db_cassa_query;
	dbb->free_result      = db_cassa_free_result;
	dbb->insert           = db_cassa_insert;
	dbb->replace          = db_cassa_replace;
	dbb->insert_update    = db_cassa_insert;
	dbb->delete           = db_cassa_delete;
	dbb->update           = db_cassa_update;
	dbb->raw_query        = db_cassa_raw_query;

	return 0;
}

static void mod_destroy(void)
{
	dbcassa_destroy_htable();
}
