/*
 * $Id$
 *
 * Export vontact attrs as PV
 *
 * Copyright (C) 2008 Daniel-Constantin Mierla (asipto.com)
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
 */

/*!
 * \file
 * \brief SIP registrar module - export contacts as PV
 * \ingroup registrar   
 */  


#include <string.h>
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../mod_fix.h"
#include "../../route.h"
#include "../../action.h"
#include "../../lib/kcore/faked_msg.h"
#include "../usrloc/usrloc.h"
#include "reg_mod.h"
#include "common.h"
#include "regpv.h"

typedef struct _regpv_profile {
	str pname;
	str domain;
	str aor;
	int flags;
	unsigned int aorhash;
	int nrc;
	ucontact_t* contacts;
	struct _regpv_profile *next;
} regpv_profile_t;

typedef struct _regpv_name {
	regpv_profile_t *rp;
	int attr;
	pv_xavp_name_t* xname;
} regpv_name_t;

static regpv_profile_t *_regpv_profile_list = NULL;

static inline regpv_profile_t* regpv_get_profile(str *name)
{
	regpv_profile_t *rp;

	if(name==NULL || name->len<=0)
	{
		LM_ERR("invalid parameters\n");
		return NULL;
	}

	rp = _regpv_profile_list;
	while(rp)
	{
		if(rp->pname.len == name->len
				&& strncmp(rp->pname.s, name->s, name->len)==0)
			return rp;
		rp = rp->next;
	}

	rp = (regpv_profile_t*)pkg_malloc(sizeof(regpv_profile_t));
	if(rp==NULL)
	{
		LM_ERR("no more pkg\n");
		return NULL;
	}
	memset(rp, 0, sizeof(regpv_profile_t));
	rp->pname.s = (char*)pkg_malloc((name->len+1)*sizeof(char));
	if(rp->pname.s==NULL)
	{
		LM_ERR("no more pkg\n");
		pkg_free(rp);
		return NULL;
	}
	memcpy(rp->pname.s, name->s, name->len);
	rp->pname.s[name->len] = '\0';
	rp->pname.len = name->len;

	rp->next = _regpv_profile_list;
	_regpv_profile_list = rp;
	return rp;
}

static void regpv_free_profile(regpv_profile_t *rpp)
{
	ucontact_t* ptr;
	ucontact_t* ptr0;

	if(rpp==NULL)
		return;

	ptr = rpp->contacts;
	while(ptr)
	{
		if(ptr->xavp) {
			xavp_destroy_list(&ptr->xavp);
		}
		ptr0 = ptr;
		ptr = ptr->next;
		pkg_free(ptr0);
	}
	if(rpp->domain.s!=NULL)
	{
		rpp->domain.s = 0;
		rpp->domain.len = 0;
	}
	if(rpp->aor.s!=NULL)
	{
		pkg_free(rpp->aor.s);
		rpp->aor.s = 0;
		rpp->aor.len = 0;
	}

	rpp->flags = 0;
	rpp->aorhash = 0;
	rpp->nrc = 0;
	rpp->contacts = 0;

}

void regpv_free_profiles(void)
{
	regpv_profile_t *rp;
	regpv_profile_t *rp0;

	rp = _regpv_profile_list;

	while(rp)
	{
		if(rp->pname.s!=NULL)
			pkg_free(rp->pname.s);
		rp0 = rp;
		regpv_free_profile(rp0);
		rp = rp->next;
	}
	_regpv_profile_list = 0;
}

char* regpv_xavp_fill_ni(str *in, pv_xavp_name_t *xname)
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

int regpv_parse_xavp_name(pv_spec_p sp, str *in)
{
	pv_xavp_name_t *xname=NULL;
	char *p;
	str s;

	if(in->s==NULL || in->len<=0)
		return -1;

	xname = (pv_xavp_name_t*)shm_malloc(sizeof(pv_xavp_name_t));
	if(xname==NULL)
		return -1;

	memset(xname, 0, sizeof(pv_xavp_name_t));

	s = *in;

	p = regpv_xavp_fill_ni(&s, xname);
	if(p==NULL) {
		goto error;
	}

	if(*p!='=') {
		goto done;
	}
	p++;
	if(*p!='>') {
		goto error;
	}
	p++;

	s.len = in->len - (int)(p - in->s);
	s.s = p;
	LM_INFO("xavp sublist [%.*s] - key [%.*s]\n", xname->name.len,
			xname->name.s, s.len, s.s);

	xname->next = (pv_xavp_name_t*)pkg_malloc(sizeof(pv_xavp_name_t));
	if(xname->next==NULL) {
		goto error;
	}

	memset(xname->next, 0, sizeof(pv_xavp_name_t));

	p = regpv_xavp_fill_ni(&s, xname->next);
	if(p==NULL) {
		goto error;
	}

done:
	sp->pvp.pvn.u.dname = (void*)xname;
	sp->pvp.pvn.type = PV_NAME_PVAR;
	return 0;

error:
	if(xname!=NULL) {
		pkg_free(xname);
	}
	return -1;
}

int regpv_xavp_get_value(struct sip_msg *msg, pv_param_t *param,
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

int regpv_get_xavp_from_start(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, sr_xavp_t **start)
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
	xname = ((regpv_name_t*)param->pvn.u.dname)->xname;

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
		count = xavp_count(&xname->name, start);
		idx = count + idx;
	}
	avp = xavp_get_by_index(&xname->name, idx, start);
	if(avp==NULL) {
		LM_DBG("GET XAVP AVP = NULL\n");
		return pv_get_null(msg, param, res);
	}
	if(xname->next==NULL) {
		LM_DBG("GET XAVP XNAME NEXT = NULL\n");
		return regpv_xavp_get_value(msg, param, res, avp);
	}

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
	if(avp==NULL) {
		LM_DBG("GET XAVP AVP BY INDEX = NULL\n");
		return pv_get_null(msg, param, res);
	}
	/* get all values of second key */
	if(idxf==PV_IDX_ALL)
	{
		p_ini = pv_get_buffer();
		p = p_ini;
		p_size = pv_get_buffer_size();
		do {
			if(p!=p_ini)
			{
				if(p-p_ini+REGPV_FIELD_DELIM_LEN+1>p_size)
				{
					LM_ERR("local buffer length exceeded\n");
					return pv_get_null(msg, param, res);
				}
				memcpy(p, REGPV_FIELD_DELIM, REGPV_FIELD_DELIM_LEN);
				p += REGPV_FIELD_DELIM_LEN;
			}
			if(regpv_xavp_get_value(msg, param, res, avp)<0)
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
	return regpv_xavp_get_value(msg, param, res, avp);
}


int pv_get_ulc(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	regpv_name_t *rp;
	regpv_profile_t *rpp;
	ucontact_t *c;
	int idx;
	int i;

	if(param==NULL)
	{
		LM_ERR("invalid params\n");
		return -1;
	}
	rp = (regpv_name_t*)param->pvn.u.dname;
	if(rp==NULL || rp->rp==NULL)
	{
		LM_DBG("no profile in params\n");
		return pv_get_null(msg, param, res);
	}
	rpp = rp->rp;

	if(rpp->flags==0 || rpp->contacts==NULL)
	{
		LM_DBG("profile not set or no contacts there\n");
		return pv_get_null(msg, param, res);
	}
	/* get index */
	if(pv_get_spec_index(msg, param, &idx, &i)!=0)
	{
		LM_ERR("invalid index\n");
		return -1;
	}

	/* work only with positive indexes by now */
	if(idx<0)
		idx = 0;

	/* get contact */
	i = 0;
	c = rpp->contacts;
	while(c)
	{
		if(i == idx)
			break;
		i++;
		c = c->next;
	}
	if(c==NULL)
		return pv_get_null(msg, param, res);

	switch(rp->attr)
	{
		case 0: /* aor */
			return  pv_get_strval(msg, param, res, &rpp->aor);
		break;
		case 1: /* domain */
			return  pv_get_strval(msg, param, res, &rpp->domain);
		break;
		case 2: /* aorhash */
			return pv_get_uintval(msg, param, res, rpp->aorhash);
		break;
		case 3: /* addr */
			return  pv_get_strval(msg, param, res, &c->c);
		break;
		case 4: /* path */
			return  pv_get_strval(msg, param, res, &c->path);
		break;
		case 5: /* received */
			return  pv_get_strval(msg, param, res, &c->received);
		break;
		case 6: /* expires */
			return pv_get_uintval(msg, param, res,
					(unsigned int)c->expires);
		break;
		case 7: /* callid */
			return  pv_get_strval(msg, param, res, &c->callid);
		break;
		case 8: /* q */
			return pv_get_sintval(msg, param, res, (int)c->q);
		break;
		case 9: /* cseq */
			return pv_get_sintval(msg, param, res, c->cseq);
		break;
		case 10: /* flags */
			return pv_get_uintval(msg, param, res, c->flags);
		break;
		case 11: /* cflags */
			return pv_get_uintval(msg, param, res, c->cflags);
		break;
		case 12: /* user agent */
			return  pv_get_strval(msg, param, res, &c->user_agent);
		break;
		case 14: /* socket */
			if(c->sock==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &c->sock->sock_str);
		break;
		case 15: /* modified */
			return pv_get_uintval(msg, param, res,
					(unsigned int)c->last_modified);
		break;
		case 16: /* methods */
			return pv_get_uintval(msg, param, res, c->methods);
		break;
		case 17: /* count */
			return pv_get_sintval(msg, param, res, rpp->nrc);
		break;
		case 18: /* ruid */
			return  pv_get_strval(msg, param, res, &c->ruid);
		break;
		case 19: /* reg-id */
			return pv_get_uintval(msg, param, res, c->reg_id);
		break;
		case 20: /* instance */
			if(c->instance.len>0)
				return  pv_get_strval(msg, param, res, &c->instance);
		break;
		case 21: /* xavp */
			if(c->xavp)
				return regpv_get_xavp_from_start(msg, param, res, &c->xavp);
		break;
	}

	return pv_get_null(msg, param, res);
}

int pv_set_ulc(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	return 0;
}

int pv_parse_ulc_name(pv_spec_p sp, str *in)
{
	str pn;
	str pa;
	regpv_name_t *rp;
	regpv_profile_t *rpp;

	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	pa.s = in->s;
	while(pa.s < in->s + in->len - 2)
	{
		if(*pa.s=='=')
			break;
		pa.s++;
	}
	
	if(pa.s >= in->s + in->len - 2)
	{
		LM_ERR("invalid contact pv name %.*s\n", in->len, in->s);
		return -1;
	}
	if(*(pa.s+1) != '>')
	{
		LM_ERR("invalid contact pv name %.*s.\n", in->len, in->s);
		return -1;
	}

	pn.s = in->s;
	pn.len = pa.s - pn.s;

	LM_DBG("get profile [%.*s]\n", pn.len, pn.s);

	rpp = regpv_get_profile(&pn);
	if(rpp==NULL)
	{
		LM_ERR("cannot get profile [%.*s]\n", pn.len, pn.s);
		return -1;
	}
	pa.s += 2;
	pa.len = in->s + in->len - pa.s;
	LM_DBG("get attr [%.*s]\n", pa.len, pa.s);

	rp = (regpv_name_t*)pkg_malloc(sizeof(regpv_name_t));
	if(rp==0)
	{
		LM_ERR("no more pkg\n");
		return -1;
	}
	memset(rp, 0, sizeof(regpv_name_t));
	rp->rp = rpp;

	if(strstr(pa.s, "=>")) {
		regpv_parse_xavp_name(sp, &pa);
		pv_xavp_name_t* xname = (pv_xavp_name_t*)sp->pvp.pvn.u.dname;
		LM_DBG("ulc parse xavp name [%.*s] \n", xname->name.len, xname->name.s);
		rp->xname = xname;
		rp->attr = 21;
		goto done;
	}

	switch(pa.len)
	{
		case 1: 
			if(strncmp(pa.s, "q", 1)==0)
				rp->attr = 8;
			else goto error;
		break;
		case 3: 
			if(strncmp(pa.s, "aor", 3)==0)
				rp->attr = 0;
			else goto error;
		break;
		case 4: 
			if(strncmp(pa.s, "addr", 4)==0)
				rp->attr = 3;
			else if(strncmp(pa.s, "path", 4)==0)
				rp->attr = 4;
			else if(strncmp(pa.s, "cseq", 4)==0)
				rp->attr = 9;
			else if(strncmp(pa.s, "ruid", 4)==0)
				rp->attr = 18;
			else goto error;
		break;
		case 5: 
			if(strncmp(pa.s, "flags", 5)==0)
				rp->attr = 10;
			else if(strncmp(pa.s, "count", 5)==0)
				rp->attr = 17;
			else if(strncmp(pa.s, "regid", 5)==0)
				rp->attr = 19;
			else goto error;
		break;
		case 6: 
			if(strncmp(pa.s, "domain", 6)==0)
				rp->attr = 1;
			else if(strncmp(pa.s, "callid", 6)==0)
				rp->attr = 7;
			else if(strncmp(pa.s, "cflags", 6)==0)
				rp->attr = 11;
			else if(strncmp(pa.s, "socket", 6)==0)
				rp->attr = 14;
			else goto error;
		break;
		case 7: 
			if(strncmp(pa.s, "aorhash", 7)==0)
				rp->attr = 2;
			else if(strncmp(pa.s, "expires", 7)==0)
				rp->attr = 6;
			else if(strncmp(pa.s, "methods", 7)==0)
				rp->attr = 16;
			else goto error;
		break;
		case 8: 
			if(strncmp(pa.s, "received", 8)==0)
				rp->attr = 5;
			else if(strncmp(pa.s, "modified", 8)==0)
				rp->attr = 15;
			else if(strncmp(pa.s, "instance", 8)==0)
				rp->attr = 20;
			else goto error;
		break;
		case 10: 
			if(strncmp(pa.s, "user_agent", 10)==0)
				rp->attr = 12;
			else goto error;
		break;
		default:
			goto error;
	}
done:
	sp->pvp.pvn.u.dname = (void*)rp;
	sp->pvp.pvn.type = PV_NAME_PVAR;

	return 0;

error:
	LM_ERR("unknown contact attr name in %.*s\n", in->len, in->s);
	return -1;
}

int pv_fetch_contacts(struct sip_msg* msg, char* table, char* uri,
		char* profile)
{
	urecord_t* r;
	ucontact_t* ptr;
	ucontact_t* ptr0;
	ucontact_t* c0;
	regpv_profile_t *rpp;
	str aor = {0, 0};
	str u = {0, 0};
	int res;
	int olen;
	int ilen;
	int n;
	char *p;
	int match_return_flags = reg_match_return_flags_param;
	sr_xavp_t *vavp=NULL;

	rpp = regpv_get_profile((str*)profile);
	if(rpp==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	/* check and free if profile already set */
	if(rpp->flags)
		regpv_free_profile(rpp);

	if(fixup_get_svalue(msg, (gparam_p)uri, &u)!=0 || u.len<=0)
	{
		LM_ERR("invalid uri parameter\n");
		return -1;
	}

	if (extract_aor(&u, &aor, NULL) < 0) {
		LM_ERR("failed to extract Address Of Record\n");
		return -1;
	}

	/* copy aor and ul domain */
	rpp->aor.s = (char*)pkg_malloc(aor.len*sizeof(char));
	if(rpp->aor.s==NULL)
	{
		LM_ERR("no more pkg\n");
		return -1;
	}
	memcpy(rpp->aor.s, aor.s, aor.len);
	rpp->aor.len = aor.len;
	rpp->domain = *((udomain_head_t*)table)->name;
	rpp->flags = 1;

	/* copy contacts */
	ilen = sizeof(ucontact_t);
	ul.lock_udomain((udomain_t*)table, &aor);
	res = ul.get_urecord((udomain_t*)table, &aor, &r);
	if (res > 0) {
		LM_DBG("'%.*s' Not found in usrloc\n", aor.len, ZSW(aor.s));
		ul.unlock_udomain((udomain_t*)table, &aor);
		return -1;
	}

	if(reg_xavp_cfg.s!=NULL) {
		if( (vavp = xavp_get_child_with_ival(&reg_xavp_cfg, &match_return_flags_name)) != NULL) {
			match_return_flags = vavp->val.v.i;
			LM_INFO("match return flags set to %d\n", match_return_flags);
		}
	}

	ptr = r->contacts;
	ptr0 = NULL;
	n = 0;
	while(ptr)
	{
		olen = (ptr->c.len + ptr->received.len + ptr->path.len
			+ ptr->callid.len + ptr->user_agent.len + ptr->ruid.len
			+ ptr->instance.len)*sizeof(char) + ilen;
		c0 = (ucontact_t*)pkg_malloc(olen);
		if(c0==NULL)
		{
			LM_ERR("no more pkg\n");
			ul.release_urecord(r);
			ul.unlock_udomain((udomain_t*)table, &aor);
			goto error;
		}
		memcpy(c0, ptr, ilen);
		c0->domain = NULL;
		c0->aor = NULL;
		c0->next = NULL;
		c0->prev = NULL;

		c0->c.s = (char*)c0 + ilen;
		memcpy(c0->c.s, ptr->c.s, ptr->c.len);
		c0->c.len = ptr->c.len;
		p = c0->c.s + c0->c.len;
		
		if(ptr->received.s!=NULL)
		{
			c0->received.s = p;
			memcpy(c0->received.s, ptr->received.s, ptr->received.len);
			c0->received.len = ptr->received.len;
			p += c0->received.len;
		}
		if(ptr->path.s!=NULL)
		{
			c0->path.s = p;
			memcpy(c0->path.s, ptr->path.s, ptr->path.len);
			c0->path.len = ptr->path.len;
			p += c0->path.len;
		}
		c0->callid.s = p;
		memcpy(c0->callid.s, ptr->callid.s, ptr->callid.len);
		c0->callid.len = ptr->callid.len;
		p += c0->callid.len;
		if(ptr->user_agent.s!=NULL)
		{
			c0->user_agent.s = p;
			memcpy(c0->user_agent.s, ptr->user_agent.s, ptr->user_agent.len);
			c0->user_agent.len = ptr->user_agent.len;
			p += c0->user_agent.len;
		}
		if(ptr->ruid.s!=NULL)
		{
			c0->ruid.s = p;
			memcpy(c0->ruid.s, ptr->ruid.s, ptr->ruid.len);
			c0->ruid.len = ptr->ruid.len;
			p += c0->ruid.len;
		}
		if(ptr->instance.s!=NULL)
		{
			c0->instance.s = p;
			memcpy(c0->instance.s, ptr->instance.s, ptr->instance.len);
			c0->instance.len = ptr->instance.len;
			p += c0->instance.len;
		}
		if(ptr->xavp != NULL && match_return_flags == 1)
		{
			LM_DBG("adding contact xavp \n");
			c0->xavp = xavp_clone_level_nodata(ptr->xavp);
		}
		if(ptr0==NULL)
		{
			rpp->contacts = c0;
		} else {
			ptr0->next = c0;
			c0->prev = ptr0;
		}
		n++;
		ptr0 = c0;
		ptr = ptr->next;
	}
	ul.release_urecord(r);
	ul.unlock_udomain((udomain_t*)table, &aor);
	rpp->nrc = n;
	LM_DBG("fetched <%d> contacts for <%.*s> in [%.*s]\n",
			n, aor.len, aor.s, rpp->pname.len, rpp->pname.s);
	return 1;

error:
	regpv_free_profile(rpp);
	return -1;
}
int pv_free_contacts(struct sip_msg* msg, char* profile, char* s2)
{
	regpv_profile_t *rpp;

	rpp = regpv_get_profile((str*)profile);
	if(rpp==0)
		return -1;

	regpv_free_profile(rpp);

	return 1;
}

void reg_ul_expired_contact(ucontact_t* ptr, int type, void* param)
{
	str profile = {"exp", 3};
	regpv_profile_t *rpp;
	ucontact_t* c0;
	int backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;
	int olen;
	int ilen;
	char *p;

	if(reg_expire_event_rt<0)
		return;

	if (faked_msg_init() < 0)
	{
		LM_ERR("faked_msg_init() failed\n");
		return;
	}

	rpp = regpv_get_profile(&profile);
	if(rpp==0)
	{
		LM_ERR("error getting profile structure\n");
		return;
	}
	/* check and free if profile already set */
	if(rpp->flags)
		regpv_free_profile(rpp);

	/* copy aor and ul domain */
	rpp->aor.s = (char*)pkg_malloc(ptr->aor->len*sizeof(char));
	if(rpp->aor.s==NULL)
	{
		LM_ERR("no more pkg\n");
		return;
	}
	memcpy(rpp->aor.s, ptr->aor->s, ptr->aor->len);
	rpp->aor.len = ptr->aor->len;
	rpp->domain = *ptr->domain;
	rpp->flags = 1;

	/* copy contact */
	ilen = sizeof(ucontact_t);

	olen = (ptr->c.len + ptr->received.len + ptr->path.len
			+ ptr->callid.len + ptr->user_agent.len + ptr->ruid.len
			+ ptr->instance.len)*sizeof(char) + ilen;
	c0 = (ucontact_t*)pkg_malloc(olen);
	if(c0==NULL)
	{
		LM_ERR("no more pkg\n");
		goto error;
	}
	memcpy(c0, ptr, ilen);
	c0->domain = NULL;
	c0->aor = NULL;
	c0->next = NULL;
	c0->prev = NULL;

	c0->c.s = (char*)c0 + ilen;
	memcpy(c0->c.s, ptr->c.s, ptr->c.len);
	c0->c.len = ptr->c.len;
	p = c0->c.s + c0->c.len;

	if(ptr->received.s!=NULL)
	{
		c0->received.s = p;
		memcpy(c0->received.s, ptr->received.s, ptr->received.len);
		c0->received.len = ptr->received.len;
		p += c0->received.len;
	}
	if(ptr->path.s!=NULL)
	{
		c0->path.s = p;
		memcpy(c0->path.s, ptr->path.s, ptr->path.len);
		c0->path.len = ptr->path.len;
		p += c0->path.len;
	}
	c0->callid.s = p;
	memcpy(c0->callid.s, ptr->callid.s, ptr->callid.len);
	c0->callid.len = ptr->callid.len;
	p += c0->callid.len;
	if(ptr->user_agent.s!=NULL)
	{
		c0->user_agent.s = p;
		memcpy(c0->user_agent.s, ptr->user_agent.s, ptr->user_agent.len);
		c0->user_agent.len = ptr->user_agent.len;
		p += c0->user_agent.len;
	}
	if(ptr->ruid.s!=NULL)
	{
		c0->ruid.s = p;
		memcpy(c0->ruid.s, ptr->ruid.s, ptr->ruid.len);
		c0->ruid.len = ptr->ruid.len;
		p += c0->ruid.len;
	}
	if(ptr->instance.s!=NULL)
	{
		c0->instance.s = p;
		memcpy(c0->instance.s, ptr->instance.s, ptr->instance.len);
		c0->instance.len = ptr->instance.len;
		p += c0->instance.len;
	}
	if(ptr->xavp != NULL)
	{
		c0->xavp = xavp_clone_level_nodata(ptr->xavp);
	}		

	rpp->contacts = c0;
	rpp->nrc = 1;
	LM_DBG("saved contact for <%.*s> in [%.*s]\n",
			ptr->aor->len, ptr->aor->s, rpp->pname.len, rpp->pname.s);

	fmsg = faked_msg_next();
	backup_rt = get_route_type();
	set_route_type(REQUEST_ROUTE);
	init_run_actions_ctx(&ctx);
	run_top_route(event_rt.rlist[reg_expire_event_rt], fmsg, 0);
	set_route_type(backup_rt);

	return;
error:
	regpv_free_profile(rpp);
	return;
}
