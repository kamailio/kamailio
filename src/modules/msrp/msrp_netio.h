/**
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
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

#ifndef _MSRP_NETIO_H_
#define _MSRP_NETIO_H_

#include "../../dns_cache.h"

#include "msrp_parser.h"

int msrp_forward_frame(msrp_frame_t *mf, int flags);
int msrp_send_buffer(str *buf, str *addr, int flags);

int msrp_relay(msrp_frame_t *mf);
int msrp_reply(msrp_frame_t *mf, str *code, str *text, str *xhdrs);

struct dest_info *msrp_uri_to_dstinfo(struct dns_srv_handle* dns_h,
		struct dest_info* dst, struct socket_info *force_send_socket,
		snd_flags_t sflags, str *uri);
struct socket_info *msrp_get_local_socket(str *sockaddr);

#endif
