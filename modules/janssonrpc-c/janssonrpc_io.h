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

#ifndef _JANSSONRPC_IO_H_
#define _JANSSONRPC_IO_H_

#include <jansson.h>
#include <event2/bufferevent.h>
#include <stdbool.h>
#include "../../route_struct.h"
#include "../../pvar.h"
#include "janssonrpc_server.h"
#include "janssonrpc_request.h"
#include "janssonrpc.h"

/* event bases */
struct event_base* global_ev_base;
struct evdns_base* global_evdns_base;

typedef enum
{ CMD_CONNECT = 1000
, CMD_RECONNECT
, CMD_CLOSE
, CMD_UPDATE_SERVER_GROUP
, CMD_SEND
} cmd_type;

typedef struct jsonrpc_req_cmd {
	str method, params, route, conn;
	unsigned int t_hash, t_label, timeout;
	bool notify_only;
	int retry;
	struct sip_msg *msg;
} jsonrpc_req_cmd_t;

typedef struct jsonrpc_pipe_cmd jsonrpc_pipe_cmd_t;
struct jsonrpc_pipe_cmd
{
	cmd_type type;
	union {
		jsonrpc_server_t* server;
		jsonrpc_req_cmd_t* req_cmd;
		jsonrpc_server_group_t* new_grp;
	};
};

int jsonrpc_io_child_process(int data_pipe);
int send_pipe_cmd(cmd_type type, void* data);
int handle_response(json_t *response);
jsonrpc_pipe_cmd_t* create_pipe_cmd();
void free_pipe_cmd(jsonrpc_pipe_cmd_t* cmd);
jsonrpc_req_cmd_t* create_req_cmd();
void free_req_cmd(jsonrpc_req_cmd_t* cmd);
int  set_non_blocking(int fd);
void bev_read_cb(struct bufferevent* bev, void* arg);

/* Remember to update the docs if you add or change these */
typedef enum
{ JRPC_ERR_BUG       = -1000
, JRPC_ERR_TIMEOUT   = -100
, JRPC_ERR_SERVER_DISCONNECT = -75
, JRPC_ERR_RETRY     = -50
, JRPC_ERR_BAD_RESP  = -20
, JRPC_ERR_TO_VAL    = -11
, JRPC_ERR_PARSING   = -10
, JRPC_ERR_SEND      = -5
, JRPC_ERR_REQ_BUILD = -1
} jsonrpc_error;


#endif /* _JSONRPC_IO_H_ */
