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

#ifndef PV_XAVP_H
#define PV_XAVP_H

#include "../../core/parser/parse_uri.h"
#include "../../core/xavp.h"
#include "../../core/pvar.h"

#include "pv_headers.h"

sr_xavp_t *pvh_xavp_new_value(str *name, sr_xval_t *val);
int pvh_xavp_append_value(str *name, sr_xval_t *val, sr_xavp_t **start);
int pvh_xavp_set_value(str *name, sr_xval_t *val, int idx, sr_xavp_t **start);
sr_xval_t *pvh_xavp_get_value(
		struct sip_msg *msg, str *xname, str *name, int idx);
sr_xavp_t *pvh_xavp_get_child(struct sip_msg *msg, str *xname, str *name);
int pvh_xavp_is_null(sr_xavp_t *xavp);
void pvh_xavp_free_data(void *p, sr_xavp_sfree_f sfree);
int pvh_xavp_keys_count(sr_xavp_t **start);
void pvh_free_to_params(struct to_param *param, sr_xavp_sfree_f sfree);
int pvh_set_xavp(struct sip_msg *msg, str *xname, str *name, void *data,
		sr_xtype_t type, int idx, int append);
int pvh_free_xavp(str *xname);
int pvh_parse_header_name(pv_spec_p sp, str *hname);

int pvh_get_header(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
int pvh_set_header(
		struct sip_msg *msg, pv_param_t *param, int op, pv_value_t *val);
int pvh_get_uri(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
int pvh_set_uri(
		struct sip_msg *msg, pv_param_t *param, int op, pv_value_t *val);
int pvh_merge_uri(struct sip_msg *msg, enum action_type type, str *cur,
		str *new, xavp_c_data_t *c_data);
int pvh_get_reply_sr(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
int pvh_set_reply_sr(
		struct sip_msg *msg, pv_param_t *param, int op, pv_value_t *val);

int pvh_get_branch_index(struct sip_msg *msg, int *br_idx);
int pvh_get_branch_xname(struct sip_msg *msg, str *xname, str *dst);
int pvh_clone_branch_xavp(struct sip_msg *msg, str *xname);

#endif /* PV_XAVP_H */
