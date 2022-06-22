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

#ifndef PV_FUNC_H
#define PV_FUNC_H

#include "../../core/parser/msg_parser.h"

int pvh_parse_msg(sip_msg_t *msg);

int pvh_collect_headers(struct sip_msg *msg);
int pvh_apply_headers(struct sip_msg *msg);
int pvh_reset_headers(struct sip_msg *msg);

int pvh_check_header(struct sip_msg *msg, str *hname);
int pvh_append_header(struct sip_msg *msg, str *hname, str *hvalue);
int pvh_modify_header(struct sip_msg *msg, str *hname, str *hvalue, int indx);
int pvh_remove_header(struct sip_msg *msg, str *hname, int indx);
int pvh_header_param_exists(struct sip_msg *msg, str *hname, str *hvalue);
int pvh_remove_header_param_helper(str *orig, const str *toRemove, str *dst);

#endif /* PV_FUNC_H */
