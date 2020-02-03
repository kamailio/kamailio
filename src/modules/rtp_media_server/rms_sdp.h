/*
 * Copyright (C) 2017-2018 Julien Chavanton jchavanton@gmail.com
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef rms_sdp_h
#define rms_sdp_h

#include "../../core/sr_module.h"
#include <mediastreamer2/mediastream.h>

typedef struct rms_sdp_info
{
	str remote_ip;
	str local_ip;
	str payloads;
	int remote_port;
	int ipv6;
	str new_body;
	str recv_body;
	int udp_local_port;
} rms_sdp_info_t;

int rms_get_sdp_info(rms_sdp_info_t *sdp_info, struct sip_msg *msg);
int rms_sdp_set_body(struct sip_msg *msg, str *new_body);
int rms_sdp_prepare_new_body(rms_sdp_info_t *, PayloadType *);
void rms_sdp_info_init(rms_sdp_info_t *sdp_info);
int rms_sdp_info_clone(rms_sdp_info_t *dst, rms_sdp_info_t *src);
void rms_sdp_info_free(rms_sdp_info_t *sdp_info);
PayloadType *rms_sdp_select_payload(rms_sdp_info_t *);

#endif
