/**
 * $Id$
 *
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "../../mem/mem.h"

#include "t_lookup.h"
#include "t_var.h"

static struct cell *_pv_T_req = NULL;
static struct cell *_pv_T_rpl = NULL;
static struct sip_msg _pv_treq;
static struct sip_msg _pv_trpl;
static struct sip_msg *_pv_treq_p = NULL;
static struct sip_msg *_pv_trpl_p = NULL;
static unsigned int _pv_treq_id = 0;
static unsigned int _pv_trpl_id = 0;
static char *_pv_treq_buf = NULL;
static char *_pv_trpl_buf = NULL;
static unsigned int _pv_treq_size = 0;
static unsigned int _pv_trpl_size = 0;

int pv_t_copy_msg(struct sip_msg *src, struct sip_msg *dst)
{
	dst->id = src->id;
	dst->rcv = src->rcv;
	dst->set_global_address=src->set_global_address;
	dst->set_global_port=src->set_global_port;

	if (parse_msg(dst->buf, dst->len, dst)!=0)
	{
		LM_ERR("parse msg failed\n");
		return -1;
	}
	return 0;
}

int pv_t_update_req(struct sip_msg *msg)
{
	struct cell * t;
	int branch;

	if(msg==NULL)
		return 1;

	if(msg!=FAKED_REPLY && msg->first_line.type!=SIP_REPLY)
		return 1;

	t = get_t();

	if(t==NULL || t==T_UNDEFINED)
	{
		if(msg==FAKED_REPLY)
			return 1;
		branch=-1;
		if (t_check(msg, &branch ) == -1)
			return 1;
		t = get_t();
		if ((t == 0) || (t == T_UNDEFINED))
			return 1;

	}

	if(t->uas.request==NULL)
		return 1;

	if(_pv_T_req==t && t->uas.request==_pv_treq_p
			&& t->uas.request->id==_pv_treq_id)
		return 0;

	/* make a copy */
	if(_pv_treq_buf==NULL || _pv_treq_size<t->uas.request->len+1)
	{
		if(_pv_treq_buf!=NULL)
			pkg_free(_pv_treq_buf);
		if(_pv_treq_p)
			free_sip_msg(&_pv_treq);
		_pv_treq_p = NULL;
		_pv_treq_id = 0;
		_pv_T_req = NULL;
		_pv_treq_size = t->uas.request->len+1;
		_pv_treq_buf = (char*)pkg_malloc(_pv_treq_size*sizeof(char));
		if(_pv_treq_buf==NULL)
		{
			LM_ERR("no more pkg\n");
			_pv_treq_size = 0;
			return -1;
		}
	}
	memset(&_pv_treq, 0, sizeof(struct sip_msg));
	memcpy(_pv_treq_buf, t->uas.request->buf, t->uas.request->len);
	_pv_treq_buf[t->uas.request->len] = '\0';
	_pv_treq.len = t->uas.request->len;
	_pv_treq.buf = _pv_treq_buf;
	_pv_treq_p = t->uas.request;
	_pv_treq_id = t->uas.request->id;
	_pv_T_req = t;


	pv_t_copy_msg(t->uas.request, &_pv_treq);

	return 0;
}

int pv_t_update_rpl(struct sip_msg *msg)
{
	struct cell * t;
	int branch;

	if(msg==NULL)
		return 1;

	if(msg==FAKED_REPLY || msg->first_line.type!=SIP_REQUEST)
		return 1;

	t = get_t();

	if(t==NULL || t==T_UNDEFINED)
	{
		if(t_lookup_request(msg, 0)<=0)
			return 1;
		t = get_t();

		if(t==NULL || t==T_UNDEFINED)
			return 1;
	}
	if ( (branch=t_get_picked_branch())<0 )
		return 1;
	if(t->uac[branch].reply==NULL || t->uac[branch].reply==FAKED_REPLY)
		return 1;

	if(_pv_T_rpl==t && t->uac[branch].reply==_pv_trpl_p
			&& t->uac[branch].reply->id==_pv_trpl_id)
		return 0;

	/* make a copy */
	if(_pv_trpl_buf==NULL || _pv_trpl_size<t->uac[branch].reply->len+1)
	{
		if(_pv_trpl_buf!=NULL)
			pkg_free(_pv_trpl_buf);
		if(_pv_trpl_p)
			free_sip_msg(&_pv_trpl);
		_pv_trpl_p = NULL;
		_pv_trpl_id = 0;
		_pv_T_rpl = NULL;
		_pv_trpl_size = t->uac[branch].reply->len+1;
		_pv_trpl_buf = (char*)pkg_malloc(_pv_trpl_size*sizeof(char));
		if(_pv_trpl_buf==NULL)
		{
			LM_ERR("no more pkg\n");
			_pv_trpl_size = 0;
			return -1;
		}
	}
	memset(&_pv_trpl, 0, sizeof(struct sip_msg));
	memcpy(_pv_trpl_buf, t->uac[branch].reply->buf, t->uac[branch].reply->len);
	_pv_trpl_buf[t->uac[branch].reply->len] = '\0';
	_pv_trpl.len = t->uac[branch].reply->len;
	_pv_trpl.buf = _pv_trpl_buf;
	_pv_trpl_p = t->uac[branch].reply;
	_pv_trpl_id = t->uac[branch].reply->id;
	_pv_T_rpl = t;

	pv_t_copy_msg(t->uac[branch].reply, &_pv_trpl);

	return 0;
}

int pv_get_t_var_req(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	pv_spec_t *pv=NULL;

	if(pv_t_update_req(msg))
		return pv_get_null(msg, param, res);

	pv = (pv_spec_t*)param->pvn.u.dname;
	if(pv==NULL || pv_alter_context(pv))
		return pv_get_null(msg, param, res);

	return pv_get_spec_value(&_pv_treq, pv, res);
}

int pv_get_t_var_rpl(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	pv_spec_t *pv=NULL;

	if(pv_t_update_rpl(msg))
		return pv_get_null(msg, param, res);

	pv = (pv_spec_t*)param->pvn.u.dname;
	if(pv==NULL || pv_alter_context(pv))
		return pv_get_null(msg, param, res);

	return pv_get_spec_value(&_pv_trpl, pv, res);
}

int pv_parse_t_var_name(pv_spec_p sp, str *in)
{
	pv_spec_t *pv=NULL;

	if(in->s==NULL || in->len<=0)
		return -1;

	pv = (pv_spec_t*)pkg_malloc(sizeof(pv_spec_t));
	if(pv==NULL)
		return -1;

	memset(pv, 0, sizeof(pv_spec_t));

	if(pv_parse_spec(in, pv)==NULL)
		goto error;

	sp->pvp.pvn.u.dname = (void*)pv;
	sp->pvp.pvn.type = PV_NAME_PVAR;
	return 0;

error:
	LM_ERR("invalid pv name [%.*s]\n", in->len, in->s);
	if(pv!=NULL)
		pkg_free(pv);
	return -1;
}

