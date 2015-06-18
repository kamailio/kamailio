/*
 * $Id$
 *
 * Copyright (C) 2007 voice-system.ro
 * Copyright (C) 2009 asipto.com
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
 */

/*! \file
 * \brief Support for transformations
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../ut.h" 
#include "../../trim.h" 
#include "../../pvapi.h"
#include "../../dset.h"

#include "../../parser/parse_param.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_nameaddr.h"

#include "../../lib/kcore/strcommon.h"

#include "../../mod_fix.h"

#include "kz_trans.h"
#include "kz_json.h"
#include "kz_amqp.h"



/*! transformation buffer size */
#define KZ_TR_BUFFER_SIZE 65536
#define KZ_TR_BUFFER_SLOTS	4

/*! transformation buffer */
static char **_kz_tr_buffer_list = NULL;

static char *_kz_tr_buffer = NULL;

static int _kz_tr_buffer_idx = 0;

#define KZ_TR_ALLOC_PARSE_SIZE	2048

static pv_spec_t**  _kz_parse_specs  = NULL;
static tr_param_t** _kz_parse_params = NULL;
static int _kz_tr_parse_spec = 0;
static int _kz_tr_parse_params = 0;


/*!
 *
 */
int kz_tr_init_buffers(void)
{
	int i;

	_kz_tr_buffer_list = (char**)malloc(KZ_TR_BUFFER_SLOTS * sizeof(char*));

	if(_kz_tr_buffer_list==NULL)
		return -1;
	for(i=0; i<KZ_TR_BUFFER_SLOTS; i++) {
		_kz_tr_buffer_list[i] = (char*)malloc(KZ_TR_BUFFER_SIZE);
		if(_kz_tr_buffer_list[i]==NULL)
			return -1;
	}

	_kz_parse_specs = (pv_spec_t**)malloc(KZ_TR_ALLOC_PARSE_SIZE * sizeof(pv_spec_t*));
	for(i=0; i < KZ_TR_ALLOC_PARSE_SIZE; i++)
		_kz_parse_specs[i] = NULL;

	_kz_parse_params = (tr_param_t**)malloc(KZ_TR_ALLOC_PARSE_SIZE * sizeof(tr_param_t*));
	for(i=0; i < KZ_TR_ALLOC_PARSE_SIZE; i++)
		_kz_parse_params[i] = NULL;

	return 0;
}

void kz_tr_clear_buffers(void)
{
	int i;
	if(_kz_tr_buffer_list != NULL) {
		for(i=0; i<KZ_TR_BUFFER_SLOTS; i++) {
			if(_kz_tr_buffer_list[i] != NULL) {
				free(_kz_tr_buffer_list[i]);
				_kz_tr_buffer_list[i] = NULL;
			}
		}
		free(_kz_tr_buffer_list);
		_kz_tr_buffer_list = NULL;
	}

	if(_kz_parse_specs != NULL) {
		for(i=0; i<KZ_TR_ALLOC_PARSE_SIZE; i++) {
			if(_kz_parse_specs[i] != NULL) {
				free(_kz_parse_specs[i]);
				_kz_parse_specs[i] = NULL;
			}
		}
		free(_kz_parse_specs);
		_kz_parse_specs = NULL;
	}

	if(_kz_parse_params != NULL) {
		for(i=0; i<KZ_TR_ALLOC_PARSE_SIZE; i++) {
			if(_kz_parse_params[i] != NULL) {
				free(_kz_parse_params[i]);
				_kz_parse_params[i] = NULL;
			}
		}
		free(_kz_parse_params);
		_kz_parse_params = NULL;
	}

}

char *kz_tr_set_crt_buffer(void)
{
	_kz_tr_buffer = _kz_tr_buffer_list[_kz_tr_buffer_idx];
	_kz_tr_buffer_idx = (_kz_tr_buffer_idx + 1) % KZ_TR_BUFFER_SLOTS;
	return _kz_tr_buffer;
}

#define kz_tr_string_clone_result do { \
		if(val->rs.len> KZ_TR_BUFFER_SIZE-1) { \
			LM_ERR("result is too big\n"); \
			return -1; \
		} \
		strncpy(_kz_tr_buffer, val->rs.s, val->rs.len); \
		val->rs.s = _kz_tr_buffer; \
	} while(0);

void kz_destroy_pv_value(pv_value_t *val) {

	if(val->flags & PV_VAL_PKG)
		pkg_free(val->rs.s);
	else if(val->flags & PV_VAL_SHM)
		shm_free(val->rs.s);
	pkg_free(val);
}

void kz_free_pv_value(pv_value_t *val ) {
	if(val->flags & PV_VAL_PKG)
		pkg_free(val->rs.s);
	else if(val->flags & PV_VAL_SHM)
		shm_free(val->rs.s);
}

pv_value_t* kz_alloc_pv_value() {
	pv_value_t* v = (pv_value_t*) pkg_malloc(sizeof(pv_value_t));
	if(v != NULL)
		memset(v, 0, sizeof(pv_value_t));
	return v;
}


/*!
 * \brief Evaluate kazoo transformations
 * \param msg SIP message
 * \param tp transformation
 * \param subtype transformation type
 * \param val pseudo-variable
 * \return 0 on success, -1 on error
 */
int kz_tr_eval(struct sip_msg *msg, tr_param_t *tp, int subtype, pv_value_t *val)
{

	str sv;
	pv_value_t* pv;
	pv_value_t v;
    str v2 = {0,0};
	void* v1 = NULL;

	if(val==NULL || (val->flags&PV_VAL_NULL))
		return -1;


	kz_tr_set_crt_buffer();

	switch(subtype)
	{
		case TR_KAZOO_ENCODE:
			if(!(val->flags&PV_VAL_STR))
				return -1;

			pv = kz_alloc_pv_value();
			if(pv == NULL)
			{
				LM_ERR("kazoo encode transform : no more private memory\n");
				return -1;
			}

			if( kz_amqp_encode_ex(&val->rs, pv ) != 1) {
				LM_ERR("error encoding value\n");
				kz_destroy_pv_value(pv);
				return -1;
			}

			strncpy(_kz_tr_buffer, pv->rs.s, pv->rs.len);
			_kz_tr_buffer[pv->rs.len] = '\0';

			val->flags = PV_VAL_STR;
			val->ri = 0;
			val->rs.s = _kz_tr_buffer;
			val->rs.len = pv->rs.len;

			kz_destroy_pv_value(pv);
			kz_free_pv_value(val);

			break;
		case TR_KAZOO_JSON:
			if(!(val->flags&PV_VAL_STR))
				return -1;

			if(tp==NULL)
			{
				LM_ERR("kazoo json transform invalid parameter\n");
				return -1;
			}

			pv = kz_alloc_pv_value();
			if(pv == NULL)
			{
				LM_ERR("kazoo encode transform : no more private memory\n");
				return -1;
			}


			if(tp->type == TR_PARAM_STRING)
			{
				v1 = tp->v.s.s;
				if(fixup_spve_null(&v1, 1) != 0) {
					LM_ERR("cannot get spve_value from TR_PARAM_STRING : %.*s\n", tp->v.s.len, tp->v.s.s);
					return -1;
				}
				if (fixup_get_svalue(msg, (gparam_p)v1, &v2) != 0) {
					LM_ERR("cannot get value from TR_PARAM_STRING\n");
					fixup_free_spve_null(&v1, 1);
					return -1;
				}
				fixup_free_spve_null(&v1, 1);
				sv = v2;
			} else {
				if(pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v)!=0
						|| (!(v.flags&PV_VAL_STR)) || v.rs.len<=0)
				{
					LM_ERR("value cannot get spec value in json transform\n");
					kz_destroy_pv_value(pv);
					return -1;
				}
				sv = v.rs;
			}


			if(kz_json_get_field_ex(&val->rs, &sv, pv ) != 1) {
				LM_ERR("error getting json\n");
				kz_destroy_pv_value(pv);
				return -1;
			}
            
			strncpy(_kz_tr_buffer, pv->rs.s, pv->rs.len);
			_kz_tr_buffer[pv->rs.len] = '\0';

			val->flags = PV_VAL_STR;
			val->ri = 0;
			val->rs.s = _kz_tr_buffer;
			val->rs.len = pv->rs.len;

			kz_destroy_pv_value(pv);
			kz_free_pv_value(val);

			break;

		default:
			LM_ERR("unknown kazoo transformation subtype %d\n", subtype);
			return -1;
	}
	return 0;
}

#define _kz_tr_parse_sparam(_p, _p0, _tp, _spec, _ps, _in, _s) \
	while(is_in_str(_p, _in) && (*_p==' ' || *_p=='\t' || *_p=='\n')) _p++; \
	if(*_p==PV_MARKER) \
	{ /* pseudo-variable */ \
		_spec = (pv_spec_t*)malloc(sizeof(pv_spec_t)); \
		if(_spec==NULL) \
		{ \
			LM_ERR("no more private memory!\n"); \
			goto error; \
		} \
		_s.s = _p; _s.len = _in->s + _in->len - _p; \
		_p0 = pv_parse_spec(&_s, _spec); \
		if(_p0==NULL) \
		{ \
			LM_ERR("invalid spec in substr transformation: %.*s!\n", \
				_in->len, _in->s); \
			goto error; \
		} \
		_p = _p0; \
		_tp = (tr_param_t*)malloc(sizeof(tr_param_t)); \
		if(_tp==NULL) \
		{ \
			LM_ERR("no more private memory!\n"); \
			goto error; \
		} \
		memset(_tp, 0, sizeof(tr_param_t)); \
		_tp->type = TR_PARAM_SPEC; \
		_tp->v.data = (void*)_spec; \
		_kz_parse_specs[_kz_tr_parse_spec++] = _spec; \
		_kz_parse_params[_kz_tr_parse_params++] = _tp; \
	} else { /* string */ \
		_ps = _p; \
		while(is_in_str(_p, _in) && *_p!='\t' && *_p!='\n' \
				&& *_p!=TR_PARAM_MARKER && *_p!=TR_RBRACKET) \
				_p++; \
		if(*_p=='\0') \
		{ \
			LM_ERR("invalid param in transformation: %.*s!!\n", \
				_in->len, _in->s); \
			goto error; \
		} \
		_tp = (tr_param_t*)malloc(sizeof(tr_param_t)); \
		if(_tp==NULL) \
		{ \
			LM_ERR("no more private memory!\n"); \
			goto error; \
		} \
		memset(_tp, 0, sizeof(tr_param_t)); \
		_tp->type = TR_PARAM_STRING; \
		_tp->v.s.len = _p - _ps; \
		_tp->v.s.s = (char*)malloc((tp->v.s.len+1)*sizeof(char)); \
		strncpy(_tp->v.s.s, _ps, tp->v.s.len); \
		_tp->v.s.s[tp->v.s.len] = '\0'; \
		_kz_parse_params[_kz_tr_parse_params++] = _tp; \
	}


/*!
 * \brief Helper fuction to parse a kazoo transformation
 * \param in parsed string
 * \param t transformation
 * \return pointer to the end of the transformation in the string - '}', null on error
 */
char* kz_tr_parse(str* in, trans_t *t)
{
	char *p;
	char *p0;
	char *ps;
	str name;
	str s;
	pv_spec_t *spec = NULL;
	tr_param_t *tp = NULL;

	if(in==NULL || t==NULL)
		return NULL;

	p = in->s;
	name.s = in->s;
	t->type = TR_KAZOO;
	t->trf = kz_tr_eval;

	/* find next token */
	while(is_in_str(p, in) && *p!=TR_PARAM_MARKER && *p!=TR_RBRACKET) p++;
	if(*p=='\0')
	{
		LM_ERR("invalid transformation: %.*s\n",
				in->len, in->s);
		goto error;
	}
	name.len = p - name.s;
	trim(&name);

	if(name.len==6 && strncasecmp(name.s, "encode", 6)==0)
	{
		t->subtype = TR_KAZOO_ENCODE;
		goto done;
	} else if(name.len==4 && strncasecmp(name.s, "json", 4)==0) {
		t->subtype = TR_KAZOO_JSON;
		if(*p!=TR_PARAM_MARKER)
		{
			LM_ERR("invalid json transformation: %.*s!\n", in->len, in->s);
			goto error;
		}
		p++;
		_kz_tr_parse_sparam(p, p0, tp, spec, ps, in, s);
		t->params = tp;
		tp = 0;
		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_RBRACKET)
		{
			LM_ERR("invalid json transformation: %.*s!!\n",
				in->len, in->s);
			goto error;
		}
		goto done;
	}

	LM_ERR("unknown kazoo transformation: %.*s/%.*s/%d!\n", in->len, in->s,
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


