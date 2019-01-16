/**
 *
 * Copyright (C) 2018 Charles Chance (Sipcentric Ltd)
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

#ifndef _PRESENCE_DMQ_H_
#define _PRESENCE_DMQ_H_

#include "presentity.h"
#include "../dmq/bind_dmq.h"
#include "../../lib/srutils/srjson.h"
#include "../../core/strutils.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_content.h"

extern dmq_api_t pres_dmqb;
extern dmq_peer_t *pres_dmq_peer;
extern dmq_resp_cback_t pres_dmq_resp_callback;

typedef enum
{
	PRES_DMQ_NONE,
	PRES_DMQ_UPDATE_PRESENTITY,
	PRES_DMQ_SYNC,
} pres_dmq_action_t;

int pres_dmq_initialize();
int pres_dmq_handle_msg(
		struct sip_msg *msg, peer_reponse_t *resp, dmq_node_t *node);
int pres_dmq_replicate_presentity(presentity_t *presentity, str *body,
		int new_t, str *cur_etag, char *sphere, str *ruid, dmq_node_t *node);
int pres_dmq_resp_callback_f(
		struct sip_msg *msg, int code, dmq_node_t *node, void *param);
#endif
