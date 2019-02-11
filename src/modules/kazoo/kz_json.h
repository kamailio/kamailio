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


int kz_json_get_count(str* json, str* field, pv_value_p dst_val);
int kz_json_get_field(struct sip_msg* msg, char* json, char* field, char* dst);
int kz_json_get_field_ex(str* json, str* field, pv_value_p dst_val);
int kz_json_get_keys(struct sip_msg* msg, char* json, char* field, char* dst);
enum json_type kz_json_get_type(struct json_object *jso);

struct json_object* kz_json_parse(const char *str);
struct json_object* kz_json_get_object(struct json_object* jso, const char *key);

#if defined(__GNUC__) && !defined(__STRICT_ANSI__) && __STDC_VERSION__ >= 199901L

# define json_foreach(obj,key,val) \
	char *key; \
	struct json_object *val __attribute__((__unused__)); \
	for(struct lh_entry *entry ## key = json_object_get_object(obj)->head, *entry_next ## key = NULL; \
		({ if(entry ## key) { \
			key = (char*)entry ## key->k; \
			val = (struct json_object*)entry ## key->v; \
			entry_next ## key = entry ## key->next; \
		} ; entry ## key; }); \
		entry ## key = entry_next ## key )

# define json_foreach_key(obj,key) \
	char *key; \
	for(struct lh_entry *entry ## key = json_object_get_object(obj)->head, *entry_next ## key = NULL; \
		({ if(entry ## key) { \
			key = (char*)entry ## key->k; \
			entry_next ## key = entry ## key->next; \
		} ; entry ## key; }); \
		entry ## key = entry_next ## key )

#else /* ANSI C or MSC */

# define json_foreach(obj,key,val) \
	char *key;\
	struct json_object *val; \
	struct lh_entry *entry ## key; \
	struct lh_entry *entry_next ## key = NULL; \
	for(entry ## key = json_object_get_object(obj)->head; \
		(entry ## key ? ( \
			key = (char*)entry ## key->k, \
			val = (struct json_object*)entry ## key->v, \
			entry_next ## key = entry ## key->next, \
			entry ## key) : 0); \
		entry ## key = entry_next ## key)

# define json_foreach_key(obj,key) \
	char *key;\
	struct lh_entry *entry ## key; \
	struct lh_entry *entry_next ## key = NULL; \
	for(entry ## key = json_object_get_object(obj)->head; \
		(entry ## key ? ( \
			key = (char*)entry ## key->k, \
			entry_next ## key = entry ## key->next, \
			entry ## key) : 0); \
		entry ## key = entry_next ## key)

#endif /* defined(__GNUC__) && !defined(__STRICT_ANSI__) && __STDC_VERSION__ >= 199901L */



#endif /* KZ_JSON_H_ */
