/**
 *
 * Copyright (C) 2014 Alex Hermann (SpeakUp BV)
 * Based on ht_dmq.c Copyright (C) 2013 Charles Chance (Sipcentric Ltd)
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

#ifndef _DLG_DMQ_H_
#define _DLG_DMQ_H_

#include "dlg_hash.h"
#include "../dmq/bind_dmq.h"
#include "../../lib/srutils/srjson.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_content.h"

extern dmq_api_t dlg_dmqb;
extern dmq_peer_t* dlg_dmq_peer;
extern dmq_resp_cback_t dlg_dmq_resp_callback;

typedef enum {
	DLG_DMQ_NONE,
	DLG_DMQ_UPDATE,
	DLG_DMQ_STATE,
	DLG_DMQ_RM,
	DLG_DMQ_SYNC,
} dlg_dmq_action_t;

int dlg_dmq_initialize();
int dlg_dmq_handle_msg(struct sip_msg* msg, peer_reponse_t* resp, dmq_node_t* node);
int dlg_dmq_replicate_action(dlg_dmq_action_t action, dlg_cell_t* dlg, int needlock);
int dlg_dmq_resp_callback_f(struct sip_msg* msg, int code, dmq_node_t* node, void* param);
#endif
