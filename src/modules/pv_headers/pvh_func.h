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

#ifndef PV_FUNC_H
#define PV_FUNC_H

#include "../../core/parser/msg_parser.h"

int pvh_collect_headers(struct sip_msg *msg, int is_auto);
int pvh_apply_headers(struct sip_msg *msg, int is_auto);
int pvh_reset_headers(struct sip_msg *msg);

int pvh_check_header(struct sip_msg *msg, str *hname);
int pvh_append_header(struct sip_msg *msg, str *hname, str *hvalue);
int pvh_modify_header(struct sip_msg *msg, str *hname, str *hvalue, int indx);
int pvh_remove_header(struct sip_msg *msg, str *hname, int indx);

#endif /* PV_FUNC_H */
