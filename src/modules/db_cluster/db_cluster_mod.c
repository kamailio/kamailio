/*
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

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/rpc_lookup.h"
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

static rpc_export_t rpc_methods[];

struct module_exports exports = {
	"db_cluster",		/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,					/* RPC method exports */
	0,					/* exported pseudo-variables */
	0,					/* response handling function */
	mod_init,			/* module initialization function */
	0,					/* per-child init function */
	0					/* module destroy function */
};


int mod_init(void)
{
	LM_DBG("Setting up DB cluster\n");

	if(rpc_register_array(rpc_methods) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

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
	dbb->insert_async     = db_cluster_insert_async;
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

str dbcl_connection_active_str = str_init("active");
str dbcl_connection_disabled_str = str_init("disabled");

str *dbcl_get_status_str(int status)
{
	if (status & DBCL_CON_INACTIVE)
		return &dbcl_connection_disabled_str;
	else
		return &dbcl_connection_active_str;

	return 0;
}

static int dbcl_active_count_connections(str cluster)
{
	dbcl_cls_t *cls=NULL;
	int count = 0;
	int i, j;

	cls = dbcl_get_cluster(&cluster);

	if(cls==NULL)
	{
		LM_ERR("cluster not found [%.*s]\n", cluster.len, cluster.s);
		return 0;
	}

	for(i=1; i<DBCL_PRIO_SIZE; i++)
	{
		for(j=0; j<cls->rlist[i].clen; j++)
		{
			if(cls->rlist[i].clist[j] != NULL)
			{
				LM_INFO("read connection [%.*s]\n", cls->rlist[i].clist[j]->name.len, cls->rlist[i].clist[j]->name.s);

				if(cls->rlist[i].clist[j]->sinfo==NULL)
					return 0;

				if(cls->rlist[i].clist[j]->sinfo->state == 0)
				{
					count++;
				}
			}
		}
	}

	return count;
}

static void dbcl_rpc_list_connections(rpc_t *rpc, void *c)
{
	void *handle;
	dbcl_cls_t *cls=NULL;
	str cluster;
	int i, j;
	unsigned int ticks;

	if (rpc->scan(c, "S", &cluster) != 1) {
		rpc->fault(c, 500, "Not enough parameters (cluster)");
		return;
	}

	cls = dbcl_get_cluster(&cluster);

	if(cls==NULL)
	{
		LM_ERR("cluster not found [%.*s]\n", cluster.len, cluster.s);
		rpc->fault(c, 500, "Cluster not found");
		return;
	}

	for(i=1; i<DBCL_PRIO_SIZE; i++)
	{
		for(j=0; j<cls->rlist[i].clen; j++)
		{
			if(cls->rlist[i].clist[j] != NULL)
			{
				LM_INFO("read connection [%.*s]\n", cls->rlist[i].clist[j]->name.len, cls->rlist[i].clist[j]->name.s);

				if(rpc->add(c, "{", &handle) < 0)
					goto error;

				if(cls->rlist[i].clist[j]->sinfo==NULL)
					goto error;

				if (cls->rlist[i].clist[j]->sinfo->aticks != 0)
					ticks = cls->rlist[i].clist[j]->sinfo->aticks - get_ticks();
				else
					ticks = 0;

				if(rpc->struct_add(handle, "SSdSdd", "connection", &cls->rlist[i].clist[j]->name, 
								"url", &cls->rlist[i].clist[j]->db_url,
								"flags", cls->rlist[i].clist[j]->flags,
								"state", dbcl_get_status_str(cls->rlist[i].clist[j]->sinfo->state),
								"ticks", ticks,
								"ref", cls->ref) 
						< 0)
					goto error;
			}
		}
	}

	return;

error:
	LM_ERR("Failed to add item to RPC response\n");
	rpc->fault(c, 500, "Server failure");
	return;
}

static void dbcl_rpc_disable_connection(rpc_t *rpc, void *c)
{
	dbcl_cls_t *cls=NULL;
	dbcl_con_t *con=NULL;
	str cluster;
	str connection;
	int seconds;

	if (rpc->scan(c, "SSd", &cluster, &connection, &seconds) < 3) {
		rpc->fault(c, 500, "Not enough parameters (cluster) (connection) (seconds)");
		return;
	}

	cls = dbcl_get_cluster(&cluster);

	if(cls==NULL)
	{
		LM_INFO("cluster not found [%.*s]\n", cluster.len, cluster.s);
		rpc->fault(c, 500, "Cluster not found");
		return;
	}

	con = dbcl_get_connection(&connection);

	if(con==NULL)
	{
		LM_INFO("connection not found [%.*s]\n", connection.len, connection.s);
		rpc->fault(c, 500, "Cluster connection not found");
		return;
	}

	if(con->sinfo==NULL)
		rpc->fault(c, 500, "Cluster state info missing.");
		return;

	/* Overwrite the number of seconds if the connection is already disabled. */
	if (con->sinfo->state & DBCL_CON_INACTIVE)
	{
		if(dbcl_disable_con(con, seconds) < 0) {
			rpc->fault(c, 500, "Failed disabling cluster connection.");
			return;
		}
		rpc->rpl_printf(c, "Ok. Cluster connection re-disabled.");
		return;
	}

	if (dbcl_active_count_connections(cluster) <= 1)
	{
		rpc->fault(c, 500, "Cannot disable last active connection in a cluster");
		return;
	}

	if(dbcl_disable_con(con, seconds) < 0) {
		rpc->fault(c, 500, "Failed disabling cluster connection.");
		return;
	};
	rpc->rpl_printf(c, "Ok. Cluster connection disabled.");
	return;
}

static void dbcl_rpc_enable_connection(rpc_t *rpc, void *c)
{
	dbcl_cls_t *cls=NULL;
	dbcl_con_t *con=NULL;
	str cluster;
	str connection;

	if (rpc->scan(c, "SS", &cluster, &connection) < 2) {
		rpc->fault(c, 500, "Not enough parameters (cluster) (connection)");
		return;
	}

	cls = dbcl_get_cluster(&cluster);

	if(cls==NULL)
	{   
		LM_INFO("cluster not found [%.*s]\n", cluster.len, cluster.s);
		rpc->fault(c, 500, "Cluster not found");
		return;
	}

	con = dbcl_get_connection(&connection);

	if(con==NULL)
	{   
		LM_INFO("connection not found [%.*s]\n", connection.len, connection.s);
		rpc->fault(c, 500, "Cluster connection not found");
		return;
	}
	if(dbcl_enable_con(con) < 0) {
		rpc->fault(c, 500, "Failed to enable cluster connection.");
		return;
	}
	rpc->rpl_printf(c, "Ok. Cluster connection enabled.");
	return;
}

static void dbcl_rpc_list_clusters(rpc_t *rpc, void *c)
{
	void *handle;
	dbcl_cls_t *cls=NULL;

	cls = dbcl_get_cluster_root();

	if(cls==NULL)
	{
		LM_ERR("root not set\n");
		rpc->fault(c, 500, "Clusters not found");
		return;
	}

	while(cls)
	{
		LM_INFO("cluster found ID [%u] NAME [%.*s]\n", cls->clsid, cls->name.len, cls->name.s);

		if(rpc->add(c, "{", &handle) < 0)
			goto error;

		if(rpc->struct_add(handle, "S", "cluster", &cls->name) < 0)
			goto error;

		cls = cls->next;
	}

	return;

error:
	LM_ERR("Failed to add item to RPC response\n");
	rpc->fault(c, 500, "Server failure");
	return;
}


static const char *dbcl_rpc_list_clusters_doc[2] = {"Print all clusters", 0};
static const char *dbcl_rpc_list_connections_doc[2] = {"Print all database connections of a cluster", 0};
static const char *dbcl_rpc_disable_connection_doc[2] = {"Disable a connection of a cluster for a period", 0};
static const char *dbcl_rpc_enable_connection_doc[2] = {"Enable a connection of a cluster", 0};

static rpc_export_t rpc_methods[] = {
	{"dbcl.list_clusters", dbcl_rpc_list_clusters, dbcl_rpc_list_clusters_doc, RET_ARRAY},
	{"dbcl.list_connections", dbcl_rpc_list_connections, dbcl_rpc_list_connections_doc, RET_ARRAY},
	{"dbcl.disable_connection", dbcl_rpc_disable_connection, dbcl_rpc_disable_connection_doc, 0},
	{"dbcl.enable_connection", dbcl_rpc_enable_connection, dbcl_rpc_enable_connection_doc, 0},
	{0, 0, 0, 0}
};
