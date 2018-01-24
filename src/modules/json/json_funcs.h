/**
 *
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _JSON_FUNCS_H_
#define _JSON_FUNCS_H_

#include "../../core/parser/msg_parser.h"
#include <json.h>

int json_get_field(struct sip_msg* msg, char* json, char* field, char* dst);

#define json_extract_field(json_name, field)                                \
	do {                                                                    \
		struct json_object *obj = json_get_object(json_obj, json_name); \
		field.s = (char *)json_object_get_string(obj);                      \
		if(field.s == NULL) {                                               \
			LM_DBG("Json-c error - failed to extract field [%s]\n",         \
					json_name);                                             \
			field.s = "";                                                   \
		} else {                                                            \
			field.len = strlen(field.s);                                    \
		}                                                                   \
		LM_DBG("%s: [%s]\n", json_name, field.s ? field.s : "Empty");       \
	} while(0);


extern char tr_json_escape_char;
extern str json_event_key;
extern str json_event_sub_key;

int tr_json_get_field(struct sip_msg *msg, char *json, char *field, char *dst);
int tr_json_get_keys(struct sip_msg *msg, char *json, char *field, char *dst);

static inline struct json_object *json_parse(const char *str)
{
	struct json_tokener *tok;
	struct json_object *obj;

	tok = json_tokener_new();
	if(!tok) {
		LM_ERR("Error parsing json: could not allocate tokener\n");
		return NULL;
	}

	obj = json_tokener_parse_ex(tok, str, -1);
	if(tok->err != json_tokener_success) {
		LM_ERR("Error parsing json: %s\n", json_tokener_error_desc(tok->err));
		LM_ERR("%s\n", str);
		if(obj != NULL) {
			json_object_put(obj);
		}
		obj = NULL;
	}

	json_tokener_free(tok);
	return obj;
}

static inline struct json_object *json_get_object(
		struct json_object *jso, const char *key)
{
	struct json_object *result = NULL;
	json_object_object_get_ex(jso, key, &result);
	return result;
}

#endif
