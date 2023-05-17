/*
 * siptrace module - helper module to trace sip messages
 *
 * Copyright (C) 2017 kamailio.org
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
 */

#ifndef _SIPTRACE_SEND_H_
#define _SIPTRACE_SEND_H_

#include "../../core/parser/msg_parser.h"
#include "../../core/ip_addr.h"
#include "siptrace_data.h"

int sip_trace_prepare(sip_msg_t *msg);
int sip_trace_xheaders_write(struct _siptrace_data *sto);
int sip_trace_xheaders_read(struct _siptrace_data *sto);
int sip_trace_xheaders_free(struct _siptrace_data *sto);
int trace_send_duplicate(char *buf, int len, struct dest_info *dst2);

/**
 *
 */
#define siptrace_copy_proto_olen(vproto, vbuf, vlen) \
	do {                                             \
		switch(vproto) {                             \
			case PROTO_TCP:                          \
				strcpy(vbuf, "tcp:");                \
				vlen = 4;                            \
				break;                               \
			case PROTO_TLS:                          \
				strcpy(vbuf, "tls:");                \
				vlen = 4;                            \
				break;                               \
			case PROTO_SCTP:                         \
				strcpy(vbuf, "sctp:");               \
				vlen = 5;                            \
				break;                               \
			case PROTO_WS:                           \
				strcpy(vbuf, "ws:");                 \
				vlen = 3;                            \
				break;                               \
			case PROTO_WSS:                          \
				strcpy(vbuf, "wss:");                \
				vlen = 4;                            \
				break;                               \
			default:                                 \
				strcpy(vbuf, "udp:");                \
				vlen = 4;                            \
		}                                            \
	} while(0)

#define siptrace_copy_proto(vproto, vbuf)               \
	do {                                                \
		int __olen;                                     \
		siptrace_copy_proto_olen(vproto, vbuf, __olen); \
		(void)__olen;                                   \
	} while(0)

char *siptrace_proto_name(int vproto);

#endif
