/*
 * Copyright (C) 2005 iptelorg GmbH
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
/*!
 * \file
 * \brief Kamailio core :: IP address handling
 * \author andrei
 * \ingroup core
 * Module: \ref core
 */

#ifndef onsend_h
#define onsend_h


#include "ip_addr.h"
#include "script_cb.h"
#include "sr_compat.h"
#include "action.h"
#include "route.h"
#include "kemi.h"

typedef struct onsend_info
{
	dest_info_t *dst;			   /* destination info */
	union sockaddr_union *to;	   /* destination address */
	struct socket_info *send_sock; /* local send socket */
	char *buf;					   /* outgoing buffer */
	int len;					   /* outgoing buffer len */
	sip_msg_t *msg;				   /* original sip msg struct */
	int rmode;					   /* runtime execution mode */
	int rplcode;				   /* reply code */
} onsend_info_t;

extern onsend_info_t *p_onsend;

#define get_onsend_info() (p_onsend)

/*
 * returns: 0 drop the message, >= ok, <0 error (but forward the message)
 * it also migh change dst->send_flags!
 * WARNING: buf must be 0 terminated (to allow regex matches on it) */
int run_onsend(sip_msg_t *orig_msg, dest_info_t *dst, char *buf, int len);

#define onsend_route_enabled(rtype)                                    \
	((onsend_rt.rlist[DEFAULT_RT]                                      \
					 ? ((rtype == SIP_REPLY) ? onsend_route_reply : 1) \
					 : 0)                                              \
			|| (kemi_onsend_route_callback.len > 0 && sr_kemi_eng_get()))

int run_onsend_evroute(
		onsend_info_t *sndinfo, int evrt, str *evcb, str *evname);

#endif
