/*
 * Copyright (C) 2025 Viktor Litvinov
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
#ifndef _RTPENGINE_DMQ_H_
#define _RTPENGINE_DMQ_H_

#include "rtpengine_hash.h"
#include "rtpengine.h"
#include "../dmq/bind_dmq.h"
#include "../../core/utils/srjson.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_content.h"

extern dmq_api_t rtpengine_dmqb;
extern dmq_peer_t *rtpengine_dmq_peer;

typedef enum
{
	RTPENGINE_DMQ_NONE,
	RTPENGINE_DMQ_INSERT,
	RTPENGINE_DMQ_REMOVE,
	RTPENGINE_DMQ_SYNC,
} rtpengine_dmq_action_t;

int rtpengine_dmq_init();
int rtpengine_dmq_handle_msg(
		struct sip_msg *msg, peer_reponse_t *resp, dmq_node_t *node);
int rtpengine_dmq_replicate_action(rtpengine_dmq_action_t action, str callid,
		str viabranch, struct rtpengine_hash_entry *entry, dmq_node_t *node);
int rtpengine_dmq_replicate_insert(
		str callid, str viabranch, struct rtpengine_hash_entry *entry);
int rtpengine_dmq_replicate_remove(str callid, str viabranch);
int rtpengine_dmq_replicate_sync();
#endif
