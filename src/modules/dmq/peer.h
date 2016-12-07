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


#ifndef _PEER_H_
#define _PEER_H_

#include <string.h>
#include <stdlib.h>
#include "dmqnode.h"
#include "../../lock_ops.h"
#include "../../str.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"

typedef struct peer_response {
	int resp_code;
	str content_type;
	str reason;
	str body;
} peer_reponse_t;

typedef int(*peer_callback_t)(struct sip_msg*, peer_reponse_t* resp, dmq_node_t* node);
typedef int(*init_callback_t)();

typedef struct dmq_peer {
	str peer_id;
	str description;
	peer_callback_t callback;
	init_callback_t init_callback;
	struct dmq_peer* next;
} dmq_peer_t;

typedef struct dmq_peer_list {
	gen_lock_t lock;
	dmq_peer_t* peers;
	int count;
} dmq_peer_list_t;

extern dmq_peer_list_t* peer_list;

dmq_peer_list_t* init_peer_list();
dmq_peer_t* search_peer_list(dmq_peer_list_t* peer_list, dmq_peer_t* peer);
typedef dmq_peer_t* (*register_dmq_peer_t)(dmq_peer_t*);

dmq_peer_t* add_peer(dmq_peer_list_t* peer_list, dmq_peer_t* peer);
dmq_peer_t* find_peer(str peer_id);
int empty_peer_callback(struct sip_msg* msg, peer_reponse_t* resp, dmq_node_t* dmq_node);

#endif

