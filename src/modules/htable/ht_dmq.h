/**
 *
 * Copyright (C) 2013 Charles Chance (Sipcentric Ltd)
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

#ifndef _HT_DMQ_H_
#define _HT_DMQ_H_

#include "../dmq/bind_dmq.h"
#include "../../lib/srutils/srjson.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_content.h"

extern dmq_api_t ht_dmqb;
extern dmq_peer_t* ht_dmq_peer;
extern dmq_resp_cback_t ht_dmq_resp_callback;

typedef enum {
		HT_DMQ_NONE,
        HT_DMQ_SET_CELL,
        HT_DMQ_SET_CELL_EXPIRE,
        HT_DMQ_DEL_CELL,
        HT_DMQ_RM_CELL_RE
} ht_dmq_action_t;

int ht_dmq_initialize();
int ht_dmq_handle_msg(struct sip_msg* msg, peer_reponse_t* resp, dmq_node_t* dmq_node);
int ht_dmq_replicate_action(ht_dmq_action_t action, str* htname, str* cname, int type, int_str* val, int mode);
int ht_dmq_replay_action(ht_dmq_action_t action, str* htname, str* cname, int type, int_str* val, int mode);
int ht_dmq_resp_callback_f(struct sip_msg* msg, int code, dmq_node_t* node, void* param);

#endif
