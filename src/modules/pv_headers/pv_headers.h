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

#ifndef PV_HEADERS_H
#define PV_HEADERS_H

#include "../../core/parser/parse_addr_spec.h"
#include "../../modules/uac/api.h"

typedef struct _xavp_c_data
{
	struct to_body to_b;
	struct to_param *to_params;
	str value;
} xavp_c_data_t;

extern uac_api_t pvh_uac;

#define PVH_HDRS_COLLECTED 0
#define PVH_HDRS_APPLIED 1
typedef struct _pvh_params
{
	str xavi_name;
	str xavi_helper_xname;
	str xavi_parsed_xname;
	unsigned int hdr_value_size;
	int flags[2];
	str skip_hdrs;
	str split_hdrs;
	str single_hdrs;
	unsigned int auto_msg;
} _pvh_params_t;
extern _pvh_params_t _pvh_params;

extern unsigned int pvh_hdr_name_size;
extern str pvh_hdr_from;
extern str pvh_hdr_to;
extern str pvh_hdr_reply_reason;
extern int pvh_branch;
extern int pvh_reply_counter;

#endif /* PV_HEADERS_H */
