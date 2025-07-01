/*
 * pv_headers
 *
 * Copyright (C)
 * 2020-2023 Victor Seva <vseva@sipwise.com>
 * 2018 Kirill Solomko <ksolomko@sipwise.com>
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
 *
 */

#ifndef PV_XAVP_H
#define PV_XAVP_H

#include "../../core/str.h"
#include "../../core/xavp.h"

#include "pv_headers.h"

int pvh_reply_append(sr_xavp_t **start);

sr_xavp_t *pvh_set_xavi(struct sip_msg *msg, str *xname, str *name, void *data,
		sr_xtype_t type, int idx, int append);
int pvh_xavi_keys_count(sr_xavp_t **start);
sr_xavp_t *pvh_xavi_get_child(struct sip_msg *msg, str *xname, str *name);
int pvh_avp_is_null(sr_xavp_t *avp);

int pvh_get_branch_xname(struct sip_msg *msg, str *xname, str *dst);
int pvh_clone_branch_xavi(struct sip_msg *msg, str *xname);

int pvh_parse_header_name(pv_spec_p sp, str *hname);
int pvh_get_header(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
int pvh_set_header(
		struct sip_msg *msg, pv_param_t *param, int op, pv_value_t *val);
xavp_c_data_t *pvh_set_parsed(
		struct sip_msg *msg, str *hname, str *cur, str *new);

int pvh_set_uri(
		struct sip_msg *msg, pv_param_t *param, int op, pv_value_t *val);
int pvh_get_uri(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
int pvh_merge_uri(struct sip_msg *msg, enum action_type type, str *cur,
		str *new, xavp_c_data_t *c_data);

int pvh_get_reply_sr(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
int pvh_set_reply_sr(
		struct sip_msg *msg, pv_param_t *param, int op, pv_value_t *val);

#endif /* PV_XAVP_H */
