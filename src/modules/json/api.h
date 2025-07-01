/**
 *
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _JSON_API_H_
#define _JSON_API_H_

#include "../../core/sr_module.h"

typedef struct json_object *(*json_parse_f)(const char *str);
typedef struct json_object *(*json_get_object_f)(
		struct json_object *json_obj, const char *str);
typedef int (*json_extract_field_f)(
		struct json_object *json_obj, char *json_name, str *var);

typedef struct json_api
{
	json_parse_f json_parse;
	json_get_object_f get_object;
	json_extract_field_f extract_field;
} json_api_t;

typedef int (*bind_json_f)(json_api_t *api);
int bind_json(json_api_t *api);

/**
 * @brief Load the JSON API
 */
static inline int json_load_api(json_api_t *api)
{
	bind_json_f bindjson;

	bindjson = (bind_json_f)find_export("bind_json", 0, 0);
	if(bindjson == 0) {
		LM_ERR("cannot find bind_json\n");
		return -1;
	}
	if(bindjson(api) < 0) {
		LM_ERR("cannot bind json api\n");
		return -1;
	}
	return 0;
}

#endif
