/*
 * $Id$
 *
 * Kazoo module interface
 *
 * Copyright (C) 2010-2014 2600Hz
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2014-08  first version (2600hz)
 */

#ifndef KZ_JSON_H_
#define KZ_JSON_H_

#include "../../core/parser/msg_parser.h"
#include <json.h>


int kz_json_get_field(struct sip_msg* msg, char* json, char* field, char* dst);
int kz_json_get_field_ex(str* json, str* field, pv_value_p dst_val);
int kz_json_get_keys(struct sip_msg* msg, char* json, char* field, char* dst);

struct json_object* kz_json_parse(const char *str);
struct json_object* kz_json_get_object(struct json_object* jso, const char *key);

#endif /* KZ_JSON_H_ */
