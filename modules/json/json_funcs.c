/**
 * $Id$
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

#include <stdio.h>
#include <string.h>
#include <json.h>

#include "../../mod_fix.h"
#include "../../lvalue.h"

#include "json_funcs.h"

int json_get_field(struct sip_msg* msg, char* json, char* field, char* dst)
{
	str json_s;
	str field_s;
	pv_spec_t *dst_pv;
	pv_value_t dst_val;
	char *value;
	struct json_object *j = NULL;

	if (fixup_get_svalue(msg, (gparam_p)json, &json_s) != 0) {
		LM_ERR("cannot get json string value\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_p)field, &field_s) != 0) {
		LM_ERR("cannot get field string value\n");
		return -1;
	}

	dst_pv = (pv_spec_t *)dst;


	j = json_tokener_parse(json_s.s);

	if (is_error(j)) {
		LM_ERR("empty or invalid JSON\n");
		if(j!=NULL) json_object_put(j);
		return -1;
	}

	value = (char*)json_object_to_json_string(json_object_object_get(j, field_s.s));

	dst_val.rs.s = value;
	dst_val.rs.len = strlen(value);
	dst_val.flags = PV_VAL_STR;
	dst_pv->setf(msg, &dst_pv->pvp, (int)EQ_T, &dst_val);

	if(j!=NULL) json_object_put(j);
	return 1;
}
