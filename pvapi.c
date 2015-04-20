/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 * \brief Kamailio core :: PV API specification
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
#include "pvapi.h"
#include "pvar.h"

#define PV_TABLE_SIZE	32  /*!< pseudo-variables table size */
#define TR_TABLE_SIZE	16  /*!< transformations table size */


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

static pv_cache_t* _pv_cache[PV_CACHE_SIZE];
static int _pv_cache_set = 0;

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
void pv_init_cache(void)
{
	memset(_pv_cache, 0, sizeof(pv_cache_t*)*PV_CACHE_SIZE);
	_pv_cache_set = 1;
}

/**
 *
 */
pv_cache_t **pv_cache_get_table(void)
{
	if(_pv_cache_set==1) return _pv_cache;
	return NULL;
}

/**
 * @brief Check if a char is valid according to the PV syntax
 * @param c checked char
 * @return 1 if char is valid, 0 if not valid
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
int pv_locate_name(str *in)
{
	int i;
	int pcount;

	if(in==NULL || in->s==NULL || in->len<2)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(in->s[0]!=PV_MARKER)
	{
		LM_ERR("missing pv marker [%.*s]\n", in->len, in->s);
		return -1;
	}
	if(in->s[1]==PV_MARKER)
	{
		return 2;
	}
	pcount = 0;
	if(in->s[1]==PV_LNBRACKET)
	{
		/* name with parenthesis: $(...) */
		pcount = 1;
		for(i=2; i<in->len; i++)
		{
			if(in->s[i]==PV_LNBRACKET)
				pcount++;
			else if(in->s[i]==PV_RNBRACKET)
				pcount--;
			if(pcount==0)
				return i+1;
		}
		/* non-closing name parenthesis */
		LM_ERR("non-closing name parenthesis [%.*s]\n",in->len,in->s);
		return -1;
	}

	/* name without parenthesis: $xyz(...) */
	for(i=1; i<in->len; i++)
	{
		if(!is_pv_valid_char(in->s[i]))
		{
			if(in->s[i]==PV_LNBRACKET)
			{
				/* inner-name parenthesis */
				pcount = 1;
				break;
			} else {
				return i;
			}
		}
	}
	if(pcount==0)
		return i;

	i++;
	for( ; i<in->len; i++)
	{
		if(in->s[i]==PV_LNBRACKET)
			pcount++;
		else if(in->s[i]==PV_RNBRACKET)
			pcount--;
		if(pcount==0)
			return i+1;
	}
	/* non-closing inner-name parenthesis */
	LM_ERR("non-closing inner-name parenthesis [%.*s]\n",in->len,in->s);
	return -1;
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
pv_spec_t* pv_cache_add(str *name)
{
	pv_cache_t *pvn;
	unsigned int pvid;
	char *p;

	if(_pv_cache_set==0)
	{
		LM_DBG("PV cache not initialized, doing it now\n");
		pv_init_cache();
	}
	pvid = get_hash1_raw(name->s, name->len);
	pvn = (pv_cache_t*)pkg_malloc(sizeof(pv_cache_t) + name->len + 1);
	if(pvn==0)
	{
		LM_ERR("no more memory\n");
		return NULL;
	}
	memset(pvn, 0, sizeof(pv_cache_t) + name->len + 1);
	pvn->pvname.len = name->len;
	pvn->pvname.s = (char*)pvn + sizeof(pv_cache_t);
	memcpy(pvn->pvname.s, name->s, name->len);
	p = pv_parse_spec(&pvn->pvname, &pvn->spec);

	if(p==NULL)
	{
		pkg_free(pvn);
		return NULL;
	}
	pvn->pvid = pvid;
	pvn->next = _pv_cache[pvid%PV_CACHE_SIZE];
	_pv_cache[pvid%PV_CACHE_SIZE] = pvn;

	LM_DBG("pvar [%.*s] added in cache\n", name->len, name->s);
	return &pvn->spec;
}

/**
 *
 */
pv_spec_t* pv_cache_lookup(str *name)
{
	pv_cache_t *pvi;
	unsigned int pvid;
	int found;

	if(_pv_cache_set==0)
		return NULL;

	pvid = get_hash1_raw(name->s, name->len);
	pvi = _pv_cache[pvid%PV_CACHE_SIZE];
	while(pvi)
	{
		if(pvi->pvid == pvid) {
			if(pvi->pvname.len==name->len)
			{
				found = strncmp(pvi->pvname.s, name->s, name->len);

				if(found==0)
				{
					LM_DBG("pvar [%.*s] found in cache\n",
							name->len, name->s);
					return &pvi->spec;
				}
			}
		}
		pvi = pvi->next;
	}
	return NULL;
}

/**
 *
 */
pv_spec_t* pv_cache_get(str *name)
{
	pv_spec_t *pvs;
	str tname;

	if(name->s==NULL || name->len==0)
	{
		LM_ERR("invalid parameters\n");
		return NULL;
	}

	tname.s = name->s;
	tname.len = pv_locate_name(name);

	if(tname.len < 0)
		return NULL;

	pvs = pv_cache_lookup(&tname);

	if(pvs!=NULL)
		return pvs;

	return pv_cache_add(&tname);
}

str* pv_cache_get_name(pv_spec_t *spec)
{
	int i;
	pv_cache_t *pvi;
	if(spec==NULL)
	{
		LM_ERR("invalid parameters\n");
		return NULL;
	}

	if(_pv_cache_set==0)
		return NULL;

	for(i=0;i<PV_CACHE_SIZE;i++)
	{
		pvi = _pv_cache[i];
		while(pvi)
		{
			if(&pvi->spec == spec)
			{
				LM_DBG("pvar[%p]->name[%.*s] found in cache\n", spec,
					pvi->pvname.len, pvi->pvname.s);
				return &pvi->pvname;
			}
			pvi = pvi->next;
		}
	}
	return NULL;
}

/**
 *
 */
pv_spec_t* pv_spec_lookup(str *name, int *len)
{
	pv_spec_t *pvs;
	str tname;

	if(len!=NULL)
		*len = 0;
	if(name->s==NULL || name->len==0)
	{
		LM_ERR("invalid parameters\n");
		return NULL;
	}

	tname.s = name->s;
	tname.len = pv_locate_name(name);

	if(tname.len < 0)
		return NULL;

	if(len!=NULL)
		*len = tname.len;

	pvs = pv_cache_lookup(&tname);

	if(pvs!=NULL)
		return pvs;

	LM_DBG("PV <%.*s> is not in cache\n", tname.len, tname.s);
	return pv_cache_add(&tname);
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
 * convert strz to pv_value_t
 */
int pv_get_strzval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, char *sval)
{
	if(res==NULL)
		return -1;

	res->rs.s = sval;
	res->rs.len = strlen(sval);
	res->flags = PV_VAL_STR;
	return 0;
}

/**
 * convert char* with len to pv_value_t
 */
int pv_get_strlval(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res, char *sval, int slen)
{
	if(res==NULL)
		return -1;

	res->rs.s = sval;
	res->rs.len = slen;
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

/**
 *
 */
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

/**
 *
 */
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
	if(*p=='+' && in->len==1)
	{
		sp->pvp.pvi.type = PV_IDX_ITR;
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

/**
 *
 */
int pv_init_iname(pv_spec_p sp, int param)
{
	if(sp==NULL)
		return -1;
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.name.n = param;
	return 0;
}

/**
 *
 */
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
	int len;

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
		e->spec = pv_spec_lookup(&s, &len);
		if(e->spec==NULL)
			goto error;
		p0 = p + len;
		
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

/**
 *
 */
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

/**
 * parse AVP name
 * @return 0 on success, -1 on error
 */
int pv_parse_avp_name(pv_spec_p sp, str *in)
{
	char *p;
	char *s;
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
			LM_ERR("invalid name [%.*s]\n", in->len, in->s);
			pv_spec_free(nsp);
			return -1;
		}
		//LM_ERR("dynamic name [%.*s]\n", in->len, in->s);
		//pv_print_spec(nsp);
		sp->pvp.pvn.type = PV_NAME_PVAR;
		sp->pvp.pvn.u.dname = (void*)nsp;
		return 0;
	}
	/*LM_DBG("static name [%.*s]\n", in->len, in->s);*/
	if(km_parse_avp_spec(in, &sp->pvp.pvn.u.isname.type,
					  &sp->pvp.pvn.u.isname.name)!=0)
	{
		LM_ERR("bad avp name [%.*s]\n", in->len, in->s);
		return -1;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	return 0;
}

/**
 * fill avp name details (id and type)
 * @return 0 on success, -1 on error
 */
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

/**
 *
 */
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
	if(ip->pvi.type == PV_IDX_ITR) {
		*flags = PV_IDX_ITR;
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

/**
 *
 */
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

/**
 *
 */
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
	int n;
	pv_value_t tok;
	pv_elem_p it;
	char *cur;
	
	if(msg==NULL || list==NULL || buf==NULL || len==NULL)
		return -1;

	if(*len <= 0)
		return -1;

	*buf = '\0';
	cur = buf;
	
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
		if(it->spec!=NULL && it->spec->type!=PVT_NONE
				&& pv_get_spec_value(msg, it->spec, &tok)==0)
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
		while(is_in_str(p, in) && (*p==' '||*p=='\t'||*p==','||*p==';'||*p=='\n'))
			p++;
		if(!is_in_str(p, in))
		{
			if(head==NULL)
				LM_ERR("parse error in name list [%.*s]\n", in->len, in->s);
			return head;
		}
		s.s = p;
		s.len = in->s + in->len - p;
		p = pv_parse_spec(&s, &spec);
		if(p==NULL)
		{
			LM_ERR("parse error in item [%.*s]\n", s.len, s.s);
			goto error;
		}
		if(type && spec.type!=type)
		{
			LM_ERR("wrong type for item [%.*s]\n", (int)(p-s.s), s.s);
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



/** destroy the content of pv_spec_t structure.
 */
void pv_spec_destroy(pv_spec_t *spec)
{
	if(spec==0) return;
	/* free name if it is PV */
	if(spec->pvp.pvn.nfree)
		spec->pvp.pvn.nfree((void*)(&spec->pvp.pvn));
	if(spec->trans)
		tr_free((trans_t*)spec->trans);
}

/** free the pv_spec_t structure.
 */
void pv_spec_free(pv_spec_t *spec)
{
	if(spec==0) return;
	pv_spec_destroy(spec);
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
	else if(val->flags&PV_VAL_SHM) shm_free(val->rs.s);

	memset(val, 0, sizeof(pv_value_t));
}

int pv_printf_s(struct sip_msg* msg, pv_elem_p list, str *s)
{
	s->s = pv_get_buffer();
	s->len = pv_get_buffer_size();
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
 * \param t one or more transformations
 * \param v pseudo-variable value
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

/**
 *
 */
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


/********************************************************
 * core PVs, initialization and destroy APIs
 ********************************************************/

static pv_export_t _core_pvs[] = {
	{{"null", (sizeof("null")-1)}, /* */
		PVT_NULL, pv_get_null, 0,
		0, 0, 0, 0},

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

/** init pv api (optional).
 * @return 0 on success, -1 on error
 */
int pv_init_api(void)
{
	pv_init_table();
	tr_init_table();
	if(pv_init_buffer()<0)
		return -1;
	if(register_pvars_mod("core", _core_pvs)<0)
		return -1;
	return 0;
}


/** destroy pv api. */
void pv_destroy_api(void)
{
	/* free PV and TR hash tables */
	pv_table_free();
	tr_table_free();
	pv_destroy_buffer();
	return;
}

/**
 * - buffer to print PVs
 */
static char **_pv_print_buffer = NULL;
#define PV_DEFAULT_PRINT_BUFFER_SIZE 8192 /* 8kB */
static int _pv_print_buffer_size  = PV_DEFAULT_PRINT_BUFFER_SIZE;
static int _pv_print_buffer_size_active  = 0;
/* 6 mod params + 4 direct usage from mods */
#define PV_DEFAULT_PRINT_BUFFER_SLOTS 10
static int _pv_print_buffer_slots = PV_DEFAULT_PRINT_BUFFER_SLOTS;
static int _pv_print_buffer_slots_active = 0;
static int _pv_print_buffer_index = 0;

/**
 *
 */
int pv_init_buffer(void)
{
	int i;

	/* already initialized ?!? */
	if(_pv_print_buffer!=NULL)
		return 0;

	_pv_print_buffer =
		(char**)pkg_malloc(_pv_print_buffer_slots*sizeof(char*));
	if(_pv_print_buffer==NULL)
	{
		LM_ERR("cannot init PV print buffer slots\n");
		return -1;
	}
	memset(_pv_print_buffer, 0, _pv_print_buffer_slots*sizeof(char*));
	for(i=0; i<_pv_print_buffer_slots; i++)
	{
		_pv_print_buffer[i] =
			(char*)pkg_malloc(_pv_print_buffer_size*sizeof(char));
		if(_pv_print_buffer[i]==NULL)
		{
			LM_ERR("cannot init PV print buffer slot[%d]\n", i);
			return -1;
		}
	}
	LM_DBG("PV print buffer initialized to [%d][%d]\n",
			_pv_print_buffer_slots, _pv_print_buffer_size);
	_pv_print_buffer_slots_active = _pv_print_buffer_slots;
	_pv_print_buffer_size_active = _pv_print_buffer_size;

	return 0;
}

/**
 *
 */
void pv_destroy_buffer(void)
{
	int i;

	if(_pv_print_buffer==NULL)
		return;
	for(i=0; i<_pv_print_buffer_slots_active; i++)
	{
		if(_pv_print_buffer[i]!=NULL)
			pkg_free(_pv_print_buffer[i]);
	}
	pkg_free(_pv_print_buffer);
	_pv_print_buffer_slots_active = 0;
	_pv_print_buffer_size_active = 0;
	_pv_print_buffer = NULL;
}

/**
 *
 */
int pv_reinit_buffer(void)
{
	if(_pv_print_buffer_size==_pv_print_buffer_size_active
			&& _pv_print_buffer_slots==_pv_print_buffer_slots_active)
		return 0;
	pv_destroy_buffer();
	return pv_init_buffer();
}

/**
 *
 */
char* pv_get_buffer(void)
{
	char *p;

	p = _pv_print_buffer[_pv_print_buffer_index];
	_pv_print_buffer_index = (_pv_print_buffer_index+1)%_pv_print_buffer_slots;

	return p;
}

/**
 *
 */
int pv_get_buffer_size(void)
{
	return _pv_print_buffer_size;
}

/**
 *
 */
int pv_get_buffer_slots(void)
{
	return _pv_print_buffer_slots;
}

/**
 *
 */
void pv_set_buffer_size(int n)
{
	_pv_print_buffer_size = n;
	if(_pv_print_buffer_size<=0)
		_pv_print_buffer_size = PV_DEFAULT_PRINT_BUFFER_SIZE;
}

/**
 *
 */
void pv_set_buffer_slots(int n)
{
	_pv_print_buffer_slots = n;
	if(_pv_print_buffer_slots<=0)
		_pv_print_buffer_slots = PV_DEFAULT_PRINT_BUFFER_SLOTS;
}
