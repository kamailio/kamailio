/*
 * Copyright (C) 2026 Andrei Lashchinskii <koorwork+kamailio@gmail.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef NDB_TARANTOOL_KEMI_H
#define NDB_TARANTOOL_KEMI_H

#include "../../core/kemi.h"
#include "../../core/pvar.h"
#include "../../core/sr_module.h"
#include "../../core/str.h"

int sr_kemi_tarantool_call(
		sip_msg_t *msg, str *proc_name, str *params_json, str *res_dst);
int sr_kemi_tarantool_eval(
		sip_msg_t *msg, str *expr, str *params_json, str *res_dst);

int sr_kemi_tarantool_call_srv(sip_msg_t *msg, str *srv_name, str *proc_name,
		str *params_json, str *res_dst);
int sr_kemi_tarantool_eval_srv(sip_msg_t *msg, str *srv_name, str *expr,
		str *params_json, str *res_dst);

int tnt_set_result_pvs(sip_msg_t *msg, pv_spec_t *pvs, str *res_val);

int sr_kemi_ndb_tarantool_register(void);

#endif /* NDB_TARANTOOL_KEMI_H */
