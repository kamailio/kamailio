/*
 * JSON_PUA module interface
 *
 * Copyright (C) 2016 Weave Communications
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This module was based on the Kazoo module created by 2600hz.
 * Thank you to 2600hz and their brilliant VoIP developers.
 *
 */

#include <stdio.h>
#include <string.h>

#include "../../core/mod_fix.h"
#include "../../core/sr_module.h"

#include "json_pua_mod.h"

MODULE_VERSION

static cmd_export_t cmds[] = {
	{"json_pua_publish", (cmd_function) json_pua_publish, 1, 0, 0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
		{"pua_include_entity", INT_PARAM, &dbn_include_entity},
		{"presentity_table", PARAM_STR, &json_presentity_table},
		{"db_url", PARAM_STR, &json_db_url},
		{"pua_mode", INT_PARAM, &dbn_pua_mode},
		{"db_table_lock_type", INT_PARAM, &db_table_lock_type},
		{0, 0, 0}
};

struct module_exports exports = {
		"json_pua",
		DEFAULT_DLFLAGS,             /* dlopen flags */
		cmds,						 /* Exported functions */
		params,						 /* Exported parameters */
		0,							 /* exported statistics */
		0,							 /* exported MI functions */
		0, 0,						 /* extra processes */
		mod_init,					 /* module initialization function */
		0,							 /* response function*/
		0,							 /* destroy function */
		mod_child_init				 /* per-child init function */
};

static int mod_init(void)
{
	if (dbn_pua_mode == 1) {
		json_db_url.len = json_db_url.s ? strlen(json_db_url.s) : 0;
		LM_DBG("db_url=%s/%d/%p\n", ZSW(json_db_url.s), json_db_url.len,json_db_url.s);
		json_presentity_table.len = strlen(json_presentity_table.s);

		if (json_db_url.len > 0) {

			/* binding to database module  */
			if (db_bind_mod(&json_db_url, &json_pa_dbf)) {
				LM_ERR("database module not found\n");
				return -1;
			}

			if (!DB_CAPABILITY(json_pa_dbf, DB_CAP_ALL)) {
				LM_ERR("database module does not implement all functions"
						" needed by JSON_PUA module\n");
				return -1;
			}

			json_pa_db = json_pa_dbf.init(&json_db_url);
			if (!json_pa_db) {
				LM_ERR("connection to database failed\n");
				return -1;
			}

			if (db_table_lock_type != 1) {
				db_table_lock = DB_LOCKING_NONE;
			}

			json_pa_dbf.close(json_pa_db);
			json_pa_db = NULL;
		}
	}

	return 0;
}

static int mod_child_init(int rank)
{
	if(dbn_pua_mode == 1) {
		if(json_pa_dbf.init == 0) {
			LM_CRIT("child_init: database not bound\n");
			return -1;
		}
		json_pa_db = json_pa_dbf.init(&json_db_url);
		if(!json_pa_db) {
			LM_ERR("child %d: unsuccessful connecting to database\n", rank);
			return -1;
		}

		if(json_pa_dbf.use_table(json_pa_db, &json_presentity_table) < 0) {
			LM_ERR("child %d:unsuccessful use_table presentity_table\n", rank);
			return -1;
		}
		LM_DBG("child %d: Database connection opened successfully\n", rank);
	}

	return 0;
}