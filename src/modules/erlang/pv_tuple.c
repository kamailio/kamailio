/**
 * Copyright (C) 2015 Bicom Systems Ltd, (bicomsystems.com)
 *
 * Author: Seudin Kasumovic (seudin.kasumovic@gmail.com)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdlib.h>

#include "../../pvar.h"

#include "pv_tuple.h"
#include "pv_xbuff.h"

static str tuple_list=str_init("[tuples]");

static int counter;
static char *tuple_fmt_buff = NULL;

sr_xavp_t *xavp_get_tuples()
{
	sr_xavp_t *list;
	list = xavp_get(&tuple_list,NULL);

	if(!list) counter = 0;

	return list;
}

sr_xavp_t *pv_tuple_get_tuple(str *name)
{
	return xavp_get_child(&tuple_list,name);
}

int pv_tuple_set(struct sip_msg* msg,  pv_param_t* param, int op, pv_value_t* val)
{
	str name;
	sr_xavp_t *tuples_root;
	sr_xavp_t *tuple;
	sr_xavp_t *th,*new,*old,*prv=NULL;
	sr_xavp_t *tuple_xavp;
	sr_xavp_t *elem_xavp;
	sr_xval_t tuple_val;
	pv_param_t p;
	pv_value_t empty;
	int idx = 0;
	int idxf = 0;
	int attr = 0;

	if (param->pvn.type != PV_NAME_INTSTR || !(param->pvn.u.isname.type & AVP_NAME_STR)) {
		LM_ERR("invalid variable name type\n");
		return -1;
	}

	memset((void*)&empty,0,sizeof(pv_value_t));
	empty.flags = PV_VAL_NULL;

	memset((void*)&tuple_val,0,sizeof(sr_xval_t));

	/* list name */
	name = param->pvn.u.isname.name.s;

	tuples_root = xavp_get_tuples();

	if(!tuples_root) {

		if(pv_xbuff_new_xavp(&tuple_xavp,&empty,&counter,'t')) {
			LM_ERR("failed to create new value\n");
			return -1;
		}
		tuple_val.type = SR_XTYPE_XAVP;
		tuple_val.v.xavp = tuple_xavp;

		tuple = xavp_add_xavp_value(&tuple_list,&name,&tuple_val,xavp_get_crt_list());

		if (!tuple)
			goto err;
	}

	tuple=xavp_get_child(&tuple_list, &name);

	if (!tuple) {

		if(pv_xbuff_new_xavp(&tuple_xavp,&empty,&counter,'t')) {
			LM_ERR("failed to create new value\n");
			return -1;
		}

		tuple_val.type = SR_XTYPE_XAVP;
		tuple_val.v.xavp = tuple_xavp;

		tuple=xavp_add_value(&name,&tuple_val,&tuples_root->val.v.xavp);

		if (!tuple)
			goto err;
	}

	th = tuple->val.v.xavp;

	if(pv_xbuff_new_xavp(&elem_xavp,val,&counter,0)) {
		LM_ERR("failed to create new value\n");
		return -1;
	}

	/* work on copy of index! */
	p = *param;

	/* fix index */
	attr = xbuff_get_attr_flags(p.pvi.type);
	p.pvi.type = xbuff_fix_index(p.pvi.type);

	/* get the index */
	if(pv_get_spec_index(msg, &p, &idx, &idxf))
	{
		LM_ERR("invalid index\n");
		return -1;
	}

	if (xbuff_is_attr_set(attr)) {
		LM_ERR("read only attribute %.*s\n",STR_FMT(&xbuff_attr_name(attr)));
		return -1;
	}

	/* prepend on list when no index */
	if (xbuff_is_no_index(attr)) {
		if (th->val.type == SR_XTYPE_NULL) {
			th->val.type = SR_XTYPE_XAVP;
			th->val.v.xavp = elem_xavp;
			return 0;
		} else {
			return xavp_add(elem_xavp,&th->val.v.xavp);
		}
	}

	if (idxf == PV_IDX_ALL) {
		xavp_destroy_list(&th->val.v.xavp);
		if (elem_xavp->val.type == SR_XTYPE_NULL) {
			/* create empty list */
			xavp_destroy_list(&elem_xavp);

			memset((void*)&th->val,0,sizeof(sr_xval_t));
			th->val.type = SR_XTYPE_NULL;
			return 0;
		} else {
			return xavp_add(elem_xavp,&th->val.v.xavp);
		}
	}

	/* set by index */
	old = xavp_get_nth(&th->val.v.xavp,idx,&prv);
	new = elem_xavp;

	if (old) {
		new->next = old->next;
		if (prv) {
			prv->next = new;
		} else {
			new->next = old->next;
			th->val.v.xavp = new;
		}
		old->next = NULL;
		xavp_destroy_list(&old);
	} else {
		if (prv) {
			prv->next = new;
		} else {
			th->val.v.xavp = new;
		}
	}

	return 0;

err:
	LM_ERR("failed to add value into tuple\n");
	xavp_destroy_list(&tuple_xavp);
	xavp_destroy_list(&elem_xavp);

	return -1;
}

int pv_tuple_get_value(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, sr_xavp_t *avp)
{
	static char _pv_xavp_buf[128];
	str s;

	if (!avp) return pv_get_null(msg,param,res);

	switch(avp->val.type) {
	case SR_XTYPE_NULL:
		return pv_get_null(msg, param, res);
		break;
	case SR_XTYPE_INT:
		return pv_get_sintval(msg, param, res, avp->val.v.i);
		break;
	case SR_XTYPE_STR:
		switch (avp->name.s[0]) {
		case 'a':
			if(snprintf(_pv_xavp_buf, 128, "<<atom:%p>>", avp->val.v.xavp)<0)
				return pv_get_null(msg, param, res);
			break;
		default:
			return pv_get_strval(msg, param, res, &avp->val.v.s);
		}
		break;
	case SR_XTYPE_TIME:
		if(snprintf(_pv_xavp_buf, 128, "%lu", (long unsigned)avp->val.v.t)<0)
			return pv_get_null(msg, param, res);
		break;
	case SR_XTYPE_LONG:
		if(snprintf(_pv_xavp_buf, 128, "%ld", (long unsigned)avp->val.v.l)<0)
			return pv_get_null(msg, param, res);
		break;
	case SR_XTYPE_LLONG:
		if(snprintf(_pv_xavp_buf, 128, "%lld", avp->val.v.ll)<0)
			return pv_get_null(msg, param, res);
		break;
	case SR_XTYPE_XAVP:
		switch(avp->name.s[0]) {
		case 'l':
			if(snprintf(_pv_xavp_buf, 128, "<<list:%p>>", avp->val.v.xavp)<0)
				return pv_get_null(msg, param, res);
			break;
		case 't':
		default:
				if(snprintf(_pv_xavp_buf, 128, "<<tuple:%p>>", avp->val.v.xavp)<0)
					return pv_get_null(msg, param, res);
		}
		break;
	case SR_XTYPE_DATA:
		if (avp->name.s[0] == 'p') {
			if(snprintf(_pv_xavp_buf, 128, "<<pid:%p>>", avp->val.v.data)<0)
				return pv_get_null(msg, param, res);
		} else if(snprintf(_pv_xavp_buf, 128, "<<binary:%p>>", avp->val.v.data)<0)
			return pv_get_null(msg, param, res);
		break;
	default:
		return pv_get_null(msg, param, res);
	}
	s.s = _pv_xavp_buf;
	s.len = strlen(_pv_xavp_buf);
	return pv_get_strval(msg, param, res, &s);
}

int pv_tuple_get(struct sip_msg* msg,  pv_param_t* param, pv_value_t* res)
{
	str name;
	pv_spec_t *nsp = NULL;
	pv_name_t *pvn;
	pv_index_t *pvi;
	sr_xavp_t *tuples_root;
	sr_xavp_t *tuple;
	sr_xavp_t *th;
	sr_xavp_t *xavp;
	pv_param_t p;
	int idx=0;
	int idxf=0;
	int attr;
	int i,count;

	ei_x_buff xbuff;

	if(param==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	if (param->pvn.type != PV_NAME_INTSTR || !(param->pvn.u.isname.type & AVP_NAME_STR))
				return -1;

	if( param->pvn.type == PV_NAME_PVAR) {
			nsp = param->pvn.u.dname;
	}

	/* work on copy of index! */
	p = *param;

	if (nsp) {
		pvi = &nsp->pvp.pvi;
		pvn = &nsp->pvp.pvn;
	} else {
		pvi = &p.pvi;
		pvn = &p.pvn;
	}

	/* list name */
	name = pvn->u.isname.name.s;

	/* fix index */
	attr = xbuff_get_attr_flags(pvi->type);
	pvi->type = xbuff_fix_index(pvi->type);

	/* get the index */
	if(pv_get_spec_index(msg, &p, &idx, &idxf))
	{
		LM_ERR("invalid index\n");
		return -1;
	}

	tuples_root = xavp_get_tuples();
	if(!tuples_root) {
		return pv_get_null(msg,param,res);
	}

	tuple = xavp_get(&name,tuples_root->val.v.xavp);
	if (!tuple) {
		return pv_get_null(msg,param,res);
	}

	th = tuple->val.v.xavp;

	switch (xbuff_is_attr_set(attr)) {
	case XBUFF_ATTR_TYPE:
		if (xbuff_is_no_index(attr)) {
			return pv_get_strval(msg,param,res,&xbuff_types[XBUFF_TYPE_TUPLE]);
		} else {
			xavp = xavp_get_nth(&th->val.v.xavp,idx,NULL);
			return pv_xbuff_get_type(msg,param,res,xavp);
		}
		break;
	case XBUFF_ATTR_LENGTH:
		xavp = xbuff_is_no_index(attr) ? th : xavp_get_nth(&th->val.v.xavp,idx,NULL);
		if (xavp) {
			count = xavp_get_count(xavp->val.v.xavp);
			return pv_get_uintval(msg,param,res,(unsigned int)count);
		} else {
			return pv_get_null(msg,param,res);
		}
		break;
	case XBUFF_ATTR_FORMAT:
		/*
		 * Prints a term, in clear text, to the PV value pointed to by res.
		 * It tries to resemble the term printing in the erlang shell.
		 */
		ei_x_new_with_version(&xbuff);
		xavp = xbuff_is_no_index(attr) ? th : xavp_get_nth(&th->val.v.xavp,idx,NULL);
		if (!xavp || xavp_encode(&xbuff,xavp,1)) {
			ei_x_free(&xbuff);
			return -1;
		}
		i = 1;
		if (ei_s_print_term(&tuple_fmt_buff,xbuff.buff,&i)<0) {
			LM_ERR("BUG: xbuff[index] doesn't contain a valid term!\n");
			ei_x_free(&xbuff);
			return -1;
		}
		i = pv_get_strzval(msg,param,res,tuple_fmt_buff);
		ei_x_free(&xbuff);
		return i;
	}

	/* get whole list */
	if ((idxf == PV_IDX_ALL) || xbuff_is_no_index(attr)) {
		return pv_tuple_get_value(msg,param,res,tuple);
	}

	if (th->val.type == SR_XTYPE_NULL) {
		return pv_get_null(msg,param,res);
	}

	/* get by idx */
	xavp = xavp_get_nth(&th->val.v.xavp,idx,NULL);
	if (!xavp) {
		return pv_get_null(msg,param,res);
	}

	return pv_tuple_get_value(msg,param,res,xavp);
}

/*
 * free format buffer for tuple
 */
void free_tuple_fmt_buff() {
	if (tuple_fmt_buff) {
		free(tuple_fmt_buff);
	}
	tuple_fmt_buff = 0;
}
