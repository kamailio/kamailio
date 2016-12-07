/**
 * Copyright (C) 2013 Flowroute LLC (flowroute.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "../../sr_module.h"
#include "../../route.h"
#include "../../route_struct.h"
#include "../../resolve.h"
#include "../../parser/parse_param.h"
#include "../../mem/mem.h"
#include "../../lvalue.h"
#include "../../str.h"

#include "janssonrpc.h"
#include "janssonrpc_srv.h"
#include "janssonrpc_request.h"
#include "janssonrpc_io.h"
#include "janssonrpc_server.h"

int refresh_srv(jsonrpc_srv_t* srv_obj)
{
	DEBUG("Refreshing SRV for %.*s\n", STR(srv_obj->srv));
	int retval = 0;

	if(!srv_obj) {
		ERR("Trying to refresh NULL SRV\n");
		return -1;
	}

	unsigned int ttl = ABSOLUTE_MIN_SRV_TTL;
	str srv = srv_obj->srv;
	jsonrpc_server_group_t* conn_group = srv_obj->cgroup;

	if(!conn_group) {
		ERR("SRV (%.*s) has no connections\n", STR(srv));
		return -1;
	}

	struct rdata *l, *head;
	struct srv_rdata *srv_record;
	str name;

	jsonrpc_server_group_t* new_grp = NULL;

	// dns lookup
	head = get_record(srv.s, T_SRV, RES_AR);
	if (head == NULL) {
		ERR("No SRV record returned for %.*s\n", STR(srv));
		return -1;
	}

	// get all the servers from the srv record
	server_list_t* new_servers = NULL;
	jsonrpc_server_t* new_server = NULL;
	server_list_t* rm_servers = NULL;
	jsonrpc_server_t* rm_server = NULL;
	int iter = 0;
	for (l=head, iter=0; l; l=l->next, iter++) {
		if (l->type != T_SRV)
			continue;
		srv_record = (struct srv_rdata*)l->rdata;
		if (srv_record == NULL) {
			ERR("BUG: null rdata\n");
			return -1;
		}

		if (l->ttl < jsonrpc_min_srv_ttl) {
			ttl = jsonrpc_min_srv_ttl;
		} else {
			ttl = l->ttl;
		}

		srv_obj->ttl = ttl;

		name.s = srv_record->name;
		name.len = srv_record->name_len;

		DBG("server %s\n", srv_record->name);

		jsonrpc_server_group_t* cgroup = NULL;
		for(cgroup=conn_group; cgroup!=NULL; cgroup=cgroup->next) {
			new_server = create_server();
			CHECK_MALLOC(new_server);

			new_server->conn = shm_strdup(cgroup->conn);
			CHECK_MALLOC(new_server->conn.s);

			new_server->addr = shm_strdup(name);
			CHECK_MALLOC(new_server->addr.s);

			new_server->srv = shm_strdup(srv);
			CHECK_MALLOC(new_server->srv.s);

			new_server->port = srv_record->port;
			new_server->priority = srv_record->priority;
			new_server->weight = srv_record->weight;
			new_server->ttl = ttl;
			new_server->added = false;

			addto_server_list(new_server, &new_servers);
		}
	}

	if(iter <= 0) goto end;

	/* aquire global_server_group lock */
	/* this lock is only released when the old global_server_group
	 * is freed in the IO process */
	lock_get(jsonrpc_server_group_lock); /* blocking */
	//print_group(global_server_group); /* debug */


	INIT_SERVER_LOOP

	// copy existing servers
	server_list_t* node;
	FOREACH_SERVER_IN(global_server_group)
		server->added = false;
		if(STR_EQ(server->srv, srv)) {
			for(node=new_servers; node!=NULL; node=node->next) {
				new_server = node->server;
				if(server_eq(new_server, server)) {
					new_server->added = true;
					server->added = true;
					server->ttl = srv_obj->ttl;
					jsonrpc_add_server(server, &new_grp);
				}
			}
		} else {
			server->added = true;
			jsonrpc_add_server(server, &new_grp);
		}
	ENDFOR

	FOREACH_SERVER_IN(global_server_group)
		if(server->added == false) {
			addto_server_list(server, &rm_servers);
		}
	ENDFOR

	// add and connect new servers
	for(node=new_servers; node!=NULL; node=node->next) {
		new_server = node->server;
		if(new_server->added == false) {

			jsonrpc_add_server(new_server, &new_grp);

			if(send_pipe_cmd(CMD_CONNECT, new_server) <0) {
				print_server(new_server);
			}

		} else {
			free_server(new_server);
		}
	}

	// close old servers
	for(node=rm_servers; node!=NULL; node=node->next) {

		rm_server = node->server;

		if(send_pipe_cmd(CMD_CLOSE, rm_server) <0) {
			print_server(rm_server);
		}
	}

	if(send_pipe_cmd(CMD_UPDATE_SERVER_GROUP, new_grp)<0) {
		free_server_group(&new_grp);
		lock_release(jsonrpc_server_group_lock);
	}

end:
	// free server lists
	free_server_list(new_servers);
	free_server_list(rm_servers);

	return retval;
}

void free_srv(jsonrpc_srv_t* srv)
{
	if(!srv)
		return;

	CHECK_AND_FREE(srv->srv.s);

	free_server_group(&(srv->cgroup));
}

jsonrpc_srv_t* create_srv(str srv, str conn, unsigned int ttl)
{
	jsonrpc_srv_t* new_srv = shm_malloc(sizeof(jsonrpc_srv_t));
	if(!new_srv) goto error;
	new_srv->srv = shm_strdup(srv);

	if (ttl < jsonrpc_min_srv_ttl) {
		new_srv->ttl = jsonrpc_min_srv_ttl;
	} else {
		new_srv->ttl = ttl;
	}

	if(create_server_group(CONN_GROUP, &(new_srv->cgroup))<0) goto error;
	new_srv->cgroup->conn = shm_strdup(conn);
	if(!(new_srv->cgroup->conn.s)) return NULL;

	return new_srv;
error:
	ERR("create_srv failed\n");
	free_srv(new_srv);
	return NULL;
}

void refresh_srv_cb(unsigned int ticks, void* params)
{
	if(!params) {
		ERR("params is (null)\n");
		return;
	}

	if(!global_srv_list) {
		return;
	}

	srv_cb_params_t* p = (srv_cb_params_t*)params;

	cmd_pipe = p->cmd_pipe;
	jsonrpc_min_srv_ttl = p->srv_ttl;

	if(cmd_pipe == 0) {
		ERR("cmd_pipe is not set\n");
		return;
	}

	jsonrpc_srv_t* srv;
	for(srv=global_srv_list; srv!=NULL; srv=srv->next) {
		if(ticks % srv->ttl == 0) {
			refresh_srv(srv);
		}
	}

}

void addto_srv_list(jsonrpc_srv_t* srv, jsonrpc_srv_t** list)
{
	if (*list == NULL) {
		*list = srv;
		return;
	}

	jsonrpc_srv_t* node = *list;
	jsonrpc_srv_t* prev = *list;
	jsonrpc_server_group_t* cgroup;
	jsonrpc_server_group_t* cprev;
	for(node=*list; node!=NULL; prev=node, node=node->next) {
		if(STR_EQ(srv->srv, node->srv)) {
			for(cgroup=node->cgroup, cprev=node->cgroup;
					cgroup!=NULL;
					cprev=cgroup, cgroup=cgroup->next) {
				if(STR_EQ(cgroup->conn, srv->cgroup->conn)) {
					INFO("Trying to add identical srv\n");
					goto clean;
				}
			}
			if(create_server_group(CONN_GROUP, &(cprev->next))<0) goto clean;
			cprev->next->conn = shm_strdup(srv->cgroup->conn);
			CHECK_MALLOC_GOTO(cprev->next->conn.s, clean);
			node->ttl = srv->ttl;
			goto clean;
		}
	}

	prev->next = srv;
	return;
clean:
	free_srv(srv);
}

void print_srv(jsonrpc_srv_t* list)
{
	INFO("------SRV list------\n");
	jsonrpc_srv_t* node = NULL;
	for(node=list; node!=NULL; node=node->next) {
		INFO("-----------------\n");
		INFO("| srv: %.*s\n", STR(node->srv));
		INFO("| ttl: %d\n", node->ttl);
		print_group(&(node->cgroup));
		INFO("-----------------\n");
	}
}

