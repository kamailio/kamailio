/*
 * PV Headers
 *
 * Copyright (C) 2018 Kirill Solomko <ksolomko@sipwise.com>
 *
 * This file is part of SIP Router, a free SIP server.
 *
 * SIP Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP Router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef PV_HDR_H
#define PV_HDR_H

#include "../../core/parser/msg_parser.h"
#include "../../core/xavp.h"

#include "pv_headers.h"

int pvh_real_hdr_append(struct sip_msg *msg, str *hname, str *hvalue);
int pvh_real_hdr_replace(struct sip_msg *msg, str *hname, str *hvalue);
int pvh_real_hdr_del_by_name(struct sip_msg *msg, str *hname);
int pvh_real_hdr_remove_display(struct sip_msg *msg, str *hname);
int pvh_real_replace_reply_reason(struct sip_msg *msg, str *value);
int pvh_create_hdr_str(str *hname, str *hvalue, str *dst);

#endif /* PV_HDR_H */