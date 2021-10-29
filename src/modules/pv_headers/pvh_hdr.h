/*
 * pv_headers
 *
 * Copyright (C)
 * 2020 Victor Seva <vseva@sipwise.com>
 * 2018 Kirill Solomko <ksolomko@sipwise.com>
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

#ifndef PV_HDR_H
#define PV_HDR_H

#include "../../core/parser/msg_parser.h"
#include "../../core/xavp.h"

#include "pv_headers.h"

int pvh_hdrs_collected(struct sip_msg *msg);
int pvh_hdrs_applied(struct sip_msg *msg);
void pvh_hdrs_set_collected(struct sip_msg *msg);
void pvh_hdrs_set_applied(struct sip_msg *msg);
void pvh_hdrs_reset_flags(struct sip_msg *msg);

int pvh_real_hdr_append(struct sip_msg *msg, str *hname, str *hvalue);
int pvh_real_hdr_del_by_name(struct sip_msg *msg, str *hname);
int pvh_real_hdr_remove_display(struct sip_msg *msg, str *hname);
int pvh_real_replace_reply_reason(struct sip_msg *msg, str *value);
int pvh_create_hdr_str(str *hname, str *hvalue, str *dst);

#endif /* PV_HDR_H */