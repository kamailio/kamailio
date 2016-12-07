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

#ifndef _JANSSONRPC_SERVER_H_
#define _JANSSONRPC_SERVER_H_

#include <stdbool.h>
#include <event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include "../../locking.h"
#include "netstring.h"

/* interval (in seconds) at which failed servers are retried */
#define JSONRPC_RECONNECT_INTERVAL  3

/* default values */
#define JSONRPC_DEFAULT_PRIORITY 0
#define JSONRPC_DEFAULT_WEIGHT 1
#define JSONRPC_DEFAULT_HWM 0 /* unlimited */

typedef struct jsonrpc_server {
	str conn, addr, srv; /* shared mem */
	int port;
	unsigned int  status, ttl, hwm;
	unsigned int  req_count;
	unsigned int priority, weight;
	bool added;
	struct bufferevent* bev; /* local mem */
	netstring_t* buffer;
} jsonrpc_server_t;

typedef enum {
	CONN_GROUP,
	PRIORITY_GROUP,
	WEIGHT_GROUP
} server_group_t;

/* servers are organized in the following order:
 * 1) conn
 * 2) priority
 * 3) weight
 ***/
typedef struct jsonrpc_server_group {
	server_group_t type;
	struct jsonrpc_server_group* sub_group; // NULL when type is WEIGHT_GROUP
	union {
		str conn; // when type is CONN_GROUP
		unsigned int priority; // when type is PRIORITY_GROUP
		unsigned int weight; //when type is WEIGHT_GROUP
	};
	jsonrpc_server_t* server; // only when type is WEIGHT_GROUP
	struct jsonrpc_server_group* next;
} jsonrpc_server_group_t;

gen_lock_t* jsonrpc_server_group_lock;

typedef struct server_list {
	jsonrpc_server_t* server;
	struct server_list* next;
} server_list_t;

/* where all the servers are stored */
jsonrpc_server_group_t** global_server_group;

int  jsonrpc_parse_server(char *_server, jsonrpc_server_group_t** group_ptr);
int jsonrpc_server_from_srv(str conn, str srv,
		unsigned int hwm, jsonrpc_server_group_t** group_ptr);

void close_server(jsonrpc_server_t* server);
/* Do not call close_server() from outside the IO process.
 * Server's have a bufferevent that is part of local memory and free'd
 * at disconnect */

jsonrpc_server_t* create_server();
void free_server(jsonrpc_server_t* server);
int create_server_group(server_group_t type, jsonrpc_server_group_t** new_grp);
int jsonrpc_add_server(jsonrpc_server_t* server, jsonrpc_server_group_t** group);
unsigned int  server_group_size(jsonrpc_server_group_t* group);
void free_server_group(jsonrpc_server_group_t** grp);
int server_eq(jsonrpc_server_t* a, jsonrpc_server_t* b);
void addto_server_list(jsonrpc_server_t* server, server_list_t** list);
void free_server_list(server_list_t* list);

#define INIT_SERVER_LOOP  \
	jsonrpc_server_group_t* cgroup = NULL; \
	jsonrpc_server_group_t* pgroup = NULL; \
	jsonrpc_server_group_t* wgroup = NULL; \
	jsonrpc_server_t* server = NULL;

#define FOREACH_SERVER_IN(ii) \
	if(ii == NULL) { \
		cgroup = NULL; \
	} else { \
		cgroup = *(ii); \
	} \
	pgroup = NULL; \
	wgroup = NULL; \
	server = NULL; \
	for(; cgroup!=NULL; cgroup=cgroup->next) { \
		for(pgroup=cgroup->sub_group; pgroup!=NULL; pgroup=pgroup->next) { \
			for(wgroup=pgroup->sub_group; wgroup!=NULL; wgroup=wgroup->next) { \
				server = wgroup->server;

#define ENDFOR }}}

/* debugging only */
void print_server(jsonrpc_server_t* server);
void print_group(jsonrpc_server_group_t** group);

#endif /* _JSONRPC_SERVER_H_ */
