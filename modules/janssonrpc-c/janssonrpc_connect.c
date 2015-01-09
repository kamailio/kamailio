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

void wait_server_backoff(unsigned int timeout /* seconds */,
		jsonrpc_server_t* server, bool delay);

void bev_connect(jsonrpc_server_t* server);

void bev_disconnect(struct bufferevent* bev)
{
	// close bufferevent
	if(bev != NULL) {
		short enabled = bufferevent_get_enabled(bev);
		if(EV_READ & enabled)
			bufferevent_disable(bev, EV_READ);
		if(EV_WRITE & enabled)
			bufferevent_disable(bev, EV_WRITE);
		bufferevent_free(bev);
		bev = NULL;
	}
}


/* This will immediately close a server socket and clean out any pending
 * requests that are waiting on that socket.
 * */
void force_disconnect(jsonrpc_server_t* server)
{
	if(!server) {
		ERR("Trying to disconnect a NULL server.\n");
		return;
	}

	// clear the netstring buffer when disconnecting
	free_netstring(server->buffer);
	server->buffer = NULL;

	server->status = JSONRPC_SERVER_DISCONNECTED;

	// close bufferevent
	bev_disconnect(server->bev);
	INFO("Disconnected from server %.*s:%d for conn %.*s.\n",
			STR(server->addr), server->port, STR(server->conn));


	/* clean out requests */
	jsonrpc_request_t* req = NULL;
	int key = 0;
	for (key=0; key < JSONRPC_DEFAULT_HTABLE_SIZE; key++) {
		for (req = request_table[key]; req != NULL; req = req->next) {
			if(req->server != NULL && req->server == server) {
				fail_request(JRPC_ERR_SERVER_DISCONNECT, req,
						"Failing request for server shutdown");
			}
		}
	}
}

typedef struct server_backoff_args {
	struct event* ev;
	jsonrpc_server_t* server;
	unsigned int timeout;
} server_backoff_args_t;

void server_backoff_cb(int fd, short event, void *arg)
{
	if(!arg)
		return;

	server_backoff_args_t* a = (server_backoff_args_t*)arg;
	if(!a)
		return;

	unsigned int timeout = a->timeout;

	/* exponential backoff */
	if(timeout < 1) {
		timeout = 1;
	} else {
		timeout = timeout * 2;
		if(timeout > 60) {
			timeout = 60;
		}
	}

	close(fd);
	CHECK_AND_FREE_EV(a->ev);
	pkg_free(arg);

	wait_server_backoff(timeout, a->server, false);
}

void wait_server_backoff(unsigned int timeout /* seconds */,
		jsonrpc_server_t* server, bool delay)
{
	if(!server) {
		ERR("Trying to close/reconnect a NULL server\n");
		return;
	}

	if(delay == false) {
		if (requests_using_server(server) <= 0) {
			if(server->status == JSONRPC_SERVER_RECONNECTING) {
				bev_connect(server);
			} else if(server->status == JSONRPC_SERVER_CLOSING) {
				close_server(server);
			}
			return;
		}
	}

	const struct timeval tv = {timeout, 0};

	server_backoff_args_t* args = pkg_malloc(sizeof(server_backoff_args_t));
	CHECK_MALLOC_VOID(args);
	memset(args, 0, sizeof(server_backoff_args_t));

	args->ev = evtimer_new(global_ev_base, server_backoff_cb, (void*)args);
	CHECK_MALLOC_GOTO(args->ev, error);

	args->server = server;
	args->timeout = timeout;

	if(evtimer_add(args->ev, &tv)<0) {
		ERR("event_add failed while setting request timer (%s).", strerror(errno));
		goto error;
	}

	return;

error:
	ERR("schedule_server failed.\n");

	if(args) {
		if(args->ev) {
			evtimer_del(args->ev);
		}
		pkg_free(args);
	}

	if (server->status == JSONRPC_SERVER_CLOSING) {
		ERR("Closing server now...\n");
		close_server(server);
	} else if (server->status == JSONRPC_SERVER_RECONNECTING) {
		ERR("Reconnecting server now...\n");
		force_reconnect(server);
	}
}

void wait_close(jsonrpc_server_t* server)
{
	if(!server) {
		ERR("Trying to close null server.\n");
		return;
	}

	server->status = JSONRPC_SERVER_CLOSING;
	wait_server_backoff(1, server, false);
}

void wait_reconnect(jsonrpc_server_t* server)
{
	if(!server) {
		ERR("Trying to reconnect null server.\n");
		return;
	}

	server->status = JSONRPC_SERVER_RECONNECTING;
	wait_server_backoff(1, server, false);
}

void connect_servers(jsonrpc_server_group_t** group)
{
	INIT_SERVER_LOOP
	FOREACH_SERVER_IN(group)
		server = wgroup->server;
		if(server->status != JSONRPC_SERVER_FAILURE
				&& server->status != JSONRPC_SERVER_RECONNECTING)
		{
			bev_connect(server);
		}
	ENDFOR
}

void force_reconnect(jsonrpc_server_t* server)
{
	INFO("Reconnecting to server %.*s:%d for conn %.*s.\n",
			STR(server->addr), server->port, STR(server->conn));
	force_disconnect(server);
	bev_connect(server);
}

/* helper for bev_connect_cb() and bev_connect() */
void connect_failed(jsonrpc_server_t* server)
{
	bev_disconnect(server->bev);

	server->status = JSONRPC_SERVER_RECONNECTING;
	wait_server_backoff(JSONRPC_RECONNECT_INTERVAL, server, true);
}

void bev_connect_cb(struct bufferevent* bev, short events, void* arg)
{
	jsonrpc_server_t* server = (jsonrpc_server_t*)arg;
	if(!arg) {
		ERR("Trying to connect null server\n");
		return;
	}

	if (events & (BEV_EVENT_ERROR|BEV_EVENT_EOF)) {
		WARN("Connection error for %.*s:%d\n", STR(server->addr), server->port);
		if (events & BEV_EVENT_ERROR) {
			int err = bufferevent_socket_get_dns_error(bev);
			if(err) {
				ERR("DNS error for %.*s: %s\n",
					STR(server->addr), evutil_gai_strerror(err));
			}
		}
		goto failed;
	} else if(events & BEV_EVENT_CONNECTED) {

		if (server->status == JSONRPC_SERVER_CONNECTED) {
			return;
		}

		server->status = JSONRPC_SERVER_CONNECTED;
		INFO("Connected to host %.*s:%d\n",
				STR(server->addr), server->port);
	}

	return;

failed:
	connect_failed(server);
}

void bev_connect(jsonrpc_server_t* server)
{
	if(!server) {
		ERR("Trying to connect null server\n");
		return;
	}

	INFO("Connecting to server %.*s:%d for conn %.*s.\n",
			STR(server->addr), server->port, STR(server->conn));

	server->bev = bufferevent_socket_new(
			global_ev_base,
			-1,
			BEV_OPT_CLOSE_ON_FREE);
	if(!(server->bev)) {
		ERR("Could not create bufferevent for  %.*s:%d\n", STR(server->addr), server->port);
		connect_failed(server);
		return;
	}

	bufferevent_setcb(
			server->bev,
			bev_read_cb,
			NULL,
			bev_connect_cb,
			server);
	bufferevent_enable(server->bev, EV_READ|EV_WRITE);
	if(bufferevent_socket_connect_hostname(
			server->bev,
			global_evdns_base,
			AF_UNSPEC,
			server->addr.s,
			server->port)<0) {
		WARN("Failed to connect to %.*s:%d\n", STR(server->addr), server->port);
		connect_failed(server);
	}
}

