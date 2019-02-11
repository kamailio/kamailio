/*
 * JSON module interface
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
 * Emmanuel Schmidbauer <eschmidbauer@gmail.com>
 *
 */

#include "../../core/trim.h"
#include "../../core/mod_fix.h"

#include "json_trans.h"

/*! transformation buffer size */
#define JSON_TR_BUFFER_SIZE 65536
#define JSON_TR_BUFFER_SLOTS 4

/*! transformation buffer */
static char **_json_tr_buffer_list = NULL;

static char *_json_tr_buffer = NULL;

static int _json_tr_buffer_idx = 0;

#define JSON_TR_ALLOC_PARSE_SIZE 2048

static pv_spec_t **_json_parse_specs = NULL;
static tr_param_t **_json_parse_params = NULL;
static int _json_tr_parse_spec = 0;
static int _json_tr_parse_params = 0;


/*!
 *
 */
int json_tr_init_buffers(void)
{
	int i;

	_json_tr_buffer_list = (char **)malloc(JSON_TR_BUFFER_SLOTS * sizeof(char *));

	if(_json_tr_buffer_list == NULL)
		return -1;
	for(i = 0; i < JSON_TR_BUFFER_SLOTS; i++) {
		_json_tr_buffer_list[i] = (char *)malloc(JSON_TR_BUFFER_SIZE);
		if(_json_tr_buffer_list[i] == NULL)
			return -1;
	}

	_json_parse_specs =
			(pv_spec_t **)malloc(JSON_TR_ALLOC_PARSE_SIZE * sizeof(pv_spec_t *));
	for(i = 0; i < JSON_TR_ALLOC_PARSE_SIZE; i++)
		_json_parse_specs[i] = NULL;

	_json_parse_params = (tr_param_t **)malloc(
			JSON_TR_ALLOC_PARSE_SIZE * sizeof(tr_param_t *));
	for(i = 0; i < JSON_TR_ALLOC_PARSE_SIZE; i++)
		_json_parse_params[i] = NULL;

	return 0;
}

void json_tr_clear_buffers(void)
{
	int i;
	if(_json_tr_buffer_list != NULL) {
		for(i = 0; i < JSON_TR_BUFFER_SLOTS; i++) {
			if(_json_tr_buffer_list[i] != NULL) {
				free(_json_tr_buffer_list[i]);
				_json_tr_buffer_list[i] = NULL;
			}
		}
		free(_json_tr_buffer_list);
		_json_tr_buffer_list = NULL;
	}

	if(_json_parse_specs != NULL) {
		for(i = 0; i < JSON_TR_ALLOC_PARSE_SIZE; i++) {
			if(_json_parse_specs[i] != NULL) {
				free(_json_parse_specs[i]);
				_json_parse_specs[i] = NULL;
			}
		}
		free(_json_parse_specs);
		_json_parse_specs = NULL;
	}

	if(_json_parse_params != NULL) {
		for(i = 0; i < JSON_TR_ALLOC_PARSE_SIZE; i++) {
			if(_json_parse_params[i] != NULL) {
				free(_json_parse_params[i]);
				_json_parse_params[i] = NULL;
			}
		}
		free(_json_parse_params);
		_json_parse_params = NULL;
	}
}

char *json_tr_set_crt_buffer(void)
{
	_json_tr_buffer = _json_tr_buffer_list[_json_tr_buffer_idx];
	_json_tr_buffer_idx = (_json_tr_buffer_idx + 1) % JSON_TR_BUFFER_SLOTS;
	return _json_tr_buffer;
}

#define json_tr_string_clone_result                       \
	do {                                                 \
		if(val->rs.len > JSON_TR_BUFFER_SIZE - 1) {       \
			LM_ERR("result is too big\n");               \
			return -1;                                   \
		}                                                \
		strncpy(_json_tr_buffer, val->rs.s, val->rs.len); \
		val->rs.s = _json_tr_buffer;                      \
	} while(0);

void json_destroy_pv_value(pv_value_t *val)
{
	if(val->flags & PV_VAL_PKG)
		pkg_free(val->rs.s);
	else if(val->flags & PV_VAL_SHM)
		shm_free(val->rs.s);
	pkg_free(val);
}

void json_free_pv_value(pv_value_t *val)
{
	if(val->flags & PV_VAL_PKG)
		pkg_free(val->rs.s);
	else if(val->flags & PV_VAL_SHM)
		shm_free(val->rs.s);
}

pv_value_t *json_alloc_pv_value()
{
	pv_value_t *v = (pv_value_t *)pkg_malloc(sizeof(pv_value_t));
	if(v != NULL)
		memset(v, 0, sizeof(pv_value_t));
	return v;
}

#define KEY_SAFE(C)                                   \
	((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') \
			|| (C >= '0' && C <= '9') || (C == '-' || C == '~' || C == '_'))

#define HI4(C) (C >> 4)
#define LO4(C) (C & 0x0F)

#define hexint(C) (C < 10 ? ('0' + C) : ('A' + C - 10))

char *json_util_encode(const str *key, char *dest)
{
	if((key->len == 1) && (key->s[0] == '#' || key->s[0] == '*')) {
		*dest++ = key->s[0];
		return dest;
	}
	char *p, *end;
	for(p = key->s, end = key->s + key->len; p < end; p++) {
		if(KEY_SAFE(*p)) {
			*dest++ = *p;
		} else if(*p == '.') {
			memcpy(dest, "\%2E", 3);
			dest += 3;
		} else if(*p == ' ') {
			*dest++ = '+';
		} else {
			*dest++ = '%';
			sprintf(dest, "%c%c", hexint(HI4(*p)), hexint(LO4(*p)));
			dest += 2;
		}
	}
	*dest = '\0';
	return dest;
}

int json_encode_ex(str *unencoded, pv_value_p dst_val)
{
	char routing_key_buff[256];
	memset(routing_key_buff, 0, sizeof(routing_key_buff));
	json_util_encode(unencoded, routing_key_buff);

	int len = strlen(routing_key_buff);
	dst_val->rs.s = pkg_malloc(len + 1);
	memcpy(dst_val->rs.s, routing_key_buff, len);
	dst_val->rs.s[len] = '\0';
	dst_val->rs.len = len;
	dst_val->flags = PV_VAL_STR | PV_VAL_PKG;

	return 1;
}

/*!
 * \brief Evaluate JSON transformations
 * \param msg SIP message
 * \param tp transformation
 * \param subtype transformation type
 * \param val pseudo-variable
 * \return 0 on success, -1 on error
 */
int json_tr_eval(
		struct sip_msg *msg, tr_param_t *tp, int subtype, pv_value_t *val)
{

	str sv;
	pv_value_t *pv;
	pv_value_t v;
	str v2 = {0, 0};
	void *v1 = NULL;

	if(val == NULL || (val->flags & PV_VAL_NULL))
		return -1;


	json_tr_set_crt_buffer();

	switch(subtype) {
		case TR_JSON_ENCODE:
			if(!(val->flags & PV_VAL_STR))
				return -1;

			pv = json_alloc_pv_value();
			if(pv == NULL) {
				LM_ERR("JSON encode transform : no more private memory\n");
				return -1;
			}

			if(json_encode_ex(&val->rs, pv) != 1) {
				LM_ERR("error encoding value\n");
				json_destroy_pv_value(pv);
				return -1;
			}

			strncpy(_json_tr_buffer, pv->rs.s, pv->rs.len);
			_json_tr_buffer[pv->rs.len] = '\0';

			val->flags = PV_VAL_STR;
			val->ri = 0;
			val->rs.s = _json_tr_buffer;
			val->rs.len = pv->rs.len;

			json_destroy_pv_value(pv);
			json_free_pv_value(val);

			break;
		case TR_JSON_PARSE:
			if(!(val->flags & PV_VAL_STR))
				return -1;

			if(tp == NULL) {
				LM_ERR("JSON json transform invalid parameter\n");
				return -1;
			}

			pv = json_alloc_pv_value();
			if(pv == NULL) {
				LM_ERR("JSON encode transform : no more private memory\n");
				return -1;
			}


			if(tp->type == TR_PARAM_STRING) {
				v1 = tp->v.s.s;
				if(fixup_spve_null(&v1, 1) != 0) {
					LM_ERR("cannot get spve_value from TR_PARAM_STRING : "
						   "%.*s\n",
							tp->v.s.len, tp->v.s.s);
					pkg_free(pv);
					return -1;
				}
				if(fixup_get_svalue(msg, (gparam_p)v1, &v2) != 0) {
					LM_ERR("cannot get value from TR_PARAM_STRING\n");
					fixup_free_spve_null(&v1, 1);
					pkg_free(pv);
					return -1;
				}
				fixup_free_spve_null(&v1, 1);
				sv = v2;
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v) != 0
						|| (!(v.flags & PV_VAL_STR)) || v.rs.len <= 0) {
					LM_ERR("value cannot get spec value in json transform\n");
					json_destroy_pv_value(pv);
					return -1;
				}
				sv = v.rs;
			}


			if(tr_json_get_field_ex(&val->rs, &sv, pv) != 1) {
				LM_ERR("error getting json\n");
				json_destroy_pv_value(pv);
				return -1;
			}

			strncpy(_json_tr_buffer, pv->rs.s, pv->rs.len);
			_json_tr_buffer[pv->rs.len] = '\0';

			val->flags = PV_VAL_STR;
			val->ri = 0;
			val->rs.s = _json_tr_buffer;
			val->rs.len = pv->rs.len;

			json_destroy_pv_value(pv);
			json_free_pv_value(val);

			break;

		default:
			LM_ERR("unknown JSON transformation subtype %d\n", subtype);
			return -1;
	}
	return 0;
}

#define _json_tr_parse_sparam(_p, _p0, _tp, _spec, _ps, _in, _s)                \
	while(is_in_str(_p, _in) && (*_p == ' ' || *_p == '\t' || *_p == '\n'))    \
		_p++;                                                                  \
	if(*_p == PV_MARKER) { /* pseudo-variable */                               \
		_spec = (pv_spec_t *)malloc(sizeof(pv_spec_t));                        \
		if(_spec == NULL) {                                                    \
			LM_ERR("no more private memory!\n");                               \
			goto error;                                                        \
		}                                                                      \
		_s.s = _p;                                                             \
		_s.len = _in->s + _in->len - _p;                                       \
		_p0 = pv_parse_spec(&_s, _spec);                                       \
		if(_p0 == NULL) {                                                      \
			LM_ERR("invalid spec in substr transformation: %.*s!\n", _in->len, \
					_in->s);                                                   \
			goto error;                                                        \
		}                                                                      \
		_p = _p0;                                                              \
		_tp = (tr_param_t *)malloc(sizeof(tr_param_t));                        \
		if(_tp == NULL) {                                                      \
			LM_ERR("no more private memory!\n");                               \
			goto error;                                                        \
		}                                                                      \
		memset(_tp, 0, sizeof(tr_param_t));                                    \
		_tp->type = TR_PARAM_SPEC;                                             \
		_tp->v.data = (void *)_spec;                                           \
		_json_parse_specs[_json_tr_parse_spec++] = _spec;                        \
		_json_parse_params[_json_tr_parse_params++] = _tp;                       \
	} else { /* string */                                                      \
		_ps = _p;                                                              \
		while(is_in_str(_p, _in) && *_p != '\t' && *_p != '\n'                 \
				&& *_p != TR_PARAM_MARKER && *_p != TR_RBRACKET)               \
			_p++;                                                              \
		if(*_p == '\0') {                                                      \
			LM_ERR("invalid param in transformation: %.*s!!\n", _in->len,      \
					_in->s);                                                   \
			goto error;                                                        \
		}                                                                      \
		_tp = (tr_param_t *)malloc(sizeof(tr_param_t));                        \
		if(_tp == NULL) {                                                      \
			LM_ERR("no more private memory!\n");                               \
			goto error;                                                        \
		}                                                                      \
		memset(_tp, 0, sizeof(tr_param_t));                                    \
		_tp->type = TR_PARAM_STRING;                                           \
		_tp->v.s.len = _p - _ps;                                               \
		_tp->v.s.s = (char *)malloc((tp->v.s.len + 1) * sizeof(char));         \
		strncpy(_tp->v.s.s, _ps, tp->v.s.len);                                 \
		_tp->v.s.s[tp->v.s.len] = '\0';                                        \
		_json_parse_params[_json_tr_parse_params++] = _tp;                       \
	}


/*!
 * \brief Helper fuction to parse a JSON transformation
 * \param in parsed string
 * \param t transformation
 * \return pointer to the end of the transformation in the string - '}', null on error
 */
char *json_tr_parse(str *in, trans_t *t)
{
	char *p;
	char *p0;
	char *ps;
	str name;
	str s;
	pv_spec_t *spec = NULL;
	tr_param_t *tp = NULL;

	if(in == NULL || t == NULL)
		return NULL;

	p = in->s;
	name.s = in->s;
	t->type = TR_JSON;
	t->trf = json_tr_eval;

	/* find next token */
	while(is_in_str(p, in) && *p != TR_PARAM_MARKER && *p != TR_RBRACKET)
		p++;
	if(*p == '\0') {
		LM_ERR("invalid transformation: %.*s\n", in->len, in->s);
		goto error;
	}
	name.len = p - name.s;
	trim(&name);

	if(name.len == 5 && strncasecmp(name.s, "encode", 6) == 0) {
		t->subtype = TR_JSON_ENCODE;
		goto done;
	} else if(name.len == 5 && strncasecmp(name.s, "parse", 5) == 0) {
		t->subtype = TR_JSON_PARSE;
		if(*p != TR_PARAM_MARKER) {
			LM_ERR("invalid json transformation: %.*s!\n", in->len, in->s);
			goto error;
		}
		p++;
		_json_tr_parse_sparam(p, p0, tp, spec, ps, in, s);
		t->params = tp;
		tp = 0;
		while(*p && (*p == ' ' || *p == '\t' || *p == '\n'))
			p++;
		if(*p != TR_RBRACKET) {
			LM_ERR("invalid json transformation: %.*s!!\n", in->len, in->s);
			goto error;
		}
		goto done;
	}

	LM_ERR("unknown JSON transformation: %.*s/%.*s/%d!\n", in->len, in->s,
			name.len, name.s, name.len);
error:
	if(tp)
		free(tp);
	if(spec)
		free(spec);
	return NULL;
done:
	t->name = name;
	return p;
}
