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

#include <stdio.h>
#include <string.h>
#include <json.h>

#include "../../core/mod_fix.h"
#include "../../core/lvalue.h"

#include "json_funcs.h"

int _json_get_field(
		struct sip_msg *msg, char *json, char *field, char *dst, int field_type)
{
	str json_s;
	str field_s;
	pv_spec_t *dst_pv;
	pv_value_t dst_val;
	char *value;
	struct json_object *j = NULL;
	struct json_object *oj = NULL;
	int ret;

	if(fixup_get_svalue(msg, (gparam_p)json, &json_s) != 0) {
		LM_ERR("cannot get json string value\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)field, &field_s) != 0) {
		LM_ERR("cannot get field string value\n");
		return -1;
	}

	dst_pv = (pv_spec_t *)dst;

	j = json_tokener_parse(json_s.s);

	if(j == NULL) {
		LM_ERR("empty or invalid JSON\n");
		return -1;
	}

	json_object_object_get_ex(j, field_s.s, &oj);
	if(oj != NULL) {
		if(field_type == JSON_FIELD_STRING) {
			value = (char *)json_object_get_string(oj);
		} else {
			value = (char *)json_object_to_json_string(oj);
		}
		dst_val.rs.s = value;
		dst_val.rs.len = strlen(value);
		dst_val.flags = PV_VAL_STR;
		dst_pv->setf(msg, &dst_pv->pvp, (int)EQ_T, &dst_val);
		ret = 1;
	} else {
		ret = -1;
	}

	json_object_put(j);
	return ret;
}

int json_get_field(struct sip_msg *msg, char *json, char *field, char *dst)
{
	return _json_get_field(msg, json, field, dst, JSON_FIELD_DEFAULT);
}


int json_get_string(struct sip_msg *msg, char *json, char *field, char *dst)
{
	return _json_get_field(msg, json, field, dst, JSON_FIELD_STRING);
}

#define json_foreach_key(obj, key)                                        \
	char *key;                                                            \
	struct lh_entry *entry##key;                                          \
	struct lh_entry *entry_next##key = NULL;                              \
	for(entry##key = json_object_get_object(obj)->head;                   \
			(entry##key ? (key = (char *)entry##key->k,                   \
						  entry_next##key = entry##key->next, entry##key) \
						: 0);                                             \
			entry##key = entry_next##key)


static str json_pv_str_empty = {"", 0};

char **str_split(char *a_str, const char a_delim)
{
	char **result = 0;
	size_t count = 0;
	char *tmp = a_str;
	char *last_comma = 0;
	char delim[2];
	delim[0] = a_delim;
	delim[1] = 0;
	int len = 0;

	/* Count how many elements will be extracted. */
	while(*tmp) {
		if(a_delim == *tmp) {
			count++;
			last_comma = tmp;
		}
		tmp++;
	}

	/* Add space for trailing token. */
	count += last_comma < (a_str + strlen(a_str) - 1);

	/* Add space for terminating null string so caller
	   knows where the list of returned strings ends. */
	count++;

	result = pkg_malloc(sizeof(char *) * count);

	if(result) {
		size_t idx = 0;
		char *token = strtok(a_str, delim);

		while(token) {
			assert(idx < count);
			len = strlen(token);
			char *ptr = pkg_malloc((len + 1) * sizeof(char));
			*(result + idx) = ptr;
			memcpy(ptr, token, len);
			ptr[len] = '\0';
			int i = 0;
			while(i < len) {
				if(ptr[i] == tr_json_escape_char)
					ptr[i] = '.';
				i++;
			}
			token = strtok(0, delim);
			idx++;
		}
		assert(idx == count - 1);
		*(result + idx) = 0;
	}

	return result;
}

struct json_object *tr_json_get_field_object(str *json, str *field)
{
	char **tokens;
	char *dup;
	char f1[25], f2[25]; //, f3[25];
	int i;

	dup = pkg_malloc(json->len + 1);
	memcpy(dup, json->s, json->len);
	dup[json->len] = '\0';
	struct json_object *j = json_tokener_parse(dup);
	pkg_free(dup);

	if(j == NULL) {
		LM_ERR("empty or invalid JSON\n");
		return NULL;
	}

	struct json_object *jtree = NULL;
	struct json_object *ret = NULL;

	LM_DBG("getting json %.*s\n", field->len, field->s);

	dup = pkg_malloc(field->len + 1);
	memcpy(dup, field->s, field->len);
	dup[field->len] = '\0';
	tokens = str_split(dup, '.');
	pkg_free(dup);

	if(tokens) {
		jtree = j;
		for(i = 0; *(tokens + i); i++) {
			if(jtree != NULL) {
				str field = str_init(*(tokens + i));
				// check for idx []
				int sresult = sscanf(field.s, "%[^[][%[^]]]", f1, f2); //, f3);
				LM_DBG("CHECK IDX %d - %s , %s, %s\n", sresult, field.s, f1,
						(sresult > 1 ? f2 : "(null)"));

				jtree = json_get_object(jtree, f1);
				if(jtree != NULL) {
					char *value = (char *)json_object_get_string(jtree);
					LM_DBG("JTREE OK %s\n", value);
				}
				if(jtree != NULL && sresult > 1
						&& json_object_is_type(jtree, json_type_array)) {
					int idx = atoi(f2);
					jtree = json_object_array_get_idx(jtree, idx);
					if(jtree != NULL) {
						char *value = (char *)json_object_get_string(jtree);
						LM_DBG("JTREE IDX OK %s\n", value);
					}
				}
			}
			pkg_free(*(tokens + i));
		}
		pkg_free(tokens);
	}


	if(jtree != NULL)
		ret = json_object_get(jtree);

	json_object_put(j);

	return ret;
}


int tr_json_get_field_ex(str *json, str *field, pv_value_p dst_val)
{
	struct json_object *jtree = tr_json_get_field_object(json, field);


	if(jtree != NULL) {
		char *value = (char *)json_object_get_string(jtree);
		int len = strlen(value);
		dst_val->rs.s = pkg_malloc(len + 1);
		memcpy(dst_val->rs.s, value, len);
		dst_val->rs.s[len] = '\0';
		dst_val->rs.len = len;
		dst_val->flags = PV_VAL_STR | PV_VAL_PKG;
		dst_val->ri = 0;
		json_object_put(jtree);
	} else {
		dst_val->flags = PV_VAL_NULL;
		dst_val->rs = json_pv_str_empty;
		dst_val->ri = 0;
	}
	return 1;
}


int tr_json_get_field(struct sip_msg *msg, char *json, char *field, char *dst)
{
	str json_s;
	str field_s;
	pv_spec_t *dst_pv;
	pv_value_t dst_val;

	if(fixup_get_svalue(msg, (gparam_p)json, &json_s) != 0) {
		LM_ERR("cannot get json string value\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)field, &field_s) != 0) {
		LM_ERR("cannot get field string value\n");
		return -1;
	}

	if(tr_json_get_field_ex(&json_s, &field_s, &dst_val) != 1)
		return -1;

	dst_pv = (pv_spec_t *)dst;
	dst_pv->setf(msg, &dst_pv->pvp, (int)EQ_T, &dst_val);
	if(dst_val.flags & PV_VAL_PKG) {
		pkg_free(dst_val.rs.s);
	} else if(dst_val.flags & PV_VAL_SHM) {
		shm_free(dst_val.rs.s);
	}

	return 1;
}

struct json_object *json_parse(const char *str)
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

struct json_object *json_get_object(struct json_object *jso, const char *key)
{
	struct json_object *result = NULL;
	json_object_object_get_ex(jso, key, &result);
	return result;
}

int tr_json_get_keys(struct sip_msg *msg, char *json, char *field, char *dst)
{
	str json_s;
	str field_s;
	int_str keys_avp_name;
	unsigned short keys_avp_type;
	pv_spec_t *avp_spec;

	if(fixup_get_svalue(msg, (gparam_p)json, &json_s) != 0) {
		LM_ERR("cannot get json string value\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)field, &field_s) != 0) {
		LM_ERR("cannot get field string value\n");
		return -1;
	}

	if(dst == NULL) {
		LM_ERR("avp spec is null\n");
		return -1;
	}

	avp_spec = (pv_spec_t *)dst;

	if(avp_spec->type != PVT_AVP) {
		LM_ERR("invalid avp spec\n");
		return -1;
	}

	if(pv_get_avp_name(0, &avp_spec->pvp, &keys_avp_name, &keys_avp_type)
			!= 0) {
		LM_ERR("invalid AVP definition\n");
		return -1;
	}

	struct json_object *jtree = tr_json_get_field_object(&json_s, &field_s);

	if(jtree != NULL) {
		json_foreach_key(jtree, k)
		{
			LM_DBG("ITERATING KEY %s\n", k);
			int_str v1;
			v1.s.s = k;
			v1.s.len = strlen(k);
			if(add_avp(AVP_VAL_STR | keys_avp_type, keys_avp_name, v1) < 0) {
				LM_ERR("failed to create AVP\n");
				json_object_put(jtree);
				return -1;
			}
		}
		json_object_put(jtree);
	}

	return 1;
}
