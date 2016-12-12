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

#ifndef _JANSSONRPC_REQUEST_H_
#define _JANSSONRPC_REQUEST_H_

#include "janssonrpc_io.h"
#include "janssonrpc_server.h"

#define JSONRPC_DEFAULT_HTABLE_SIZE 500
#define JSONRPC_MAX_ID 1000000
#define RETRY_MAX_TIME 60000 /* milliseconds */

typedef enum {
	RPC_REQUEST,
	RPC_NOTIFICATION
} rpc_type;

typedef struct jsonrpc_request jsonrpc_request_t;
struct jsonrpc_request {
	rpc_type type;
	int id;
	jsonrpc_request_t *next; /* pkg */
	jsonrpc_server_t* server; /* shm */
	jsonrpc_req_cmd_t* cmd; /* shm */
	json_t* payload;
	struct event* timeout_ev; /* pkg */
	struct event* retry_ev; /* pkg */
	int retry;
	unsigned int ntries;
	unsigned int timeout;
};

jsonrpc_request_t* request_table[JSONRPC_DEFAULT_HTABLE_SIZE];

jsonrpc_request_t* create_request(jsonrpc_req_cmd_t* cmd);
void print_request(jsonrpc_request_t* req);
jsonrpc_request_t* pop_request(int id);
unsigned int requests_using_server(jsonrpc_server_t* server);
void free_request(jsonrpc_request_t* req);
int schedule_retry(jsonrpc_request_t* req);

int jsonrpc_send(str conn, jsonrpc_request_t* req, bool notify_only);
void fail_request(int code, jsonrpc_request_t* req, char* error_str);

#endif /* _JSONRPC_H_ */
