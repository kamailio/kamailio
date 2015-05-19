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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <string.h>

#include "../../mod_fix.h"
#include "../../lvalue.h"

#include "kz_json.h"

static str kz_pv_str_empty = {"", 0};

char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;
    int len = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
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

    result = pkg_malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            len = strlen(token);
            char* ptr = pkg_malloc( (len+1) * sizeof(char));
            *(result + idx) = ptr;
        	memcpy(ptr, token, len);
        	ptr[len] = '\0';
            token = strtok(0, delim);
            idx++;
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}


int kz_json_get_field_ex(str* json, str* field, pv_value_p dst_val)
{
  char** tokens;
  char* dup;
  char f1[25], f2[25];//, f3[25];
  int i;

  dup = pkg_malloc(json->len+1);
  memcpy(dup, json->s, json->len);
  dup[json->len] = '\0';
  struct json_object *j = json_tokener_parse(dup);
  pkg_free(dup);

  if (is_error(j)) {
	  LM_ERR("empty or invalid JSON\n");
	  return -1;
  }

  struct json_object *jtree = NULL;

  dup = pkg_malloc(field->len+1);
  memcpy(dup, field->s, field->len);
  dup[field->len] = '\0';
  tokens = str_split(dup, '.');
  pkg_free(dup);

    if (tokens)
    {
    	jtree = j;
        for (i = 0; *(tokens + i); i++)
        {
        	if(jtree != NULL) {
				str field = str_init(*(tokens + i));
				// check for idx []
				int sresult = sscanf(field.s, "%[^[][%[^]]]", f1, f2); //, f3);
				LM_DBG("CHECK IDX %d - %s , %s, %s\n", sresult, field.s, f1, (sresult > 1? f2 : "(null)"));

				jtree = kz_json_get_object(jtree, f1);
				if(jtree != NULL) {
					char *value = (char*)json_object_get_string(jtree);
					LM_DBG("JTREE OK %s\n", value);
				}
				if(jtree != NULL && sresult > 1 && json_object_is_type(jtree, json_type_array)) {
					int idx = atoi(f2);
					jtree = json_object_array_get_idx(jtree, idx);
					if(jtree != NULL) {
						char *value = (char*)json_object_get_string(jtree);
						LM_DBG("JTREE IDX OK %s\n", value);
					}
				}
        	}
            pkg_free(*(tokens + i));
        }
        pkg_free(tokens);
    }

	if(jtree != NULL) {
		char *value = (char*)json_object_get_string(jtree);
		int len = strlen(value);
		dst_val->rs.s = pkg_malloc(len+1);
		memcpy(dst_val->rs.s, value, len);
		dst_val->rs.s[len] = '\0';
		dst_val->rs.len = len;
		dst_val->flags = PV_VAL_STR | PV_VAL_PKG;
        dst_val->ri = 0;
	} else {
		dst_val->flags = PV_VAL_NULL;
        dst_val->rs = kz_pv_str_empty;
        dst_val->ri = 0;
	}

	json_object_put(j);

	return 1;
}


int kz_json_get_field(struct sip_msg* msg, char* json, char* field, char* dst)
{
  str json_s;
  str field_s;
  pv_spec_t *dst_pv;
  pv_value_t dst_val;

	if (fixup_get_svalue(msg, (gparam_p)json, &json_s) != 0) {
		LM_ERR("cannot get json string value\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_p)field, &field_s) != 0) {
		LM_ERR("cannot get field string value\n");
		return -1;
	}


	if(kz_json_get_field_ex(&json_s, &field_s, &dst_val) != 1)
		return -1;

	dst_pv = (pv_spec_t *)dst;
	dst_pv->setf(msg, &dst_pv->pvp, (int)EQ_T, &dst_val);
	if(dst_val.flags & PV_VAL_PKG)
		pkg_free(dst_val.rs.s);
	else if(dst_val.flags & PV_VAL_SHM)
		shm_free(dst_val.rs.s);

	return 1;
}

struct json_object* kz_json_parse(const char *str)
{
    struct json_tokener* tok;
    struct json_object* obj;

    tok = json_tokener_new();
    if (!tok) {
      LM_ERR("Error parsing json: cpuld not allocate tokener\n");
      return NULL;
    }

    obj = json_tokener_parse_ex(tok, str, -1);
    if(tok->err != json_tokener_success) {        
      LM_ERR("Error parsing json: %s\n", json_tokener_error_desc(tok->err));
      LM_ERR("%s\n", str);
      if (obj != NULL)
	   json_object_put(obj);
      obj = NULL;
    }

    json_tokener_free(tok);
    return obj;
}

struct json_object* kz_json_get_object(struct json_object* jso, const char *key)
{
	struct json_object *result = NULL;
	json_object_object_get_ex(jso, key, &result);
	return result;
}
