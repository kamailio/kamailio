/*
 * NSQ module interface
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
 * Contributor(s):
 * Emmanuel Schmidbauer <emmanuel@getweave.com>
 *
 */

#ifndef __NSQ_JSON_H_
#define __NSQ_JSON_H_

#include "../../core/mod_fix.h"
#include "../../core/lvalue.h"
#include "../../core/parser/msg_parser.h"
#include <json.h>

#define json_extract_field(json_name, field)  do {                      \
	struct json_object* obj =  nsq_json_get_object(json_obj, json_name); \
	field.s = (char*)json_object_get_string(obj);                       \
	if (field.s == NULL) {                                              \
	  LM_DBG("Json-c error - failed to extract field [%s]\n", json_name); \
	  field.s = "";                                                     \
	} else {                                                            \
	  field.len = strlen(field.s);                                      \
	}                                                                   \
	LM_DBG("%s: [%s]\n", json_name, field.s?field.s:"Empty");           \
  } while (0);


extern char nsq_json_escape_char;
extern str nsq_event_key;
extern str nsq_event_sub_key;

int nsq_json_get_field(struct sip_msg* msg, char* json, char* field, char* dst);
int nsq_json_get_field_ex(str* json, str* field, pv_value_p dst_val);
int nsq_json_get_keys(struct sip_msg* msg, char* json, char* field, char* dst);

struct json_object* nsq_json_parse(const char *str);
struct json_object* nsq_json_get_object(struct json_object* jso, const char *key);

#endif /* __NSQ_JSON_H_ */
