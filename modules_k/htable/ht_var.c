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
		       
#include "ht_api.h"
#include "ht_var.h"

/* pkg copy */
ht_cell_t *_htc_local=NULL;

int pv_get_ht_cell(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	str htname;
	ht_cell_t *htc=NULL;

	if(pv_printf_s(msg, (pv_elem_t*)param->pvn.u.dname, &htname)!=0)
	{
		LM_ERR("cannot get $ht name\n");
		return -1;
	}
	htc = ht_cell_pkg_copy(&htname, _htc_local);
	if(htc==NULL)
		return pv_get_null(msg, param, res);
	if(_htc_local!=htc)
	{
		ht_cell_pkg_free(_htc_local);
		_htc_local=htc;
	}

	if(htc->flags&AVP_VAL_STR)
		return pv_get_strval(msg, param, res, &htc->value.s);
	
	/* integer */
	return pv_get_sintval(msg, param, res, htc->value.n);
}

int pv_set_ht_cell(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	str htname;
	int_str isval;

	if(pv_printf_s(msg, (pv_elem_t*)param->pvn.u.dname, &htname)!=0)
	{
		LM_ERR("cannot get $ht name\n");
		return -1;
	}
	LM_DBG("set value for $ht(%.*s)\n", htname.len, htname.s);
	if(val==NULL)
	{
		/* delete it */
		ht_del_cell(&htname);
		return 0;
	}

	if(val->flags&PV_TYPE_INT)
	{
		isval.n = val->ri;
		if(ht_set_cell(&htname, 0, &isval)!=0)
		{
			LM_ERR("cannot set $ht(%.*s)\n", htname.len, htname.s);
			return -1;
		}
	} else {
		isval.s = val->rs;
		if(ht_set_cell(&htname, AVP_VAL_STR, &isval)!=0)
		{
			LM_ERR("cannot set $ht(%.*s)\n", htname.len, htname.s);
			return -1;
		}
	}
	return 0;
}

int pv_parse_ht_name(pv_spec_p sp, str *in)
{
	pv_elem_t *pvename=NULL;
	if(in->s==NULL || in->len<=0)
		return -1;
	if(pv_parse_format(in, &pvename)<0 || pvename==NULL)
	{
		LM_ERR("wrong format[%.*s]\n", in->len, in->s);
		return -1;
	}
	sp->pvp.pvn.u.dname = (void*)pvename;
	sp->pvp.pvn.type = PV_NAME_PVAR;
	return 0;
}

