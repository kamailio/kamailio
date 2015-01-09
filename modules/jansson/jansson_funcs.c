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

#include "../../mod_fix.h"
#include "../../lvalue.h"

#include "jansson_path.h"
#include "jansson_funcs.h"
#include "jansson_utils.h"

int janssonmod_get(struct sip_msg* msg, char* path_in, char* src_in, char* dst)
{
	str src_s;
	str path_s;
	pv_spec_t *dst_pv;
	pv_value_t dst_val;

	if (fixup_get_svalue(msg, (gparam_p)src_in, &src_s) != 0) {
		ERR("cannot get json string value\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_p)path_in, &path_s) != 0) {
		ERR("cannot get path string value\n");
		return -1;
	}

	dst_pv = (pv_spec_t *)dst;

	json_t* json = NULL;
	json_error_t parsing_error;

	json = json_loads(src_s.s, JSON_REJECT_DUPLICATES, &parsing_error);

	if(!json) {
		ERR("failed to parse: %.*s\n", src_s.len, src_s.s);
		ERR("json error at line %d: %s\n",
				parsing_error.line, parsing_error.text);
		goto fail;
	}

	char* path = path_s.s;

	json_t* v = json_path_get(json, path);
	if(!v) {
		goto fail;
	}

	char* freeme = NULL;

	if(jansson_to_val(&dst_val, &freeme, v)<0) goto fail;

	dst_pv->setf(msg, &dst_pv->pvp, (int)EQ_T, &dst_val);

	if(freeme!=NULL) {
		free(freeme);
	}

	json_decref(json);
	return 1;

fail:
	json_decref(json);
	return -1;
}

#define STR_EQ_STATIC(a,b) ((a.len == sizeof(b)-1) && (strncmp(a.s, b, sizeof(b)-1)==0))

int janssonmod_set(unsigned int append, struct sip_msg* msg, char* type_in,
		 char* path_in, char* value_in, char* result_in)
{
	str type_s;
	str value_s;
	str path_s;

	pv_spec_t* result_pv;
	pv_value_t result_val;

	if (fixup_get_svalue(msg, (gparam_p)type_in, &type_s) != 0){
		ERR("cannot get type string value\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_p)value_in, &value_s) != 0){
		ERR("cannot get value string\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_p)path_in, &path_s) != 0){
		ERR("cannot get path string value\n");
		return -1;
	}

	result_pv = (pv_spec_t *)result_in;

	if(pv_get_spec_value(msg, result_pv, &result_val)!=0
			|| result_val.flags != PV_VAL_STR) {
		result_val.flags = PV_VAL_STR;
		result_val.rs.s = "{}";
		result_val.rs.len = strlen("{}");
	}

/*
	ALERT("type is: %.*s\n", type_s.len, type_s.s);
	ALERT("path is: %.*s\n", path_s.len, path_s.s);
	ALERT("value is: %.*s\n", value_s.len, value_s.s);
	ALERT("result is: %.*s\n", result_val.rs.len, result_val.rs.s);
*/

	char* result = result_val.rs.s;

	json_t* result_json = NULL;
	json_t* value = NULL;
	char* freeme = NULL;
	json_error_t parsing_error;
	char* endptr;

	/* check the type */
	if(STR_EQ_STATIC(type_s, "object") || STR_EQ_STATIC(type_s, "obj")){
		value = json_loads(value_s.s, JSON_REJECT_DUPLICATES, &parsing_error);
		if(value && !json_is_object(value)) {
			ERR("value to add is not an object\n");
			goto fail;
		}

	}else if(STR_EQ_STATIC(type_s, "array")) {
		value = json_loads(value_s.s, JSON_REJECT_DUPLICATES, &parsing_error);
		if(value && !json_is_array(value)) {
			ERR("value to add is not an array\n");
			goto fail;
		}

	}else if(STR_EQ_STATIC(type_s, "string")
				|| STR_EQ_STATIC(type_s, "str")) {
		value = json_string(value_s.s);
		if(!value || !json_is_string(value)) {
			ERR("value to add is not a string\n");
			goto fail;
		}

	}else if(STR_EQ_STATIC(type_s, "integer")
				|| STR_EQ_STATIC(type_s, "int")) {
		int i = strtol(value_s.s, &endptr, 10);
		if(*endptr != '\0') {
			ERR("parsing int failed for \"%s\"\n", value_s.s);
			goto fail;
		}
		value = json_integer(i);
		if(!value || !json_is_integer(value)) {
			ERR("value to add is not an integer\n");
			goto fail;
		}

	}else if(STR_EQ_STATIC(type_s, "real")) {
		double d = strtod(value_s.s, &endptr);
		if(*endptr != '\0') {
			ERR("parsing real failed for \"%s\"\n", value_s.s);
			goto fail;
		}
		value = json_real(d);
		if(!value || !json_is_real(value)) {
			ERR("value to add is not a real\n");
			goto fail;
		}

	}else if(STR_EQ_STATIC(type_s, "true")) {
		value = json_true();

	}else if(STR_EQ_STATIC(type_s, "false")) {
		value = json_false();

	}else if(STR_EQ_STATIC(type_s, "null")) {
		value = json_null();

	} else {
		ERR("unrecognized input type\n");
		goto fail;
	}

	if(!value) {
		ERR("parsing failed for \"%s\"\n", value_s.s);
		ERR("value error at line %d: %s\n",
				parsing_error.line, parsing_error.text);
		goto fail;
	}

	char* path = path_s.s;

	result_json = json_loads(result, JSON_REJECT_DUPLICATES, &parsing_error);

	if(!result_json) {
		ERR("result has json error at line %d: %s\n",
				parsing_error.line, parsing_error.text);
		goto fail;
	}

	if(json_path_set(result_json, path, value, append)<0) {
		goto fail;
	}

	if(jansson_to_val(&result_val, &freeme, result_json)<0) goto fail;

	result_pv->setf(msg, &result_pv->pvp, (int)EQ_T, &result_val);

	if(freeme) free(freeme);
	json_decref(result_json);
	return 1;

fail:
	if(freeme) free(freeme);
	json_decref(result_json);
	return -1;
}

int janssonmod_array_size(struct sip_msg* msg, char* path_in, char* src_in, char* dst)
{
	str src_s;
	str path_s;
	pv_spec_t *dst_pv;
	pv_value_t dst_val;

	if (fixup_get_svalue(msg, (gparam_p)src_in, &src_s) != 0) {
		ERR("cannot get json string value\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_p)path_in, &path_s) != 0) {
		ERR("cannot get path string value\n");
		return -1;
	}

	dst_pv = (pv_spec_t *)dst;

	json_t* json = NULL;
	json_error_t parsing_error;

	json = json_loads(src_s.s, JSON_REJECT_DUPLICATES, &parsing_error);

	if(!json) {
		ERR("json error at line %d: %s\n",
				parsing_error.line, parsing_error.text);
		goto fail;
	}

	char* path = path_s.s;

	json_t* v = json_path_get(json, path);
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
	dst_val.flags = PV_TYPE_INT|PV_VAL_INT;

	dst_pv->setf(msg, &dst_pv->pvp, (int)EQ_T, &dst_val);

	json_decref(json);
	return 1;

fail:
	json_decref(json);
	return -1;
}
