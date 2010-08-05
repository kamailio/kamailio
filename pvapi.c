/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of SIP-Router, a free SIP server.
 *
 * SIP-Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-Router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*!
 * \file
 * \brief SIP-router core ::  PV API specification
 * \ingroup core
 * Module: \ref core
 */


#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "mem/mem.h"
#include "mem/shm_mem.h"
#include "ut.h"
#include "dprint.h"
#include "hashes.h"
#include "route.h"
#include "pvapi_init.h"
#include "pvar.h"

#define is_in_str(p, in) (p<in->s+in->len && *p)

#define PV_TABLE_SIZE	16
#define TR_TABLE_SIZE	4


void tr_destroy(trans_t *t);
void tr_free(trans_t *t);

typedef struct _pv_item
{
	pv_export_t pve;
	unsigned int pvid;
	struct _pv_item *next;
} pv_item_t, *pv_item_p;

static pv_item_t* _pv_table[PV_TABLE_SIZE];
static int _pv_table_set = 0;


/**
 *
 */
void pv_init_table(void)
{
	memset(_pv_table, 0, sizeof(pv_item_t*)*PV_TABLE_SIZE);
	_pv_table_set = 1;
}

/**
 *
 */
static int is_pv_valid_char(char c)
{
	if((c>='0' && c<='9') || (c>='a' && c<='z') || (c>='A' && c<='Z')
			|| (c=='_') || (c=='.') || (c=='?') /* ser $? */)
		return 1;
	return 0;
}

/**
 *
 */
int pv_table_add(pv_export_t *e)
{
	char *p;
	str  *in;
	pv_item_t *pvi = NULL;
	pv_item_t *pvj = NULL;
	pv_item_t *pvn = NULL;
	int found;
	unsigned int pvid;

	if(e==NULL || e->name.s==NULL || e->getf==NULL || e->type==PVT_NONE)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}
	
	if(_pv_table_set==0)
	{
		LM_DBG("PV table not initialized, doing it now\n");
		pv_init_table();
	}
	in = &(e->name);
	p = in->s;	
	while(is_in_str(p,in) && is_pv_valid_char(*p))
		p++;
	if(is_in_str(p,in))
	{
		LM_ERR("invalid char [%c] in [%.*s]\n", *p, in->len, in->s);
		return -1;
	}
	found = 0;
	//pvid = get_hash1_raw(in->s, in->len);
	pvid = get_hash1_raw(in->s, in->len);

	pvi = _pv_table[pvid%PV_TABLE_SIZE];
	while(pvi)
	{
		if(pvi->pvid > pvid)
			break;
		if(pvi->pve.name.len==in->len)
		{
			found = strncmp(pvi->pve.name.s, in->s, in->len);

			if(found==0)
			{
				LM_ERR("pvar [%.*s] already exists\n", in->len, in->s);
				return -1;
			}
		}
		pvj = pvi;
		pvi = pvi->next;
	}

	pvn = (pv_item_t*)pkg_malloc(sizeof(pv_item_t));
	if(pvn==0)
	{
		LM_ERR("no more memory\n");
		return -1;
	}
	memset(pvn, 0, sizeof(pv_item_t));
	memcpy(&(pvn->pve), e, sizeof(pv_export_t));
	pvn->pvid = pvid;

	if(pvj==0)
	{
		pvn->next = _pv_table[pvid%PV_TABLE_SIZE];
		_pv_table[pvid%PV_TABLE_SIZE] = pvn;
		goto done;
	}
	pvn->next = pvj->next;
	pvj->next = pvn;

done:
	return 0;
}

/**
 *
 */
int register_pvars_mod(char *mod_name, pv_export_t *items)
{
	int ret;
	int i;

	if (items==0)
		return 0;

	for ( i=0 ; items[i].name.s ; i++ ) {
		ret = pv_table_add(&items[i]);
		if (ret!=0) {
			LM_ERR("failed to register pseudo-variable <%.*s> for module %s\n",
					items[i].name.len, items[i].name.s, mod_name);
		}
	}
	return 0;
}

/**
 *
 */
int pv_table_free(void)
{
	pv_item_p xe;
	pv_item_p xe1;
	int i;

	for(i=0; i<PV_TABLE_SIZE; i++)
	{
		xe = _pv_table[i];
		while(xe!=0)
		{
			xe1 = xe;
			xe = xe->next;
			pkg_free(xe1);
		}
	}
	memset(_pv_table, 0, sizeof(pv_item_t*)*PV_TABLE_SIZE);
	_pv_table_set = 0;
	
	return 0;
}

/********** helper functions ********/
/**
 * convert unsigned int to pv_value_t
 */
int pv_get_uintval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, unsigned int uival)
{
	int l = 0;
	char *ch = NULL;

	if(res==NULL)
		return -1;

	ch = int2str(uival, &l);
	res->rs.s = ch;
	res->rs.len = l;

	res->ri = (int)uival;
	res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;
	return 0;
}

/**
 * convert signed int to pv_value_t
 */
int pv_get_sintval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, int sival)
{
	int l = 0;
	char *ch = NULL;

	if(res==NULL)
		return -1;

	ch = sint2str(sival, &l);
	res->rs.s = ch;
	res->rs.len = l;

	res->ri = sival;
	res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;
	return 0;
}

/**
 * convert str to pv_value_t
 */
int pv_get_strval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, str *sval)
{
	if(res==NULL)
		return -1;

	res->rs = *sval;
	res->flags = PV_VAL_STR;
	return 0;
}

/**
 * convert str-int to pv_value_t (type is str)
 */
int pv_get_strintval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, str *sval, int ival)
{
	if(res==NULL)
		return -1;

	res->rs = *sval;
	res->ri = ival;
	res->flags = PV_VAL_STR|PV_VAL_INT;
	return 0;
}

/**
 * convert int-str to pv_value_t (type is int)
 */
int pv_get_intstrval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, int ival, str *sval)
{
	if(res==NULL)
		return -1;

	res->rs = *sval;
	res->ri = ival;
	res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;
	return 0;
}

/*** ============================= ***/
static str pv_str_marker = { PV_MARKER_STR, 1 };
static int pv_get_marker(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	return pv_get_strintval(msg, param, res, &pv_str_marker,
			(int)pv_str_marker.s[0]);
}

static str pv_str_empty  = { "", 0 };
static str pv_str_null   = { "<null>", 6 };
int pv_get_null(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	if(res==NULL)
		return -1;
	
	res->rs = pv_str_empty;
	res->ri = 0;
	res->flags = PV_VAL_NULL;
	return 0;
}

pv_export_t* pv_lookup_spec_name(str *pvname, pv_spec_p e)
{
	pv_item_t *pvi;
	unsigned int pvid;

	if(pvname==0 || e==0)
	{
		LM_ERR("bad parameters\n");
		return NULL;
	}

	/* search in PV table */
	// pvid = get_hash1_raw(pvname->s, pvname->len);
	pvid = get_hash1_raw(pvname->s, pvname->len);
	pvi = _pv_table[pvid%PV_TABLE_SIZE];
	while(pvi)
	{
		if(pvi->pvid > pvid)
			break;

		if(pvi->pvid==pvid && pvi->pve.name.len==pvname->len
			&& memcmp(pvi->pve.name.s, pvname->s, pvname->len)==0)
		{
			/*LM_DBG("found [%.*s] [%d]\n", pvname->len, pvname->s,
					_pv_names_table[i].type);*/
			/* copy data from table to spec */
			e->type = pvi->pve.type;
			e->getf = pvi->pve.getf;
			e->setf = pvi->pve.setf;
			return &(pvi->pve);
		}
		pvi = pvi->next;
	}

	return NULL;
}

int pv_parse_index(pv_spec_p sp, str *in)
{
	char *p;
	char *s;
	int sign;
	pv_spec_p nsp = 0;

	if(in==NULL || in->s==NULL || sp==NULL)
		return -1;
	p = in->s;
	if(*p==PV_MARKER)
	{
		nsp = (pv_spec_p)pkg_malloc(sizeof(pv_spec_t));
		if(nsp==NULL)
		{
			LM_ERR("no more memory\n");
			return -1;
		}
		s = pv_parse_spec(in, nsp);
		if(s==NULL)
		{
			LM_ERR("invalid index [%.*s]\n", in->len, in->s);
			pv_spec_free(nsp);
			return -1;
		}
		sp->pvp.pvi.type = PV_IDX_PVAR;
		sp->pvp.pvi.u.dval = (void*)nsp;
		return 0;
	}
	if(*p=='*' && in->len==1)
	{
		sp->pvp.pvi.type = PV_IDX_ALL;
		return 0;
	}
	sign = 1;
	if(*p=='-')
	{
		sign = -1;
		p++;
	}
	sp->pvp.pvi.u.ival = 0;
	while(p<in->s+in->len && *p>='0' && *p<='9')
	{
		sp->pvp.pvi.u.ival = sp->pvp.pvi.u.ival * 10 + *p - '0';
		p++;
	}
	if(p!=in->s+in->len)
	{
		LM_ERR("invalid index [%.*s]\n", in->len, in->s);
		return -1;
	}
	sp->pvp.pvi.u.ival *= sign;
	sp->pvp.pvi.type = PV_IDX_INT;
	return 0;
}

int pv_init_iname(pv_spec_p sp, int param)
{
	if(sp==NULL)
		return -1;
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.name.n = param;
	return 0;
}

char* pv_parse_spec2(str *in, pv_spec_p e, int silent)
{
	char *p;
	str s;
	str pvname;
	int pvstate;
	trans_t *tr = NULL;
	pv_export_t *pte = NULL;
	int n=0;

	if(in==NULL || in->s==NULL || e==NULL || *in->s!=PV_MARKER)
	{
		if (!silent) LM_ERR("bad parameters\n");
		return NULL;
	}
	
	/* LM_DBG("***** input [%.*s] (%d)\n", in->len, in->s, in->len); */
	tr = 0;
	pvstate = 0;
	memset(e, 0, sizeof(pv_spec_t));
	p = in->s;
	p++;
	if(*p==PV_LNBRACKET)
	{
		p++;
		pvstate = 1;
	}
	pvname.s = p;
	if(*p == PV_MARKER) {
		p++;
		if(pvstate==1)
		{
			if(*p!=PV_RNBRACKET)
				goto error;
			p++;
		}
		e->getf = pv_get_marker;
		e->type = PVT_MARKER;
		pvname.len = 1;
		goto done_all;
	}
	while(is_in_str(p,in) && is_pv_valid_char(*p))
		p++;
	pvname.len = p - pvname.s;
	if(pvstate==1)
	{
		if(*p==PV_RNBRACKET)
		{ /* full pv name ended here*/
			goto done_inm;
		} else if(*p==PV_LNBRACKET) {
			p++;
			pvstate = 2;
		} else if(*p==PV_LIBRACKET) {
			p++;
			pvstate = 3;
		} else if(*p==TR_LBRACKET) {
			p++;
			pvstate = 4;
		} else {
			if (!silent)
				LM_ERR("invalid char '%c' in [%.*s] (%d)\n",
							*p, in->len, in->s, pvstate);
			goto error;
		}
	} else { 
		if(!is_in_str(p, in)) {
			p--;
			goto done_inm;
		} else if(*p==PV_LNBRACKET) {
			p++;
			pvstate = 5;
		} else {
			/* still in input str, but end of PV */
			/* p is increased at the end, so decrement here */
			p--;
			goto done_inm;
		}
	}

done_inm:
	if((pte = pv_lookup_spec_name(&pvname, e))==NULL)
	{
		if (!silent) 
			LM_ERR("error searching pvar \"%.*s\"\n", pvname.len, pvname.s);
		goto error;
	}
	if(pte->parse_name!=NULL && pvstate!=2 && pvstate!=5)
	{
		if (!silent) 
			LM_ERR("pvar \"%.*s\" expects an inner name\n",
						pvname.len, pvname.s);
		goto error;
	}
	if(pvstate==2 || pvstate==5)
	{
		if(pte->parse_name==NULL)
		{
			if (!silent)
				LM_ERR("pvar \"%.*s\" does not get name param\n",
						pvname.len, pvname.s);
			goto error;
		}
		s.s = p;
		n = 0;
		while(is_in_str(p, in))
		{
			if(*p==PV_RNBRACKET)
			{
				if(n==0)
					break;
				n--;
			}
			if(*p == PV_LNBRACKET)
				n++;
			p++;
		}

		if(!is_in_str(p, in))
			goto error;

		if(p==s.s)
		{
			if (!silent)
				LM_ERR("pvar \"%.*s\" does not get empty name param\n",
						pvname.len, pvname.s);
			goto error;
		}
		s.len = p - s.s;
		if(pte->parse_name(e, &s)!=0)
		{
			if (!silent)
				LM_ERR("pvar \"%.*s\" has an invalid name param [%.*s]\n",
						pvname.len, pvname.s, s.len, s.s);
			goto error;
		}
		if(pvstate==2)
		{
			p++;
			if(*p==PV_RNBRACKET)
			{ /* full pv name ended here*/
				goto done_vnm;
			} else if(*p==PV_LIBRACKET) {
				p++;
				pvstate = 3;
			} else if(*p==TR_LBRACKET) {
				p++;
				pvstate = 4;
			} else {
				if (!silent)
					LM_ERR("invalid char '%c' in [%.*s] (%d)\n",
								*p, in->len, in->s, pvstate);
				goto error;
			}
		} else {
			if(*p==PV_RNBRACKET)
			{ /* full pv name ended here*/
				p++;
				goto done_all;
			} else {
				if (!silent)
					LM_ERR("invalid char '%c' in [%.*s] (%d)\n",
								*p, in->len, in->s, pvstate);
				goto error;
			}
		}
	}
done_vnm:
	if(pvstate==3)
	{
		if(pte->parse_index==NULL)
		{
			if (!silent)
				LM_ERR("pvar \"%.*s\" does not get index param\n",
						pvname.len, pvname.s);
			goto error;
		}
		s.s = p;
		n = 0;
		while(is_in_str(p, in))
		{
			if(*p==PV_RIBRACKET)
			{
				if(n==0)
					break;
				n--;
			}
			if(*p == PV_LIBRACKET)
				n++;
			p++;
		}
		if(!is_in_str(p, in))
			goto error;

		if(p==s.s)
		{
			if (!silent)
				LM_ERR("pvar \"%.*s\" does not get empty index param\n",
						pvname.len, pvname.s);
			goto error;
		}
		s.len = p - s.s;
		if(pte->parse_index(e, &s)!=0)
		{
			if (!silent)
				LM_ERR("pvar \"%.*s\" has an invalid index param [%.*s]\n",
						pvname.len, pvname.s, s.len, s.s);
			goto error;
		}
		p++;
		if(*p==PV_RNBRACKET)
		{ /* full pv name ended here*/
			goto done_idx;
		} else if(*p==TR_LBRACKET) {
			p++;
			pvstate = 4;
		} else {
			if (!silent)
				LM_ERR("invalid char '%c' in [%.*s] (%d)\n",
							*p, in->len, in->s, pvstate);
			goto error;
		}
	}
done_idx:
	if(pvstate==4)
	{
		s.s = p-1;
		n = 0;
		while(is_in_str(p, in))
		{
			if(*p==TR_RBRACKET)
			{
				if(n==0)
				{
					/* yet another transformation */
					p++;
					while(is_in_str(p, in) && (*p==' ' || *p=='\t')) p++;

					if(!is_in_str(p, in) || *p != TR_LBRACKET)
					{
						p--;
						break;
					}
				}
				n--;
			}
			if(*p == TR_LBRACKET)
				n++;
			p++;
		}
		if(!is_in_str(p, in))
			goto error;

		if(p==s.s)
		{
			if (!silent)
				LM_ERR("pvar \"%.*s\" does not get empty index param\n",
						pvname.len, pvname.s);
			goto error;
		}
		s.len = p - s.s + 1;

		p = tr_lookup(&s, &tr);
		if(p==NULL)
		{
			if (!silent)
				LM_ERR("bad tr in pvar name \"%.*s\"\n", pvname.len, pvname.s);
			goto error;
		}
		if(*p!=PV_RNBRACKET)
		{
			if (!silent)
				LM_ERR("bad pvar name \"%.*s\" (%c)!\n", in->len, in->s, *p);
			goto error;
		}
		e->trans = (void*)tr;
	}
	p++;

done_all:
	if(pte!=NULL && pte->init_param)
		pte->init_param(e, pte->iparam);
	return p;

error:
	if(p!=NULL){
		if (!silent)
			LM_ERR("wrong char [%c/%d] in [%.*s] at [%d (%d)]\n", *p, (int)*p,
					in->len, in->s, (int)(p-in->s), pvstate);
	}else{
		if (!silent)
			LM_ERR("invalid parsing in [%.*s] at (%d)\n",
						in->len, in->s, pvstate);
	}
	return NULL;

} /* end: pv_parse_spec */

/**
 *
 */
int pv_parse_format(str *in, pv_elem_p *el)
{
	char *p, *p0;
	int n = 0;
	pv_elem_p e, e0;
	str s;

	if(in==NULL || in->s==NULL || el==NULL)
		return -1;

	/*LM_DBG("parsing [%.*s]\n", in->len, in->s);*/
	
	if(in->len == 0)
	{
		*el = pkg_malloc(sizeof(pv_elem_t));
		if(*el == NULL)
			goto error;
		memset(*el, 0, sizeof(pv_elem_t));
		(*el)->text = *in;
		return 0;
	}

	p = in->s;
	*el = NULL;
	e = e0 = NULL;

	while(is_in_str(p,in))
	{
		e0 = e;
		e = pkg_malloc(sizeof(pv_elem_t));
		if(!e)
			goto error;
		memset(e, 0, sizeof(pv_elem_t));
		n++;
		if(*el == NULL)
			*el = e;
		if(e0)
			e0->next = e;
	
		e->text.s = p;
		while(is_in_str(p,in) && *p!=PV_MARKER)
			p++;
		e->text.len = p - e->text.s;
		
		if(*p == '\0' || !is_in_str(p,in))
			break;
		s.s = p;
		s.len = in->s+in->len-p;
		p0 = pv_parse_spec(&s, &e->spec);
		
		if(p0==NULL)
			goto error;
		if(*p0 == '\0')
			break;
		p = p0;
	}
	/*LM_DBG("format parsed OK: [%d] items\n", n);*/

	if(*el == NULL)
		return -1;

	return 0;

error:
	pv_elem_free_all(*el);
	*el = NULL;
	return -1;
}

int pv_get_spec_name(struct sip_msg* msg, pv_param_p ip, pv_value_t *name)
{
	if(msg==NULL || ip==NULL || name==NULL)
		return -1;
	memset(name, 0, sizeof(pv_value_t));

	if(ip->pvn.type==PV_NAME_INTSTR)
	{
		if(ip->pvn.u.isname.type&AVP_NAME_STR)
		{
			name->rs = ip->pvn.u.isname.name.s;
			name->flags = PV_VAL_STR;
		} else {
			name->ri = ip->pvn.u.isname.name.n;
			name->flags = PV_VAL_INT|PV_TYPE_INT;
		}
		return 0;
	} else if(ip->pvn.type==PV_NAME_PVAR) {
		/* pvar */
		if(pv_get_spec_value(msg, (pv_spec_p)(ip->pvn.u.dname), name)!=0)
		{
			LM_ERR("cannot get name value\n");
			return -1;
		}
		if(name->flags&PV_VAL_NULL || name->flags&PV_VAL_EMPTY)
		{
			LM_ERR("null or empty name\n");
			return -1;
		}
		return 0;
	}
	LM_ERR("name type is PV_NAME_OTHER - cannot resolve\n");
	return -1;
}

int pv_get_avp_name(struct sip_msg* msg, pv_param_p ip, int_str *avp_name,
		unsigned short *name_type)
{
	pv_value_t tv;
	if(ip==NULL || avp_name==NULL || name_type==NULL)
		return -1;
	memset(avp_name, 0, sizeof(int_str));
	*name_type = 0;

	if(ip->pvn.type==PV_NAME_INTSTR)
	{
		*name_type = ip->pvn.u.isname.type;
		if(ip->pvn.u.isname.type&AVP_NAME_STR)
		{
			avp_name->s = ip->pvn.u.isname.name.s;
			*name_type |= AVP_NAME_STR;
		} else {
			avp_name->n = ip->pvn.u.isname.name.n;
			/* *name_type &= AVP_SCRIPT_MASK; */
			*name_type = 0;
		}
		return 0;
	}
	/* pvar */
	if(pv_get_spec_value(msg, (pv_spec_p)(ip->pvn.u.dname), &tv)!=0)
	{
		LM_ERR("cannot get avp value\n");
		return -1;
	}
	if(tv.flags&PV_VAL_NULL || tv.flags&PV_VAL_EMPTY)
	{
		LM_ERR("null or empty name\n");
		return -1;
	}
		
	if((tv.flags&PV_TYPE_INT) && (tv.flags&PV_VAL_INT))
	{
		avp_name->n = tv.ri;
	} else {
		avp_name->s = tv.rs;
		*name_type = AVP_NAME_STR;
	}
	return 0;
}


int pv_get_spec_index(struct sip_msg* msg, pv_param_p ip, int *idx, int *flags)
{
	pv_value_t tv;
	if(ip==NULL || idx==NULL || flags==NULL)
		return -1;

	*idx = 0;
	*flags = 0;

	if(ip->pvi.type == PV_IDX_ALL) {
		*flags = PV_IDX_ALL;
		return 0;
	}
	
	if(ip->pvi.type == PV_IDX_INT)
	{
		*idx = ip->pvi.u.ival;
		return 0;
	}

	/* pvar */
	if(pv_get_spec_value(msg, (pv_spec_p)ip->pvi.u.dval, &tv)!=0)
	{
		LM_ERR("cannot get index value\n");
		return -1;
	}
	if(!(tv.flags&PV_VAL_INT))
	{
		LM_ERR("invalid index value\n");
		return -1;
	}
	*idx = tv.ri;
	return 0;
}

int pv_get_spec_value(struct sip_msg* msg, pv_spec_p sp, pv_value_t *value)
{
	int ret = 0;

	if(msg==NULL || sp==NULL || sp->getf==NULL || value==NULL
			|| sp->type==PVT_NONE)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	
	memset(value, 0, sizeof(pv_value_t));

	ret = (*sp->getf)(msg, &(sp->pvp), value);
	if(ret!=0)
		return ret;
		
	if(sp->trans)
		return tr_exec(msg, (trans_t*)sp->trans, value);
	return ret;
}

int pv_set_spec_value(struct sip_msg* msg, pv_spec_p sp, int op,
		pv_value_t *value)
{
	if(sp==NULL || !pv_is_w(sp))
		return 0; /* no op */
	if(pv_alter_context(sp) && is_route_type(LOCAL_ROUTE))
		return 0; /* no op */
	return sp->setf(msg, &sp->pvp, op, value);
}

/**
 *
 */
int pv_printf(struct sip_msg* msg, pv_elem_p list, char *buf, int *len)
{
	int n, h;
	pv_value_t tok;
	pv_elem_p it;
	char *cur;
	
	if(msg==NULL || list==NULL || buf==NULL || len==NULL)
		return -1;

	if(*len <= 0)
		return -1;

	*buf = '\0';
	cur = buf;
	
	h = 0;
	n = 0;
	for (it=list; it; it=it->next)
	{
		/* put the text */
		if(it->text.s && it->text.len>0)
		{
			if(n+it->text.len < *len)
			{
				memcpy(cur, it->text.s, it->text.len);
				n += it->text.len;
				cur += it->text.len;
			} else {
				LM_ERR("no more space for text [%d]\n", it->text.len);
				goto overflow;
			}
		}
		/* put the value of the specifier */
		if(it->spec.type!=PVT_NONE
				&& pv_get_spec_value(msg, &(it->spec), &tok)==0)
		{
			if(tok.flags&PV_VAL_NULL)
				tok.rs = pv_str_null;
			if(n+tok.rs.len < *len)
			{
				if(tok.rs.len>0)
				{
					memcpy(cur, tok.rs.s, tok.rs.len);
					n += tok.rs.len;
					cur += tok.rs.len;
				}
			} else {
				LM_ERR("no more space for spec value\n");
				goto overflow;
			}
		}
	}

	goto done;
	
overflow:
	LM_ERR("buffer overflow -- increase the buffer size...\n");
	return -1;

done:
#ifdef EXTRA_DEBUG
	LM_DBG("final buffer length %d\n", n);
#endif
	*cur = '\0';
	*len = n;
	return 0;
}

/**
 *
 */
pvname_list_t* parse_pvname_list(str *in, unsigned int type)
{
	pvname_list_t* head = NULL;
	pvname_list_t* al = NULL;
	pvname_list_t* last = NULL;
	char *p;
	pv_spec_t spec;
	str s;

	if(in==NULL || in->s==NULL)
	{
		LM_ERR("bad parameters\n");
		return NULL;
	}

	p = in->s;
	while(is_in_str(p, in))
	{
		while(is_in_str(p, in) && (*p==' '||*p=='\t'||*p==','||*p==';'))
			p++;
		if(!is_in_str(p, in))
		{
			if(head==NULL)
				LM_ERR("wrong item name list [%.*s]\n", in->len, in->s);
			return head;
		}
		s.s=p;
		s.len = in->s+in->len-p;
		p = pv_parse_spec(&s, &spec);
		if(p==NULL || (type && spec.type!=type))
		{
			LM_ERR("wrong item name list [%.*s]!\n", in->len, in->s);
			goto error;
		}
		al = (pvname_list_t*)pkg_malloc(sizeof(pvname_list_t));
		if(al==NULL)
		{
			LM_ERR("no more memory!\n");
			goto error;
		}
		memset(al, 0, sizeof(pvname_list_t));
		memcpy(&al->sname, &spec, sizeof(pv_spec_t));

		if(last==NULL)
		{
			head = al;
			last = al;
		} else {
			last->next = al;
			last = al;
		}
	}

	return head;

error:
	while(head)
	{
		al = head;
		head=head->next;
		pkg_free(al);
	}
	return NULL;
}



/** frees only the contests of a pv_spec_t. */
void pv_spec_free_contents(pv_spec_t *spec)
{
	/* TODO: free name if it is PV */
	if(spec->trans)
		tr_free((trans_t*)spec->trans);
}



/**
 *
 */
void pv_spec_free(pv_spec_t *spec)
{
	if(spec==0) return;
	pv_spec_free_contents(spec);
	pkg_free(spec);
}

/**
 *
 */
int pv_elem_free_all(pv_elem_p log)
{
	pv_elem_p t;
	while(log)
	{
		t = log;
		log = log->next;
		pkg_free(t);
	}
	return 0;
}

/**
 *
 */
void pv_value_destroy(pv_value_t *val)
{
	if(val==0) return;
	if(val->flags&PV_VAL_PKG) pkg_free(val->rs.s);
	if(val->flags&PV_VAL_SHM) shm_free(val->rs.s);
	memset(val, 0, sizeof(pv_value_t));
}

#define PV_PRINT_BUF_SIZE  1024
#define PV_PRINT_BUF_NO    3
int pv_printf_s(struct sip_msg* msg, pv_elem_p list, str *s)
{
	static int buf_itr = 0;
	static char buf[PV_PRINT_BUF_NO][PV_PRINT_BUF_SIZE];

	s->s = buf[buf_itr];
	s->len = PV_PRINT_BUF_SIZE;
	buf_itr = (buf_itr+1)%PV_PRINT_BUF_NO;
	return pv_printf( msg, list, s->s, &s->len);
}

/********************************************************
 * Transformations API
 ********************************************************/

/**
 *
 */
static inline char* tr_get_class(str *in, char *p, str *tclass)
{
	tclass->s = p;
	while(is_in_str(p, in) && *p!=TR_CLASS_MARKER) p++;
	if(*p!=TR_CLASS_MARKER || tclass->s == p)
	{
		LM_ERR("invalid transformation: %.*s (%c)!\n", in->len, in->s, *p);
		return NULL;
	}
	tclass->len = p - tclass->s;
	p++;

	return p;
}

/**
 *
 */
static inline trans_t* tr_new(void)
{
	trans_t *t = NULL;

	t = (trans_t*)pkg_malloc(sizeof(trans_t));
	if(t == NULL)
	{
		LM_ERR("no more private memory\n");
		return NULL;
	}
	memset(t, 0, sizeof(trans_t));
	return t;
}

char* tr_lookup(str *in, trans_t **tr)
{
	char *p;
	char *p0;
	str tclass;
	tr_export_t *te = NULL; 
	trans_t *t = NULL;
	trans_t *t0 = NULL;
	str s;

	if(in==NULL || in->s==NULL || tr==NULL)
		return NULL;
	
	p = in->s;
	do {
		while(is_in_str(p, in) && (*p==' ' || *p=='\t' || *p=='\n')) p++;
		if(*p != TR_LBRACKET)
			break;
		p++;

		if((t = tr_new())==NULL) return NULL;

		if(t0==NULL) *tr = t;
		else t0->next = t;
		t0 = t;

		/* find transformation class */
		p = tr_get_class(in, p, &tclass);
		if(p==NULL) goto error;

		/* locate transformation */
		te = tr_lookup_class(&tclass);
		if(te==NULL)
		{
			LM_ERR("unknown transformation: [%.*s] in [%.*s]\n",
				tclass.len, tclass.s, in->len, in->s);
			goto error;
		}

		s.s = p; s.len = in->s + in->len - p;
		p0 = te->tparse(&s, t);
		if(p0==NULL)
			goto error;
		p = p0;

		if(*p != TR_RBRACKET)
		{
			LM_ERR("invalid transformation: %.*s | %c !!\n", in->len,
					in->s, *p);
			goto error;
		}

		p++;
		if(!is_in_str(p, in))
			break;
	} while(1);

	return p;
error:
	LM_ERR("error parsing [%.*s]\n", in->len, in->s);
	t = *tr;
	while(t)
	{
		t0 = t;
		t = t->next;
		tr_destroy(t0);
		pkg_free(t0);
	}
	return NULL;
}

/*!
 * \brief Destroy transformation including eventual parameter
 * \param t transformation
 */
void tr_destroy(trans_t *t)
{
	tr_param_t *tp;
	tr_param_t *tp0;
	if(t==NULL) return;

	tp = t->params;
	while(tp)
	{
		tp0 = tp;
		tp = tp->next;
		tr_param_free(tp0);
	}
	memset(t, 0, sizeof(trans_t));
}

/*!
 * \brief Exec transformation on a pseudo-variable value
 * \param msg SIP message
 * \param tr one or more transformations
 * \param val pseudo-variable value
 * \return 0 on success, -1 on error
 */

int tr_exec(struct sip_msg *msg, trans_t *t, pv_value_t *v)
{
	int r;
	trans_t *i;

	if(t==NULL || v==NULL)
	{
		LM_DBG("invalid parameters\n");
		return -1;
	}
	
	for(i = t; i!=NULL; i=i->next)
	{
		r = (*i->trf)(msg, i->params, i->subtype, v);
		if(r!=0)
			return r;
	}
	return 0;
}

/*!
 * \brief Free allocated memory of transformation list
 * \param t transformation list
 */
void tr_free(trans_t *t)
{
	trans_t *t0;

	while(t)
	{
		t0 = t;
		t = t->next;
		tr_destroy(t0);
		pkg_free(t0);
	}
}


/*!
 * \brief Free transformation parameter list
 * \param tp transformation list
 */
void tr_param_free(tr_param_t *tp)
{
	tr_param_t *tp0;

	if(tp==NULL) return;
	while(tp)
	{
		tp0 = tp;
		tp = tp->next;
		if(tp0->type==TR_PARAM_SPEC)
			pv_spec_free((pv_spec_t*)tp0->v.data);
		pkg_free(tp0);
	}
}

typedef struct _tr_item
{
	tr_export_t tre;
	unsigned int trid;
	struct _tr_item *next;
} tr_item_t, *tr_item_p;

static tr_item_t* _tr_table[TR_TABLE_SIZE];
static int _tr_table_set = 0;


/**
 *
 */
void tr_init_table(void)
{
	memset(_tr_table, 0, sizeof(tr_item_t*)*TR_TABLE_SIZE);
	_tr_table_set = 1;
}

/**
 *
 */
int tr_table_add(tr_export_t *e)
{
	tr_item_t *tri = NULL;
	tr_item_t *trj = NULL;
	tr_item_t *trn = NULL;
	int found;
	unsigned int trid;

	if(e==NULL || e->tclass.s==NULL)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}
	
	if(_tr_table_set==0)
	{
		LM_DBG("TR table not initialized, doing it now\n");
		tr_init_table();
	}

	found = 0;
	// trid = get_hash1_raw(e->tclass.s, e->tclass.len);
	trid = get_hash1_raw(e->tclass.s, e->tclass.len);

	tri = _tr_table[trid%TR_TABLE_SIZE];
	while(tri)
	{
		if(tri->tre.tclass.len==e->tclass.len)
		{
			found = strncmp(tri->tre.tclass.s, e->tclass.s, e->tclass.len);
			if(found==0)
			{
				LM_ERR("TR class [%.*s] already exists\n", e->tclass.len,
						e->tclass.s);
				return -1;
			}
		}
		trj = tri;
		tri = tri->next;
	}

	trn = (tr_item_t*)pkg_malloc(sizeof(tr_item_t));
	if(trn==0)
	{
		LM_ERR("no more memory\n");
		return -1;
	}
	memset(trn, 0, sizeof(tr_item_t));
	memcpy(&(trn->tre), e, sizeof(tr_export_t));
	trn->trid = trid;

	//LM_DBG("TR class [%.*s] added to entry [%d]\n", e->tclass.len,
	//					e->tclass.s, trid%TR_TABLE_SIZE);
	if(trj==0)
	{
		trn->next = _tr_table[trid%TR_TABLE_SIZE];
		_tr_table[trid%TR_TABLE_SIZE] = trn;
		goto done;
	}
	trn->next = trj->next;
	trj->next = trn;

done:
	return 0;
}

/**
 *
 */
int register_trans_mod(char *mod_name, tr_export_t *items)
{
	int ret;
	int i;

	if (items==0)
		return 0;

	for ( i=0 ; items[i].tclass.s ; i++ ) {
		ret = tr_table_add(&items[i]);
		if (ret!=0) {
			LM_ERR("failed to register pseudo-variable <%.*s> for module %s\n",
					items[i].tclass.len, items[i].tclass.s, mod_name);
		}
	}
	return 0;
}

/**
 *
 */
int tr_table_free(void)
{
	tr_item_p te;
	tr_item_p te1;
	int i;

	for(i=0; i<TR_TABLE_SIZE; i++)
	{
		te = _tr_table[i];
		while(te!=0)
		{
			te1 = te;
			te = te->next;
			pkg_free(te1);
		}
	}
	memset(_tr_table, 0, sizeof(tr_item_t*)*TR_TABLE_SIZE);
	_tr_table_set = 0;
	
	return 0;
}

tr_export_t* tr_lookup_class(str *tclass)
{
	tr_item_t *tri;
	unsigned int trid;

	if(tclass==0 || tclass->s==0)
	{
		LM_ERR("bad parameters\n");
		return NULL;
	}

	/* search in TR table */
	// trid = get_hash1_raw(tclass->s, tclass->len);
	trid = get_hash1_raw(tclass->s, tclass->len);
	tri = _tr_table[trid%TR_TABLE_SIZE];
	while(tri)
	{
		if(tri->trid==trid && tri->tre.tclass.len==tclass->len
				&& memcmp(tri->tre.tclass.s, tclass->s, tclass->len)==0)
			return &(tri->tre);
		tri = tri->next;
	}

	return NULL;
}


/** init pv api (optional).
 * @return 0 on success, -1 on error
 */
int init_pv_api(void)
{
	pv_init_table();
	tr_init_table();
	return 0;
}


/** destroy pv api. */
void destroy_pv_api(void)
{
	/* free PV and TR hash tables */
	pv_table_free();
	tr_table_free();
	return;
}
