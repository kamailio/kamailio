/* 
 * $Id$ 
 *
 * Generic db cluster module interface
 *
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
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

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../lib/srdb1/db.h"
#include "dbcl_data.h"
#include "dbcl_api.h"

MODULE_VERSION

int mod_init(void);
int db_cluster_bind_api(db_func_t *dbb);

int dbcl_con_param(modparam_t type, void *val);
int dbcl_cls_param(modparam_t type, void *val);

int dbcl_inactive_interval = 300;
int dbcl_max_query_length = 0;

/*! \brief
 * DB Cluster module interface
 */
static cmd_export_t cmds[] = {
	{"db_bind_api",         (cmd_function)db_cluster_bind_api,      0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

/*! \brief
 * Exported parameters
 */
static param_export_t params[] = {
	{"connection",  PARAM_STRING|USE_FUNC_PARAM, (void*)dbcl_con_param},
	{"cluster",     PARAM_STRING|USE_FUNC_PARAM, (void*)dbcl_cls_param},
	{"inactive_interval",     INT_PARAM,    &dbcl_inactive_interval},
	{"max_query_length",     INT_PARAM,    &dbcl_max_query_length},
	{0, 0, 0}
};

struct module_exports exports = {	
	"db_cluster",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,          /*  module parameters */
	0,               /* exported statistics */
	0,               /* exported MI functions */
	0,               /* exported pseudo-variables */
	0,               /* extra processes */
	mod_init,        /* module initialization function */
	0,               /* response function*/
	0,               /* destroy function */
	0                /* per-child init function */
};


int mod_init(void)
{
	LM_DBG("Setting up DB cluster\n");
	return 0;
}

int db_cluster_bind_api(db_func_t *dbb)
{
	if(dbb==NULL)
		return -1;

	memset(dbb, 0, sizeof(db_func_t));

	dbb->use_table        = db_cluster_use_table;
	dbb->init             = db_cluster_init;
	dbb->close            = db_cluster_close;
	dbb->query            = db_cluster_query;
	dbb->fetch_result     = db_cluster_fetch_result;
	dbb->raw_query        = db_cluster_raw_query;
	dbb->free_result      = db_cluster_free_result;
	dbb->insert           = db_cluster_insert;
	dbb->delete           = db_cluster_delete;
	dbb->update           = db_cluster_update;
	dbb->replace          = db_cluster_replace;
	dbb->last_inserted_id = db_cluster_last_inserted_id;
	dbb->insert_update    = db_cluster_insert_update;
	dbb->insert_delayed   = db_cluster_insert_delayed;
	dbb->affected_rows    = db_cluster_affected_rows;

	return 0;
}

int dbcl_con_param(modparam_t type, void *val)
{
	return dbcl_parse_con_param((char*)val);
}

int dbcl_cls_param(modparam_t type, void *val)
{
	return dbcl_parse_cls_param((char*)val);
}
