/**
 * $Id$
 *
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
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
		       
#include "../../route.h"

#include "dlg_var.h"

dlg_ctx_t _dlg_ctx;

int dlg_cfg_cb(struct sip_msg *foo, unsigned int flags, void *bar)
{
	memset(&_dlg_ctx, 0, sizeof(dlg_ctx_t));

	return 1;
}

int pv_get_dlg_ctx(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	if(param==NULL)
		return -1;
	switch(param->pvn.u.isname.name.n)
	{
		case 1:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.flags);
		case 2:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.timeout);
		case 3:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.to_bye);
		case 4:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.to_route);
		case 5:
			_dlg_ctx.set = (_dlg_ctx.dlg==NULL)?0:1;
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.set);
		case 6:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dir);
		default:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.on);
	}
	return 0;
}

int pv_set_dlg_ctx(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	int n;
	char *rtp;

	if(param==NULL)
		return -1;

	if(val==NULL)
		n = 0;
	else
		n = val->ri;

	switch(param->pvn.u.isname.name.n)
	{
		case 1:
			_dlg_ctx.flags = n;
		break;
		case 2:
			_dlg_ctx.timeout = n;
		break;
		case 3:
			_dlg_ctx.to_bye = n;
		break;
		case 4:
			if(val->flags&PV_VAL_STR) {
				if(val->rs.s[val->rs.len]=='\0'
						&& val->rs.len<DLG_TOROUTE_SIZE) {
					_dlg_ctx.to_route = route_lookup(&main_rt, val->rs.s);
					strcpy(_dlg_ctx.to_route_name, val->rs.s);
				} else _dlg_ctx.to_route = 0;
			} else {
				if(n!=0) {
					rtp = int2str(n, NULL);
					_dlg_ctx.to_route = route_lookup(&main_rt, rtp);
					strcpy(_dlg_ctx.to_route_name, rtp);
				} else _dlg_ctx.to_route = 0;
			}
			if(_dlg_ctx.to_route <0) _dlg_ctx.to_route = 0;
		break;
		default:
			_dlg_ctx.on = n;
		break;
	}
	return 0;
}

int pv_parse_dlg_ctx_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 2: 
			if(strncmp(in->s, "on", 2)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else goto error;
		break;
		case 3: 
			if(strncmp(in->s, "set", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 5;
			else if(strncmp(in->s, "dir", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 6;
			else goto error;
		break;
		case 5: 
			if(strncmp(in->s, "flags", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else goto error;
		break;
		case 7: 
			if(strncmp(in->s, "timeout", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else goto error;
		break;
		case 11: 
			if(strncmp(in->s, "timeout_bye", 11)==0)
				sp->pvp.pvn.u.isname.name.n = 3;
			else goto error;
		break;
		case 13:
			if(strncmp(in->s, "timeout_route", 13)==0)
				sp->pvp.pvn.u.isname.name.n = 4;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV name %.*s\n", in->len, in->s);
	return -1;
}

int pv_get_dlg(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if(param==NULL)
		return -1;
	if(_dlg_ctx.dlg == NULL)
		return pv_get_null(msg, param, res);
	switch(param->pvn.u.isname.name.n)
	{
		case 1:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->h_id);
		case 2:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->state);
		case 3:
			if(_dlg_ctx.dlg->route_set[DLG_CALLEE_LEG].s==NULL
					|| _dlg_ctx.dlg->route_set[DLG_CALLEE_LEG].len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->route_set[DLG_CALLEE_LEG]);
		case 4:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->dflags);
		case 5:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->sflags);
		case 6:
			if(_dlg_ctx.dlg->callid.s==NULL
					|| _dlg_ctx.dlg->callid.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->callid);
		case 7:
			if(_dlg_ctx.dlg->to_uri.s==NULL
					|| _dlg_ctx.dlg->to_uri.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->to_uri);
		case 8:
			if(_dlg_ctx.dlg->tag[DLG_CALLEE_LEG].s==NULL
					|| _dlg_ctx.dlg->tag[DLG_CALLEE_LEG].len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->tag[DLG_CALLEE_LEG]);
		case 9:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->toroute);
		case 10:
			if(_dlg_ctx.dlg->cseq[DLG_CALLEE_LEG].s==NULL
					|| _dlg_ctx.dlg->cseq[DLG_CALLEE_LEG].len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->cseq[DLG_CALLEE_LEG]);
		case 11:
			if(_dlg_ctx.dlg->route_set[DLG_CALLER_LEG].s==NULL
					|| _dlg_ctx.dlg->route_set[DLG_CALLER_LEG].len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->route_set[DLG_CALLER_LEG]);
		case 12:
			if(_dlg_ctx.dlg->from_uri.s==NULL
					|| _dlg_ctx.dlg->from_uri.len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->from_uri);
		case 13:
			if(_dlg_ctx.dlg->tag[DLG_CALLER_LEG].s==NULL
					|| _dlg_ctx.dlg->tag[DLG_CALLER_LEG].len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->tag[DLG_CALLER_LEG]);
		case 14:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->lifetime);
		case 15:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->start_ts);
		case 16:
			if(_dlg_ctx.dlg->cseq[DLG_CALLER_LEG].s==NULL
					|| _dlg_ctx.dlg->cseq[DLG_CALLER_LEG].len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->cseq[DLG_CALLER_LEG]);
		case 17:
			if(_dlg_ctx.dlg->contact[DLG_CALLEE_LEG].s==NULL
					|| _dlg_ctx.dlg->contact[DLG_CALLEE_LEG].len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->contact[DLG_CALLEE_LEG]);
		case 18:
			if(_dlg_ctx.dlg->bind_addr[DLG_CALLEE_LEG]==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->bind_addr[DLG_CALLEE_LEG]->sock_str);
		case 19:
			if(_dlg_ctx.dlg->contact[DLG_CALLER_LEG].s==NULL
					|| _dlg_ctx.dlg->contact[DLG_CALLER_LEG].len<=0)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->contact[DLG_CALLER_LEG]);
		case 20:
			if(_dlg_ctx.dlg->bind_addr[DLG_CALLER_LEG]==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res,
					&_dlg_ctx.dlg->bind_addr[DLG_CALLER_LEG]->sock_str);
		case 21:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->h_entry);
		default:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_dlg_ctx.dlg->ref);
	}
	return 0;
}

int pv_parse_dlg_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 3: 
			if(strncmp(in->s, "ref", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else goto error;
		break;
		case 4: 
			if(strncmp(in->s, "h_id", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else goto error;
		break;
		case 5: 
			if(strncmp(in->s, "state", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else if(strncmp(in->s, "to_rs", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 3;
			else goto error;
		break;
		case 6: 
			if(strncmp(in->s, "dflags", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 4;
			else if(strncmp(in->s, "sflags", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 5;
			else if(strncmp(in->s, "callid", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 6;
			else if(strncmp(in->s, "to_uri", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 7;
			else if(strncmp(in->s, "to_tag", 6)==0)
				sp->pvp.pvn.u.isname.name.n = 8;
			else goto error;
		break;
		case 7: 
			if(strncmp(in->s, "toroute", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 9;
			else if(strncmp(in->s, "to_cseq", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 10;
			else if(strncmp(in->s, "from_rs", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 11;
			else if(strncmp(in->s, "h_entry", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 21;
			else goto error;
		break;
		case 8: 
			if(strncmp(in->s, "from_uri", 8)==0)
				sp->pvp.pvn.u.isname.name.n = 12;
			else if(strncmp(in->s, "from_tag", 8)==0)
				sp->pvp.pvn.u.isname.name.n = 13;
			else if(strncmp(in->s, "lifetime", 8)==0)
				sp->pvp.pvn.u.isname.name.n = 14;
			else if(strncmp(in->s, "start_ts", 8)==0)
				sp->pvp.pvn.u.isname.name.n = 15;
			else goto error;
		break;
		case 9: 
			if(strncmp(in->s, "from_cseq", 9)==0)
				sp->pvp.pvn.u.isname.name.n = 16;
			else goto error;
		break;
		case 10: 
			if(strncmp(in->s, "to_contact", 10)==0)
				sp->pvp.pvn.u.isname.name.n = 17;
			else goto error;
		break;
		case 11: 
			if(strncmp(in->s, "to_bindaddr", 11)==0)
				sp->pvp.pvn.u.isname.name.n = 18;
			else goto error;
		break;
		case 12: 
			if(strncmp(in->s, "from_contact", 12)==0)
				sp->pvp.pvn.u.isname.name.n = 19;
			else goto error;
		break;
		case 13: 
			if(strncmp(in->s, "from_bindaddr", 20)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV name %.*s\n", in->len, in->s);
	return -1;
}

void dlg_set_ctx_dialog(struct dlg_cell *dlg)
{
	_dlg_ctx.dlg = dlg;
}

struct dlg_cell* dlg_get_ctx_dialog(void)
{
	return _dlg_ctx.dlg;
}

dlg_ctx_t* dlg_get_dlg_ctx(void)
{
	return &_dlg_ctx;
}
