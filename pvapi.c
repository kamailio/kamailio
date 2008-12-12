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
 * \brief PV API specification
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
#include "pvar.h"

#define is_in_str(p, in) (p<in->s+in->len && *p)

#define PV_TABLE_SIZE	16
#define TR_TABLE_SIZE	2


void tr_destroy(trans_t *t);
void tr_free(trans_t *t);
void tr_param_free(tr_param_t *tp);

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
void pv_init_table()
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
			|| (c=='_') || (c=='.'))
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
	int pvid;

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
	pvid = get_hash1_raw(in->s, in->len);

	pvi = _pv_table[pvid%PV_TABLE_SIZE];
	while(pvi)
	{
		if(pvi->pvid > pvid)
			break;
		if(pvi->pve.name.len > in->len)
			break;
		if(pvi->pve.name.len==in->len)
		{
			found = strncmp(pvi->pve.name.s, in->s, in->len);
			if(found>0)
				break;
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
		memset(_pv_table, 0, sizeof(pv_item_t*)*PV_TABLE_SIZE);
		_pv_table_set = 0;
	}
	
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

	ch = int2str(sival, &l);
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
	pvid = get_hash1_raw(pvname->s, pvname->len);
	pvi = _pv_table[pvid%PV_TABLE_SIZE];
	while(pvi)
	{
		if(pvi->pvid > pvid)
			break;
		if(pvi->pve.name.len > pvname->len)
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
void pv_spec_free(pv_spec_t *spec)
{
	if(spec==0) return;
	/* TODO: free name if it is PV */
	if(spec->trans)
		tr_free((trans_t*)spec->trans);
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
static inline trans_t* tr_new()
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

static tr_item_t* _tr_table[PV_TABLE_SIZE];
static int _tr_table_set = 0;


/**
 *
 */
void tr_init_table()
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
	int trid;

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
	trid = get_hash1_raw(e->tclass.s, e->tclass.len);

	tri = _tr_table[trid%PV_TABLE_SIZE];
	while(tri)
	{
		if(tri->trid > trid)
			break;
		if(tri->tre.tclass.len > e->tclass.len)
			break;
		if(tri->tre.tclass.len==e->tclass.len)
		{
			found = strncmp(tri->tre.tclass.s, e->tclass.s, e->tclass.len);
			if(found>0)
				break;
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

	if(trj==0)
	{
		trn->next = _tr_table[trid%PV_TABLE_SIZE];
		_tr_table[trid%PV_TABLE_SIZE] = trn;
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
		memset(_tr_table, 0, sizeof(tr_item_t*)*TR_TABLE_SIZE);
		_tr_table_set = 0;
	}
	
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
	trid = get_hash1_raw(tclass->s, tclass->len);
	tri = _tr_table[trid%TR_TABLE_SIZE];
	while(tri)
	{
		if(tri->trid > trid)
			break;
		if(tri->tre.tclass.len > tclass->len)
			break;

		if(tri->trid==trid && tri->tre.tclass.len==tclass->len
				&& memcmp(tri->tre.tclass.s, tclass->s, tclass->len)==0)
			return &(tri->tre);
		tri = tri->next;
	}

	return NULL;
}

