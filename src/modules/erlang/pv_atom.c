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

#include "../../str.h"
#include "../../xavp.h"
#include "../../pvapi.h"

#include "pv_atom.h"
#include "pv_xbuff.h"

#include <ei.h>

static str atom_list=str_init("[atoms]");
static char *atom_fmt_buff = NULL;
static int counter;

int pv_atom_parse_name(pv_spec_t *sp, str *in)
{
	char *p;
	str idx;
	str name;
	str attr;
	int l;

	if (in->s == NULL || in->len <= 0)
		return -1;

	p = in->s;

	name.s = p;

	while (is_in_str(p, in)) {
		if (*p == '[' || *p== '=')
			break;
		if (!is_pv_xbuff_valid_char(*p)) {
			l = p-in->s;
			LM_ERR("invalid character in var name %.*s at %d\n",STR_FMT(in),l);
			goto error;
		}
		p++;
	}

	/* from in->s to p */
	name.len = p - in->s;

	if (pv_parse_avp_name(sp,&name))
		goto error;

	if (is_in_str(p,in) && *p =='[')
	{
		idx.s=++p;

		while (is_in_str(p,in)) {
			if (*p == ']' || *p == '=')
				break;
			p++;
		}

		if (is_in_str(p,in) && *p==']') {
			idx.len = p - idx.s;

			LM_ERR("index isn't allowed for this variable\n");
			goto error;
		}
		p++;
	} else {
		xbuff_set_attr_flag(sp->pvp.pvi.type,XBUFF_NO_IDX);
	}

	if (is_in_str(p,in) && *p =='=')
	{
		p++;

		if (!is_in_str(p,in) || *p!='>') {
			l = p-in->s;
			LM_ERR("invalid operator (expected =>) for accessing attribute in token %.*s at position %d\n",STR_FMT(in),l);
			goto error;
		}

		attr.s = ++p;

		while (is_in_str(p,in)) {
			if (!is_pv_xbuff_valid_char(*p)) {
				l = p-in->s;
				LM_ERR("invalid character in attribute name in token %.*s at %d\n",STR_FMT(in),l);
				goto error;
			}
			p++;
		}

		attr.len = p - attr.s;

		if (attr.len > 0 ) {

			if (STR_EQ(attr,xbuff_attr_name(XBUFF_ATTR_TYPE))) {
				xbuff_set_attr_flag(sp->pvp.pvi.type,XBUFF_ATTR_TYPE);
			} else if (STR_EQ(attr,xbuff_attr_name(XBUFF_ATTR_FORMAT))) {
				xbuff_set_attr_flag(sp->pvp.pvi.type,XBUFF_ATTR_FORMAT);
			} else if (STR_EQ(attr,xbuff_attr_name(XBUFF_ATTR_LENGTH))) {
				LM_ERR("attribute isn't supported for this variable\n");
				goto error;
			} else {
				LM_ERR("unknown attribute %.*s\n",STR_FMT(&attr));
				goto error;
			}
		}
	}

	if (p < in->s + in->len) {
		l = p-in->s;
		LM_ERR("unexpected token in %.*s at %d\n",STR_FMT(in),l);
		goto error;
	}

	return 0;

error:

	return -1;
}

sr_xavp_t *xavp_get_atoms()
{
	sr_xavp_t *list;
	list = xavp_get(&atom_list,NULL);

	if(!list) counter = 0;

	return list;
}

sr_xavp_t *pv_atom_get_atom(str *name)
{
	return xavp_get_child(&atom_list, name);
}

int pv_atom_set(struct sip_msg* msg,  pv_param_t* param, int op, pv_value_t* val)
{
	str name;
	sr_xavp_t *atoms_root;
	sr_xavp_t *atom;
	sr_xavp_t *new,*old=NULL;
	sr_xavp_t *atom_xavp;
	sr_xval_t atom_val;

	if (param->pvn.type != PV_NAME_INTSTR || !(param->pvn.u.isname.type & AVP_NAME_STR)) {
		LM_ERR("invalid variable name type\n");
		return -1;
	}

	if(pv_xbuff_new_xavp(&atom_xavp,val,&counter,'a')) {
		LM_ERR("failed to create new value\n");
		return -1;
	}

	/* atom name */
	name = param->pvn.u.isname.name.s;

	memset((void*)&atom_val,0,sizeof(sr_xval_t));

	atoms_root = xavp_get_atoms();

	if(!atoms_root) {

		atom_val.type = SR_XTYPE_XAVP;
		atom_val.v.xavp = atom_xavp;
		atom = xavp_add_xavp_value(&atom_list,&name,&atom_val,xavp_get_crt_list());

		if (!atom)
			goto err;

		return 0;
	}

	atom = xavp_get_child(&atom_list, &name);

	if (!atom) {

		atom_val.type = SR_XTYPE_XAVP;
		atom_val.v.xavp = atom_xavp;

		new = xavp_add_value(&name,&atom_val,&atoms_root->val.v.xavp);

		if (!new)
			goto err;

		return 0;
	}

	old = atom->val.v.xavp;
	new = atom_xavp;

	if (old) {
		xavp_destroy_list(&old);
	}

	atom->val.v.xavp = new;

	return 0;

err:
	LM_ERR("failed to set atom value\n");
	xavp_destroy_list(&atom_xavp);

	return -1;
}

int pv_atom_get_value(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, sr_xavp_t *avp)
{
	static char _pv_xavp_buf[128];
	str s;

	if (!avp) return pv_get_null(msg,param,res);

	switch(avp->val.type) {
	case SR_XTYPE_NULL:
		return pv_get_null(msg, param, res);
		break;
	case SR_XTYPE_STR:
		if(snprintf(_pv_xavp_buf, 128, "<<atom:%p>>", avp)<0)
			return pv_get_null(msg, param, res);
		break;
	case SR_XTYPE_XAVP:
		if(snprintf(_pv_xavp_buf, 128, "<<atom:%p>>", avp->val.v.xavp)<0)
			return pv_get_null(msg, param, res);
		break;
	case SR_XTYPE_DATA:
	case SR_XTYPE_INT:
	case SR_XTYPE_TIME:
	case SR_XTYPE_LONG:
	case SR_XTYPE_LLONG:
		LM_ERR("BUG: unexpected atom value\n");
		return pv_get_null(msg, param, res);
		break;
	default:
		return pv_get_null(msg, param, res);
	}
	s.s = _pv_xavp_buf;
	s.len = strlen(_pv_xavp_buf);
	return pv_get_strval(msg, param, res, &s);
}

int pv_atom_get(struct sip_msg* msg,  pv_param_t* param, pv_value_t* res)
{
	str name;
	sr_xavp_t *atoms_root;
	sr_xavp_t *atom;
	sr_xavp_t *xavp;
	int attr;
	int i;

	ei_x_buff xbuff;

	if(param==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	if (param->pvn.type != PV_NAME_INTSTR || !(param->pvn.u.isname.type & AVP_NAME_STR))
				return -1;

	/* tuple name */
	name = param->pvn.u.isname.name.s;
	/* attributes */
	attr = xbuff_get_attr_flags(param->pvi.type);

	atoms_root = xavp_get_atoms();
	if(!atoms_root) {
		return pv_get_null(msg,param,res);
	}

	atom = xavp_get(&name,atoms_root->val.v.xavp);
	if (!atom) {
		return pv_get_null(msg,param,res);
	}

	xavp = atom->val.v.xavp;

	switch (xbuff_is_attr_set(attr)) {
	case XBUFF_ATTR_TYPE:
		return pv_get_strval(msg,param,res,&xbuff_types[XBUFF_TYPE_ATOM]);
		break;
	case XBUFF_ATTR_LENGTH: /* always 1 */
		return pv_get_uintval(msg,param,res,1);
		break;
	case XBUFF_ATTR_FORMAT:
		/*
		 * Prints a term, in clear text, to the PV value pointed to by res.
		 * It tries to resemble the term printing in the erlang shell.
		 */
		ei_x_new_with_version(&xbuff);
		if (xavp && xavp_encode(&xbuff,xavp,1)) {
			ei_x_free(&xbuff);
			return -1;
		} else {
			ei_x_encode_atom(&xbuff,"undefined");
		}
		i = 1;
		if (ei_s_print_term(&atom_fmt_buff,xbuff.buff,&i)<0) {
			LM_ERR("BUG: xbuff[index] doesn't contain a valid term!\n");
			ei_x_free(&xbuff);
			return -1;
		}
		i = pv_get_strzval(msg,param,res,atom_fmt_buff);
		ei_x_free(&xbuff);
		return i;
	}

	if (!xavp) {
		return pv_get_null(msg,param,res);
	}

	return pv_atom_get_value(msg,param,res,xavp);
}

/*
 * free format buffer for tuple
 */
void free_atom_fmt_buff() {
	if (atom_fmt_buff) {
		free(atom_fmt_buff);
	}
	atom_fmt_buff = 0;
}
