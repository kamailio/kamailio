/**
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

/*! \file
 * \ingroup sqlops
 * \brief Kamailio SQL-operations :: Variables
 *
 * - Module: \ref sqlops
 */


#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../mod_fix.h"

#include "sql_api.h"
#include "sql_var.h"

typedef struct _sql_pv {
	str resname;
	sql_result_t *res;
	int type;
	gparam_t row;
	gparam_t col;
} sql_pv_t;

int pv_get_dbr(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	sql_pv_t *spv;
	int row;
	int col;

	spv = (sql_pv_t*)param->pvn.u.dname;

	if(spv->res==NULL)
	{
		spv->res = sql_get_result(&spv->resname);
		if(spv->res==NULL)
			return pv_get_null(msg, param, res);
	}

	switch(spv->type)
	{
		case 1:
			return pv_get_sintval(msg, param, res, spv->res->nrows);
		break;
		case 2:
			return pv_get_sintval(msg, param, res, spv->res->ncols);
		break;
		case 3:
			if(fixup_get_ivalue(msg, &spv->row, &row)!=0)
				return pv_get_null(msg, param, res);
			if(fixup_get_ivalue(msg, &spv->col, &col)!=0)
				return pv_get_null(msg, param, res);
			if(row>=spv->res->nrows)
				return pv_get_null(msg, param, res);
			if(col>=spv->res->ncols)
				return pv_get_null(msg, param, res);
			if(spv->res->vals[row][col].flags&PV_VAL_NULL)
				return pv_get_null(msg, param, res);
			if(spv->res->vals[row][col].flags&PV_VAL_INT)
				return pv_get_sintval(msg, param, res, 
						spv->res->vals[row][col].value.n);
			return pv_get_strval(msg, param, res, 
						&spv->res->vals[row][col].value.s);
		break;
		case 4:
			if(fixup_get_ivalue(msg, &spv->col, &col)!=0)
				return pv_get_null(msg, param, res);
			if(col>=spv->res->ncols)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, 
						&spv->res->cols[col].name);
		break;
	}
	return 0;
}


int sql_parse_index(str *in, gparam_t *gp)
{
	if(in->s[0]==PV_MARKER)
	{
		gp->type = GPARAM_TYPE_PVS;
		gp->v.pvs = (pv_spec_t*)pkg_malloc(sizeof(pv_spec_t));
		if (gp->v.pvs == NULL)
		{
			LM_ERR("no pkg memory left for pv_spec_t\n");
		    pkg_free(gp);
		    return -1;
		}

		if(pv_parse_spec(in, gp->v.pvs)==NULL)
		{
			LM_ERR("invalid PV identifier\n");
		    pkg_free(gp->v.pvs);
		    pkg_free(gp);
			return -1;
		}
	} else {
		gp->type = GPARAM_TYPE_INT;
		if(str2sint(in, &gp->v.i) != 0)
		{
			LM_ERR("bad number <%.*s>\n", in->len, in->s);
			return -1;
		}
	}
	return 0;
}

int pv_parse_dbr_name(pv_spec_p sp, str *in)
{
	sql_pv_t *spv=NULL;
	char *p;
	str pvs;
	str tok;

	spv = (sql_pv_t*)pkg_malloc(sizeof(sql_pv_t));
	if(spv==NULL)
		return -1;

	memset(spv, 0, sizeof(sql_pv_t));

	p = in->s;

	while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
		p++;
	if(p>in->s+in->len || *p=='\0')
		goto error;
	spv->resname.s = p;
	while(p < in->s + in->len)
	{
		if(*p=='=' || *p==' ' || *p=='\t' || *p=='\n' || *p=='\r')
			break;
		p++;
	}
	if(p>in->s+in->len || *p=='\0')
		goto error;
	spv->resname.len = p - spv->resname.s;
	spv->res = sql_get_result(&spv->resname);

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
	p = pvs.s+pvs.len-1;
	while(p>pvs.s && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
			p--;
	if(p==pvs.s)
	{
		LM_ERR("invalid key in [%.*s]\n", in->len, in->s);
		goto error;
	}
	pvs.len = p - pvs.s + 1;

	LM_DBG("res [%.*s] - key [%.*s]\n", spv->resname.len, spv->resname.s,
			pvs.len, pvs.s);
	if(pvs.len==4 && strncmp(pvs.s, "rows", 4)==0)
	{
		spv->type = 1;
	} else if(pvs.len==4 && strncmp(pvs.s, "cols", 4)==0) {
		spv->type = 2;
	} else if(pvs.s[0]=='[') {
		spv->type = 3;
		p = pvs.s+1;
		while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
			p++;
		if(p>in->s+in->len || *p=='\0')
			goto error_index;
		tok.s = p;
		while(p < in->s + in->len)
		{
			if(*p==',' || *p==' ' || *p=='\t' || *p=='\n' || *p=='\r')
				break;
			p++;
		}
		if(p>in->s+in->len || *p=='\0')
			goto error_index;
		tok.len = p - tok.s;
		if(sql_parse_index(&tok, &spv->row)!=0)
			goto error_index;
		while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
			p++;
		if(p>in->s+in->len || *p=='\0' || *p!=',')
			goto error_index;
		p++;
		while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
			p++;
		if(p>in->s+in->len || *p=='\0')
			goto error_index;
		tok.s = p;
		while(p < in->s + in->len)
		{
			if(*p==']' || *p==' ' || *p=='\t' || *p=='\n' || *p=='\r')
				break;
			p++;
		}
		if(p>in->s+in->len || *p=='\0')
			goto error_index;
		tok.len = p - tok.s;
		if(sql_parse_index(&tok, &spv->col)!=0)
			goto error_index;
		while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
			p++;
		if(p>in->s+in->len || *p=='\0' || *p!=']')
			goto error_index;
	} else if(pvs.len>9 && strncmp(pvs.s, "colname", 7)==0) {
		spv->type = 4;
		p = pvs.s+7;
		while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
			p++;
		if(p>in->s+in->len || *p=='\0' || *p!='[')
			goto error_index;
		p++;
		tok.s = p;
		while(p < in->s + in->len)
		{
			if(*p==']' || *p==' ' || *p=='\t' || *p=='\n' || *p=='\r')
				break;
			p++;
		}
		if(p>in->s+in->len || *p=='\0')
			goto error_index;
		tok.len = p - tok.s;
		if(sql_parse_index(&tok, &spv->col)!=0)
			goto error_index;
		while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
			p++;
		if(p>in->s+in->len || *p=='\0' || *p!=']')
			goto error_index;
	} else {
		LM_ERR("unknow key [%.*s]\n", pvs.len, pvs.s);
		return -1;
	}
	sp->pvp.pvn.u.dname = (void*)spv;
	sp->pvp.pvn.type = PV_NAME_PVAR;

	return 0;

error:
	LM_ERR("invalid pv name [%.*s]\n", in->len, in->s);
	if(spv!=NULL)
		pkg_free(spv);
	return -1;

error_index:
	LM_ERR("invalid index in [%.*s]\n", pvs.len, pvs.s);
	if(spv!=NULL)
		pkg_free(spv);
	return -1;
}

