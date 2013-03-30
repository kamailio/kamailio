/*
 * $Id$
 *
 * Copyright (C) 2012 Crocodile RCS Ltd
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _WS_HANDSHAKE_H
#define _WS_HANDSHAKE_H

#include "../../sr_module.h"
#include "../../parser/msg_parser.h"
#include "ws_mod.h"

#define DEFAULT_SUB_PROTOCOLS	(SUB_PROTOCOL_SIP | SUB_PROTOCOL_MSRP)
#define SUB_PROTOCOL_ALL	(SUB_PROTOCOL_SIP | SUB_PROTOCOL_MSRP)
extern int ws_sub_protocols;

extern stat_var *ws_failed_handshakes;
extern stat_var *ws_successful_handshakes;
extern stat_var *ws_sip_successful_handshakes;
extern stat_var *ws_msrp_successful_handshakes;

int ws_handle_handshake(struct sip_msg *msg);
struct mi_root *ws_mi_disable(struct mi_root *cmd, void *param);
struct mi_root *ws_mi_enable(struct mi_root *cmd, void *param);

#endif /* _WS_HANDSHAKE_H */
