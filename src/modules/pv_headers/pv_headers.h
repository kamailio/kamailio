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

extern uac_api_t uac;

extern str xavi_name;
extern str xavi_parsed_xname;
extern str xavi_helper_xname;

extern unsigned int header_name_size;
extern unsigned int header_value_size;

extern str _hdr_from;
extern str _hdr_to;
extern str _hdr_reply_reason;
extern int _branch;
extern int _reply_counter;

extern int FL_PV_HDRS_COLLECTED;
extern int FL_PV_HDRS_APPLIED;

#endif /* PV_HEADERS_H */
