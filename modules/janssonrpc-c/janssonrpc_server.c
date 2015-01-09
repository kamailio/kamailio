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
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <event.h>

#include "../../sr_module.h"
#include "../../route.h"
#include "../../route_struct.h"
#include "../../resolve.h"
#include "../../parser/parse_param.h"
#include "../../mem/mem.h"
#include "../../lvalue.h"

#include "netstring.h"
#include "janssonrpc.h"
#include "janssonrpc_request.h"
#include "janssonrpc_io.h"
#include "janssonrpc_srv.h"
#include "janssonrpc_server.h"
#include "janssonrpc_connect.h"

/* used for debugging only */
void print_server(jsonrpc_server_t* server)
{
	INFO("\t----- server ------\n");
	INFO("\t|pointer: %p\n", server);
	INFO("\t|conn: %.*s\n", STR(server->conn));
	INFO("\t|addr: %.*s\n", STR(server->addr));
	switch (server->status) {
	case JSONRPC_SERVER_CONNECTED:
		INFO("\t|status: connected\n");
		break;
	case JSONRPC_SERVER_DISCONNECTED:
		INFO("\t|status: disconnected\n");
		break;
	case JSONRPC_SERVER_FAILURE:
		INFO("\t|status: failure\n");
		break;
	case JSONRPC_SERVER_CLOSING:
		INFO("\t|status: closing\n");
		break;
	case JSONRPC_SERVER_RECONNECTING:
		INFO("\t|status: reconnecting\n");
		break;
	default:
		INFO("\t|status: invalid (%d)\n", server->status);
		break;
	}
	INFO("\t|srv: %.*s\n", STR(server->srv));
	INFO("\t|ttl: %d\n", server->ttl);
	INFO("\t|port: %d\n", server->port);
	INFO("\t|priority: %d\n", server->priority);
	INFO("\t|weight: %d\n", server->weight);
	INFO("\t|hwm: %d\n", server->hwm);
	INFO("\t|req_count: %d\n", server->req_count);
	if(server->added) {
		INFO("\t|added: true\n");
	} else {
		INFO("\t|added: false\n");
	}
	INFO("\t-------------------\n");
}

/* used for debugging only */
void print_group(jsonrpc_server_group_t** group)
{
	jsonrpc_server_group_t* grp = NULL;

	INFO("group addr is %p\n", group);

	if(group == NULL)
		return;

	for (grp=*group; grp != NULL; grp=grp->next) {
		switch(grp->type) {
		case CONN_GROUP:
			INFO("Connection group: %.*s\n", STR(grp->conn));
			print_group(&(grp->sub_group));
			break;
		case PRIORITY_GROUP:
			INFO("Priority group: %d\n", grp->priority);
			print_group(&(grp->sub_group));
			break;
		case WEIGHT_GROUP:
			INFO("Weight group: %d\n", grp->weight);
			print_server(grp->server);
			break;
		}
	}
}

int jsonrpc_parse_server(char* server_s, jsonrpc_server_group_t **group_ptr)
{
	if(group_ptr == NULL) {
		ERR("Trying to add server to null group ptr\n");
		return -1;
	}

	str s;
	param_hooks_t phooks;
	param_t* pit=NULL;
	param_t* freeme=NULL;
	str conn;
	str addr;
	addr.s = NULL;
	str srv;
	srv.s = NULL;

	unsigned int priority = JSONRPC_DEFAULT_PRIORITY;
	unsigned int weight = JSONRPC_DEFAULT_WEIGHT;
	unsigned int hwm = JSONRPC_DEFAULT_HWM;
	unsigned int port = 0;

	s.s = server_s;
	s.len = strlen(server_s);
	if (s.s[s.len-1] == ';')
		s.len--;

	if (parse_params(&s, CLASS_ANY, &phooks, &pit)<0) {
		ERR("Failed parsing params value\n");
		return -1;
	}

	freeme = pit;

	for (; pit;pit=pit->next)
	{
		if PIT_MATCHES("conn") {
			conn = shm_strdup(pit->body);
			CHECK_MALLOC(conn.s);

		} else if PIT_MATCHES("srv") {
			srv = shm_strdup(pit->body);
			CHECK_MALLOC(srv.s);

		} else if PIT_MATCHES("addr") {
			addr = shm_strdup(pit->body);
			CHECK_MALLOC(addr.s);

		} else if PIT_MATCHES("port") {
			port = atoi(pit->body.s);

		} else if PIT_MATCHES("priority") {
			priority = atoi(pit->body.s);

		} else if PIT_MATCHES("weight") {
			weight = atoi(pit->body.s);

		} else if PIT_MATCHES("hwm") {
			hwm = atoi(pit->body.s);

		} else if PIT_MATCHES("proto") {
			if(strncmp(pit->body.s, "tcp", sizeof("tcp")-1) != 0) {
				ERR("Unsupported proto=%.*s. Only tcp is supported.\n",
						STR(pit->body));
				goto error;
			}
		} else {
			ERR("Unrecognized parameter: %.*s\n", STR(pit->name));
			goto error;
		}

		DEBUG("%.*s = %.*s\n", STR(pit->name), STR(pit->body));
	}

	if(conn.s == NULL) {
		ERR("No conn defined! conn parameter is required.\n");
		goto error;
	}

	if (srv.s != NULL) {
		if (addr.s != NULL
			|| port != 0
			|| weight != JSONRPC_DEFAULT_WEIGHT
			|| priority != JSONRPC_DEFAULT_PRIORITY) {
			ERR("addr, port, weight, and priority are not supported when using srv\n");
			goto error;
		}

		if (jsonrpc_server_from_srv(conn, srv, hwm, group_ptr)<0) goto error;

	} else {

		if (addr.s == NULL || port == 0) {
			ERR("no address/port defined\n");
			goto error;
		}

		jsonrpc_server_t* server = create_server();
		CHECK_MALLOC(server);

		server->conn = conn;
		server->addr = addr;
		server->port = port;
		server->priority = priority;
		server->weight = weight;
		server->hwm = hwm;

		if(jsonrpc_add_server(server, group_ptr)<0) goto error;
	}

	//print_group(group_ptr); /* debug */

	CHECK_AND_FREE(srv.s);
	if (freeme) free_params(freeme);
	return 0;

error:
	CHECK_AND_FREE(srv.s);
	if (freeme) free_params(freeme);
	return -1;
}

int jsonrpc_server_from_srv(str conn, str srv,
		unsigned int hwm, jsonrpc_server_group_t** group_ptr)
{
	struct rdata *l, *head;
	struct srv_rdata *srv_record;
	str name;
	unsigned int ttl = jsonrpc_min_srv_ttl;

	jsonrpc_server_t* server = NULL;

	resolv_init();

	head = get_record(srv.s, T_SRV, RES_AR);
	if (head == NULL) {
		ERR("No SRV record returned for %.*s\n", STR(srv));
		goto error;
	}
	for (l=head; l; l=l->next) {
		if (l->type != T_SRV)
			continue;
		srv_record = (struct srv_rdata*)l->rdata;
		if (srv_record == NULL) {
			ERR("BUG: null rdata\n");
			goto error;
		}

		if (l->ttl < jsonrpc_min_srv_ttl) {
			ttl = jsonrpc_min_srv_ttl;
		} else {
			ttl = l->ttl;
		}

		name.s = srv_record->name;
		name.len = srv_record->name_len;

		DBG("server %s\n", srv_record->name);

		server = create_server();
		CHECK_MALLOC(server);

		server->conn = shm_strdup(conn);
		CHECK_MALLOC_GOTO(server->conn.s, error);

		server->addr = shm_strdup(name);
		CHECK_MALLOC_GOTO(server->addr.s, error);

		server->srv = shm_strdup(srv);
		CHECK_MALLOC_GOTO(server->srv.s, error);

		server->port = srv_record->port;
		server->priority = srv_record->priority;
		server->weight = srv_record->weight;
		server->ttl = ttl;
		server->hwm = hwm;

		if(jsonrpc_add_server(server, group_ptr)<0) goto error;
	}

	jsonrpc_srv_t* new_srv = create_srv(srv, conn, ttl);
	addto_srv_list(new_srv, &global_srv_list);

	free_rdata_list(head);

	return 0;
error:
	CHECK_AND_FREE(server);
	if (head) free_rdata_list(head);

	return -1;
}

int create_server_group(server_group_t type, jsonrpc_server_group_t** grp)
{
	if(grp == NULL) {
		ERR("Trying to dereference null group pointer\n");
		return -1;
	}

	jsonrpc_server_group_t* new_grp =
		shm_malloc(sizeof(jsonrpc_server_group_t));
	CHECK_MALLOC(new_grp);

	switch(type) {
	case CONN_GROUP:
		DEBUG("Creating new connection group\n");
		new_grp->conn.s = NULL;
		new_grp->conn.len = 0;
		break;
	case PRIORITY_GROUP:
		DEBUG("Creating new priority group\n");
		new_grp->priority = JSONRPC_DEFAULT_PRIORITY;
		break;
	case WEIGHT_GROUP:
		DEBUG("Creating new weight group\n");
		new_grp->server = NULL;
		new_grp->weight = JSONRPC_DEFAULT_WEIGHT;
		break;
	}

	new_grp->next = NULL;
	new_grp->sub_group = NULL;
	new_grp->type = type;
	*grp = new_grp;
	return 0;
}

void free_server_group(jsonrpc_server_group_t** grp)
{
	if(grp == NULL)
		return;

	jsonrpc_server_group_t* next = NULL;
	jsonrpc_server_group_t* cgroup = NULL;
	jsonrpc_server_group_t* pgroup = NULL;
	jsonrpc_server_group_t* wgroup = NULL;

	cgroup=*grp;
	while(cgroup!=NULL) {
		pgroup=cgroup->sub_group;
		while(pgroup!=NULL) {
			wgroup=pgroup->sub_group;
			while(wgroup!=NULL) {
				next = wgroup->next;
				CHECK_AND_FREE(wgroup);
				wgroup = next;
			}
			next = pgroup->next;
			CHECK_AND_FREE(pgroup);
			pgroup = next;
		}
		next = cgroup->next;
		CHECK_AND_FREE(cgroup->conn.s);
		CHECK_AND_FREE(cgroup);
		cgroup = next;
	}
}

int insert_server_group(jsonrpc_server_group_t* new_grp,
		jsonrpc_server_group_t** parent)
{
	if(parent == NULL) {
		ERR("Trying to insert into NULL group\n");
		return -1;
	}

	jsonrpc_server_group_t* head = *parent;

	if (head == NULL) {
		*parent = new_grp;
	} else {
		if (new_grp->type != head->type) {
			ERR("Inserting group (%d) into the wrong type of list (%d)\n",
				new_grp->type, head->type);
			return -1;
		}

		jsonrpc_server_group_t* current = head;
		jsonrpc_server_group_t** prev = parent;

		while (1) {
			if(new_grp->type == PRIORITY_GROUP
					&& new_grp->priority < current->priority) {
			 /* Priority groups are organized in ascending order.*/
				new_grp->next = current;
				*prev = new_grp;
				break;
			} else if (new_grp->type == WEIGHT_GROUP ) {
			/* Weight groups are special in how they are organized in order
		     * to facilitate load balancing and weighted random selection.
			 *
			 * The weight in the head of a weight group list represents
			 * the total weight of the list. Subsequent nodes represent the
			 * remaining total.
			 *
			 * In order to achieve this, the weight to be inserted is added
			 * to each node that is passed before insertion.
			 *
			 * Weight groups are organized in descending order.
			 *
			 * The actual weight of a node can be found in its server.
			 * */
				if(new_grp->server == NULL) {
					ERR("Trying to insert an empty weight group.\n");
					return -1;
				}
				if(new_grp->server->weight != new_grp->weight) {
					ERR("Weight of the new node (%d) doesn't match its server (%d). This is a bug. Please report this to the maintainer.\n",
							new_grp->server->weight, new_grp->weight);
					return -1;
				}
				if(new_grp->weight > current->server->weight) {
					new_grp->weight += current->weight;
					new_grp->next = current;
					*prev = new_grp;
					break;
				} else {
					current->weight += new_grp->weight;
				}
			}

			if(current->next == NULL) {
				current->next = new_grp;
				break;
			}
			prev = &((*prev)->next); // This is madness. Madness? THIS IS POINTERS!
			current = current->next;
		}
	}
	return 0;
}

unsigned int server_group_size(jsonrpc_server_group_t* grp)
{
	unsigned int size = 0;
	for(;grp != NULL; grp=grp->next) {
		size++;
	}
	return size;
}

jsonrpc_server_t* create_server()
{
	jsonrpc_server_t* server = shm_malloc(sizeof(jsonrpc_server_t));
	CHECK_MALLOC_NULL(server);
	memset(server, 0, sizeof(jsonrpc_server_t));

	server->priority = JSONRPC_DEFAULT_PRIORITY;
	server->weight = JSONRPC_DEFAULT_WEIGHT;
	server->status = JSONRPC_SERVER_DISCONNECTED;

	return server;
}

void free_server(jsonrpc_server_t* server)
{
	if(!server)
		return;

	CHECK_AND_FREE(server->conn.s);
	CHECK_AND_FREE(server->addr.s);
	CHECK_AND_FREE(server->srv.s);

	if ((server->buffer)!=NULL) free_netstring(server->buffer);
	memset(server, 0, sizeof(jsonrpc_server_t));
	shm_free(server);
	server = NULL;
}

int server_eq(jsonrpc_server_t* a, jsonrpc_server_t* b)
{
	if(!a || !b)
		return 0;

	if(!STR_EQ(a->conn, b->conn)) return 0;
	if(!STR_EQ(a->srv, b->srv)) return 0;
	if(!STR_EQ(a->addr, b->addr)) return 0;
	if(a->port != b->port) return 0;
	if(a->priority != b->priority) return 0;
	if(a->weight != b->weight) return 0;

	return 1;
}

int jsonrpc_add_server(jsonrpc_server_t* server, jsonrpc_server_group_t** group_ptr)
{
	jsonrpc_server_group_t* conn_grp = NULL;
	jsonrpc_server_group_t* priority_grp = NULL;
	jsonrpc_server_group_t* weight_grp = NULL;

	if(group_ptr == NULL) {
		ERR("Trying to add server to null group\n");
		return -1;
	}

	if(create_server_group(WEIGHT_GROUP, &weight_grp) < 0) goto error;

	weight_grp->weight = server->weight;
	weight_grp->server = server;

	/* find conn group */
	for (conn_grp=*group_ptr; conn_grp != NULL; conn_grp=conn_grp->next) {
		if (strncmp(conn_grp->conn.s, server->conn.s, server->conn.len) == 0)
			break;
	}

	if (conn_grp == NULL) {
		if(create_server_group(CONN_GROUP, &conn_grp) < 0) goto error;
		if(create_server_group(PRIORITY_GROUP, &priority_grp) < 0) goto error;

		priority_grp->priority = server->priority;
		priority_grp->sub_group = weight_grp;

		conn_grp->conn = shm_strdup(server->conn);
		CHECK_MALLOC_GOTO(conn_grp->conn.s, error);

		conn_grp->sub_group = priority_grp;
		if(insert_server_group(conn_grp, group_ptr) < 0) goto error;
		goto success;
	}

	/* find priority group */
	for (priority_grp=conn_grp->sub_group;
			priority_grp != NULL;
			priority_grp=priority_grp->next) {
		if (priority_grp->priority == server->priority) break;
	}

	if (priority_grp == NULL) {
		if(create_server_group(PRIORITY_GROUP, &priority_grp) < 0) goto error;

		priority_grp->priority = server->priority;
		priority_grp->sub_group = weight_grp;

		if(insert_server_group(priority_grp, &(conn_grp->sub_group)) < 0) goto error;
		goto success;
	}

	if(insert_server_group(weight_grp, &(priority_grp->sub_group)) < 0) goto error;

success:
	return 0;
error:
	ERR("Failed to add server: %s, %s, %d\n",
			server->conn.s, server->addr.s, server->port);
	CHECK_AND_FREE(conn_grp);
	CHECK_AND_FREE(priority_grp);
	CHECK_AND_FREE(weight_grp);
	CHECK_AND_FREE(server);
	return -1;
}

void addto_server_list(jsonrpc_server_t* server, server_list_t** list)
{
	server_list_t* new_node = (server_list_t*)pkg_malloc(sizeof(server_list_t));
	CHECK_MALLOC_VOID(new_node);

	new_node->server = server;
	new_node->next = NULL;

	if (*list == NULL) {
		*list = new_node;
		return;
	}

	server_list_t* node = *list;
	for(; node->next!=NULL; node=node->next);

	node->next = new_node;
}

void free_server_list(server_list_t* list)
{
	if (!list)
		return;

	server_list_t* node = NULL;
	for(node=list; node!=NULL; node=node->next)
	{
		pkg_free(node);
	}
}

void close_server(jsonrpc_server_t* server)
{
	if(!server)
		return;

	INFO("Closing server %.*s:%d for conn %.*s.\n",
			STR(server->addr), server->port, STR(server->conn));
	force_disconnect(server);

	free_server(server);
}

