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
#include <jansson.h>
#include <event.h>
#include <event2/dns.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>

#include "../../sr_module.h"
#include "../../route.h"
#include "../../mem/mem.h"
#include "../../action.h"
#include "../../route_struct.h"
#include "../../lvalue.h"
#include "../../rand/fastrand.h"
#include "../tm/tm_load.h"
#include "../jansson/jansson_utils.h"

#include "janssonrpc.h"
#include "janssonrpc_request.h"
#include "janssonrpc_server.h"
#include "janssonrpc_io.h"
#include "janssonrpc_connect.h"
#include "netstring.h"

struct tm_binds tmb;

void cmd_pipe_cb(int fd, short event, void *arg);
void io_shutdown(int sig);

int jsonrpc_io_child_process(int cmd_pipe)
{
	global_ev_base = event_base_new();
	global_evdns_base = evdns_base_new(global_ev_base, 1);

	set_non_blocking(cmd_pipe);
	struct event* pipe_ev = event_new(global_ev_base, cmd_pipe, EV_READ | EV_PERSIST, cmd_pipe_cb, NULL);
	if(!pipe_ev) {
		ERR("Failed to create pipe event\n");
		return -1;
	}

	if(event_add(pipe_ev, NULL)<0) {
		ERR("Failed to start pipe event\n");
		return -1;
	}

	connect_servers(global_server_group);

#if 0
	/* attach shutdown signal handler */
	/* The shutdown handler are intended to clean up the remaining memory
	 * in the IO process. However, catching the signals causes unpreditable
	 * behavior in the Kamailio shutdown process, so this should be disabled
	 * except when doing memory debugging. */
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = io_shutdown;
	if(sigaction(SIGTERM, &sa, NULL) == -1) {
		ERR("Failed to attach IO shutdown handler to SIGTERM\n");
	} else if(sigaction(SIGINT, NULL, &sa) == -1) {
		ERR("Failed to attach IO shutdown handler to SIGINT\n");
	}
#endif

	if(event_base_dispatch(global_ev_base)<0) {
		ERR("IO couldn't start event loop\n");
		return -1;
	}
	return 0;
}

void io_shutdown(int sig)
{
	INFO("Shutting down JSONRPC IO process...\n");
	lock_get(jsonrpc_server_group_lock); /* blocking */

	INIT_SERVER_LOOP
	FOREACH_SERVER_IN(global_server_group)
		close_server(server);
	ENDFOR

	evdns_base_free(global_evdns_base, 0);
	event_base_loopexit(global_ev_base, NULL);
	event_base_free(global_ev_base);

	lock_release(jsonrpc_server_group_lock);
}

int send_to_script(pv_value_t* val, jsonrpc_req_cmd_t* req_cmd)
{
	if(!(req_cmd)) return -1;

	if(req_cmd->route.len <= 0) return -1;

	jsonrpc_result_pv.setf(req_cmd->msg, &jsonrpc_result_pv.pvp, (int)EQ_T, val);

	int n = route_lookup(&main_rt, req_cmd->route.s);
	if(n<0) {
		ERR("no such route: %s\n", req_cmd->route.s);
		return -1;
	}

	struct action* route = main_rt.rlist[n];

	if(tmb.t_continue(req_cmd->t_hash, req_cmd->t_label, route)<0) {
		ERR("Failed to resume transaction\n");
		return -1;
	}
	return 0;
}

json_t* internal_error(int code, json_t* data)
{
	json_t* ret = json_object();
	json_t* inner = json_object();
	char* message;

	switch(code){
	case JRPC_ERR_REQ_BUILD:
		message = "Failed to build request";
		break;
	case JRPC_ERR_SEND:
		message = "Failed to send";
		break;
	case JRPC_ERR_BAD_RESP:
		message = "Bad response result";
		json_object_set(ret, "data", data);
		break;
	case JRPC_ERR_RETRY:
		message = "Retry failed";
		break;
	case JRPC_ERR_SERVER_DISCONNECT:
		message = "Server disconnected";
		break;
	case JRPC_ERR_TIMEOUT:
		message = "Message timeout";
		break;
	case JRPC_ERR_PARSING:
		message = "JSON parse error";
		break;
	case JRPC_ERR_BUG:
		message = "There is a bug";
		break;
	default:
		ERR("Unrecognized error code: %d\n", code);
		message = "Unknown error";
		break;
	}

	json_t* message_js = json_string(message);
	json_object_set(inner, "message", message_js);
	if(message_js) json_decref(message_js);

	json_t* code_js = json_integer(code);
	json_object_set(inner, "code", code_js);
	if(code_js) json_decref(code_js);

	if(data) {
		json_object_set(inner, "data", data);
	}

	json_object_set(ret, "internal_error", inner);
	if(inner) json_decref(inner);
	return ret;
}

void fail_request(int code, jsonrpc_request_t* req, char* err_str)
{
	char* req_s;
	char* freeme = NULL;
	pv_value_t val;
	json_t* error;

	if(!req) {
null_req:
		WARN("%s: (null)\n", err_str);
		goto end;
	}

	if(!(req->cmd) || (req->cmd->route.len <= 0)) {
no_route:
		req_s = json_dumps(req->payload, JSON_COMPACT);
		if(req_s) {
			WARN("%s: \n%s\n", err_str, req_s);
			free(req_s);
			goto end;
		}
		goto null_req;
	}

	error = internal_error(code, req->payload);
	jsontoval(&val, &freeme, error);
	if(error) json_decref(error);
	if(send_to_script(&val, req->cmd)<0) {
		goto no_route;
	}

end:
	if(freeme) free(freeme);
	free_req_cmd(req->cmd);
	free_request(req);
}

void timeout_cb(int fd, short event, void *arg)
{
	jsonrpc_request_t* req = (jsonrpc_request_t*)arg;
	if(!req)
		return;

	if(!(req->server)) {
		ERR("No server defined for request\n");
		return;
	}

	if(schedule_retry(req)<0) {
		fail_request(JRPC_ERR_TIMEOUT, req, "Request timeout");
	}
}


int server_tried(jsonrpc_server_t* server, server_list_t* tried)
{
	if(!server)
		return 0;

	int t = 0;
	for(;tried!=NULL;tried=tried->next)
	{
		if(tried->server &&
			server == tried->server)
		{
			t = 1;
		}
	}
	return t;
}

/* loadbalance_by_weight() uses an algorithm to randomly pick a server out of
 * a list based on its relative weight.
 *
 * It is loosely inspired by this:
 * http://eli.thegreenplace.net/2010/01/22/weighted-random-generation-in-python/
 *
 * The insert_server_group() function provides the ability to get the combined
 * weight of all the servers off the head of the list, making it possible to
 * compute in O(n) in the worst case and O(1) in the best.
 *
 * A random number out of the total weight is chosen. Each node is inspected and
 * its weight added to a recurring sum. Once the sum is larger than the random
 * number the last server that was seen is chosen.
 *
 * A weight of 0 will almost never be chosen, unless if maybe all the other
 * servers are offline.
 *
 * The exception is when all the servers in a group have a weight of 0. In
 * this case, the load should be distributed evenly across each of them. This
 * requires finding the size of the list beforehand.
 * */
void loadbalance_by_weight(jsonrpc_server_t** s,
		jsonrpc_server_group_t* grp, server_list_t* tried)
{
	*s = NULL;

	if(grp == NULL) {
		ERR("Trying to pick from an empty group\n");
		return;
	}

	if(grp->type != WEIGHT_GROUP) {
		ERR("Trying to pick from a non weight group\n");
		return;
	}

	jsonrpc_server_group_t* head = grp;
	jsonrpc_server_group_t* cur = grp;

	unsigned int pick = 0;
	if(head->weight == 0) {
		unsigned int size = 0;
		size = server_group_size(cur);
		if(size == 0) return;

		pick = fastrand_max(size-1);

		int i;
		for(i=0;
			(i <= pick || *s == NULL)
				&& cur != NULL;
			i++, cur=cur->next)
		{
			if(cur->server->status == JSONRPC_SERVER_CONNECTED) {
				if(!server_tried(cur->server, tried)
					&& (cur->server->hwm <= 0
						|| cur->server->req_count < cur->server->hwm))
				{
					*s = cur->server;
				}
			}
		}
	} else {
		pick = fastrand_max(head->weight - 1);

		unsigned int sum = 0;
		while(1) {
			if(cur == NULL) break;
			if(cur->server->status == JSONRPC_SERVER_CONNECTED) {
				if(!server_tried(cur->server, tried)
					&& (cur->server->hwm <= 0
						|| cur->server->req_count < cur->server->hwm))
				{
					*s = cur->server;
				}
			}
			sum += cur->server->weight;
			if(sum > pick && *s != NULL) break;
			cur = cur->next;
		}
	}
}

int jsonrpc_send(str conn, jsonrpc_request_t* req, bool notify_only)
{
	char* json = (char*)json_dumps(req->payload, JSON_COMPACT);

	char* ns;
	size_t bytes;
	bytes = netstring_encode_new(&ns, json, (size_t)strlen(json));

	bool sent = false;
	jsonrpc_server_group_t* c_grp = NULL;
	if(global_server_group != NULL)
		c_grp = *global_server_group;
	jsonrpc_server_group_t* p_grp = NULL;
	jsonrpc_server_group_t* w_grp = NULL;
	jsonrpc_server_t* s = NULL;
	server_list_t* tried_servers = NULL;
	DEBUG("SENDING DATA\n");
	for(; c_grp != NULL; c_grp = c_grp->next) {

		if(strncmp(conn.s, c_grp->conn.s, c_grp->conn.len) != 0) continue;

		for(p_grp = c_grp->sub_group; p_grp != NULL; p_grp = p_grp->next)
		{
			w_grp = p_grp->sub_group;
			while(!sent) {
				loadbalance_by_weight(&s, w_grp, tried_servers);
				if (s == NULL || s->status != JSONRPC_SERVER_CONNECTED) {
					break;
				}

				if(bufferevent_write(s->bev, ns, bytes) == 0) {
					sent = true;
					if(!notify_only) {
						s->req_count++;
						if (s->hwm > 0 && s->req_count >= s->hwm) {
							WARN("%.*s:%d in connection group %.*s has exceeded its high water mark (%d)\n",
									STR(s->addr), s->port,
									STR(s->conn), s->hwm);
						}
					}
					req->server = s;
					break;
				} else {
					addto_server_list(s, &tried_servers);
				}
			}

			if (sent) {
				break;
			}

			WARN("Failed to send to priority group, %d\n", p_grp->priority);
			if(p_grp->next != NULL) {
				INFO("Proceeding to next priority group, %d\n",
						p_grp->next->priority);
			}
		}

		if (sent) {
			break;
		}

	}

	if(!sent) {
		WARN("Failed to send to connection group, \"%.*s\"\n",
				STR(conn));
		if(schedule_retry(req)<0) {
			fail_request(JRPC_ERR_RETRY, req, "Failed to schedule retry");
		}
	}

	free_server_list(tried_servers);
	if(ns) pkg_free(ns);
	if(json) free(json);

	if (sent && notify_only == false) {

		const struct timeval tv = ms_to_tv(req->timeout);

		req->timeout_ev = evtimer_new(global_ev_base, timeout_cb, (void*)req);
		if(event_add(req->timeout_ev, &tv)<0) {
			ERR("event_add failed while setting request timer (%s).",
					strerror(errno));
			return -1;
		}
	}

	return sent;
}


void cmd_pipe_cb(int fd, short event, void *arg)
{
	struct jsonrpc_pipe_cmd *cmd;

	if (read(fd, &cmd, sizeof(cmd)) != sizeof(cmd)) {
		ERR("FATAL ERROR: failed to read from command pipe: %s\n",
				strerror(errno));
		return;
	}


	switch(cmd->type) {
	case CMD_CLOSE:
		if(cmd->server) {
			wait_close(cmd->server);
		}
		goto end;
		break;
	case CMD_RECONNECT:
		if(cmd->server) {
			wait_reconnect(cmd->server);
		}
		goto end;
		break;
	case CMD_CONNECT:
		if(cmd->server) {
			bev_connect(cmd->server);
		}
		goto end;
		break;
	case CMD_UPDATE_SERVER_GROUP:
		if(cmd->new_grp) {
			jsonrpc_server_group_t* old_grp = *global_server_group;
			*global_server_group = cmd->new_grp;
			free_server_group(&old_grp);
		}
		lock_release(jsonrpc_server_group_lock);
		goto end;
		break;

	case CMD_SEND:
		break;

	default:
		ERR("Unrecognized pipe command: %d\n", cmd->type);
		goto end;
		break;
	}

	/* command is SEND */

	jsonrpc_req_cmd_t* req_cmd = cmd->req_cmd;
	if(req_cmd == NULL) {
		ERR("req_cmd is NULL. Invalid send command\n");
		goto end;
	}

	jsonrpc_request_t* req = NULL;
	req = create_request(req_cmd);
	if (!req || !req->payload) {
		json_t* error = internal_error(JRPC_ERR_REQ_BUILD, NULL);
		pv_value_t val;
		char* freeme = NULL;
		jsontoval(&val, &freeme, error);
		if(req_cmd->route.len <=0 && send_to_script(&val, req_cmd)<0) {
			ERR("Failed to build request (method: %.*s, params: %.*s)\n",
					STR(req_cmd->method), STR(req_cmd->params));
		}
		if(freeme) free(freeme);
		if(error) json_decref(error);
		free_req_cmd(req_cmd);
		goto end;
	}

	int sent = jsonrpc_send(req_cmd->conn, req, req_cmd->notify_only);

	char* type;
	if (sent<0) {
		if (req_cmd->notify_only == false) {
			type = "Request";
		} else {
			type = "Notification";
		}
		WARN("%s could not be sent to connection group: %.*s\n",
				type, STR(req_cmd->conn));
		fail_request(JRPC_ERR_SEND, req, "Failed to send request");
	}

end:
	free_pipe_cmd(cmd);
}

int handle_response(json_t* response)
{
	int retval = 0;
	jsonrpc_request_t* req = NULL;
	json_t* return_obj = NULL;
	json_t* internal = NULL;
	char* freeme = NULL;


	/* check if json object */
	if(!json_is_object(response)){
		WARN("jsonrpc response is not an object\n");
		return -1;
	}

	/* check version */
	json_t* version = json_object_get(response, "jsonrpc");
	if(!version) {
		WARN("jsonrpc response does not have a version.\n");
		retval = -1;
		goto end;
	}

	const char* version_s = json_string_value(version);
	if(!version_s){
		WARN("jsonrpc response version is not a string.\n");
		retval = -1;
		goto end;
	}

	if (strlen(version_s) != (sizeof(JSONRPC_VERSION)-1)
			|| strncmp(version_s, JSONRPC_VERSION, sizeof(JSONRPC_VERSION)-1) != 0) {
		WARN("jsonrpc response version is not %s. version: %s\n",
				JSONRPC_VERSION, version_s);
		retval = -1;
		goto end;
	}

	/* check for an id */
	json_t* _id = json_object_get(response, "id");
	if(!_id) {
		WARN("jsonrpc response does not have an id.\n");
		retval = -1;
		goto end;
	}

	int id = json_integer_value(_id);
	if (!(req = pop_request(id))) {
		/* don't fail the server for an unrecognized id */
		retval = 0;
		goto end;
	}

	return_obj = json_object();

	json_t* error = json_object_get(response, "error");
	// if the error value is null, we don't care
	bool _error = error && (json_typeof(error) != JSON_NULL);

	json_t* result = json_object_get(response, "result");

	if(_error) {
		json_object_set(return_obj, "error", error);
	}

	if(result) {
		json_object_set(return_obj, "result", result);
	}

	if ((!result && !_error) || (result && _error)) {
		WARN("bad response\n");
		internal = internal_error(JRPC_ERR_BAD_RESP, req->payload);
		json_object_update(return_obj, internal);
		if(internal) json_decref(internal);
	}

	pv_value_t val;

	if(jsontoval(&val, &freeme, return_obj)<0) {
		fail_request(
				JRPC_ERR_TO_VAL,
				req,
				"Failed to convert response json to pv\n");
		retval = -1;
		goto end;
	}

	char* error_s = NULL;

	if(send_to_script(&val, req->cmd)>=0) {
		goto free_and_end;
	}

	if(_error) {
		// get code from error
		json_t* _code = json_object_get(error, "code");
		if(_code) {
			int code = json_integer_value(_code);

			// check if code is in global_retry_ranges
			retry_range_t* tmpr;
			for(tmpr = global_retry_ranges;
					tmpr != NULL;
					tmpr = tmpr->next) {
				if((tmpr->start < tmpr->end
						&& tmpr->start <= code && code <= tmpr->end)
				|| (tmpr->end < tmpr->start
						&& tmpr->end <= code && code <= tmpr->start)
				|| (tmpr->start == tmpr->end && tmpr->start == code)) {
					if(schedule_retry(req)==0) {
						goto end;
					}
					break;
				}
			}

		}
		error_s = json_dumps(error, JSON_COMPACT);
		if(error_s) {
			WARN("Request recieved an error: \n%s\n", error_s);
			free(error_s);
		} else {
			fail_request(
					JRPC_ERR_BAD_RESP,
					req,
					"Could not convert 'error' response to string");
			retval = -1;
			goto end;
		}
	}


free_and_end:
	free_req_cmd(req->cmd);
	free_request(req);

end:
	if(freeme) free(freeme);
	if(return_obj) json_decref(return_obj);
	return retval;
}

void handle_netstring(jsonrpc_server_t* server)
{
	unsigned int old_count = server->req_count;
	server->req_count--;
	if (server->hwm > 0
			&& old_count >= server->hwm
			&& server->req_count < server->hwm) {
		INFO("%.*s:%d in connection group %.*s is back to normal\n",
				STR(server->addr), server->port, STR(server->conn));
	}

	json_error_t error;

	json_t* res = json_loads(server->buffer->string, 0, &error);

	if (res) {
		if(handle_response(res)<0){
			ERR("Cannot handle jsonrpc response: %s\n", server->buffer->string);
		}
		json_decref(res);
	} else {
		ERR("Failed to parse json: %s\n", server->buffer->string);
		ERR("PARSE ERROR: %s at %d,%d\n",
				error.text, error.line, error.column);
	}
}

void bev_read_cb(struct bufferevent* bev, void* arg)
{
	jsonrpc_server_t* server = (jsonrpc_server_t*)arg;
	int retval = 0;
	while (retval == 0) {
		int retval = netstring_read_evbuffer(bev, &server->buffer);

		if (retval == NETSTRING_INCOMPLETE) {
			return;
		} else if (retval < 0) {
			char* msg = "";
			switch(retval) {
			case NETSTRING_ERROR_TOO_LONG:
				msg = "too long";
				break;
			case NETSTRING_ERROR_NO_COLON:
				msg = "no colon after length field";
				break;
			case NETSTRING_ERROR_TOO_SHORT:
				msg = "too short";
				break;
			case NETSTRING_ERROR_NO_COMMA:
				msg = "missing comma";
				break;
			case NETSTRING_ERROR_LEADING_ZERO:
				msg = "length field has a leading zero";
				break;
			case NETSTRING_ERROR_NO_LENGTH:
				msg = "missing length field";
				break;
			case NETSTRING_INCOMPLETE:
				msg = "incomplete";
				break;
			default:
				ERR("bad netstring: unknown error (%d)\n", retval);
				goto reconnect;
			}
			ERR("bad netstring: %s\n", msg);
reconnect:
			force_reconnect(server);
			return;
		}

		handle_netstring(server);
		free_netstring(server->buffer);
		server->buffer = NULL;
	}
}

int set_non_blocking(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0)
		return flags;
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0)
		return -1;

	return 0;
}

jsonrpc_pipe_cmd_t* create_pipe_cmd()
{
	jsonrpc_pipe_cmd_t* cmd = NULL;
	cmd = (jsonrpc_pipe_cmd_t*)shm_malloc(sizeof(jsonrpc_pipe_cmd_t));
	if(!cmd) {
		ERR("Failed to malloc pipe cmd.\n");
		return NULL;
	}
	memset(cmd, 0, sizeof(jsonrpc_pipe_cmd_t));

	return cmd;
}

void free_pipe_cmd(jsonrpc_pipe_cmd_t* cmd)
{
	if(!cmd) return;

	shm_free(cmd);
}

jsonrpc_req_cmd_t* create_req_cmd()
{
	jsonrpc_req_cmd_t* req_cmd = NULL;
	req_cmd = (jsonrpc_req_cmd_t*)shm_malloc(sizeof(jsonrpc_req_cmd_t));
	CHECK_MALLOC_NULL(req_cmd);
	memset(req_cmd, 0, sizeof(jsonrpc_req_cmd_t));

	req_cmd->conn = null_str;
	req_cmd->method = null_str;
	req_cmd->params = null_str;
	req_cmd->route = null_str;
	return req_cmd;
}

void free_req_cmd(jsonrpc_req_cmd_t* req_cmd)
{
	if(req_cmd) {
		CHECK_AND_FREE(req_cmd->conn.s);
		CHECK_AND_FREE(req_cmd->method.s);
		CHECK_AND_FREE(req_cmd->params.s);
		CHECK_AND_FREE(req_cmd->route.s);
		shm_free(req_cmd);
	}
}

int send_pipe_cmd(cmd_type type, void* data)
{
	char* name = "";
	jsonrpc_pipe_cmd_t* cmd = NULL;
	cmd = create_pipe_cmd();
	CHECK_MALLOC(cmd);

	cmd->type = type;

	switch(type) {
	case CMD_CONNECT:
		cmd->server = (jsonrpc_server_t*)data;
		name = "connect";
		break;
	case CMD_RECONNECT:
		cmd->server = (jsonrpc_server_t*)data;
		name = "reconnect";
		break;
	case CMD_CLOSE:
		cmd->server = (jsonrpc_server_t*)data;
		name = "close";
		break;
	case CMD_UPDATE_SERVER_GROUP:
		cmd->new_grp = (jsonrpc_server_group_t*)data;
		name = "update";
		break;
	case CMD_SEND:
		cmd->req_cmd = (jsonrpc_req_cmd_t*)data;
		name = "send";
		break;
	default:
		ERR("Unknown command type %d", type);
		goto error;
	}

	DEBUG("sending %s command\n", name);

	if (write(cmd_pipe, &cmd, sizeof(cmd)) != sizeof(cmd)) {
		ERR("Failed to send '%s' cmd to io pipe: %s\n", name, strerror(errno));
		goto error;
	}

	return 0;
error:
	free_pipe_cmd(cmd);
	return -1;
}
