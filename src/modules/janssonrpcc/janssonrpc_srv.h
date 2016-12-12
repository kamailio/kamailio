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

#ifndef _JANSSONRPC_SRV_H_
#define _JANSSONRPC_SRV_H_

#include "janssonrpc_server.h"

typedef struct jsonrpc_srv jsonrpc_srv_t;
struct jsonrpc_srv {
	str srv;
	unsigned int ttl;
	jsonrpc_server_group_t* cgroup;
	jsonrpc_srv_t* next;
};

typedef struct srv_cb_params {
	int cmd_pipe;
	unsigned int srv_ttl;
} srv_cb_params_t;

jsonrpc_srv_t* global_srv_list;

unsigned int jsonrpc_min_srv_ttl;

jsonrpc_srv_t* create_srv(str srv, str conn, unsigned int ttl);
void addto_srv_list(jsonrpc_srv_t* srv, jsonrpc_srv_t** list);
void refresh_srv_cb(unsigned int ticks, void* params);
void print_srv(jsonrpc_srv_t* list);

#define JSONRPC_DEFAULT_MIN_SRV_TTL 5
#define ABSOLUTE_MIN_SRV_TTL 1

#endif /* _JSONRPC_SRV_H_ */
