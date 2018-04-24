/*
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

/*!
 * \file
 * \brief Regular expressions
 * \ingroup textops
 * Module: \ref textops
 */


#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../core/mem/mem.h"
#include "../../core/dprint.h"
#include "../../core/trim.h"
#include "../../core/re.h"
#include "../../core/ut.h"

#include "txt_var.h"

int tr_txt_eval_re(struct sip_msg *msg, tr_param_t *tp, int subtype,
		pv_value_t *val)
{
	struct subst_expr *se = NULL;
	int nmatches;
	str* result;
#define TR_TXT_BUF_SIZE	2048
	static char tr_txt_buf[TR_TXT_BUF_SIZE];
	pv_value_t v;

	if(val==NULL || (!(val->flags&PV_VAL_STR)) || val->rs.len<=0)
		return -1;
	
	switch(subtype)
	{
		case TR_TXT_RE_SUBST:
			if (tp->type == TR_PARAM_SUBST) {
				se = (struct subst_expr*)tp->v.data;
				if (se==NULL)
					return 0;
			} else if (tp->type == TR_PARAM_SPEC) {
				if (pv_get_spec_value(msg, (pv_spec_p)tp->v.data, &v)!=0
						|| (!(v.flags&PV_VAL_STR)) || v.rs.len<=0)
				{
					LM_ERR("Can't evaluate regexp\n");
					return -1;
				}
				se=subst_parser(&v.rs);
				if (se==0) {
					LM_ERR("Can't compile regexp\n");
					return -1;
				}
			} else {
				LM_ERR("Unknown parameter type\n");
				return -1;
			}
			if(val->rs.len>=TR_TXT_BUF_SIZE-1)
			{
				LM_ERR("PV value too big %d, increase buffer size\n",
						val->rs.len);
				goto error;
			}
			memcpy(tr_txt_buf, val->rs.s, val->rs.len);
			tr_txt_buf[val->rs.len] = '\0';
			/* pkg malloc'ed result */
			result=subst_str(tr_txt_buf, msg, se, &nmatches);
			if (result == NULL)
			{
				if (nmatches==0)
				{
					LM_DBG("no match for subst expression\n");
					break;
				}
				if (nmatches<0)
					LM_ERR("subst failed\n");
				goto error;
			}
			if(result->len>=TR_TXT_BUF_SIZE-1)
			{
				LM_ERR("subst result too big %d, increase buffer size\n",
						result->len);
				pkg_free(result->s);
				pkg_free(result);
				goto error;
			}
			memcpy(tr_txt_buf, result->s, result->len);
			tr_txt_buf[result->len] = '\0';
			memset(val, 0, sizeof(pv_value_t));
			val->flags = PV_VAL_STR;
			val->rs.s = tr_txt_buf;
			val->rs.len = result->len;
			pkg_free(result->s);
			pkg_free(result);
			break;
		default:
			LM_ERR("unknown subtype %d\n", subtype);
			goto error;
	}

	if (tp->type == TR_PARAM_SPEC) {
		subst_expr_free(se);
	}
	return 0;

error:
	if (tp->type == TR_PARAM_SPEC && se!=NULL) {
		subst_expr_free(se);
	}
	return -1;
}

char* tr_txt_parse_re(str *in, trans_t *t)
{
	char *p;
	str name;
	str tok;
	struct subst_expr *se = NULL;
	tr_param_t *tp = NULL;
	int n;
	pv_spec_t *spec = NULL;


	if(in==NULL || t==NULL)
		return NULL;

	p = in->s;
	name.s = in->s;
	t->type = TR_TXT_RE;
	t->trf = tr_txt_eval_re;

	/* find next token */
	while(is_in_str(p, in) && *p!=TR_PARAM_MARKER && *p!=TR_RBRACKET) p++;
	if(*p=='\0')
		goto error;
	name.len = p - name.s;
	trim(&name);

	if(name.len==5 && strncasecmp(name.s, "subst", 5)==0)
	{
		t->subtype = TR_TXT_RE_SUBST;
		if(*p!=TR_PARAM_MARKER)
			goto error;
		p++;
		if(*p==PV_MARKER) {
			spec = (pv_spec_t*)pkg_malloc(sizeof(pv_spec_t));
			if(spec==NULL)
			{
				LM_ERR("no more private memory!\n");
				return 0;
			}
			tok.s = p; tok.len = in->s + in->len - p;
			p = pv_parse_spec(&tok, spec);
			if(p==NULL)
			{
				LM_ERR("invalid pv spec in transformation: %.*s!\n",
					in->len, in->s);
				pkg_free(spec);
				return 0;
			}

			tp = (tr_param_t*)pkg_malloc(sizeof(tr_param_t));
			if(tp==NULL)
			{
				LM_ERR("no more private memory!\n");
				pkg_free(spec);
				goto error;
			}
			tp->type = TR_PARAM_SPEC;
			tp->v.data = (void*)spec;
		} else {
			/* get trans here */
			n = 0;
			tok.s = p;
			while(is_in_str(p, in))
			{
				if(*p==TR_RBRACKET)
				{
					if(n==0)
							break;
					n--;
				}
				if(*p == TR_LBRACKET)
					n++;
				p++;
			}
			if(!is_in_str(p, in))
				goto error;
			if(p==tok.s)
				goto error;
			tok.len = p - tok.s;
			tp = (tr_param_t*)pkg_malloc(sizeof(tr_param_t));
			if(tp==NULL)
			{
				LM_ERR("no more private memory!\n");
				goto error;
			}
			memset(tp, 0, sizeof(tr_param_t));

			se=subst_parser(&tok);

			if (se==0)
				goto error;
			tp->type = TR_PARAM_SUBST;
			tp->v.data = (void*)se;
		}
		t->params = tp;

		while(*p && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p!=TR_RBRACKET)
			goto error;
		goto done;
	}

	LM_ERR("unknown transformation: %.*s/%.*s/%d!\n", in->len, in->s,
			name.len, name.s, name.len);
error:
	LM_ERR("invalid transformation [%.*s] <%d>\n", in->len, in->s,
			(int)(p-in->s));
	if(tp!=NULL)
		pkg_free(tp);
	if(se!=NULL)
		subst_expr_free(se);
	return NULL;
done:
	t->name = name;
	return p;
}

