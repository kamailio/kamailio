/*
 * MySQL module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
/** @addtogroup mysql
 *  @{
 */
 
#include "mysql_mod.h"
#include "km_db_mysql.h"

#include "my_uri.h"
#include "my_con.h"
#include "my_cmd.h"
#include "my_fld.h"
#include "my_res.h"

#include "../../sr_module.h"
#include "../../lib/srdb2/db.h"
#include "../../dprint.h"

int my_ping_interval = 5 * 60; /* Default is 5 minutes */
unsigned int my_connect_to = 2; /* 2 s by default */
unsigned int my_send_to = 0; /*  enabled only for mysql >= 5.25  */
unsigned int my_recv_to = 0; /* enabled only for mysql >= 5.25 */
unsigned int my_retries = 1;    /* Number of retries when command fails */

unsigned long my_client_ver = 0;

struct mysql_counters_h mysql_cnts_h;
counter_def_t mysql_cnt_defs[] =  {
	{&mysql_cnts_h.driver_err, "driver_errors", 0, 0, 0,
		"incremented each time a Mysql error happened because the server/connection has failed."},
	{0, 0, 0, 0, 0, 0 }
};
#define DEFAULT_MY_SEND_TO  2   /* in seconds */
#define DEFAULT_MY_RECV_TO  4   /* in seconds */

static int mysql_mod_init(void);

MODULE_VERSION


/*
 * MySQL database module interface
 */
static cmd_export_t cmds[] = {
	{"db_ctx",    (cmd_function)NULL,         0, 0, 0},
	{"db_con",    (cmd_function)my_con,       0, 0, 0},
	{"db_uri",    (cmd_function)my_uri,       0, 0, 0},
	{"db_cmd",    (cmd_function)my_cmd,       0, 0, 0},
	{"db_put",    (cmd_function)my_cmd_exec,  0, 0, 0},
	{"db_del",    (cmd_function)my_cmd_exec,  0, 0, 0},
	{"db_get",    (cmd_function)my_cmd_exec,  0, 0, 0},
	{"db_upd",    (cmd_function)my_cmd_exec,  0, 0, 0},
	{"db_sql",    (cmd_function)my_cmd_exec,  0, 0, 0},
	{"db_res",    (cmd_function)my_res,       0, 0, 0},
	{"db_fld",    (cmd_function)my_fld,       0, 0, 0},
	{"db_first",  (cmd_function)my_cmd_first, 0, 0, 0},
	{"db_next",   (cmd_function)my_cmd_next,  0, 0, 0},
	{"db_setopt", (cmd_function)my_setopt,    0, 0, 0},
	{"db_getopt", (cmd_function)my_getopt,    0, 0, 0},
	{"db_bind_api",         (cmd_function)db_mysql_bind_api,      0, 0, 0},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"ping_interval",   PARAM_INT, &my_ping_interval},
	{"connect_timeout", PARAM_INT, &my_connect_to},
	{"send_timeout",    PARAM_INT, &my_send_to},
	{"receive_timeout", PARAM_INT, &my_recv_to},
	{"retries",         PARAM_INT, &my_retries},

	{"timeout_interval", INT_PARAM, &db_mysql_timeout_interval},
	{"auto_reconnect",   INT_PARAM, &db_mysql_auto_reconnect},
	{"insert_delayed",   INT_PARAM, &db_mysql_insert_all_delayed},
	{"update_affected_found", INT_PARAM, &db_mysql_update_affected_found},
	{0, 0, 0}
};


struct module_exports exports = {
	"db_mysql",
	cmds,
	0,               /* RPC method */
	params,          /*  module parameters */
	mysql_mod_init,  /* module initialization function */
	0,               /* response function*/
	0,               /* destroy function */
	0,               /* oncancel function */
	0                /* per-child init function */
};


int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	if(db_mysql_alloc_buffer()<0)
		return -1;
	return 0;
}

static int mysql_mod_init(void)
{
#if MYSQL_VERSION_ID >= 40101
	my_client_ver = mysql_get_client_version();
	if ((my_client_ver >= 50025) || 
		((my_client_ver >= 40122) && 
		 (my_client_ver < 50000))) {
		if (my_send_to == 0) {
			my_send_to= DEFAULT_MY_SEND_TO;
		}
		if (my_recv_to == 0) {
			my_recv_to= DEFAULT_MY_RECV_TO;
		}
	} else if (my_recv_to || my_send_to) {
		LOG(L_WARN, "WARNING: mysql send or received timeout set, but "
			" not supported by the installed mysql client library"
			" (needed at least 4.1.22 or 5.0.25, but installed %ld)\n",
			my_client_ver);
	}
#else
	if (my_recv_to || my_send_to) {
		LOG(L_WARN, "WARNING: mysql send or received timeout set, but "
			" not supported by the mysql client library used to compile"
			" the mysql module (needed at least 4.1.1 but "
			" compiled against %ld)\n", MYSQL_VERSION_ID);
	}
#endif
	if (counter_register_array("mysql", mysql_cnt_defs) < 0)
		goto error;

	return kam_mysql_mod_init();
error:
	return -1;
}

/** @} */
