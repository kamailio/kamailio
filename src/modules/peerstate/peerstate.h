/*
 *
 * Peerstate Module - Peer State Tracking
 *
 * Copyright (C) 2025 Serdar Gucluer (Netgsm ICT Inc. - www.netgsm.com.tr)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef _PEERSTATE_H
#define _PEERSTATE_H

#include "../../core/parser/parse_uri.h"
struct str_list;

#define PEERSTATE_EVENT_ROUTE "peerstate:state_changed"

typedef enum ps_state
{
	NOT_INUSE,	 // according to dialog state
	INUSE,		 // according to dialog and register state
	RINGING,	 // according to dialog state
	UNAVAILABLE, // according to register state
	NA
} ps_state_t;

static const char *ps_state_strs[] __attribute__((unused)) = {
		"NOT_INUSE", "INUSE", "RINGING", "UNAVAILABLE", "N/A"};

#define PS_STATE_TO_STR(s)                                                     \
	((s) >= 0 && (s) < (int)(sizeof(ps_state_strs) / sizeof(ps_state_strs[0])) \
					? ps_state_strs[s]                                         \
					: "UNKNOWN")

struct ps_dlginfo_cell
{
	struct sip_uri *from;
	struct sip_uri *to;
	str callid;
	struct str_list *caller_peers;
	struct str_list *callee_peers;
	int disable_caller_notify;
	int disable_callee_notify;
};

#endif /* _PEERSTATE_H */