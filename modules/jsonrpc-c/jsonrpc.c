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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../mem/mem.h"

#include "jsonrpc.h"


jsonrpc_request_t * request_table[JSONRPC_DEFAULT_HTABLE_SIZE] = {0};
int next_id = 1;

jsonrpc_request_t* get_request(int id);
int store_request(jsonrpc_request_t* req);


jsonrpc_request_t* build_jsonrpc_request(char *method, json_object *params, char *cbdata, int (*cbfunc)(json_object*, char*, int))
{
	if (next_id>JSONRPC_MAX_ID) {
		next_id = 1;
	} else {
		next_id++;
	}

	jsonrpc_request_t *req = pkg_malloc(sizeof(jsonrpc_request_t));
	if (!req) {
		LM_ERR("Out of memory!");
		return 0;
	}
	req->id = next_id;
	req->cbfunc = cbfunc;
	req->cbdata = cbdata;
	req->next = NULL;
	req->timer_ev = NULL;
	if (!store_request(req))
		return 0;

	req->payload = json_object_new_object();

	json_object_object_add(req->payload, "id", json_object_new_int(next_id));
	json_object_object_add(req->payload, "jsonrpc", json_object_new_string("2.0"));
	json_object_object_add(req->payload, "method", json_object_new_string(method));
	json_object_object_add(req->payload, "params", params);

	return req;
}

json_object* build_jsonrpc_notification(char *method, json_object *params) 
{
	json_object *req = json_object_new_object();
	json_object_object_add(req, "jsonrpc", json_object_new_string("2.0"));
	json_object_object_add(req, "method", json_object_new_string(method));
	json_object_object_add(req, "params", params);

	return req; 
}


int handle_jsonrpc_response(json_object *response)
{
	jsonrpc_request_t *req;	
	json_object *_id = json_object_object_get(response, "id");
	int id = json_object_get_int(_id);
	
	if (!(req = get_request(id))) {
		json_object_put(response);
		return -1;
	}

	json_object *result = json_object_object_get(response, "result");
	
	if (result) {
		req->cbfunc(result, req->cbdata, 0);
	} else {
		json_object *error = json_object_object_get(response, "error");
		if (error) {
			req->cbfunc(error, req->cbdata, 1);
		} else {
			LM_ERR("Response received with neither a result nor an error.\n");
			return -1;
		}
	}
	
	if (req->timer_ev) {
		close(req->timerfd);
		event_del(req->timer_ev);
		pkg_free(req->timer_ev);
	} else {
		LM_ERR("No timer for req id %d\n", id);
	}
	pkg_free(req);
	return 1;
}

int id_hash(int id) {
	return (id % JSONRPC_DEFAULT_HTABLE_SIZE);
}

jsonrpc_request_t* get_request(int id) {
	int key = id_hash(id);
	jsonrpc_request_t *req, *prev_req = NULL;
	req = request_table[key];
	
	while (req && req->id != id) {
		prev_req = req;
		if (!(req = req->next)) {
			break;
		};
	}
	
	if (req && req->id == id) {
		if (prev_req != NULL) {
			prev_req-> next = req->next;
		} else {
			request_table[key] = NULL;
		}
		return req;
	}
	return 0;
}

void void_jsonrpc_request(int id) {
	get_request(id);
}

int store_request(jsonrpc_request_t* req) {
	int key = id_hash(req->id);
	jsonrpc_request_t* existing;

	if ((existing = request_table[key])) { /* collision */
		jsonrpc_request_t* i;
		for(i=existing; i; i=i->next) {
			if (i == NULL) {
				i = req;
				LM_ERR("!!!!!!!");
				return 1;
			}
			if (i->next == NULL) {
				i->next = req;
				return 1;
			}
		}
	} else {
		request_table[key] = req;
	}
	return 1;
}

