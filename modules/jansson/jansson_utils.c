/**
 * Copyright (C) 2013 Flowroute LLC (flowroute.com)
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

#define _GNU_SOURCE
#include <stdio.h>
#include <jansson.h>

#include "../../lvalue.h"

#include "jansson_utils.h"

int jansson_to_val(pv_value_t* val, char** freeme, json_t* v) {

	val->flags = 0;

	if(json_is_object(v) || json_is_array(v)) {
		const char* value = json_dumps(v, JSON_COMPACT|JSON_PRESERVE_ORDER);
		*freeme = (char*)value;
		val->rs.s = (char*)value;
		val->rs.len = strlen(value);
		val->flags = PV_VAL_STR;
	}else if(json_is_string(v)) {
		const char* value = json_string_value(v);
		val->rs.s = (char*)value;
		val->rs.len = strlen(value);
		val->flags = PV_VAL_STR;
	}else if(json_is_boolean(v)) {
		val->ri = json_is_true(v) ? 0 : 1;
		val->flags = PV_TYPE_INT|PV_VAL_INT;
	}else if(json_is_real(v)) {
		char* value = NULL;
		if(asprintf(&value, "%.15g", json_real_value(v))<0) {
			ERR("asprintf failed\n");
			return -1;
		}
		*freeme = value;
		val->rs.s = value;
		val->rs.len = strlen(value);
		val->flags = PV_VAL_STR;
	}else if(json_is_integer(v)) {
		int value = json_integer_value(v);
		val->ri = value;
		val->flags = PV_TYPE_INT|PV_VAL_INT;
	}else if(json_is_null(v)) {
		val->flags = PV_VAL_NULL;
	}else {
		ERR("unrecognized json type: %d\n", json_typeof(v));
		return -1;
	}
	return 0;
}
