/**
 * $Id$
 *
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "../../dprint.h"

#include "../../modules/tm/tm_load.h"

#include "uac_send.h"

#define MAX_UACH_SIZE 2048
#define MAX_UACB_SIZE 4086

/** TM bind */
struct tm_binds tmb;

struct _uac_send_info {
	unsigned int flags;
	char  b_method[32];
	str   s_method;
	char  b_ruri[MAX_URI_SIZE];
	str   s_ruri;
	char  b_turi[MAX_URI_SIZE];
	str   s_turi;
	char  b_furi[MAX_URI_SIZE];
	str   s_furi;
	char  b_hdrs[MAX_UACH_SIZE];
	str   s_hdrs;
	char  b_body[MAX_UACB_SIZE];
	str   s_body;
	char  b_ouri[MAX_URI_SIZE];
	str   s_ouri;
	unsigned int onreply;
};

static struct _uac_send_info _uac_req;

int pv_get_uac_req(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(param==NULL || tmb.t_request==NULL)
		return -1;

	switch(param->pvn.u.isname.name.n)
	{
		case 0:
			return pv_get_uintval(msg, param, res, _uac_req.flags);
		case 1:
			if(_uac_req.s_ruri.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_uac_req.s_ruri);
		case 2:
			if(_uac_req.s_turi.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_uac_req.s_turi);
		case 3:
			if(_uac_req.s_furi.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_uac_req.s_furi);
		case 4:
			if(_uac_req.s_hdrs.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_uac_req.s_hdrs);
		case 5:
			if(_uac_req.s_body.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_uac_req.s_body);
		case 6:
			if(_uac_req.s_ouri.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_uac_req.s_ouri);
		case 7:
			if(_uac_req.s_method.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_uac_req.s_method);
		default:
			return pv_get_uintval(msg, param, res, _uac_req.flags);
	}
	return 0;
}

int pv_set_uac_req(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	if(param==NULL || tmb.t_request==NULL)
		return -1;

	switch(param->pvn.u.isname.name.n)
	{
		case 0:
			if(val==NULL)
			{
				_uac_req.flags = 0;
				_uac_req.s_ruri.len = 0;
				_uac_req.s_furi.len = 0;
				_uac_req.s_turi.len = 0;
				_uac_req.s_ouri.len = 0;
				_uac_req.s_hdrs.len = 0;
				_uac_req.s_body.len = 0;
				_uac_req.s_method.len = 0;
				_uac_req.onreply = 0;
			}
			break;
		case 1:
			if(val==NULL)
			{
				_uac_req.s_ruri.len = 0;
				return 0;
			}
			if(!(val->flags&PV_VAL_STR))
			{
				LM_ERR("Invalid value type\n");
				return -1;
			}
			if(val->rs.len>=MAX_URI_SIZE)
			{
				LM_ERR("Value size too big\n");
				return -1;
			}
			memcpy(_uac_req.s_ruri.s, val->rs.s, val->rs.len);
			_uac_req.s_ruri.s[val->rs.len] = '\0';
			_uac_req.s_ruri.len = val->rs.len;
			break;
		case 2:
			if(val==NULL)
			{
				_uac_req.s_turi.len = 0;
				return 0;
			}
			if(!(val->flags&PV_VAL_STR))
			{
				LM_ERR("Invalid value type\n");
				return -1;
			}
			if(val->rs.len>=MAX_URI_SIZE)
			{
				LM_ERR("Value size too big\n");
				return -1;
			}
			memcpy(_uac_req.s_turi.s, val->rs.s, val->rs.len);
			_uac_req.s_turi.s[val->rs.len] = '\0';
			_uac_req.s_turi.len = val->rs.len;
			break;
		case 3:
			if(val==NULL)
			{
				_uac_req.s_furi.len = 0;
				return 0;
			}
			if(!(val->flags&PV_VAL_STR))
			{
				LM_ERR("Invalid value type\n");
				return -1;
			}
			if(val->rs.len>=MAX_URI_SIZE)
			{
				LM_ERR("Value size too big\n");
				return -1;
			}
			memcpy(_uac_req.s_furi.s, val->rs.s, val->rs.len);
			_uac_req.s_furi.s[val->rs.len] = '\0';
			_uac_req.s_furi.len = val->rs.len;
			break;
		case 4:
			if(val==NULL)
			{
				_uac_req.s_hdrs.len = 0;
				return 0;
			}
						if(!(val->flags&PV_VAL_STR))
			{
				LM_ERR("Invalid value type\n");
				return -1;
			}
			if(val->rs.len>=MAX_UACH_SIZE)
			{
				LM_ERR("Value size too big\n");
				return -1;
			}
			memcpy(_uac_req.s_hdrs.s, val->rs.s, val->rs.len);
			_uac_req.s_hdrs.s[val->rs.len] = '\0';
			_uac_req.s_hdrs.len = val->rs.len;
			break;
		case 5:
			if(val==NULL)
			{
				_uac_req.s_body.len = 0;
				return 0;
			}
			if(!(val->flags&PV_VAL_STR))
			{
				LM_ERR("Invalid value type\n");
				return -1;
			}
			if(val->rs.len>=MAX_UACB_SIZE)
			{
				LM_ERR("Value size too big\n");
				return -1;
			}
			memcpy(_uac_req.s_body.s, val->rs.s, val->rs.len);
			_uac_req.s_body.s[val->rs.len] = '\0';
			_uac_req.s_body.len = val->rs.len;
			break;
		case 6:
			if(val==NULL)
			{
				_uac_req.s_ouri.len = 0;
				return 0;
			}
			if(!(val->flags&PV_VAL_STR))
			{
				LM_ERR("Invalid value type\n");
				return -1;
			}
			if(val->rs.len>=MAX_URI_SIZE)
			{
				LM_ERR("Value size too big\n");
				return -1;
			}
			memcpy(_uac_req.s_ouri.s, val->rs.s, val->rs.len);
			_uac_req.s_ouri.s[val->rs.len] = '\0';
			_uac_req.s_ouri.len = val->rs.len;
			break;
		case 7:
			if(val==NULL)
			{
				_uac_req.s_method.len = 0;
				return 0;
			}
			if(!(val->flags&PV_VAL_STR))
			{
				LM_ERR("Invalid value type\n");
				return -1;
			}
			if(val->rs.len>=32)
			{
				LM_ERR("Value size too big\n");
				return -1;
			}
			memcpy(_uac_req.s_method.s, val->rs.s, val->rs.len);
			_uac_req.s_method.s[val->rs.len] = '\0';
			_uac_req.s_method.len = val->rs.len;
			break;
		case 8:
			if(val==NULL)
			{
				_uac_req.onreply = 0;
				return 0;
			}
			if(!(val->flags&PV_VAL_INT))
			{
				LM_ERR("Invalid value type\n");
				return -1;
			}
			if(val->ri>=ONREPLY_RT_NO)
			{
				LM_ERR("Value too big\n");
				return -1;
			}
			_uac_req.onreply = val->ri;
			break;
	}
	return 0;
}

int pv_parse_uac_req_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 3: 
			if(strncmp(in->s, "all", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else goto error;
		break;
		case 4: 
			if(strncmp(in->s, "ruri", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else if(strncmp(in->s, "turi", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else if(strncmp(in->s, "furi", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 3;
			else if(strncmp(in->s, "hdrs", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 4;
			else if(strncmp(in->s, "body", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 5;
			else if(strncmp(in->s, "ouri", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 6;
			else goto error;
		break;
		case 6: 
			if(strncmp(in->s, "method", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 7;
			else goto error;
		break;
		case 7: 
			if(strncmp(in->s, "onreply", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 8;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown uac_req name %.*s\n", in->len, in->s);
	return -1;
}

void uac_req_init(void)
{
	/* load the TM API */
	if (load_tm_api(&tmb)!=0) {
		LM_DBG("can't load TM API - disable it\n");
		memset(&tmb, 0, sizeof(struct tm_binds));
		return;
	}
	memset(&_uac_req, 0, sizeof(struct _uac_send_info));
	_uac_req.s_ruri.s = _uac_req.b_ruri;
	_uac_req.s_furi.s = _uac_req.b_furi;
	_uac_req.s_turi.s = _uac_req.b_turi;
	_uac_req.s_ouri.s = _uac_req.b_ouri;
	_uac_req.s_hdrs.s = _uac_req.b_hdrs;
	_uac_req.s_body.s = _uac_req.b_body;
	_uac_req.s_method.s = _uac_req.b_method;
	return;
}

/** 
 * TM callback function
 */
void uac_send_tm_callback( struct cell *t, int type, struct tmcb_params *ps)
{
	unsigned int onreply;
	if(ps->param==NULL || *ps->param==0)
	{
		LM_DBG("message id not received\n");
		goto done;
	}
	onreply = *((unsigned int*)ps->param);
	LM_DBG("completed with status %d [onreply: %u]\n",
		ps->code, onreply);

done:
	return;
}


int uac_req_send(struct sip_msg *msg, char *s1, char *s2)
{
	int ret;
	uac_req_t uac_r;

	if(_uac_req.s_ruri.len<=0 || _uac_req.s_method.len == 0
			|| tmb.t_request==NULL)
		return -1;

	memset(&uac_r, '\0', sizeof(uac_r));
	uac_r.method = &_uac_req.s_method;
	uac_r.headers = (_uac_req.s_hdrs.len <= 0) ? NULL : &_uac_req.s_hdrs;
	uac_r.body = (_uac_req.s_body.len <= 0) ? NULL : &_uac_req.s_body;
	if(_uac_req.onreply > 0)
	{
		uac_r.cb_flags = TMCB_LOCAL_COMPLETED;
		/* Callback function */
		uac_r.cb  = uac_send_tm_callback;
		/* Callback parameter */
		uac_r.cbp = (void*)(long)_uac_req.onreply;
	}
	ret = tmb.t_request(&uac_r,  /* UAC Req */
						&_uac_req.s_ruri,        /* Request-URI */
						(_uac_req.s_turi.len<=0)?&_uac_req.s_ruri:&_uac_req.s_turi, /* To */
						(_uac_req.s_furi.len<=0)?&_uac_req.s_ruri:&_uac_req.s_furi, /* From */
						(_uac_req.s_ouri.len<=0)?NULL:&_uac_req.s_ouri /* outbound uri */
		);

	if(ret<0)
		return -1;
	return 1;
}

