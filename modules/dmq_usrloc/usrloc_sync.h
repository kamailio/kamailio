/*
* Copyright (C) 2014 Andrey Rybkin <rybkin.a@bks.tv>
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

#ifndef _DMQ_SYNC_USRLOC_H_
#define _DMQ_SYNC_USRLOC_H_

#include "../dmq/bind_dmq.h"
#include "../../lib/srutils/srjson.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_content.h"
#include "../usrloc/usrloc.h"

int usrloc_dmq_flag;

extern dmq_api_t usrloc_dmqb;
extern dmq_peer_t* usrloc_dmq_peer;
extern dmq_resp_cback_t usrloc_dmq_resp_callback;
extern rpc_export_t ul_rpc[];

usrloc_api_t ul;

typedef enum {
	DMQ_NONE,
	DMQ_UPDATE,
	DMQ_RM,
	DMQ_SYNC,
} usrloc_dmq_action_t;

int usrloc_dmq_resp_callback_f(struct sip_msg* msg, int code, dmq_node_t* node, void* param);
int usrloc_dmq_initialize();
int usrloc_dmq_handle_msg(struct sip_msg* msg, peer_reponse_t* resp, dmq_node_t* node);
int usrloc_dmq_request_sync();
void ul_cb_contact(ucontact_t* c, int type, void* param);

#endif
