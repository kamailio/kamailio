/**
 *
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
		       
#include "ht_api.h"
#include "ht_var.h"
#include "ht_dmq.h"

/* pkg copy */
ht_cell_t *_htc_local=NULL;
extern ht_cell_t *ht_expired_cell;

int pv_get_ht_cell(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	str htname;
	ht_cell_t *htc=NULL;
	ht_pv_t *hpv;

	hpv = (ht_pv_t*)param->pvn.u.dname;

	if(hpv->ht==NULL)
	{
		hpv->ht = ht_get_table(&hpv->htname);
		if(hpv->ht==NULL)
			return pv_get_null(msg, param, res);
	}
	if(pv_printf_s(msg, hpv->pve, &htname)!=0)
	{
		LM_ERR("cannot get $ht name\n");
		return -1;
	}
	htc = ht_cell_pkg_copy(hpv->ht, &htname, _htc_local);
	if(htc==NULL)
	{
		if(hpv->ht->flags==PV_VAL_INT)
			return pv_get_sintval(msg, param, res, hpv->ht->initval.n);
		return pv_get_null(msg, param, res);
	}
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
	ht_pv_t *hpv;

	hpv = (ht_pv_t*)param->pvn.u.dname;

	if(hpv->ht==NULL)
		hpv->ht = ht_get_table(&hpv->htname);
	if(hpv->ht==NULL)
		return -1;

	if(pv_printf_s(msg, hpv->pve, &htname)!=0)
	{
		LM_ERR("cannot get $ht name\n");
		return -1;
	}
	LM_DBG("set value for $ht(%.*s=>%.*s)\n", hpv->htname.len, hpv->htname.s,
			htname.len, htname.s);
	if((val==NULL) || (val->flags&PV_VAL_NULL))
	{
		/* delete it */
		if (hpv->ht->dmqreplicate>0 && ht_dmq_replicate_action(HT_DMQ_DEL_CELL, &hpv->htname, &htname, 0, NULL, 0)!=0) {
			LM_ERR("dmq relication failed\n");
		}
		ht_del_cell(hpv->ht, &htname);
		return 0;
	}

	if(val->flags&PV_TYPE_INT)
	{
		isval.n = val->ri;
		if (hpv->ht->dmqreplicate>0 && ht_dmq_replicate_action(HT_DMQ_SET_CELL, &hpv->htname, &htname, 0, &isval, 1)!=0) {
			LM_ERR("dmq relication failed\n");
		}
		if(ht_set_cell(hpv->ht, &htname, 0, &isval, 1)!=0)
		{
			LM_ERR("cannot set $ht(%.*s)\n", htname.len, htname.s);
			return -1;
		}
	} else {
		isval.s = val->rs;
		if (hpv->ht->dmqreplicate>0 && ht_dmq_replicate_action(HT_DMQ_SET_CELL, &hpv->htname, &htname, AVP_VAL_STR, &isval, 1)!=0) {
			LM_ERR("dmq relication failed\n");
		}
		if(ht_set_cell(hpv->ht, &htname, AVP_VAL_STR, &isval, 1)!=0)
		{
			LM_ERR("cannot set $ht(%.*s)\n", htname.len, htname.s);
			return -1;
		}
	}
	return 0;
}

int pv_parse_ht_name(pv_spec_p sp, str *in)
{
	ht_pv_t *hpv=NULL;
	char *p;
	str pvs;

	if(in->s==NULL || in->len<=0)
		return -1;

	hpv = (ht_pv_t*)pkg_malloc(sizeof(ht_pv_t));
	if(hpv==NULL)
		return -1;

	memset(hpv, 0, sizeof(ht_pv_t));

	p = in->s;

	while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
		p++;
	if(p>in->s+in->len || *p=='\0')
		goto error;
	hpv->htname.s = p;
	while(p < in->s + in->len)
	{
		if(*p=='=' || *p==' ' || *p=='\t' || *p=='\n' || *p=='\r')
			break;
		p++;
	}
	if(p>in->s+in->len || *p=='\0')
		goto error;
	hpv->htname.len = p - hpv->htname.s;
	if(*p!='=')
	{
		while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
			p++;
		if(p>in->s+in->len || *p=='\0' || *p!='=')
			goto error;
	}
	p++;
	if(*p!='>')
		goto error;
	p++;

	pvs.len = in->len - (int)(p - in->s);
	pvs.s = p;
	LM_DBG("htable [%.*s] - key [%.*s]\n", hpv->htname.len, hpv->htname.s,
			pvs.len, pvs.s);
	if(pv_parse_format(&pvs, &hpv->pve)<0 || hpv->pve==NULL)
	{
		LM_ERR("wrong format[%.*s]\n", in->len, in->s);
		goto error;
	}
	hpv->ht = ht_get_table(&hpv->htname);
	sp->pvp.pvn.u.dname = (void*)hpv;
	sp->pvp.pvn.type = PV_NAME_OTHER;
	return 0;

error:
	if(hpv!=NULL)
		pkg_free(hpv);
	return -1;
}

int pv_get_ht_cell_expire(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	str htname;
	ht_pv_t *hpv;
	unsigned int now;

	hpv = (ht_pv_t*)param->pvn.u.dname;

	if(hpv->ht==NULL)
	{
		hpv->ht = ht_get_table(&hpv->htname);
		if(hpv->ht==NULL)
			return pv_get_null(msg, param, res);
	}
	if(pv_printf_s(msg, hpv->pve, &htname)!=0)
	{
		LM_ERR("cannot get $ht name\n");
		return -1;
	}
	if(ht_get_cell_expire(hpv->ht, &htname, &now)!=0)
		return pv_get_null(msg, param, res);
	/* integer */
	return pv_get_uintval(msg, param, res, now);
}

int pv_set_ht_cell_expire(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	str htname;
	int_str isval;
	ht_pv_t *hpv;

	hpv = (ht_pv_t*)param->pvn.u.dname;

	if(hpv->ht==NULL)
		hpv->ht = ht_get_table(&hpv->htname);
	if(hpv->ht==NULL)
		return -1;

	if(pv_printf_s(msg, hpv->pve, &htname)!=0)
	{
		LM_ERR("cannot get $ht name\n");
		return -1;
	}
	LM_DBG("set expire value for $ht(%.*s=>%.*s)\n", hpv->htname.len,
			hpv->htname.s, htname.len, htname.s);
	isval.n = 0;
	if(val!=NULL)
	{
		if(val->flags&PV_TYPE_INT)
			isval.n = val->ri;
	}
	if (hpv->ht->dmqreplicate>0 && ht_dmq_replicate_action(HT_DMQ_SET_CELL_EXPIRE, &hpv->htname, &htname, 0, &isval, 0)!=0) {
		LM_ERR("dmq relication failed\n");
	}	
	if(ht_set_cell_expire(hpv->ht, &htname, 0, &isval)!=0)
	{
		LM_ERR("cannot set $ht(%.*s)\n", htname.len, htname.s);
		return -1;
	}

	return 0;
}

int pv_get_ht_cn(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	str htname;
	ht_pv_t *hpv;
	int cnt = 0;

	hpv = (ht_pv_t*)param->pvn.u.dname;

	if(hpv->ht==NULL)
	{
		hpv->ht = ht_get_table(&hpv->htname);
		if(hpv->ht==NULL)
			return pv_get_null(msg, param, res);
	}
	if(pv_printf_s(msg, hpv->pve, &htname)!=0)
	{
		LM_ERR("cannot get $ht name\n");
		return -1;
	}
	
	cnt = ht_count_cells_re(&htname, hpv->ht, 0);

	/* integer */
	return pv_get_sintval(msg, param, res, cnt);
}

int pv_get_ht_cv(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	str htname;
	ht_pv_t *hpv;
	int cnt = 0;

	hpv = (ht_pv_t*)param->pvn.u.dname;

	if(hpv->ht==NULL)
	{
		hpv->ht = ht_get_table(&hpv->htname);
		if(hpv->ht==NULL)
			return pv_get_null(msg, param, res);
	}
	if(pv_printf_s(msg, hpv->pve, &htname)!=0)
	{
		LM_ERR("cannot get $ht name\n");
		return -1;
	}
	
	cnt = ht_count_cells_re(&htname, hpv->ht, 1);

	/* integer */
	return pv_get_sintval(msg, param, res, cnt);
}

int pv_get_ht_add(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res, int val)
{
	str htname;
	ht_cell_t *htc=NULL;
	ht_pv_t *hpv;

	hpv = (ht_pv_t*)param->pvn.u.dname;

	if(hpv->ht==NULL)
	{
		hpv->ht = ht_get_table(&hpv->htname);
		if(hpv->ht==NULL)
			return pv_get_null(msg, param, res);
	}
	if(pv_printf_s(msg, hpv->pve, &htname)!=0)
	{
		LM_ERR("cannot get $ht name\n");
		return -1;
	}
	htc = ht_cell_value_add(hpv->ht, &htname, val, 1, _htc_local);
	if(htc==NULL)
	{
		return pv_get_null(msg, param, res);
	}
	if(_htc_local!=htc)
	{
		ht_cell_pkg_free(_htc_local);
		_htc_local=htc;
	}

	if(htc->flags&AVP_VAL_STR)
		return pv_get_null(msg, param, res);

	/* integer */
	if (hpv->ht->dmqreplicate>0) {
		if (ht_dmq_replicate_action(HT_DMQ_SET_CELL, &hpv->htname, &htname, 0, &htc->value, 1)!=0) {
			LM_ERR("dmq relication failed\n");
		}
	}	
	return pv_get_sintval(msg, param, res, htc->value.n);
}

int pv_get_ht_inc(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	return pv_get_ht_add(msg, param, res, 1);
}

int pv_get_ht_dec(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	return pv_get_ht_add(msg, param, res, -1);
}

int pv_parse_ht_expired_cell(pv_spec_t *sp, str *in)
{
	if ((in->len != 3 || strncmp(in->s, "key", in->len) != 0) &&
	    (in->len != 5 || strncmp(in->s, "value", in->len) != 0))
	{
		return -1;
	}

	sp->pvp.pvn.u.isname.name.s.s = in->s;
	sp->pvp.pvn.u.isname.name.s.len = in->len;
	sp->pvp.pvn.u.isname.type = 0;
	sp->pvp.pvn.type = PV_NAME_INTSTR;

	return 0;
}

int pv_get_ht_expired_cell(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	if (res == NULL || ht_expired_cell == NULL)
	{
		return -1;
	}

	if (param->pvn.u.isname.name.s.len == 3 &&
		strncmp(param->pvn.u.isname.name.s.s, "key", 3) == 0)
	{
		res->rs = ht_expired_cell->name;
	}
	else if (param->pvn.u.isname.name.s.len == 5 &&
		strncmp(param->pvn.u.isname.name.s.s, "value", 5) == 0)
	{
		if(ht_expired_cell->flags&AVP_VAL_STR) {
			return pv_get_strval(msg, param, res, &ht_expired_cell->value.s);
		} else {
			return pv_get_sintval(msg, param, res, ht_expired_cell->value.n);
		}
	}

	if (res->rs.s == NULL)
		res->flags = PV_VAL_NULL;
	else
		res->flags = PV_VAL_STR;

	return 0;
}

int pv_parse_iterator_name(pv_spec_t *sp, str *in)
{
	if(in->len<=0)
	{
		return -1;
	}

	sp->pvp.pvn.u.isname.name.s.s = in->s;
	sp->pvp.pvn.u.isname.name.s.len = in->len;
	sp->pvp.pvn.u.isname.type = 0;
	sp->pvp.pvn.type = PV_NAME_INTSTR;

	return 0;
}

int pv_get_iterator_key(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	ht_cell_t *it=NULL;
	if (res == NULL)
	{
		return -1;
	}

	it = ht_iterator_get_current(&param->pvn.u.isname.name.s);
	if(it==NULL) {
		return pv_get_null(msg, param, res);
	}
	return pv_get_strval(msg, param, res, &it->name);
}

int pv_get_iterator_val(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	ht_cell_t *it=NULL;
	if (res == NULL)
	{
		return -1;
	}

	it = ht_iterator_get_current(&param->pvn.u.isname.name.s);
	if(it==NULL) {
		return pv_get_null(msg, param, res);
	}
	if(it->flags&AVP_VAL_STR)
		return pv_get_strval(msg, param, res, &it->value.s);

	/* integer */
	return pv_get_sintval(msg, param, res, it->value.n);
}
