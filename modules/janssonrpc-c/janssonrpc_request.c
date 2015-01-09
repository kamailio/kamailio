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

#include "../../sr_module.h"
#include "../../mem/mem.h"

#include "janssonrpc.h"
#include "janssonrpc_request.h"
#include "janssonrpc_io.h"

int next_id = 1;

int store_request(jsonrpc_request_t* req);

/* for debugging only */
void print_request(jsonrpc_request_t* req)
{
	if(!req) {
		INFO("request is (null)\n");
		return;
	}

	INFO("------request------\n");
	INFO("| id: %d\n", req->id);

	if(req->type == RPC_NOTIFICATION) {
		INFO("| type: notification\n");
	} else if(req->type == RPC_REQUEST) {
		INFO("| type: request\n");
	} else {
		INFO("| type: unknown (%d)\n", (int)req->type);
	}

	if(!(req->server)) {
		INFO("| server: (null)\n");
	} else {
		print_server(req->server);
	}

	if(!(req->cmd)) {
		INFO("| cmd: (null)\n");
	} else {
		INFO("| cmd->route: %.*s\n", STR(req->cmd->route));
	}

	INFO("| payload: %s\n", json_dumps(req->payload, 0));
	INFO("| retry: %d\n", req->retry);
	INFO("| ntries: %d\n", req->ntries);
	INFO("| timeout: %d\n", req->timeout);
	INFO("\t-------------------\n");
}

void free_request(jsonrpc_request_t* req)
{
	if(!req)
		return;

	pop_request(req->id);

	CHECK_AND_FREE_EV(req->retry_ev);
	CHECK_AND_FREE_EV(req->timeout_ev);

	if(req->payload) json_decref(req->payload);
	pkg_free(req);
}

jsonrpc_request_t* create_request(jsonrpc_req_cmd_t* cmd)
{
	if (cmd == NULL) {
		ERR("cmd is (null). Cannot build request.\n");
		return NULL;
	}

	if (cmd->params.s == NULL) {
		ERR("params is (null). Cannot build request.\n");
		return NULL;
	}

	jsonrpc_request_t* req = (jsonrpc_request_t*)pkg_malloc(sizeof(jsonrpc_request_t));
	if (!req) {
		ERR("Out of memory!");
		return NULL;
	}
	memset(req, 0, sizeof(jsonrpc_request_t));

	if (cmd->notify_only) {
		req->type = RPC_NOTIFICATION;
	} else {
		req->type = RPC_REQUEST;
	}

	/* settings for both notifications and requests */
	req->ntries = 0;
	req->next = NULL;

	req->payload = json_object();
	if(!(req->payload)) {
		ERR("Failed to create request payload\n");
		goto fail;
	}

	if(req->type == RPC_REQUEST) {
		if (next_id>JSONRPC_MAX_ID) {
			next_id = 1;
		} else {
			next_id++;
		}
		req->id = next_id;
		req->timeout = cmd->timeout;

		json_t* id_js = json_integer(next_id);
		if(id_js) {
			json_object_set(req->payload, "id", id_js);
			json_decref(id_js);
		} else {
			ERR("Failed to create request id\n");
			goto fail;
		}

		req->retry = cmd->retry;
		req->timeout = cmd->timeout;
		if (!store_request(req)) {
			ERR("store_request failed\n");
			goto fail;
		}
	} else if (req->type == RPC_NOTIFICATION) {
		req->id = 0;
		req->retry = 0;
	} else {
		ERR("Unknown RPC type: %d\n", (int)req->type);
		goto fail;
	}

	json_t* version_js = json_string(JSONRPC_VERSION);
	if(version_js) {
		json_object_set(req->payload, "jsonrpc", version_js);
		json_decref(version_js);
	} else {
		ERR("Failed to create request version\n");
		goto fail;
	}

	json_t* method_js = json_string(cmd->method.s);
	if(method_js) {
		json_object_set(req->payload, "method", method_js);
		json_decref(method_js);
	} else {
		ERR("Failed to create request method\n");
		goto fail;
	}

	json_t* params = NULL;
	json_error_t error;
	if(cmd->params.len > 0) {
		params = json_loads(cmd->params.s, 0, &error);
		if(!params) {
			ERR("Failed to parse json: %.*s\n", STR(cmd->params));
			ERR("PARSE ERROR: %s at %d,%d\n",
					error.text, error.line, error.column);
			goto fail;
		}
	}

	json_object_set(req->payload, "params", params);
	if(!(req->payload)) {
		ERR("Failed to add request payload params\n");
		goto fail;
	}

	if(params) json_decref(params);

	req->cmd = cmd;
	return req;
fail:
	ERR("Failed to create request\n");
	free_request(req);
	return NULL;
}

void retry_cb(int fd, short event, void* arg)
{
	if(!arg)
		return;

	jsonrpc_request_t* req = (jsonrpc_request_t*)arg;

	if(!(req->cmd)) {
		ERR("request has no cmd\n");
		goto error;
	}

	DEBUG("retrying request: id=%d\n", req->id);

	if(jsonrpc_send(req->cmd->conn, req, 0)<0) {
		goto error;
	}

	CHECK_AND_FREE_EV(req->retry_ev);
	return;

error:
	fail_request(JRPC_ERR_SEND, req, "Retry failed to send request");
}

int schedule_retry(jsonrpc_request_t* req)
{
	if(!req) {
		ERR("Trying to schedule retry for a null request.\n");
		return -1;
	}

	if(req->retry == 0) {
		return -1;
	}

	req->ntries++;
	if(req->retry > 0 && req->ntries > req->retry) {
		WARN("Number of retries exceeded. Failing request.\n");
		return -1;
	}

	/* next retry in milliseconds */
	unsigned int time = req->ntries * req->ntries * req->timeout;
	if(time > RETRY_MAX_TIME) {
		time = RETRY_MAX_TIME;
	}

	jsonrpc_request_t* new_req = create_request(req->cmd);

	new_req->ntries = req->ntries;

	free_request(req);

	const struct timeval tv = ms_to_tv(time);

	new_req->retry_ev = evtimer_new(global_ev_base, retry_cb, (void*)new_req);
	if(evtimer_add(new_req->retry_ev, &tv)<0) {
		ERR("event_add failed while setting request retry timer (%s).",
				strerror(errno));
		goto error;
	}

	return 0;
error:
	ERR("schedule_retry failed.\n");
	return -1;
}

int id_hash(int id) {
	return (id % JSONRPC_DEFAULT_HTABLE_SIZE);
}

jsonrpc_request_t* pop_request(int id)
{
	int key = id_hash(id);
	jsonrpc_request_t* req = request_table[key];
	jsonrpc_request_t* prev_req = NULL;

	while (req && req->id != id) {
		prev_req = req;
		if (!(req = req->next)) {
			break;
		};
	}

	if (req && req->id == id) {
		if (prev_req != NULL) {
			prev_req->next = req->next;
		} else {
			request_table[key] = NULL;
		}
		return req;
	}
	return 0;
}

int store_request(jsonrpc_request_t* req)
{
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

unsigned int requests_using_server(jsonrpc_server_t* server)
{
	unsigned int count = 0;
	jsonrpc_request_t* req = NULL;
	int key = 0;
	for (key=0; key < JSONRPC_DEFAULT_HTABLE_SIZE; key++) {
		for (req = request_table[key]; req != NULL; req = req->next) {
			if(req->server
					&& req->server == server) {
				count++;
			}
		}
	}
	return count;
}

