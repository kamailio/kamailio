/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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
 */

/*!
 * \file
 * \brief Kamailio topoh ::
 * \ingroup topoh
 * Module: \ref topoh
 */

#ifndef _TOPOS_MSG_H_
#define _TOPOS_MSG_H_

#include "../../core/parser/msg_parser.h"

#define TPS_SPLIT_VIA (1 << 0)
#define TPS_SPLIT_RECORD_ROUTE (1 << 1)
#define TPS_SPLIT_ROUTE (1 << 2)

int tps_update_hdr_replaces(sip_msg_t *msg);
char *tps_msg_update(sip_msg_t *msg, unsigned int *olen);
int tps_skip_msg(sip_msg_t *msg);

int tps_request_received(sip_msg_t *msg, int dialog);
int tps_response_received(sip_msg_t *msg);
int tps_request_sent(sip_msg_t *msg, int dialog, int local);
int tps_response_sent(sip_msg_t *msg);
int tps_mask_callid(sip_msg_t *msg);
int tps_unmask_callid(sip_msg_t *msg);
#endif
