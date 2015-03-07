/**
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

#ifndef _NOTIFICATION_PEER_H_
#define _NOTIFICATION_PEER_H_

#include "../../parser/msg_parser.h"
#include "../../parser/parse_content.h"
#include "../../ut.h"
#include "dmq.h"
#include "dmqnode.h"
#include "peer.h"
#include "dmq_funcs.h"

extern str notification_content_type;
extern int *dmq_init_callback_done;

int add_notification_peer();
int dmq_notification_callback(struct sip_msg* msg, peer_reponse_t* resp, dmq_node_t* dmq_node);
int extract_node_list(dmq_node_list_t* update_list, struct sip_msg* msg);
str* build_notification_body();
int build_node_str(dmq_node_t* node, char* buf, int buflen);
/* request a nodelist from a server
 * this is acomplished by a KDMQ request
 * KDMQ notification@server:port
 * node - the node to send to
 * forward - flag that tells if the node receiving the message is allowed to 
 *           forward the request to its own list
 */
int request_nodelist(dmq_node_t* node, int forward);
dmq_node_t* add_server_and_notify(str* server_address);

/* helper functions */
extern int notification_resp_callback_f(struct sip_msg* msg, int code,
		dmq_node_t* node, void* param);
extern dmq_resp_cback_t notification_callback;

#endif
