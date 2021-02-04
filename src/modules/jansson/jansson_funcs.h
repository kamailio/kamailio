/**
 * Copyright (C) 2011 Flowroute LLC (flowroute.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _JANSSON_FUNCS_H_
#define _JANSSON_FUNCS_H_

#include "../../core/parser/msg_parser.h"

int janssonmod_get(struct sip_msg* msg, char* path_in, char* json_in,
		char* result);
int janssonmod_set(unsigned int append, struct sip_msg* msg, char* type_in,
		char* path_in, char* value_in, char* result);
int janssonmod_array_size(struct sip_msg* msg, char* json_in,
		char* path_in, char* dst);
int janssonmod_get_helper(sip_msg_t* msg, str *path_s, str *src_s,
		pv_spec_t *dst_pv);
int jansson_xdecode(struct sip_msg* msg, char* src_in, char* xavp_in);
int jansson_xencode(struct sip_msg* msg, char* xavp, char* dst);

#endif
