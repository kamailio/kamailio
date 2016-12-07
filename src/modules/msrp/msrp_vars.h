/**
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
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
 *
 */

#ifndef _MSRP_VARS_H_
#define _MSRP_VARS_H_

#include "../../pvar.h"
#include "msrp_parser.h"

int pv_parse_msrp_name(pv_spec_t *sp, str *in);
int pv_get_msrp(sip_msg_t *msg,  pv_param_t *param, pv_value_t *res);
int pv_set_msrp(sip_msg_t *msg, pv_param_t *param, int op,
		pv_value_t *val);

char* tr_parse_msrpuri(str* in, trans_t *t);

#endif
