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

#include <stdio.h>
#include <string.h>
#include <jansson.h>

#include "../../core/mod_fix.h"
#include "../../core/lvalue.h"
#include "../../core/str.h"
#include "../../core/xavp.h"

#include "jansson_path.h"
#include "jansson_funcs.h"
#include "jansson_utils.h"

int janssonmod_get_helper(
		sip_msg_t *msg, str *path_s, str *src_s, pv_spec_t *dst_pv)
{
	char c;
	pv_value_t dst_val;
	json_t *json = NULL;
	json_error_t parsing_error;
	STR_VTOZ(src_s->s[src_s->len], c);
	json = json_loads(src_s->s, JSON_REJECT_DUPLICATES, &parsing_error);
	STR_ZTOV(src_s->s[src_s->len], c);
	if(!json) {
		ERR("failed to parse json: %.*s\n", src_s->len, src_s->s);
		ERR("json error at line %d, col %d: %s\n", parsing_error.line,
				parsing_error.column, parsing_error.text);
		goto fail;
	}

	char *path = path_s->s;

	json_t *v = json_path_get(json, path);
	if(!v) {
		goto fail;
	}

	char *freeme = NULL;

	if(jansson_to_val(&dst_val, &freeme, v) < 0)
		goto fail;

	dst_pv->setf(msg, &dst_pv->pvp, (int)EQ_T, &dst_val);

	if(freeme != NULL) {
		free(freeme);
	}

	json_decref(json);
	return 1;

fail:
	json_decref(json);
	return -1;
}

int janssonmod_get(struct sip_msg *msg, char *path_in, char *src_in, char *dst)
{
	str src_s;
	str path_s;

	if(fixup_get_svalue(msg, (gparam_p)src_in, &src_s) != 0) {
		ERR("cannot get json string value\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)path_in, &path_s) != 0) {
		ERR("cannot get path string value\n");
		return -1;
	}

	return janssonmod_get_helper(msg, &path_s, &src_s, (pv_spec_t *)dst);
}

int janssonmod_pv_get(
		struct sip_msg *msg, char *path_in, char *src_in, char *dst)
{
	str path_s;
	pv_value_t val;
	int ret;

	if((pv_get_spec_value(msg, (pv_spec_t *)src_in, &val) < 0)
			|| ((val.flags & PV_VAL_STR) == 0)) {
		ERR("cannot get json string value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)path_in, &path_s) != 0) {
		ERR("cannot get path string value\n");
		return -1;
	}

	ret = janssonmod_get_helper(msg, &path_s, &val.rs, (pv_spec_t *)dst);

	pv_value_destroy(&val);

	return ret;
}

#define STR_EQ_STATIC(a, b) \
	((a.len == sizeof(b) - 1) && (strncmp(a.s, b, sizeof(b) - 1) == 0))

int janssonmod_set(unsigned int append, struct sip_msg *msg, char *type_in,
		char *path_in, char *value_in, char *result_in)
{
	str type_s;
	str value_s;
	str path_s;
	char c;
	pv_spec_t *result_pv;
	pv_value_t result_val;

	if(fixup_get_svalue(msg, (gparam_p)type_in, &type_s) != 0) {
		ERR("cannot get type string value\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)value_in, &value_s) != 0) {
		ERR("cannot get value string\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)path_in, &path_s) != 0) {
		ERR("cannot get path string value\n");
		return -1;
	}

	result_pv = (pv_spec_t *)result_in;

	if(pv_get_spec_value(msg, result_pv, &result_val) != 0
			|| result_val.flags != PV_VAL_STR) {
		result_val.flags = PV_VAL_STR;
		result_val.rs.s = "{}";
		result_val.rs.len = strlen("{}");
	}


	LM_DBG("type is: %.*s\n", type_s.len, type_s.s);
	LM_DBG("path is: %.*s\n", path_s.len, path_s.s);
	LM_DBG("value is: %.*s\n", value_s.len, value_s.s);
	LM_DBG("result is: %.*s\n", result_val.rs.len, result_val.rs.s);

	json_t *result_json = NULL;
	json_t *value = NULL;
	char *freeme = NULL;
	json_error_t parsing_error = {0};
	char *endptr;

	/* check the type */
	if(STR_EQ_STATIC(type_s, "object") || STR_EQ_STATIC(type_s, "obj")) {
		STR_VTOZ(value_s.s[value_s.len], c);
		value = json_loads(value_s.s, JSON_REJECT_DUPLICATES, &parsing_error);
		STR_ZTOV(value_s.s[value_s.len], c);
		if(value && !json_is_object(value)) {
			ERR("value to add is not an object - \"%s\"\n", path_s.s);
			goto fail;
		}

	} else if(STR_EQ_STATIC(type_s, "array")) {
		STR_VTOZ(value_s.s[value_s.len], c);
		value = json_loads(value_s.s, JSON_REJECT_DUPLICATES, &parsing_error);
		STR_ZTOV(value_s.s[value_s.len], c);
		if(value && !json_is_array(value)) {
			ERR("value to add is not an array - \"%s\"\n", path_s.s);
			goto fail;
		}

	} else if(STR_EQ_STATIC(type_s, "string") || STR_EQ_STATIC(type_s, "str")) {
		value = json_string(value_s.s);
		if(!value || !json_is_string(value)) {
			ERR("value to add is not a string - \"%s\"\n", path_s.s);
			goto fail;
		}

	} else if(STR_EQ_STATIC(type_s, "integer")
			  || STR_EQ_STATIC(type_s, "int")) {
		long long i = strtoll(value_s.s, &endptr, 10);
		if(*endptr != '\0') {
			ERR("parsing int failed for \"%s\" - \"%s\"\n", path_s.s,
					value_s.s);
			goto fail;
		}
		value = json_integer(i);
		if(!value || !json_is_integer(value)) {
			ERR("value to add is not an integer \"%s\"\n", path_s.s);
			goto fail;
		}

	} else if(STR_EQ_STATIC(type_s, "real")) {
		double d = strtod(value_s.s, &endptr);
		if(*endptr != '\0') {
			ERR("parsing real failed for \"%s\" - \"%s\"\n", path_s.s,
					value_s.s);
			goto fail;
		}
		value = json_real(d);
		if(!value || !json_is_real(value)) {
			ERR("value to add is not a real \"%s\"\n", path_s.s);
			goto fail;
		}

	} else if(STR_EQ_STATIC(type_s, "true")) {
		value = json_true();

	} else if(STR_EQ_STATIC(type_s, "false")) {
		value = json_false();

	} else if(STR_EQ_STATIC(type_s, "null")) {
		value = json_null();

	} else {
		ERR("unrecognized input type\n");
		goto fail;
	}

	if(!value) {
		ERR("parsing failed for \"%s\"\n", value_s.s);
		ERR("value error at line %d: %s\n", parsing_error.line,
				parsing_error.text);
		goto fail;
	}

	char *path = path_s.s;
	STR_VTOZ(result_val.rs.s[result_val.rs.len], c);
	result_json =
			json_loads(result_val.rs.s, JSON_REJECT_DUPLICATES, &parsing_error);
	STR_ZTOV(result_val.rs.s[result_val.rs.len], c);
	if(!result_json) {
		ERR("result has json error at line %d: %s\n", parsing_error.line,
				parsing_error.text);
		goto fail;
	}

	if(json_path_set(result_json, path, value, append) < 0) {
		goto fail;
	}

	if(jansson_to_val(&result_val, &freeme, result_json) < 0)
		goto fail;

	result_pv->setf(msg, &result_pv->pvp, (int)EQ_T, &result_val);

	if(freeme)
		free(freeme);
	json_decref(result_json);
	return 1;

fail:
	if(freeme)
		free(freeme);
	json_decref(result_json);
	return -1;
}

int janssonmod_array_size(
		struct sip_msg *msg, char *path_in, char *src_in, char *dst)
{
	char c;
	str src_s;
	str path_s;
	pv_spec_t *dst_pv;
	pv_value_t dst_val;

	if(fixup_get_svalue(msg, (gparam_p)src_in, &src_s) != 0) {
		ERR("cannot get json string value\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)path_in, &path_s) != 0) {
		ERR("cannot get path string value\n");
		return -1;
	}

	dst_pv = (pv_spec_t *)dst;

	json_t *json = NULL;
	json_error_t parsing_error;
	STR_VTOZ(src_s.s[src_s.len], c);
	json = json_loads(src_s.s, JSON_REJECT_DUPLICATES, &parsing_error);
	STR_ZTOV(src_s.s[src_s.len], c);
	if(!json) {
		ERR("json error at line %d: %s\n", parsing_error.line,
				parsing_error.text);
		goto fail;
	}

	char *path = path_s.s;

	json_t *v = json_path_get(json, path);
	if(!v) {
		ERR("failed to find %s in json\n", path);
		goto fail;
	}

	if(!json_is_array(v)) {
		ERR("value at %s is not an array\n", path);
		goto fail;
	}

	int size = json_array_size(v);
	dst_val.ri = size;
	dst_val.flags = PV_TYPE_INT | PV_VAL_INT;

	dst_pv->setf(msg, &dst_pv->pvp, (int)EQ_T, &dst_val);

	json_decref(json);
	return 1;

fail:
	json_decref(json);
	return -1;
}


static int jansson_object2xavp(json_t *obj, str *xavp)
{
	const char *key;
	json_t *value;
	sr_xavp_t *row = NULL;
	sr_xval_t val;

	json_object_foreach(obj, key, value)
	{
		str name;
		char *freeme = NULL;

		if(jansson_to_xval(&val, &freeme, value) < 0) {
			ERR("failed to convert json object member value to xavp for key: "
				"%s\n",
					key);
			if(freeme != NULL) {
				free(freeme);
			}
			return -1;
		}

		name.s = (char *)key;
		name.len = strlen(name.s);

		xavp_add_value(&name, &val, &row);

		if(freeme != NULL) {
			free(freeme);
		}
	}

	/* Add row to result xavp */
	val.type = SR_XTYPE_XAVP;
	val.v.xavp = row;
	LM_DBG("Adding row\n");
	xavp_add_value(xavp, &val, NULL);
	return 1;
}


int jansson_xdecode(struct sip_msg *msg, char *src_in, char *xavp_in)
{
	str src_s;
	str xavp_s;
	json_t *json = NULL;
	json_error_t parsing_error;

	if(fixup_get_svalue(msg, (gparam_p)src_in, &src_s) != 0) {
		ERR("cannot get json string value\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)xavp_in, &xavp_s) != 0) {
		ERR("cannot get xavp string value\n");
		return -1;
	}

	LM_DBG("decoding '%.*s' into '%.*s'\n", src_s.len, src_s.s, xavp_s.len,
			xavp_s.s);
	json = json_loads(src_s.s, JSON_REJECT_DUPLICATES, &parsing_error);

	if(!json) {
		ERR("failed to parse json: %.*s\n", src_s.len, src_s.s);
		ERR("json error at line %d, col %d: %s\n", parsing_error.line,
				parsing_error.column, parsing_error.text);
		return -1;
	}

	if(json_is_object(json)) {
		if(jansson_object2xavp(json, &xavp_s) < 0) {
			goto fail;
		}
	} else if(json_is_array(json)) {
		size_t i;
		json_t *value;

		json_array_foreach(json, i, value)
		{
			if(jansson_object2xavp(value, &xavp_s) < 0) {
				goto fail;
			}
		}
	} else {
		LM_ERR("json root is not an object or array\n");
		goto fail;
	}

	json_decref(json);
	return 1;
fail:
	json_decref(json);
	return -1;
}

static int jansson_xavp2object(json_t *json, sr_xavp_t **head)
{
	sr_xavp_t *avp = NULL;
	json_t *it = NULL;

	if(json == NULL)
		return -1;

	avp = *head;
	if(avp->val.type != SR_XTYPE_XAVP) {
		LM_ERR("cannot iterate xavp members\n");
		return -1;
	}
	avp = avp->val.v.xavp;
	while(avp) {
		switch(avp->val.type) {
			case SR_XTYPE_NULL:
				it = json_null();
				break;
			case SR_XTYPE_LONG:
				it = json_integer((json_int_t)avp->val.v.l);
				break;
			case SR_XTYPE_STR:
				it = json_stringn(avp->val.v.s.s, avp->val.v.s.len);
				break;
			case SR_XTYPE_TIME:
				it = json_integer((json_int_t)avp->val.v.t);
				break;
			case SR_XTYPE_LLONG:
				it = json_integer((json_int_t)avp->val.v.ll);
				break;
			case SR_XTYPE_XAVP:
				it = json_string("<<xavp>>");
				break;
			case SR_XTYPE_DATA:
				it = json_string("<<data>>");
				break;
			default:
				LM_ERR("unknown xavp type: %d\n", avp->val.type);
				return -1;
		}
		if(it == NULL) {
			LM_ERR("failed to create json value\n");
			return -1;
		}
		if(json_object_set_new(json, avp->name.s, it) < 0) {
			LM_ERR("failed to add member to object\n");
			return -1;
		}
		avp = avp->next;
	}
	return 1;
}

int jansson_xencode(struct sip_msg *msg, char *xavp, char *dst)
{
	str xavp_s;
	json_t *json;
	pv_spec_t *dst_pv;
	pv_value_t dst_val;
	sr_xavp_t *avp = NULL;
	int ret = 1;

	if(fixup_get_svalue(msg, (gparam_p)xavp, &xavp_s) != 0) {
		LM_ERR("cannot get field string value\n");
		return -1;
	}

	LM_DBG("encoding '%.*s' into '%p'\n", xavp_s.len, xavp_s.s, dst);

	avp = xavp_get(&xavp_s, NULL);
	if(avp == NULL || avp->val.type != SR_XTYPE_XAVP) {
		return -1;
	}

	json = json_object();
	if(json == NULL) {
		LM_ERR("could not obtain json handle\n");
		return -1;
	}
	ret = jansson_xavp2object(json, &avp);
	if(ret > 0) {
		dst_val.rs.s = json_dumps(json, 0);
		dst_val.rs.len = strlen(dst_val.rs.s);
		dst_val.flags = PV_VAL_STR;
		dst_pv = (pv_spec_t *)dst;
		if(dst_pv->setf(msg, &dst_pv->pvp, (int)EQ_T, &dst_val) < 0) {
			ret = -1;
		}
		free(dst_val.rs.s);
	} else {
		LM_ERR("json encoding failed\n");
	}

	json_decref(json);
	return ret;
}
