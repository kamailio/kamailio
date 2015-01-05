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
#include "kz_trans.h"
#include "kz_json.h"
#include "kz_amqp.h"



/*! transformation buffer size */
#define KZ_TR_BUFFER_SIZE 65536
#define KZ_TR_BUFFER_SLOTS	8

/*! transformation buffer */
static char **_kz_tr_buffer_list = NULL;

static char *_kz_tr_buffer = NULL;

static int _kz_tr_buffer_idx = 0;

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
	return 0;
}

/*!
 *
 */
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

	if(val==NULL || (val->flags&PV_VAL_NULL))
		return -1;

	char* tofree = NULL;
	int oldflags = 0;

	kz_tr_set_crt_buffer();

	switch(subtype)
	{
		case TR_KAZOO_ENCODE:
			if(!(val->flags&PV_VAL_STR))
				return -1;

			oldflags = val->flags;
			tofree = val->rs.s;

			if( kz_amqp_encode_ex(&val->rs, val ) != 1) {
				LM_ERR("error encoding value\n");
				return -1;
			}

            /*
			// it seems that val memory is not freed
			// even with flag set to PV_VAL_PKG

			strncpy(_kz_tr_buffer, val->rs.s, val->rs.len);
			if(val->flags & PV_VAL_PKG)
				pkg_free(val->rs.s);
			else if(val->flags & PV_VAL_SHM)
				shm_free(val->rs.s);
			_kz_tr_buffer[val->rs.len] = '\0';
			val->flags = PV_VAL_STR;
			val->ri = 0;
			val->rs.s = _kz_tr_buffer;
            */

			if(oldflags & PV_VAL_PKG) {
				pkg_free(tofree);
			} else if(oldflags & PV_VAL_SHM) {
				shm_free(tofree);
			}


			break;
		case TR_KAZOO_JSON:
			if(tp==NULL)
			{
				LM_ERR("kazoo json transform invalid parameter\n");
				return -1;
			}

			oldflags = val->flags;
			tofree = val->rs.s;

			if(kz_json_get_field_ex(&val->rs, &tp->v.s, val ) != 1) {
				LM_ERR("error getting json\n");
				return -1;
			}
            
            /*
			// it seems that val memory is not freed
			// even with flag set to PV_VAL_PKG

			strncpy(_kz_tr_buffer, val->rs.s, val->rs.len);
			if(val->flags & PV_VAL_PKG)
				pkg_free(val->rs.s);
			else if(val->flags & PV_VAL_SHM)
				shm_free(val->rs.s);
			_kz_tr_buffer[val->rs.len] = '\0';
			val->flags = PV_VAL_STR;
			val->ri = 0;
			val->rs.s = _kz_tr_buffer;
            */

			if(oldflags & PV_VAL_PKG) {
				pkg_free(tofree);
			} else if(oldflags & PV_VAL_SHM) {
				shm_free(tofree);
			}

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
		_spec = (pv_spec_t*)pkg_malloc(sizeof(pv_spec_t)); \
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
		_tp = (tr_param_t*)pkg_malloc(sizeof(tr_param_t)); \
		if(_tp==NULL) \
		{ \
			LM_ERR("no more private memory!\n"); \
			goto error; \
		} \
		memset(_tp, 0, sizeof(tr_param_t)); \
		_tp->type = TR_PARAM_SPEC; \
		_tp->v.data = (void*)_spec; \
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
		_tp = (tr_param_t*)pkg_malloc(sizeof(tr_param_t)); \
		if(_tp==NULL) \
		{ \
			LM_ERR("no more private memory!\n"); \
			goto error; \
		} \
		memset(_tp, 0, sizeof(tr_param_t)); \
		_tp->type = TR_PARAM_STRING; \
		_tp->v.s.s = _ps; \
		_tp->v.s.len = _p - _ps; \
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
		tr_param_free(tp);
	if(spec)
		pv_spec_free(spec);
	return NULL;
done:
	t->name = name;
	return p;
}


