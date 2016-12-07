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

#ifndef _JANSSONRPC_CONNECT_H_
#define _JANSSONRPC_CONNECT_H_

enum
{ JSONRPC_SERVER_DISCONNECTED = 0
, JSONRPC_SERVER_CONNECTED
, JSONRPC_SERVER_CONNECTING
, JSONRPC_SERVER_FAILURE
, JSONRPC_SERVER_CLOSING
, JSONRPC_SERVER_RECONNECTING
};

/* interval (in seconds) at which failed servers are retried */
#define JSONRPC_RECONNECT_INTERVAL  3

void force_disconnect(jsonrpc_server_t* server);
/* Do not call force_disconnect() from outside the IO process.
 * Server's have a bufferevent that is part of local memory and free'd
 * at disconnect */

void wait_close(jsonrpc_server_t* server);
void wait_reconnect(jsonrpc_server_t* server);
void connect_servers(jsonrpc_server_group_t** group);
void force_reconnect(jsonrpc_server_t* server);
void bev_connect(jsonrpc_server_t* server);

#endif /* _JSONRPC_CONNECT_H_ */
