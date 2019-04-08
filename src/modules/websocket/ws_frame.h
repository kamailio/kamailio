/*
 * Copyright (C) 2012-2013 Crocodile RCS Ltd
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
 *
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 *
 */

#ifndef _WS_FRAME_H
#define _WS_FRAME_H

#include "../../core/config.h"
#include "../../core/sr_module.h"
#include "../../core/str.h"
#include "../../core/events.h"
#include "../../core/rpc.h"
#include "ws_conn.h"

typedef enum { LOCAL_CLOSE = 0, REMOTE_CLOSE } ws_close_type_t;

enum
{
	KEEPALIVE_MECHANISM_NONE = 0,
	KEEPALIVE_MECHANISM_PING = 1,
	KEEPALIVE_MECHANISM_PONG = 2,
	KEEPALIVE_MECHANISM_CONCHECK = 3
};
#define DEFAULT_KEEPALIVE_MECHANISM KEEPALIVE_MECHANISM_PING
extern int ws_keepalive_mechanism;

#define DEFAULT_KEEPALIVE_TIMEOUT 180 /* seconds */

extern str ws_ping_application_data;
#define DEFAULT_PING_APPLICATION_DATA SERVER_HDR
#define DEFAULT_PING_APPLICATION_DATA_LEN SERVER_HDR_LEN

extern stat_var *ws_failed_connections;
extern stat_var *ws_local_closed_connections;
extern stat_var *ws_received_frames;
extern stat_var *ws_remote_closed_connections;
extern stat_var *ws_transmitted_frames;
extern stat_var *ws_sip_failed_connections;
extern stat_var *ws_sip_local_closed_connections;
extern stat_var *ws_sip_received_frames;
extern stat_var *ws_sip_remote_closed_connections;
extern stat_var *ws_sip_transmitted_frames;
extern stat_var *ws_msrp_failed_connections;
extern stat_var *ws_msrp_local_closed_connections;
extern stat_var *ws_msrp_received_frames;
extern stat_var *ws_msrp_remote_closed_connections;
extern stat_var *ws_msrp_transmitted_frames;

int ws_frame_receive(sr_event_param_t *evp);
int ws_frame_transmit(sr_event_param_t *evp);
void ws_keepalive(unsigned int ticks, void *param);

int ws_close(sip_msg_t *msg);
int w_ws_close0(sip_msg_t *msg, char *p1, char *p2);
int ws_close2(sip_msg_t *msg, int status, str *reason);
int w_ws_close2(sip_msg_t *msg, char *_status, char *_reason);
int ws_close3(sip_msg_t *msg, int status, str *reason, int con);
int w_ws_close3(sip_msg_t *msg, char *_status, char *_reason, char *_con);

void ws_rpc_close(rpc_t *rpc, void *ctx);
void ws_rpc_ping(rpc_t *rpc, void *ctx);
void ws_rpc_pong(rpc_t *rpc, void *ctx);

#endif /* _WS_FRAME_H */
