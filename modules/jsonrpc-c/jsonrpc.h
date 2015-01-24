/*
 *
 * Copyright (C) 2011 Flowroute LLC (flowroute.com)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _JSONRPC_H_
#define _JSONRPC_H_

#define JSONRPC_DEFAULT_HTABLE_SIZE 500
#define JSONRPC_MAX_ID 1000000

#define JSONRPC_INTERNAL_SERVER_ERROR -32603

#include <json.h>
#include <event.h>

typedef struct jsonrpc_request jsonrpc_request_t;

struct jsonrpc_request {
	int id, timerfd;
	jsonrpc_request_t *next;
	int (*cbfunc)(json_object*, char*, int);
	char *cbdata;
	json_object *payload;
	struct event *timer_ev; 
};

json_object* build_jsonrpc_notification(char *method, json_object *params); 
jsonrpc_request_t* build_jsonrpc_request(char *method, json_object *params, char *cbdata, int (*cbfunc)(json_object*, char*, int));
int handle_jsonrpc_response(json_object *response);
void void_jsonrpc_request(int id);
#endif /* _JSONRPC_H_ */

