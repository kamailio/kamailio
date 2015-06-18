/*
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com) 
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef WITH_XAVP

#include <stdio.h>

#include "../../dprint.h"
#include "../../xavp.h"
#include "../../pvapi.h"
#include "../../parser/parse_param.h"

#include "pv_xavp.h"

#define PV_FIELD_DELIM ", "
#define PV_FIELD_DELIM_LEN (sizeof(PV_FIELD_DELIM) - 1)

int pv_xavp_get_value(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, sr_xavp_t *avp)
{
	static char _pv_xavp_buf[128];
	str s;

	switch(avp->val.type) {
		case SR_XTYPE_NULL:
			return pv_get_null(msg, param, res);
		break;
		case SR_XTYPE_INT:
			return pv_get_sintval(msg, param, res, avp->val.v.i);
		break;
		case SR_XTYPE_STR:
			return pv_get_strval(msg, param, res, &avp->val.v.s);
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
			if(snprintf(_pv_xavp_buf, 128, "<<xavp:%p>>", avp->val.v.xavp)<0)
				return pv_get_null(msg, param, res);
		break;
		case SR_XTYPE_DATA:
			if(snprintf(_pv_xavp_buf, 128, "<<data:%p>>", avp->val.v.data)<0)
				return pv_get_null(msg, param, res);
		break;
		default:
			return pv_get_null(msg, param, res);
	}
	s.s = _pv_xavp_buf;
	s.len = strlen(_pv_xavp_buf);
	return pv_get_strval(msg, param, res, &s);
}


int pv_get_xavp(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	pv_xavp_name_t *xname=NULL;
	sr_xavp_t *avp=NULL;
	int idxf = 0;
	int idx = 0;
	int count;
	char *p, *p_ini;
	int p_size;

	if(param==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	xname = (pv_xavp_name_t*)param->pvn.u.dname;

	if(xname->index.type==PVT_EXTRA)
	{
		/* get the index */
		if(pv_get_spec_index(msg, &xname->index.pvp, &idx, &idxf)!=0)
		{
			LM_ERR("invalid index\n");
			return -1;
		}
	}
	/* fix the index */
	if(idx<0)
	{
		count = xavp_count(&xname->name, NULL);
		idx = count + idx;
	}
	avp = xavp_get_by_index(&xname->name, idx, NULL);
	if(avp==NULL)
		return pv_get_null(msg, param, res);
	if(xname->next==NULL)
		return pv_xavp_get_value(msg, param, res, avp);

	idx = 0;
	idxf = 0;
	if(xname->next->index.type==PVT_EXTRA)
	{
		/* get the index */
		if(pv_get_spec_index(msg, &xname->next->index.pvp, &idx, &idxf)!=0)
		{
			LM_ERR("invalid index\n");
			return -1;
		}
	}
	/* fix the index */
	if(idx<0)
	{
		count = xavp_count(&xname->next->name, &avp->val.v.xavp);
		idx = count + idx;
	}
	avp = xavp_get_by_index(&xname->next->name, idx, &avp->val.v.xavp);
	if(avp==NULL)
		return pv_get_null(msg, param, res);
	/* get all values of second key */
	if(idxf==PV_IDX_ALL)
	{
		p_ini = pv_get_buffer();
		p = p_ini;
		p_size = pv_get_buffer_size();
		do {
			if(p!=p_ini)
			{
				if(p-p_ini+PV_FIELD_DELIM_LEN+1>p_size)
				{
					LM_ERR("local buffer length exceeded\n");
					return pv_get_null(msg, param, res);
				}
				memcpy(p, PV_FIELD_DELIM, PV_FIELD_DELIM_LEN);
				p += PV_FIELD_DELIM_LEN;
			}
			if(pv_xavp_get_value(msg, param, res, avp)<0)
			{
				LM_ERR("can get value\n");
				return pv_get_null(msg, param, res);
			}
			if(p-p_ini+res->rs.len+1>p_size)
			{
				LM_ERR("local buffer length exceeded!\n");
				return pv_get_null(msg, param, res);
			}
			memcpy(p, res->rs.s, res->rs.len);
			p += res->rs.len;
		} while ((avp=xavp_get_next(avp))!=0);
		res->rs.s = p_ini;
		res->rs.len = p - p_ini;
		return 0;
	}
	return pv_xavp_get_value(msg, param, res, avp);
}

/**
 * $xavp(name1[idx1]=>name2[idx2])
 */
int pv_set_xavp(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	pv_xavp_name_t *xname=NULL;
	sr_xavp_t *avp=NULL;
	sr_xavp_t *list=NULL;
	sr_xval_t xval;
	int idxf = 0;
	int idx = 0;
	int idxf1 = 0;
	int idx1 = 0;
	int count;

	if(param==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	xname = (pv_xavp_name_t*)param->pvn.u.dname;

	if(xname->index.type==PVT_EXTRA)
	{
		/* get the index */
		if(pv_get_spec_index(msg, &xname->index.pvp, &idx, &idxf)!=0)
		{
			LM_ERR("invalid index\n");
			return -1;
		}
	}

	if((val==NULL) || (val->flags&PV_VAL_NULL))
	{
		if(xname->next==NULL)
		{
			if(xname->index.type==PVT_EXTRA) {
				if(idxf==PV_IDX_ALL) {
					xavp_rm_by_name(&xname->name, 1, NULL);
					return 0;
				}
			}
			if(idx==0) {
				xavp_rm_by_name(&xname->name, 0, NULL);
				return 0;
			}
			/* fix the index */
			if(idx<0)
			{
				count = xavp_count(&xname->name, NULL);
				idx = count + idx + 1;
			}
			xavp_rm_by_index(&xname->name, idx, NULL);
			return 0;
		}
		
		if(xname->next->index.type==PVT_EXTRA)
		{
			/* get the index */
			if(pv_get_spec_index(msg,&xname->next->index.pvp,&idx1,&idxf1)!=0)
			{
				LM_ERR("invalid index!\n");
				return -1;
			}
		}

		if(idxf==PV_IDX_ALL) {
			/* iterate */
			avp = xavp_get(&xname->name, NULL);
			while(avp) {
				if(avp->val.type==SR_XTYPE_XAVP) {
					if(xname->next->index.type==PVT_EXTRA) {
						if(idxf1==PV_IDX_ALL) {
							xavp_rm_by_name(&xname->next->name, 1,
									&avp->val.v.xavp);
						} else {
							/* fix the index */
							idx = idx1;
							if(idx<0)
							{
								count = xavp_count(&xname->next->name,
										&avp->val.v.xavp);
								idx = count + idx1 + 1;
							}
							xavp_rm_by_index(&xname->next->name, idx,
									&avp->val.v.xavp);
						}
					} else {
						xavp_rm_by_name(&xname->next->name, 0,
								&avp->val.v.xavp);
					}
				}
				avp = xavp_get_next(avp);
			}
			return 0;
		}

		if(idx==0) {
			avp = xavp_get(&xname->name, NULL);
		} else {
			/* fix the index */
			if(idx<0)
			{
				count = xavp_count(&xname->name, NULL);
				idx = count + idx + 1;
			}
			avp = xavp_get_by_index(&xname->name, idx, NULL);
		}
		if(avp) {
			if(avp->val.type==SR_XTYPE_XAVP) {
				if(xname->next->index.type==PVT_EXTRA) {
					if(idxf1==PV_IDX_ALL) {
						xavp_rm_by_name(&xname->next->name, 1,
								&avp->val.v.xavp);
					} else {
						/* fix the index */
						idx = idx1;
						if(idx<0)
						{
							count = xavp_count(&xname->next->name,
									&avp->val.v.xavp);
							idx = count + idx1 + 1;
						}
						xavp_rm_by_index(&xname->next->name, idx,
								&avp->val.v.xavp);
					}
				} else {
					xavp_rm_by_name(&xname->next->name, 0,
							&avp->val.v.xavp);
				}
			}
		}
		return 0;
	} /* NULL assignment */

	/* build xavp value */
	memset(&xval, 0, sizeof(sr_xval_t));

	if(val->flags&PV_TYPE_INT)
	{
		xval.type = SR_XTYPE_INT;
		xval.v.i = val->ri;
	} else {
		xval.type = SR_XTYPE_STR;
		xval.v.s = val->rs;
	}

	/* where to add */
	if(xname->next==NULL)
	{
		/* xavp with single value */
		if(xname->index.type==PVT_EXTRA) {
			if(idxf==PV_IDX_ALL) {
				/* ignore: should iterate and set same value to all xavps
				 * with same name?!?! */
				return -1;
			}
			/* fix the index */
			if(idx<0)
			{
				count = xavp_count(&xname->name, NULL);
				idx = count + idx + 1;
			}
			/* set the value */
			if(xavp_set_value(&xname->name, idx, &xval, NULL)==NULL)
				return -1;
			return 0;
		}
		/* add new value */
		if(xavp_add_value(&xname->name, &xval, NULL)==NULL)
			return -1;
		return 0;
	}
		
	/* xavp with xavp list value */
	if(xname->next->index.type==PVT_EXTRA)
	{
		/* get the index */
		if(pv_get_spec_index(msg,&xname->next->index.pvp,&idx1,&idxf1)!=0)
		{
			LM_ERR("invalid index!\n");
			return -1;
		}
	}

	if(xname->index.type==PVT_EXTRA)
	{
		/* set the value */
		if(idxf==PV_IDX_ALL) {
			/* ignore: should iterate and set same value to all xavps
			 * with same name?!?! */
			return 0;
		}

		if(idx==0) {
			avp = xavp_get(&xname->name, NULL);
		} else {
			/* fix the index */
			if(idx<0)
			{
				count = xavp_count(&xname->name, NULL);
				idx = count + idx + 1;
			}
			avp = xavp_get_by_index(&xname->name, idx, NULL);
		}
		if(avp==NULL)
			return 0;

		if(avp->val.type!=SR_XTYPE_XAVP)
			return -1;
			
		if(xname->next->index.type==PVT_EXTRA) {
			if(idxf1==PV_IDX_ALL) {
				/* ignore: should iterate and set same value to all xavps
				 * with same name?!?! */
				return 0;
			}
			/* fix the index */
			idx = idx1;
			if(idx<0)
			{
				count = xavp_count(&xname->next->name,
						&avp->val.v.xavp);
				idx = count + idx1 + 1;
			}
			/* set value */
			xavp_set_value(&xname->next->name, idx, &xval, &avp->val.v.xavp);
			return 0;
		}
		/* add new value in sublist */
		if(xavp_add_value(&xname->next->name, &xval, &avp->val.v.xavp)==NULL)
			return -1;
		return 0;
	}
	/* add new xavp with xavp list */
	if(xavp_add_value(&xname->next->name, &xval, &list)==NULL)
		return -1;
	
	/* build xavp value */
	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_XAVP;
	xval.v.xavp = list;
	xavp_add_value(&xname->name, &xval, NULL);

	return 0;
}

char* pv_xavp_fill_ni(str *in, pv_xavp_name_t *xname)
{
	char *p;
	str idx;
	int n;

	if(in->s==NULL || in->len<=0 || xname==NULL)
		return NULL;
	p = in->s;

	/* eat ws */
	while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
		p++;
	if(p>in->s+in->len || *p=='\0')
		goto error;
	xname->name.s = p;
	while(p < in->s + in->len)
	{
		if(*p=='=' || *p==' ' || *p=='\t' || *p=='\n' || *p=='\r' || *p=='[')
			break;
		p++;
	}
	xname->name.len = p - xname->name.s;
	if(p>in->s+in->len || *p=='\0')
		return p;
	/* eat ws */
	while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
		p++;
	if(p>in->s+in->len || *p=='\0')
		return p;

	if(*p!='[')
		return p;
	/* there is index */
	p++;
	idx.s = p;
	n = 0;
	while(p<in->s+in->len && *p!='\0')
	{
		if(*p==']')
		{
			if(n==0)
				break;
			n--;
		}
		if(*p == '[')
			n++;
		p++;
	}
	if(p>in->s+in->len || *p=='\0')
		goto error;

	if(p==idx.s)
	{
		LM_ERR("xavp [\"%.*s\"] does not get empty index param\n",
				in->len, in->s);
		goto error;
	}
	idx.len = p - idx.s;
	if(pv_parse_index(&xname->index, &idx)!=0)
	{
		LM_ERR("idx \"%.*s\" has an invalid index param [%.*s]\n",
					in->len, in->s, idx.len, idx.s);
		goto error;
	}
	xname->index.type = PVT_EXTRA;
	p++;
	return p;
error:
	return NULL;
}

void pv_xavp_name_destroy(pv_xavp_name_t *xname)
{
	return;
}

int pv_parse_xavp_name(pv_spec_p sp, str *in)
{
	pv_xavp_name_t *xname=NULL;
	char *p;
	str s;

	if(in->s==NULL || in->len<=0)
		return -1;

	xname = (pv_xavp_name_t*)pkg_malloc(sizeof(pv_xavp_name_t));
	if(xname==NULL)
		return -1;

	memset(xname, 0, sizeof(pv_xavp_name_t));

	s = *in;

	p = pv_xavp_fill_ni(&s, xname);
	if(p==NULL)
		goto error;

	if(*p!='=')
		goto done;
	p++;
	if(*p!='>')
		goto error;
	p++;

	s.len = in->len - (int)(p - in->s);
	s.s = p;
	LM_DBG("xavp sublist [%.*s] - key [%.*s]\n", xname->name.len,
			xname->name.s, s.len, s.s);

	xname->next = (pv_xavp_name_t*)pkg_malloc(sizeof(pv_xavp_name_t));
	if(xname->next==NULL)
		goto error;

	memset(xname->next, 0, sizeof(pv_xavp_name_t));

	p = pv_xavp_fill_ni(&s, xname->next);
	if(p==NULL)
		goto error;

done:
	sp->pvp.pvn.u.dname = (void*)xname;
	sp->pvp.pvn.type = PV_NAME_PVAR;
	return 0;

error:
	if(xname!=NULL) {
		pv_xavp_name_destroy(xname);
		pkg_free(xname);
	}
	return -1;
}

int pv_xavp_print(struct sip_msg* msg, char* s1, char *s2)
{
	xavp_print_list(NULL);
	return 1;
}

/**
 *
 */
int xavp_params_explode(str *params, str *xname)
{
	param_t* params_list = NULL;
	param_hooks_t phooks;
	param_t *pit=NULL;
	str s;
	sr_xavp_t *xavp=NULL;
	sr_xval_t xval;

	if(params==NULL || xname==NULL || params->s==NULL || xname->s==NULL
			|| params->len<=0 || xname->len<=0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	s.s = params->s;
	s.len = params->len;
	if(s.s[s.len-1]==';')
		s.len--;
	if (parse_params(&s, CLASS_ANY, &phooks, &params_list)<0) {
		LM_DBG("invalid formatted values [%.*s]\n", params->len, params->s);
		return -1;
	}

	if(params_list==NULL) {
		return -1;
	}


	for (pit = params_list; pit; pit=pit->next)
	{
		memset(&xval, 0, sizeof(sr_xval_t));
		xval.type = SR_XTYPE_STR;
		xval.v.s = pit->body;
		if(xavp_add_value(&pit->name, &xval, &xavp)==NULL) {
			free_params(params_list);
			xavp_destroy_list(&xavp);
			return -1;
		}
	}
	free_params(params_list);

	/* add main xavp in root list */
	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_XAVP;
	xval.v.xavp = xavp;
	if(xavp_add_value(xname, &xval, NULL)==NULL) {
		xavp_destroy_list(&xavp);
		return -1;
	}

	return 0;
}
#endif
