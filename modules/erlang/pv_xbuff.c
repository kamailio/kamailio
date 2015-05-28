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
#include "../../hashes.h"

#include "pv_xbuff.h"

str xbuff_attributes[] = {
		STR_STATIC_INIT("type"),
		STR_STATIC_INIT("format"),
		STR_STATIC_INIT("length"),
		STR_NULL
};

str xbuff_types[] = {
		STR_STATIC_INIT("atom"),
		STR_STATIC_INIT("integer"),
		STR_STATIC_INIT("string"),
		STR_STATIC_INIT("tuple"),
		STR_STATIC_INIT("list"),
		STR_STATIC_INIT("pid"),
		STR_STATIC_INIT("ref"),
		STR_NULL
};

/**
 * atom,tuple,xbuff & list regex
 */
regex_t xbuff_type_re = {0};

static str xbuff_list=str_init("[xbuffs]");

static int counter;
static char *xbuff_fmt_buff = NULL;

void xbuff_data_free(void *p, sr_xavp_sfree_f sfree);

sr_xavp_t *xavp_get_xbuffs()
{
	sr_xavp_t *list;
	list = xavp_get(&xbuff_list,NULL);

	if(!list) counter = 0;

	return list;
}

sr_xavp_t *pv_xbuff_get_xbuff(str *name)
{
	return xavp_get_child(&xbuff_list,name);
}

sr_xavp_t *xbuff_new(str *name)
{
	sr_xavp_t *xbuffs_root;
	sr_xavp_t *xbuff;
	sr_xval_t xbuff_val;

	memset((void*)&xbuff_val,0,sizeof(sr_xval_t));
	xbuff_val.type = SR_XTYPE_NULL;

	xbuffs_root = xavp_get_xbuffs();

	if(!xbuffs_root)
	{
		xbuff = xavp_add_xavp_value(&xbuff_list,name,&xbuff_val,xavp_get_crt_list());
	}

	xbuff=xavp_get_child(&xbuff_list, name);

	if (!xbuff) {

		xbuff_val.type = SR_XTYPE_NULL;
		xbuff_val.v.xavp = NULL;

		xbuff=xavp_add_value(name,&xbuff_val,&xbuffs_root->val.v.xavp);
	}

	return xbuff;
}

/***
 * if compile success returns 0
 */
int compile_xbuff_re()
{
	char *pattern = "^<<\\(tuple\\|list\\|atom\\|pid\\|ref\\):\\(0x[[:xdigit:]]\\+\\)>>$";
	size_t bfsz = 128;
	char errbuff[128];
	int e;

	if((e=regcomp(&xbuff_type_re,pattern,0))) {
		regerror(e,&xbuff_type_re,errbuff,bfsz);
		LM_ERR("failed to compile pattern '%s' error: %s\n",pattern,errbuff);
		return -1;
	}

	return 0;
}

int xbuff_match_type_re(str *s, xbuff_type_t *type, sr_xavp_t **addr)
{
	size_t nmatch = 3;
	regmatch_t matches[3];
	int e;
	size_t bfsz = 128;
	char errbuff[128];
	str tname;
	str a;
	xbuff_type_t t;

	matches[0].rm_so = 0;
	matches[0].rm_eo = s->len;

	e = regexec(&xbuff_type_re,s->s,nmatch,matches,REG_STARTEND);

	if (e == 0) {

		tname.s   = s->s + matches[1].rm_so;
		tname.len = matches[1].rm_eo - matches[1].rm_so;

		a.s   = s->s + matches[2].rm_so;
		a.len = matches[2].rm_eo - matches[1].rm_so;

		if (STR_EQ(tname,xbuff_types[XBUFF_TYPE_ATOM])) {
			t = XBUFF_TYPE_ATOM;
		} else if (STR_EQ(tname,xbuff_types[XBUFF_TYPE_LIST])) {
			t = XBUFF_TYPE_LIST;
		} else if (STR_EQ(tname,xbuff_types[XBUFF_TYPE_TUPLE])) {
			t = XBUFF_TYPE_TUPLE;
		} else if (STR_EQ(tname,xbuff_types[XBUFF_TYPE_PID])) {
			t = XBUFF_TYPE_PID;
		} else if (STR_EQ(tname,xbuff_types[XBUFF_TYPE_REF])) {
			t = XBUFF_TYPE_REF;
		} else {
			LM_ERR("BUG: unknown xbuff type");
			return -1;
		}

		if(type) *type = t;

		if (addr)
			sscanf(a.s,"%lx>>",(long unsigned int *)addr);

		return 0;

	} else if (e != REG_NOMATCH) {
		regerror(e,&xbuff_type_re,errbuff,bfsz);
		LM_ERR("regexec error: %s\n",errbuff);
	}

	return -1;
}

int is_pv_xbuff_valid_char(char c)
{
	if((c>='0' && c<='9') || (c>='a' && c<='z') || (c>='A' && c<='Z')
			|| (c=='_'))
		return 1;
	return 0;
}

int pv_xbuff_parse_name(pv_spec_t *sp, str *in)
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

			if (pv_parse_index(sp,&idx))
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
				xbuff_set_attr_flag(sp->pvp.pvi.type,XBUFF_ATTR_LENGTH);
			} else {
				LM_ERR("unknown attribute %.*s\n",STR_FMT(&attr));
				goto error;
			}

			if (sp->pvp.pvi.type & PV_IDX_ALL) {
				LM_ERR("index [*] (all) isn't compatible with attribute %.*s\n",STR_FMT(&attr));
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


int pv_xbuff_new_xavp(sr_xavp_t **new, pv_value_t *pval, int *counter, char prefix)
{
	char s[101];
	str name;
	sr_xavp_t *xavp = NULL;
	sr_xavp_t *cxavp = NULL;
	sr_xval_t nval;
	xbuff_type_t type;

	if (!new) return -1;

	memset((void*)&nval,0,sizeof(sr_xval_t));

	if (pval->flags&PV_VAL_NULL) {
		nval.type = SR_XTYPE_NULL;
		s[0] = prefix ? prefix : 'n';
	} else if (pval->flags&PV_VAL_INT) {
		nval.type = SR_XTYPE_INT;
		nval.v.i = pval->ri;
		s[0] = prefix ? prefix : 'i';
	} else if (pval->flags&PV_VAL_STR) {
		/* check what it is */
		if (xbuff_match_type_re(&pval->rs,&type,&xavp)) {
			nval.type = SR_XTYPE_STR;
			nval.v.s = pval->rs;
			s[0] = prefix ? prefix : 's';
		} else {
			switch (type) {
			case XBUFF_TYPE_ATOM:
				s[0] = 'a';
				nval = xavp->val;
				break;
			case XBUFF_TYPE_LIST:
				s[0] = 'l';
				/* copy tree */
				cxavp = xbuff_copy_xavp(xavp);

				if (!cxavp)
					return -1;

				nval = cxavp->val;

				/* free overhead */
				cxavp->next = NULL;
				cxavp->val.v.xavp = NULL;
				xavp_destroy_list(&cxavp);

				break;
			case XBUFF_TYPE_TUPLE:
				s[0] = 't';

				/* copy tree */
				cxavp = xbuff_copy_xavp(xavp);

				if (!cxavp)
					return -1;

				nval = cxavp->val;

				/* free overhead */
				cxavp->next = NULL;
				cxavp->val.v.xavp = NULL;
				xavp_destroy_list(&cxavp);
				break;
			case XBUFF_TYPE_PID:
				s[0] = 'p';
				nval.type = SR_XTYPE_DATA;
				nval.v.data = (sr_data_t*)shm_malloc(sizeof(sr_data_t)+sizeof(erlang_pid));
				if (!nval.v.data) {
					LM_ERR("not enough shared memory\n");
					return -1;
				}
				memcpy((void*)nval.v.data,(void*)xavp,sizeof(sr_data_t)+sizeof(erlang_pid));
				break;
			case XBUFF_TYPE_REF:
				s[0] = 'r';
				nval.type = SR_XTYPE_DATA;
				nval.v.data = (sr_data_t*)shm_malloc(sizeof(sr_data_t)+sizeof(erlang_ref));
				if (!nval.v.data) {
					LM_ERR("not enough shared memory\n");
					return -1;
				}
				memcpy((void*)nval.v.data,(void*)xavp,sizeof(sr_data_t)+sizeof(erlang_ref));
				break;
			case XBUFF_TYPE_INT:
			case XBUFF_TYPE_STR:
			default:
				LM_ERR("BUG: unexpected XBUFF type!\n");
				return -1;
			}
		}
	}

	name.s = s;
	name.len = snprintf(s+1,99,"%d",(*counter)++) + 1;

	cxavp = xavp_new_value(&name,&nval);

	if (!cxavp) {
		return -1;
	}

	*new = cxavp;

	return 0;
}

int pv_xbuff_get_type(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, sr_xavp_t *avp)
{
	if (!avp) return pv_get_null(msg,param,res);

	switch(avp->name.s[0]){
	case 'n':
		return pv_get_null(msg,param,res);
		break;
	case 'i':
		return pv_get_strval(msg,param,res,&xbuff_types[XBUFF_TYPE_INT]);
		break;
	case 'l':
		return pv_get_strval(msg,param,res,&xbuff_types[XBUFF_TYPE_LIST]);
		break;
	case 'a':
		return pv_get_strval(msg, param, res, &xbuff_types[XBUFF_TYPE_ATOM]);
		break;
	case 's':
		return pv_get_strval(msg, param, res, &xbuff_types[XBUFF_TYPE_STR]);
		break;
	case 't':
		return pv_get_strval(msg, param, res, &xbuff_types[XBUFF_TYPE_TUPLE]);
		break;
	case 'p':
		return pv_get_strval(msg, param, res, &xbuff_types[XBUFF_TYPE_PID]);
		break;
	case 'r':
		return pv_get_strval(msg, param, res, &xbuff_types[XBUFF_TYPE_REF]);
		break;
	}

	return pv_get_null(msg, param, res);
}

int pv_xbuff_set(struct sip_msg* msg,  pv_param_t* param, int op, pv_value_t* val)
{
	str name;
	sr_xavp_t *xbuffs_root;
	sr_xavp_t *xbuff;
	sr_xavp_t *new,*old,*prv=NULL;
	sr_xavp_t *xbuff_xavp;
	sr_xval_t xbuff_val;
	pv_param_t p;
	int idx = 0;
	int idxf = 0;
	int attr = 0;

	if (param->pvn.type != PV_NAME_INTSTR || !(param->pvn.u.isname.type & AVP_NAME_STR)) {
		LM_ERR("invalid variable name type\n");
		return -1;
	}

	/* xbuff name */
	name = param->pvn.u.isname.name.s;

	xbuffs_root = xavp_get_xbuffs();

	if(!xbuffs_root) {

		memset((void*)&xbuff_val,0,sizeof(sr_xval_t));

		xbuff_val.type = SR_XTYPE_NULL;

		xbuff = xavp_add_xavp_value(&xbuff_list,&name,&xbuff_val,xavp_get_crt_list());

		if (!xbuff)
			goto err;
	}

	if(pv_xbuff_new_xavp(&xbuff_xavp,val,&counter,0)) {
		LM_ERR("failed to create new value\n");
		return -1;
	}

	xbuff=xavp_get_child(&xbuff_list, &name);

	if (!xbuff) {

		xbuff_val.type = SR_XTYPE_NULL;
		xbuff_val.v.xavp = NULL;

		xbuff=xavp_add_value(&name,&xbuff_val,&xbuffs_root->val.v.xavp);

		if (!xbuff)
			goto err;
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

	/* check container type */

	if (xbuff->val.v.xavp == NULL) {
		xbuff->val.type = SR_XTYPE_XAVP;
		xbuff->val.v.xavp = xbuff_xavp;
		return 0;
	}

	switch (xbuff->val.v.xavp->name.s[0]) {
	case 'l': /* list  */
	case 't': /* tuple */

		/* prepend on list when no index */
		if (xbuff_is_no_index(attr)) {
			return xavp_add(xbuff_xavp,&xbuff->val.v.xavp);
		}

		/* reset list given value */
		if (idxf == PV_IDX_ALL) {
			xavp_destroy_list(&xbuff->val.v.xavp);
			if (xbuff_xavp->val.type == SR_XTYPE_NULL) {
				/* create empty list/tuple */
				xavp_destroy_list(&xbuff_xavp);
				return 0;
			} else {
				return xavp_add(xbuff_xavp,&xbuff->val.v.xavp);
			}
		}

		/* set by index */
		old = xavp_get_nth(&xbuff->val.v.xavp,idx,&prv);
		new = xbuff_xavp;

		if (old) {
			new->next = old->next;
			if (prv) {
				prv->next = new;
			} else {
				new->next = old->next;
				xbuff->val.v.xavp = new;
			}
			old->next = NULL;
			xavp_destroy_list(&old);
		} else {
			if (prv) {
				prv->next = new;
			} else {
				xbuff->val.v.xavp = new;
			}
		}
		break;
	case 'n': /* null (empty) */
	case 's': /* string  */
	case 'a': /* atom    */
	case 'i': /* integer */
	default:  /* default */
		xavp_destroy_list(&xbuff->val.v.xavp);
		xbuff->val.v.xavp = xbuff_xavp;
		break;
	}

	return 0;

err:
	LM_ERR("failed to add value into xbuff\n");
	xavp_destroy_list(&xbuff_xavp);

	return -1;
}

int pv_xbuff_get_value(struct sip_msg *msg, pv_param_t *param,
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
	case SR_XTYPE_DATA:
		switch (avp->name.s[0]) {
		case 'p':
			if(snprintf(_pv_xavp_buf, 128, "<<pid:%p>>", avp->val.v.data)<0)
				return pv_get_null(msg, param, res);
			break;
		case 'r':
			if(snprintf(_pv_xavp_buf, 128, "<<ref:%p>>", avp->val.v.data)<0)
				return pv_get_null(msg, param, res);
			break;
		default:
			if(snprintf(_pv_xavp_buf, 128, "<<binary:%p>>", avp->val.v.data)<0)
				return pv_get_null(msg, param, res);
		}
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
//			break;
//		default:
//			LM_ERR("unexpected type!\n");
//			return pv_get_null(msg, param, res);
		}
		break;
	default:
		return pv_get_null(msg, param, res);
	}
	s.s = _pv_xavp_buf;
	s.len = strlen(_pv_xavp_buf);
	return pv_get_strval(msg, param, res, &s);
}

int pv_xbuff_get(struct sip_msg* msg,  pv_param_t* param, pv_value_t* res)
{
	str name;
	pv_spec_t *nsp = NULL;
	pv_name_t *pvn;
	pv_index_t *pvi;
	sr_xavp_t *xbuffs_root;
	sr_xavp_t *xbuff;
	sr_xavp_t *xavp;
	pv_param_t p;
	int idx=0;
	int idxf=0;
	int attr;
	int i,count;

	ei_x_buff x_buff;

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

	xbuffs_root = xavp_get_xbuffs();
	if(!xbuffs_root) {
		return pv_get_null(msg,param,res);
	}

	xbuff = xavp_get(&name,xbuffs_root->val.v.xavp);
	if (!xbuff) {
		return pv_get_null(msg,param,res);
	}

	xavp = xbuff->val.v.xavp;

	switch (xbuff_is_attr_set(attr)) {
	case XBUFF_ATTR_TYPE:
		if (xbuff_is_no_index(attr)) {
			return pv_xbuff_get_type(msg,param,res,xavp);
		} else {
			if(xavp && (xavp->name.s[0]=='l'||xavp->name.s[0]=='t')) {
				xavp=xavp->val.v.xavp;
			}
			xavp = xavp_get_nth(&xavp,idx,NULL);
			return pv_xbuff_get_type(msg,param,res,xavp);
		}
		break;
	case XBUFF_ATTR_LENGTH:
		if (xbuff_is_no_index(attr)) {
			xavp = xbuff->val.v.xavp;
		} else {
			if(xavp && (xavp->name.s[0]=='l'||xavp->name.s[0]=='t')) {
				xavp=xavp->val.v.xavp;
			}
			xavp = xavp_get_nth(&xavp,idx,NULL);
		}
		count = xavp ? xavp_get_count(xavp->val.v.xavp) : 0;
		return pv_get_uintval(msg,param,res,(unsigned int)count);
		break;
	case XBUFF_ATTR_FORMAT:
		if (xbuff_is_no_index(attr)) {
			xavp = xbuff->val.v.xavp;
		} else {
			if(xavp && (xavp->name.s[0]=='l'||xavp->name.s[0]=='t')) {
				xavp=xavp->val.v.xavp;
			}
			xavp = xavp_get_nth(&xavp,idx,NULL);
		}

		/*
		 * Prints a term, in clear text, to the PV value pointed to by res.
		 * It tries to resemble the term printing in the erlang shell.
		 */
		ei_x_new_with_version(&x_buff);
		if (!xavp || xavp_encode(&x_buff,xavp,1)) {
			ei_x_free(&x_buff);
			return pv_get_null(msg,param,res);
		}

		i = 1;
		if (ei_s_print_term(&xbuff_fmt_buff,x_buff.buff,&i)<0) {
			LM_ERR("BUG: xbuff doesn't contain a valid term!\n");
			ei_x_free(&x_buff);
			return -1;
		}
		i = pv_get_strzval(msg,param,res,xbuff_fmt_buff);
		ei_x_free(&x_buff);
		return i;
	}

	if (!xavp) {
		return pv_get_null(msg,param,res);
	}

	/* get whole xbuff */
	if (idxf == PV_IDX_ALL) {
		return pv_xbuff_get_value(msg,param,res,xavp);
	}

	/* get by idx */
	if (xavp->name.s[0]=='l'||xavp->name.s[0]=='t') {
		xavp = xavp->val.v.xavp;
	}

	xavp = xavp_get_nth(&xavp,idx,NULL);
	if (!xavp) {
		return pv_get_null(msg,param,res);
	}

	return pv_xbuff_get_value(msg,param,res,xavp);
}

/**
 * Recursive copy XAVPs, preserve order
 */
sr_xavp_t *xbuff_copy_xavp(sr_xavp_t *xavp)
{
	sr_xavp_t *new = NULL;
	sr_xavp_t *cp  = NULL;

	if (!xavp) return NULL;

	while (xavp) {
		if (new) {
			new->next = xavp_new_value(&xavp->name,&xavp->val);
			new = new->next;
		} else {
			new = xavp_new_value(&xavp->name,&xavp->val);
		}

		if (!new) {
			LM_ERR("not enough memory\n");
			return cp;
		}

		if (!cp) cp = new;

		if (xavp->val.type == SR_XTYPE_XAVP)
			new->val.v.xavp = xbuff_copy_xavp(xavp->val.v.xavp);

		xavp = xavp->next;
	}

	return cp;
}

/**
 * XAVP extension
 */

/* copy from xavp.c (not exported from XAVP) */
sr_xavp_t *xavp_new_value(str *name, sr_xval_t *val)
{
	sr_xavp_t *avp;
	int size;
	unsigned int id;

	if(name==NULL || name->s==NULL || val==NULL)
		return NULL;
	id = get_hash1_raw(name->s, name->len);

	size = sizeof(sr_xavp_t) + name->len + 1;
	if(val->type == SR_XTYPE_STR)
		size += val->v.s.len + 1;
	avp = (sr_xavp_t*)shm_malloc(size);
	if(avp==NULL)
		return NULL;
	memset(avp, 0, size);
	avp->id = id;
	avp->name.s = (char*)avp + sizeof(sr_xavp_t);
	memcpy(avp->name.s, name->s, name->len);
	avp->name.s[name->len] = '\0';
	avp->name.len = name->len;
	memcpy(&avp->val, val, sizeof(sr_xval_t));
	if(val->type == SR_XTYPE_STR)
	{
		avp->val.v.s.s = avp->name.s + avp->name.len + 1;
		memcpy(avp->val.v.s.s, val->v.s.s, val->v.s.len);
		avp->val.v.s.s[val->v.s.len] = '\0';
		avp->val.v.s.len = val->v.s.len;
	}

	return avp;
}

sr_xavp_t *xavp_get_nth(sr_xavp_t **list, int idx, sr_xavp_t **prv)
{
	sr_xavp_t *avp;
	int n = 0;

	if (list && *list)
		avp = *list;
	else
		return NULL;

	while (avp) {

		if (idx == n)
			return avp;
		n++;

		if (prv)
			*prv = avp;

		avp = avp->next;
	}

	return NULL;
}

/**
 * Encode XAVPs into ei_x_buff
 */
int xavp_encode(ei_x_buff *xbuff, sr_xavp_t *xavp,int level)
{
	int n;

	while(xavp) {
		switch (xavp->name.s[0]) {
		case 'a':
			ei_x_encode_atom_len(xbuff,xavp->val.v.s.s,xavp->val.v.s.len);
			break;
		case 's':
			ei_x_encode_string_len(xbuff,xavp->val.v.s.s,xavp->val.v.s.len);
			break;
		case 'i':
			ei_x_encode_long(xbuff,xavp->val.v.i);
			break;
		case 't':
			n = xavp_get_count(xavp->val.v.xavp);
			ei_x_encode_tuple_header(xbuff,n);
			if (xavp_encode(xbuff,xavp->val.v.xavp,level+1)) return -1;
			break;
		case 'l':
			n = xavp_get_count(xavp->val.v.xavp);
			ei_x_encode_list_header(xbuff, n);
			if (xavp_encode(xbuff, xavp->val.v.xavp, level + 1)) return -1;
			ei_x_encode_empty_list(xbuff);
			break;
		case 'p':
			ei_x_encode_pid(xbuff,xavp->val.v.data->p);
			break;
		case 'r':
			ei_x_encode_ref(xbuff,xavp->val.v.data->p);
			break;
		case 'n':
			ei_x_encode_atom(xbuff,"undefined");
			break;
		default:
			LM_ERR("BUG: unknown type for %.*s\n",STR_FMT(&xavp->name));
			return -1;
		}
		xavp = xavp->next;
	}

	return 0;
}

/**
 * Decode XAVP from ei_x_buff
 */
int xavp_decode(ei_x_buff *xbuff, int *index, sr_xavp_t **xavp,int level)
{
	int i=0;
	int type, size, arity;
	int l;
	char _s[128];
	char _fmt[128];
	str name;
	sr_xval_t val;
	sr_xavp_t **tail;
	sr_xavp_t *new;
	char *pbuf=0;
	erlang_pid *pid;
	erlang_ref *ref;
	erlang_fun fun;
	double d;
	char *p = NULL;
	sr_data_t *data;

	name.s = _s;

	if (!xavp || !xbuff) return -1;

	if (ei_get_type(xbuff->buff,index,&type,&size)) {
		LM_ERR("failed to get type\n");
		return -1;
	}


	switch (type) {
	case ERL_ATOM_EXT:
#ifdef ERL_SMALL_ATOM_EXT
	case ERL_SMALL_ATOM_EXT:
#endif
		name.len = snprintf(_s,sizeof(_s),"a%d",counter++);
		pbuf = (char*)pkg_realloc(pbuf,size+1);

		if (!pbuf) {
			LM_ERR("not enough memory!\n");
			return -1;
		}

		ei_decode_atom(xbuff->buff,index,pbuf);

		val.type = SR_XTYPE_STR;
		val.v.s.s = pbuf;
		val.v.s.len = size;

		*xavp = xavp_new_value(&name,&val);
		if (!*xavp) {
			LM_ERR("failed to create new xavp!\n");
			goto err;
		}

		break;
	case ERL_LIST_EXT:
	case ERL_SMALL_TUPLE_EXT:
	case ERL_LARGE_TUPLE_EXT:

		name.len = snprintf(_s,sizeof(_s),"%c%d", type == ERL_LIST_EXT ? 'l' : 't', counter++);

		val.v.xavp = NULL;

		if (type == ERL_LIST_EXT) {
			ei_decode_list_header(xbuff->buff,index,&arity);
		} else {
			ei_decode_tuple_header(xbuff->buff,index,&arity);
		}

		if (arity == 0) {
			val.type = SR_XTYPE_NULL;
		} else {
			val.type = SR_XTYPE_XAVP;
		}

		*xavp = xavp_new_value(&name,&val);
		if (!*xavp) {
			LM_ERR("failed to create new xavp!\n");
			goto err;
		}

		tail = &(*xavp)->val.v.xavp;

		for(l=0;l<arity;l++) {

			new = NULL;

			if (xavp_decode(xbuff,index,&new,level+1)) {
				LM_ERR("failed to decode %.*s\n",STR_FMT(&name));
				return -1;
			}

			if (!new) {
				LM_ERR("failed to create new xavp!\n");
				goto err;
			}

			*tail = new;
			tail = &new->next;
		}

		break;
	case ERL_STRING_EXT:
		name.len = snprintf(_s,sizeof(_s),"s%d",counter++);

		pbuf = (char*)pkg_realloc(pbuf,size+1);

		if (!pbuf) {
			LM_ERR("not enough memory!\n");
			return -1;
		}

		ei_decode_string(xbuff->buff,index,pbuf);

		val.type = SR_XTYPE_STR;
		val.v.s.s = pbuf;
		val.v.s.len = size;

		*xavp = xavp_new_value(&name,&val);
		if (!*xavp) {
			LM_ERR("failed to create new xavp!\n");
			goto err;
		}
		break;
	case ERL_SMALL_INTEGER_EXT:
		name.len = snprintf(_s,sizeof(_s),"i%d",counter++);

		ei_decode_long(xbuff->buff,index,&val.v.l);
		val.type = SR_XTYPE_INT;

		*xavp = xavp_new_value(&name,&val);
		if (!*xavp) {
			LM_ERR("failed to create new xavp!\n");
			goto err;
		}
		break;
	case ERL_SMALL_BIG_EXT:
	case ERL_INTEGER_EXT:
		name.len = snprintf(_s,sizeof(_s),"i%d",counter++);

		if (size > sizeof(long)) {
			ei_decode_longlong(xbuff->buff,index,&val.v.ll);
			val.type = SR_XTYPE_LLONG;
		} else {
			ei_decode_long(xbuff->buff,index,&val.v.l);
			val.type = SR_XTYPE_LONG;
		}

		*xavp = xavp_new_value(&name,&val);
		if (!*xavp) {
			LM_ERR("failed to create new xavp!\n");
			goto err;
		}
		break;
	case ERL_FLOAT_EXT:
	case NEW_FLOAT_EXT:
		name.len = snprintf(_s,sizeof(_s),"s%d",counter++);

		ei_decode_double(xbuff->buff,index,&d);
		val.v.s.s = _fmt;
		val.v.s.len = snprintf(_fmt,sizeof(_fmt),"%g",d);

		val.type = SR_XTYPE_STR;

		*xavp = xavp_new_value(&name,&val);
		if (!*xavp) {
			LM_ERR("failed to create new xavp!\n");
			goto err;
		}
		break;

	case ERL_PID_EXT:
		name.len = snprintf(_s,sizeof(_s),"p%d",counter++);

		data = (sr_data_t*)shm_malloc(sizeof(sr_data_t)+sizeof(erlang_pid));
		if (!data) {
			LM_ERR("not enough shared memory\n");
			goto err;
		}

		memset((void*)data,0,sizeof(sr_data_t)+sizeof(erlang_pid));

		data->p = pid = (void*)data+sizeof(sr_data_t);
		data->pfree = xbuff_data_free;

		if (ei_decode_pid(xbuff->buff,index,pid)<0) {
			LM_ERR("failed to decode pid\n");
			shm_free(data);
			goto err;
		}

		val.type = SR_XTYPE_DATA;
		val.v.data = data;

		*xavp = xavp_new_value(&name,&val);
		if (!*xavp) {
			LM_ERR("failed to create new xavp!\n");
			shm_free(data);
			goto err;
		}

		break;
	case ERL_REFERENCE_EXT:
	case ERL_NEW_REFERENCE_EXT:
		name.len = snprintf(_s,sizeof(_s),"r%d",counter++);

		data = (sr_data_t*)shm_malloc(sizeof(sr_data_t)+sizeof(erlang_ref));
		if (!data) {
			LM_ERR("not enough shared memory\n");
			goto err;
		}

		memset((void*)data,0,sizeof(sr_data_t)+sizeof(erlang_ref));

		data->p = ref = (void*)data+sizeof(sr_data_t);
		data->pfree = xbuff_data_free;

		if (ei_decode_ref(xbuff->buff,index,ref)<0) {
			LM_ERR("failed to decode pid\n");
			shm_free(data);
			goto err;
		}

		val.type = SR_XTYPE_DATA;
		val.v.data = data;

		*xavp = xavp_new_value(&name,&val);
		if (!*xavp) {
			LM_ERR("failed to create new xavp!\n");
			shm_free(data);
			goto err;
		}

		break;
	case ERL_FUN_EXT:
		name.len = snprintf(_s,sizeof(_s),"s%d",counter++);
		i = *index;

		ei_decode_fun(xbuff->buff,index,&fun);

		if (ei_s_print_term(&p,xbuff->buff,&i)<0) {
			LM_ERR("failed to decode fun\n");
			goto err;
		}
		val.type = SR_XTYPE_STR;
		val.v.s.s = p;
		val.v.s.len = strlen(p);

		*xavp = xavp_new_value(&name,&val);
		if (!*xavp) {
			LM_ERR("failed to create new xavp!\n");
			goto err;
		}
		break;
	default:
		LM_ERR("unknown type %c(%d)\n",(char)type,type);
	}

	pkg_free(pbuf);
	free(p);
	return 0;

err:
	pkg_free(pbuf);
	free(p);
	return -1;
}

int xavp_get_count(sr_xavp_t *list)
{
	int count = 0;

	while(list) {
		list = list->next;
		count++;
	}
	return count;
}

/*
 * free format buffer for list
 */
void free_xbuff_fmt_buff() {
	if (xbuff_fmt_buff) {
		free(xbuff_fmt_buff);
	}
	xbuff_fmt_buff = 0;
}

void xbuff_destroy_all()
{
	sr_xavp_t *list;
	list = xavp_get_xbuffs();
	if (list) xavp_destroy_list(&list);
}

/* does nothing but must be executed in xavp_free[_unsafe] */
void xbuff_data_free(void *p, sr_xavp_sfree_f sfree) {
}
