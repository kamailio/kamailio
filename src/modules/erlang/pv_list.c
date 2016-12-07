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

#include "pv_list.h"
#include "pv_xbuff.h"

static str list_list=str_init("[lists]");

static int counter;
static char *list_fmt_buff = NULL;

sr_xavp_t *xavp_get_lists()
{
	sr_xavp_t *list;
	list = xavp_get(&list_list,NULL);

	if(!list) counter = 0;

	return list;
}

sr_xavp_t *pv_list_get_list(str *name)
{
	return xavp_get_child(&list_list, name);
}

int pv_list_set(struct sip_msg* msg,  pv_param_t* param, int op, pv_value_t* val)
{
	str name;
	sr_xavp_t *lists_root;
	sr_xavp_t *list;
	sr_xavp_t *lh,*new,*old,*prv=NULL;
	sr_xavp_t *list_xavp;
	sr_xavp_t *item_xavp;
	sr_xval_t list_val;
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

	/* list name */
	name = param->pvn.u.isname.name.s;

	lists_root = xavp_get_lists();

	if(!lists_root) {

		if(pv_xbuff_new_xavp(&list_xavp,&empty,&counter,'l')) {
			LM_ERR("failed to create new value\n");
			return -1;
		}

		list_val.type = SR_XTYPE_XAVP;
		list_val.v.xavp = list_xavp;
		list = xavp_add_xavp_value(&list_list,&name,&list_val,xavp_get_crt_list());

		if (!list)
			goto err;
	}

	list=xavp_get_child(&list_list, &name);

	if (!list) {

		if(pv_xbuff_new_xavp(&list_xavp,&empty,&counter,'l')) {
			LM_ERR("failed to create new value\n");
			return -1;
		}

		list_val.type = SR_XTYPE_XAVP;
		list_val.v.xavp = list_xavp;

		list=xavp_add_value(&name,&list_val,&lists_root->val.v.xavp);

		if (!list)
			goto err;
	}

	lh = list->val.v.xavp;

	if(pv_xbuff_new_xavp(&item_xavp,val,&counter,0)) {
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
		if (lh->val.type == SR_XTYPE_NULL) {
			lh->val.type = SR_XTYPE_XAVP;
			lh->val.v.xavp = item_xavp;
			return 0;
		} else {
			return xavp_add(item_xavp,&lh->val.v.xavp);
		}
	}

	if (idxf == PV_IDX_ALL) {
		xavp_destroy_list(&lh->val.v.xavp);
		if (item_xavp->val.type == SR_XTYPE_NULL) {
			/* create empty list */
			xavp_destroy_list(&item_xavp);

			memset((void*)&lh->val,0,sizeof(sr_xval_t));
			lh->val.type = SR_XTYPE_NULL;
			return 0;
		} else {
			return xavp_add(item_xavp,&lh->val.v.xavp);
		}
	}

	/* set by index */
	old = xavp_get_nth(&lh->val.v.xavp,idx,&prv);
	new = item_xavp;

	if (old) {
		new->next = old->next;
		if (prv) {
			prv->next = new;
		} else {
			new->next = old->next;
			lh->val.v.xavp = new;
		}
		old->next = NULL;
		xavp_destroy_list(&old);
	} else {
		if (prv) {
			prv->next = new;
		} else {
			lh->val.v.xavp = new;
		}
	}

	return 0;

err:
	LM_ERR("failed to add value into list\n");
	xavp_destroy_list(&list_xavp);
	xavp_destroy_list(&item_xavp);

	return -1;
}

int pv_list_get_value(struct sip_msg *msg, pv_param_t *param,
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
		case 't':
			if(snprintf(_pv_xavp_buf, 128, "<<tuple:%p>>", avp->val.v.xavp)<0)
				return pv_get_null(msg, param, res);
			break;
		case 'l':
		default:
			if(snprintf(_pv_xavp_buf, 128, "<<list:%p>>", avp->val.v.xavp)<0)
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

int pv_list_get(struct sip_msg* msg,  pv_param_t* param, pv_value_t* res)
{
	str name;
	pv_spec_t *nsp = NULL;
	pv_name_t *pvn;
	pv_index_t *pvi;
	sr_xavp_t *lists_root;
	sr_xavp_t *list;
	sr_xavp_t *lh;
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

	lists_root = xavp_get_lists();
	if(!lists_root) {
		return pv_get_null(msg,param,res);
	}

	list = xavp_get(&name,lists_root->val.v.xavp);
	if (!list) {
		return pv_get_null(msg,param,res);
	}

	lh = list->val.v.xavp;

	switch (xbuff_is_attr_set(attr)) {
	case XBUFF_ATTR_TYPE:
		if (xbuff_is_no_index(attr)) {
			return pv_get_strval(msg,param,res,&xbuff_types[XBUFF_TYPE_LIST]);
		} else {
			xavp = xavp_get_nth(&lh->val.v.xavp,idx,NULL);
			return pv_xbuff_get_type(msg,param,res,xavp);
		}
		break;
	case XBUFF_ATTR_LENGTH:
		xavp = xbuff_is_no_index(attr) ? lh : xavp_get_nth(&lh->val.v.xavp,idx,NULL);

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

		xavp = xbuff_is_no_index(attr) ? lh : xavp_get_nth(&lh->val.v.xavp,idx,NULL);
		if (!xavp || xavp_encode(&xbuff,xavp,1)) {
			ei_x_free(&xbuff);
			return -1;
		}
		i = 1;
		if (ei_s_print_term(&list_fmt_buff,xbuff.buff,&i)<0) {
			LM_ERR("BUG: xbuff[index] doesn't contain a valid term!\n");
			ei_x_free(&xbuff);
			return -1;
		}
		i = pv_get_strzval(msg,param,res,list_fmt_buff);
		ei_x_free(&xbuff);
		return i;
	}

	/* get whole list */
	if ((idxf == PV_IDX_ALL) || xbuff_is_no_index(attr)) {
		return pv_list_get_value(msg,param,res,list);
	}

	if (lh->val.type == SR_XTYPE_NULL) {
		return pv_get_null(msg,param,res);
	}

	/* get by idx */
	xavp = xavp_get_nth(&lh->val.v.xavp,idx,NULL);
	if (!xavp) {
		return pv_get_null(msg,param,res);
	}

	return pv_list_get_value(msg,param,res,xavp);
}

/*
 * free format buffer for list
 */
void free_list_fmt_buff() {
	if (list_fmt_buff) {
		free(list_fmt_buff);
	}
	list_fmt_buff = 0;
}
