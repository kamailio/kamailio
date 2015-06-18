/**
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "../../dprint.h"
#include "../../trim.h"
#include "../../route.h"

#include "../../modules/tm/tm_load.h"

#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include "../../parser/contact/parse_contact.h"
#include "../../lib/kcore/faked_msg.h"

#include "auth.h"
#include "auth_hdr.h"
#include "uac_send.h"

#define MAX_UACH_SIZE 2048
#define MAX_UACB_SIZE 4086
#define MAX_UACD_SIZE 128

/** TM bind */
struct tm_binds tmb;

typedef struct _uac_send_info {
	unsigned int flags;
	char  b_method[32];
	str   s_method;
	char  b_ruri[MAX_URI_SIZE];
	str   s_ruri;
	char  b_turi[MAX_URI_SIZE];
	str   s_turi;
	char  b_furi[MAX_URI_SIZE];
	str   s_furi;
	char  b_callid[128];
	str   s_callid;
	char  b_hdrs[MAX_UACH_SIZE];
	str   s_hdrs;
	char  b_body[MAX_UACB_SIZE];
	str   s_body;
	char  b_ouri[MAX_URI_SIZE];
	str   s_ouri;
	char  b_sock[MAX_URI_SIZE];
	str   s_sock;
	char  b_auser[128];
	str   s_auser;
	char  b_apasswd[64];
	str   s_apasswd;
	char  b_evparam[MAX_UACD_SIZE];
	str   s_evparam;
	unsigned int evroute;
	unsigned int evcode;
	unsigned int evtype;
} uac_send_info_t;

static struct _uac_send_info _uac_req;

void uac_send_info_copy(uac_send_info_t *src, uac_send_info_t *dst)
{
	memcpy(dst, src, sizeof(uac_send_info_t));
	dst->s_method.s  = dst->b_method;
	dst->s_ruri.s    = dst->b_ruri;
	dst->s_turi.s    = dst->b_turi;
	dst->s_furi.s    = dst->b_furi;
	dst->s_hdrs.s    = dst->b_hdrs;
	dst->s_body.s    = dst->b_body;
	dst->s_ouri.s    = dst->b_ouri;
	dst->s_auser.s   = dst->b_auser;
	dst->s_apasswd.s = dst->b_apasswd;
	dst->s_callid.s  = dst->b_callid;
	dst->s_sock.s    = dst->b_sock;
	dst->s_evparam.s = dst->b_evparam;
}

uac_send_info_t *uac_send_info_clone(uac_send_info_t *ur)
{
	uac_send_info_t *tp = NULL;
	tp = (uac_send_info_t*)shm_malloc(sizeof(uac_send_info_t));
	if(tp==NULL)
	{
		LM_ERR("no more shm memory\n");
		return NULL;
	}
	uac_send_info_copy(ur, tp);

	return tp;
}

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
		case 8:
			return pv_get_uintval(msg, param, res, _uac_req.evroute);
		case 9:
			if(_uac_req.s_auser.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_uac_req.s_auser);
		case 10:
			if(_uac_req.s_apasswd.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_uac_req.s_apasswd);
		case 11:
			if(_uac_req.s_callid.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_uac_req.s_callid);
		case 12:
			if(_uac_req.s_sock.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_uac_req.s_sock);
		case 14:
			if(_uac_req.s_evparam.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_uac_req.s_evparam);
		case 15:
			return pv_get_uintval(msg, param, res, _uac_req.evcode);
		case 16:
			return pv_get_uintval(msg, param, res, _uac_req.evtype);
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
				_uac_req.s_callid.len = 0;
				_uac_req.evroute = 0;
				_uac_req.evtype = 0;
				_uac_req.evcode = 0;
				_uac_req.s_evparam.len = 0;
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
				_uac_req.evroute = 0;
				return 0;
			}
			if(!(val->flags&PV_VAL_INT))
			{
				LM_ERR("Invalid value type\n");
				return -1;
			}
			_uac_req.evroute = val->ri;
			break;
		case 9:
			if(val==NULL)
			{
				_uac_req.s_auser.len = 0;
				return 0;
			}
			if(!(val->flags&PV_VAL_STR))
			{
				LM_ERR("Invalid auth user type\n");
				return -1;
			}
			if(val->rs.len>=128)
			{
				LM_ERR("Value size too big\n");
				return -1;
			}
			memcpy(_uac_req.s_auser.s, val->rs.s, val->rs.len);
			_uac_req.s_auser.s[val->rs.len] = '\0';
			_uac_req.s_auser.len = val->rs.len;
			break;
		case 10:
			if(val==NULL)
			{
				_uac_req.s_apasswd.len = 0;
				return 0;
			}
			if(!(val->flags&PV_VAL_STR))
			{
				LM_ERR("Invalid auth password type\n");
				return -1;
			}
			if(val->rs.len>=64)
			{
				LM_ERR("Value size too big\n");
				return -1;
			}
			memcpy(_uac_req.s_apasswd.s, val->rs.s, val->rs.len);
			_uac_req.s_apasswd.s[val->rs.len] = '\0';
			_uac_req.s_apasswd.len = val->rs.len;
			break;
		case 11:
			if(val==NULL)
			{
				_uac_req.s_callid.len = 0;
				return 0;
			}
			if(!(val->flags&PV_VAL_STR))
			{
				LM_ERR("Invalid value type\n");
				return -1;
			}
			memcpy(_uac_req.s_callid.s, val->rs.s, val->rs.len);
			_uac_req.s_callid.s[val->rs.len] = '\0';
			_uac_req.s_callid.len = val->rs.len;
			break;
		case 12:
			if(val==NULL)
			{
				_uac_req.s_apasswd.len = 0;
				return 0;
			}
			if(!(val->flags&PV_VAL_STR))
			{
				LM_ERR("Invalid socket pv type\n");
				return -1;
			}
			if(val->rs.len>=MAX_URI_SIZE)
			{
				LM_ERR("Value size too big\n");
				return -1;
			}
			memcpy(_uac_req.s_sock.s, val->rs.s, val->rs.len);
			_uac_req.s_sock.s[val->rs.len] = '\0';
			_uac_req.s_sock.len = val->rs.len;
			break;
		case 14:
			if(val==NULL)
			{
				_uac_req.s_evparam.len = 0;
				return 0;
			}
			if(!(val->flags&PV_VAL_STR))
			{
				LM_ERR("Invalid value type\n");
				return -1;
			}
			if(val->rs.len>=MAX_UACD_SIZE)
			{
				LM_ERR("Value size too big\n");
				return -1;
			}
			memcpy(_uac_req.s_evparam.s, val->rs.s, val->rs.len);
			_uac_req.s_evparam.s[val->rs.len] = '\0';
			_uac_req.s_evparam.len = val->rs.len;
			break;
		case 15:
			if(val==NULL)
			{
				_uac_req.evcode = 0;
				return 0;
			}
			if(!(val->flags&PV_VAL_INT))
			{
				LM_ERR("Invalid value type\n");
				return -1;
			}
			_uac_req.evcode = val->ri;
			break;
		case 16:
			if(val==NULL)
			{
				_uac_req.evtype = 0;
				return 0;
			}
			if(!(val->flags&PV_VAL_INT))
			{
				LM_ERR("Invalid value type\n");
				return -1;
			}
			_uac_req.evtype = val->ri;
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
			else if(strncmp(in->s, "sock", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 12;
			else goto error;
		break;
		case 5:
			if(strncmp(in->s, "auser", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 9;
			else goto error;
		break;
		case 6: 
			if(strncmp(in->s, "method", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 7;
			else if(strncmp(in->s, "callid", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 11;
			else if(strncmp(in->s, "evcode", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 15;
			else if(strncmp(in->s, "evtype", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 16;
			else goto error;
		break;
		case 7: 
			if(strncmp(in->s, "evroute", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 8;
			else if(strncmp(in->s, "apasswd", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 10;
			else if(strncmp(in->s, "evparam", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 14;
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
	_uac_req.s_auser.s  = _uac_req.b_auser;
	_uac_req.s_apasswd.s  = _uac_req.b_apasswd;
	_uac_req.s_callid.s   = _uac_req.b_callid;
	_uac_req.s_sock.s     = _uac_req.b_sock;
	_uac_req.s_evparam.s  = _uac_req.b_evparam;
	return;
}

int uac_send_tmdlg(dlg_t *tmdlg, sip_msg_t *rpl)
{
	if(tmdlg==NULL || rpl==NULL)
		return -1;

	if (parse_headers(rpl, HDR_EOH_F, 0) < 0) {
		LM_ERR("error while parsing all headers in the reply\n");
		return -1;
	}
	if(parse_to_header(rpl)<0 || parse_from_header(rpl)<0) {
		LM_ERR("error while parsing From/To headers in the reply\n");
		return -1;
	}
	memset(tmdlg, 0, sizeof(dlg_t));

	str2int(&(get_cseq(rpl)->number), &tmdlg->loc_seq.value);
	tmdlg->loc_seq.is_set = 1;

	tmdlg->id.call_id = rpl->callid->body;
	trim(&tmdlg->id.call_id);

	if (get_from(rpl)->tag_value.len) {
		tmdlg->id.loc_tag = get_from(rpl)->tag_value;
	}
#if 0
	if (get_to(rpl)->tag_value.len) {
		tmdlg->id.rem_tag = get_to(rpl)->tag_value;
	}
#endif
	tmdlg->loc_uri = get_from(rpl)->uri;
	tmdlg->rem_uri = get_to(rpl)->uri;
	tmdlg->state= DLG_CONFIRMED;
	return 0;
}

#define MAX_UACH_SIZE 2048

/**
 *
 */
void uac_req_run_event_route(sip_msg_t *msg, uac_send_info_t *tp, int rcode)
{
	char *evrtname = "uac:reply";
	int rt, backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;

	rt = route_get(&event_rt, evrtname);
	if (rt < 0 || event_rt.rlist[rt] == NULL)
	{
		LM_DBG("event_route[uac:reply] does not exist\n");
		return;
	}

	uac_send_info_copy(tp, &_uac_req);
	_uac_req.evcode = rcode;
	if(msg==NULL)
	{
		_uac_req.evtype = 2;
		fmsg = faked_msg_get_next();
	} else {
		_uac_req.evtype = 1;
		fmsg = msg;
	}

	backup_rt = get_route_type();
	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ctx);
	run_top_route(event_rt.rlist[rt], fmsg, 0);
	set_route_type(backup_rt);
}

/** 
 * TM callback function
 */
void uac_send_tm_callback(struct cell *t, int type, struct tmcb_params *ps)
{
	int ret;
	struct hdr_field *hdr;
	HASHHEX response;
	str *new_auth_hdr = NULL;
	static struct authenticate_body auth;
	struct uac_credential cred;
	char  b_hdrs[MAX_UACH_SIZE];
	str   s_hdrs;
	uac_req_t uac_r;
	dlg_t tmdlg;
	uac_send_info_t *tp = NULL;

	LM_DBG("tm callback with status %d\n", ps->code);

	if(ps->param==NULL || *ps->param==0)
	{
		LM_DBG("callback param with message id not received\n");
		goto done;
	}
	tp = (uac_send_info_t*)(*ps->param);

	if(tp->evroute!=0) {
		uac_req_run_event_route((ps->rpl==FAKED_REPLY)?NULL:ps->rpl,
				tp, ps->code);
	}

	if((ps->code != 401 && ps->code != 407) || tp->s_apasswd.len<=0)
	{
		LM_DBG("completed with status %d\n", ps->code);
		goto done;
	}

	LM_DBG("completed with status %d\n", ps->code);

	hdr = get_autenticate_hdr(ps->rpl, ps->code);
	if (hdr==0)
	{
		LM_ERR("failed to extract authenticate hdr\n");
		goto error;
	}

	LM_DBG("auth header body [%.*s]\n",
		hdr->body.len, hdr->body.s);

	if (parse_authenticate_body(&hdr->body, &auth)<0)
	{
		LM_ERR("failed to parse auth hdr body\n");
		goto error;
	}

	cred.realm  = auth.realm;
	cred.user   = tp->s_auser;
	cred.passwd = tp->s_apasswd;
	cred.next   = NULL;

	do_uac_auth(&tp->s_method, &tp->s_ruri, &cred, &auth, response);
	new_auth_hdr=build_authorization_hdr(ps->code, &tp->s_ruri, &cred,
						&auth, response);
	if (new_auth_hdr==0)
	{
		LM_ERR("failed to build authorization hdr\n");
		goto error;
	}

	if(tp->s_hdrs.len <= 0) {
		snprintf(b_hdrs, MAX_UACH_SIZE,
				"%.*s",
				new_auth_hdr->len, new_auth_hdr->s);
	} else {
		snprintf(b_hdrs, MAX_UACH_SIZE,
				"%.*s%.*s",
				tp->s_hdrs.len, tp->s_hdrs.s,
				new_auth_hdr->len, new_auth_hdr->s);
	}

	s_hdrs.s = b_hdrs; s_hdrs.len = strlen(s_hdrs.s);
	pkg_free(new_auth_hdr->s);

	memset(&uac_r, 0, sizeof(uac_r));
	if(uac_send_tmdlg(&tmdlg, ps->rpl)<0)
	{
		LM_ERR("failed to build tm dialog\n");
		goto error;
	}
	tmdlg.rem_target = tp->s_ruri;
	if(tp->s_ouri.len>0)
		tmdlg.dst_uri = tp->s_ouri;
	uac_r.method = &tp->s_method;
	uac_r.headers = &s_hdrs;
	uac_r.body = (tp->s_body.len <= 0) ? NULL : &tp->s_body;
	uac_r.ssock = (tp->s_sock.len <= 0) ? NULL : &tp->s_sock;
	uac_r.dialog = &tmdlg;
	uac_r.cb_flags = TMCB_LOCAL_COMPLETED;
	ret = tmb.t_request_within(&uac_r);

	if(ret<0) {
		LM_ERR("failed to send request with authentication\n");
		goto error;
	}

done:
error:
	if(tp!=NULL)
		shm_free(tp);
	return;
}


int uac_req_send(void)
{
	int ret;
	uac_req_t uac_r;
	uac_send_info_t *tp = NULL;

	if(_uac_req.s_ruri.len<=0 || _uac_req.s_method.len == 0
			|| tmb.t_request==NULL)
		return -1;

	memset(&uac_r, '\0', sizeof(uac_r));
	uac_r.method = &_uac_req.s_method;
	uac_r.headers = (_uac_req.s_hdrs.len <= 0) ? NULL : &_uac_req.s_hdrs;
	uac_r.body = (_uac_req.s_body.len <= 0) ? NULL : &_uac_req.s_body;
	uac_r.ssock = (_uac_req.s_sock.len <= 0) ? NULL : &_uac_req.s_sock;
	if((_uac_req.s_auser.len > 0 && _uac_req.s_apasswd.len>0)
			|| (_uac_req.evroute > 0))
	{
		tp = uac_send_info_clone(&_uac_req);
		if(tp==NULL)
		{
			LM_ERR("cannot clone the uac structure\n");
			return -1;
		}

		uac_r.cb_flags = TMCB_LOCAL_COMPLETED;
		/* Callback function */
		uac_r.cb  = uac_send_tm_callback;
		/* Callback parameter */
		uac_r.cbp = (void*)tp;
	}
	uac_r.callid = (_uac_req.s_callid.len <= 0) ? NULL : &_uac_req.s_callid;
	ret = tmb.t_request(&uac_r,  /* UAC Req */
						&_uac_req.s_ruri,        /* Request-URI */
						(_uac_req.s_turi.len<=0)?&_uac_req.s_ruri:&_uac_req.s_turi, /* To */
						(_uac_req.s_furi.len<=0)?&_uac_req.s_ruri:&_uac_req.s_furi, /* From */
						(_uac_req.s_ouri.len<=0)?NULL:&_uac_req.s_ouri /* outbound uri */
		);

	if(ret<0) {
		if(tp!=NULL)
			shm_free(tp);
		return -1;
	}
	return 1;
}

int w_uac_req_send(struct sip_msg *msg, char *s1, char *s2)
{
	return uac_req_send();
}
