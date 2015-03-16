/**
 *
 * dmq module - distributed message queue
 *
 * Copyright (C) 2011 Bucur Marius - Ovidiu
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "dmq.h"
#include "bind_dmq.h"
#include "peer.h"
#include "dmq_funcs.h"

/**
 * @brief bind dmq module api
 */
int bind_dmq(dmq_api_t* api) {
	api->register_dmq_peer = register_dmq_peer;
	api->send_message = dmq_send_message;
	api->bcast_message = bcast_dmq_message;
	api->find_dmq_node_uri = find_dmq_node_uri2;
	return 0;
}

