/*
 * Copyright (C) 2012-2013 Crocodile RCS Ltd
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
 *
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 *
 */

#ifndef _WS_HANDSHAKE_H
#define _WS_HANDSHAKE_H

#include "../../core/sr_module.h"
#include "../../core/events.h"
#include "../../core/rpc.h"
#include "../../core/parser/msg_parser.h"
#include "websocket.h"

#define DEFAULT_SUB_PROTOCOLS (SUB_PROTOCOL_SIP | SUB_PROTOCOL_MSRP)
#define SUB_PROTOCOL_ALL (SUB_PROTOCOL_SIP | SUB_PROTOCOL_MSRP)
extern int ws_sub_protocols;

#define CORS_MODE_NONE 0
#define CORS_MODE_ANY 1
#define CORS_MODE_ORIGIN 2
extern int ws_cors_mode;

extern stat_var *ws_failed_handshakes;
extern stat_var *ws_successful_handshakes;
extern stat_var *ws_sip_successful_handshakes;
extern stat_var *ws_msrp_successful_handshakes;

typedef struct ws_address
{
	str proto;
	int proto_no;
	str host;
	str port;
	int port_no;
	str path;
} ws_address_t;

int ws_parse_url(str *wsurl, ws_address_t *waddr);
int ws_handle_handshake(struct sip_msg *msg);
int w_ws_handle_handshake(sip_msg_t *msg, char *p1, char *p2);
int ws_connect(sip_msg_t *msg, str *host, int port, str *path,
		str *sub_protocol, int cmode);
int ws_connect_url(sip_msg_t *msg, str *wsurl, str *sub_protocol);
int ws_send(sip_msg_t *msg, str *host, int port, str *path, str *sub_protocol,
		int cmode);
int w_ws_connect(sip_msg_t *msg, char *phost, char *pport, char *ppath,
		char *psubproto, char *pcmode);
int w_ws_connect_url(sip_msg_t *msg, char *purl, char *psubproto);
int w_ws_send(sip_msg_t *msg, char *phost, char *pport, char *ppath,
		char *psubproto, char *pcmode);
int ws_handle_handshake_response(sr_event_param_t *evp);

void ws_rpc_disable(rpc_t *rpc, void *ctx);
void ws_rpc_enable(rpc_t *rpc, void *ctx);
void ws_rpc_connect(rpc_t *rpc, void *ctx);

#endif /* _WS_HANDSHAKE_H */
