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


#ifndef _DMQ_FUNCS_H_
#define _DMQ_FUNCS_H_

#include "../../str.h"
#include "../../modules/tm/dlg.h"
#include "../../modules/tm/tm_load.h"
#include "../../config.h"
#include "peer.h"
#include "worker.h"
#include "dmqnode.h"

void ping_servers(unsigned int ticks,void *param);

typedef struct dmq_resp_cback {
	int (*f)(struct sip_msg* msg, int code, dmq_node_t* node, void* param);
	void* param;
} dmq_resp_cback_t;

typedef struct dmq_cback_param {
	dmq_resp_cback_t resp_cback;
	dmq_node_t* node;
} dmq_cback_param_t;

int cfg_dmq_send_message(struct sip_msg* msg, char* peer, char* to,
		char* body, char* content_type);
int cfg_dmq_bcast_message(struct sip_msg* msg, char* peer, char* body, 
		char* content_type);
int cfg_dmq_t_replicate(struct sip_msg* msg, char* s);
int cfg_dmq_is_from_node(struct sip_msg* msg);
dmq_peer_t* register_dmq_peer(dmq_peer_t* peer);
int dmq_send_message(dmq_peer_t* peer, str* body, dmq_node_t* node,
		dmq_resp_cback_t* resp_cback, int max_forwards, str* content_type);
int bcast_dmq_message(dmq_peer_t* peer, str* body, dmq_node_t* except,
		dmq_resp_cback_t* resp_cback, int max_forwards, str* content_type);

#endif

