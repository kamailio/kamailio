/**
 * $Id$
 *
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../parser/parse_param.h"
#include "../../ut.h"
#include "../../pvar.h"
#include "../../lvalue.h"
#include "../../shm_init.h"

#include "mtree.h"

//extern str mt_char_list = {"1234567890*",11};
extern str mt_char_list;
extern pv_spec_t pv_value;
extern pv_spec_t pv_values;
extern pv_spec_t pv_dstid;
extern pv_spec_t pv_weight;
extern int _mt_tree_type;
extern int _mt_ignore_duplicates;
extern int _mt_allow_duplicates;

/** structures containing prefix-value pairs */
static m_tree_t **_ptree = NULL; 

/* quick transaltion table */
unsigned char _mt_char_table[256];

/**
 *
 */
void mt_char_table_init(void)
{
	unsigned int i;
	for(i=0; i<=255; i++)
		_mt_char_table[i] = 255;
	for(i=0; i<mt_char_list.len; i++)
		_mt_char_table[(unsigned int)mt_char_list.s[i]] = (unsigned char)i;
}


/**
 *
 */
m_tree_t *mt_swap_list_head(m_tree_t *ntree)
{
	m_tree_t *otree;

	otree = *_ptree;
	*_ptree = ntree;

	return otree;
}

/**
 *
 */
int mt_init_list_head(void)
{
	if(_ptree!=NULL)
		return 0;
	_ptree = (m_tree_t**)shm_malloc( sizeof(m_tree_t*) );
	if (_ptree==0) {
		LM_ERR("out of shm mem for pdtree\n");
		return -1;
	}
	*_ptree=0;
	return 0;
}

/**
 *
 */
m_tree_t* mt_init_tree(str* tname, str *dbtable, int type, int multi)
{
	m_tree_t *pt = NULL;

	pt = (m_tree_t*)shm_malloc(sizeof(m_tree_t));
	if(pt==NULL)
	{
		LM_ERR("no more shm memory\n");
		return NULL;
	}
	memset(pt, 0, sizeof(m_tree_t));

	pt->type = type;
	pt->multi = multi;
	pt->reload_time = (unsigned int)time(NULL);
	pt->tname.s = (char*)shm_malloc((1+tname->len)*sizeof(char));
	if(pt->tname.s==NULL)
	{
		shm_free(pt);
		LM_ERR("no more shm memory\n");
		return NULL;
	}
	memset(pt->tname.s, 0, 1+tname->len);
	memcpy(pt->tname.s, tname->s, tname->len);
	pt->tname.len = tname->len;

	pt->dbtable.s = (char*)shm_malloc((1+dbtable->len)*sizeof(char));
	if(pt->dbtable.s==NULL)
	{
		shm_free(pt->tname.s);
		shm_free(pt);
		LM_ERR("no more shm memory\n");
		return NULL;
	}
	memset(pt->dbtable.s, 0, 1+dbtable->len);
	memcpy(pt->dbtable.s, dbtable->s, dbtable->len);
	pt->dbtable.len = dbtable->len;

	return pt;
}

int mt_add_to_tree(m_tree_t *pt, str *sp, str *svalue)
{
	int l, ivalue = 0;
	mt_node_t *itn, *itn0;
	mt_is_t *tvalues;

	if(pt==NULL || sp==NULL || sp->s==NULL
			|| svalue==NULL || svalue->s==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(sp->len>=MT_MAX_DEPTH)
	{
		LM_ERR("max prefix len exceeded\n");
		return -1;
	}

	LM_DBG("adding to tree <%.*s> of type <%d>\n", pt->tname.len,
	       pt->tname.s, pt->type);

	if ((pt->type == MT_TREE_IVAL) && (str2sint(svalue, &ivalue) != 0)) {
		LM_ERR("bad integer string <%.*s>\n", svalue->len, svalue->s);
		return -1;
	}

	l = 0;
	if(pt->head == NULL)
	{
		pt->head = (mt_node_t*)shm_malloc(MT_NODE_SIZE*sizeof(mt_node_t));
		if(pt->head == NULL)
		{
			LM_ERR("no more shm memory for tree head\n");
			return -1;
		}
		memset(pt->head, 0, MT_NODE_SIZE*sizeof(mt_node_t));
		pt->nrnodes++;
		pt->memsize +=  MT_NODE_SIZE*sizeof(mt_node_t);
	}

	itn0 = pt->head;
	if(_mt_char_table[(unsigned int)sp->s[l]]==255)
	{
		LM_ERR("invalid char %d in prefix [%c (0x%x)]\n",
				l, sp->s[l], sp->s[l]);
		return -1;			
	}
	itn = itn0[_mt_char_table[(unsigned int)sp->s[l]]].child;

	while(l < sp->len-1)
	{
		if(itn == NULL)
		{
			itn = (mt_node_t*)shm_malloc(MT_NODE_SIZE*sizeof(mt_node_t));
			if(itn == NULL)
			{
				LM_ERR("no more shm mem\n");
				return -1;
			}
			memset(itn, 0, MT_NODE_SIZE*sizeof(mt_node_t));
			pt->nrnodes++;
			pt->memsize +=  MT_NODE_SIZE*sizeof(mt_node_t);
			itn0[_mt_char_table[(unsigned int)sp->s[l]]].child = itn;
		}
		l++;	
		if(_mt_char_table[(unsigned int)sp->s[l]]==255)
		{
			LM_ERR("invalid char %d in prefix [%c (0x%x)]\n",
					l, sp->s[l], sp->s[l]);
			return -1;			
		}
		itn0 = itn;
		itn = itn0[_mt_char_table[(unsigned int)sp->s[l]]].child;
	}

	if(itn0[_mt_char_table[(unsigned int)sp->s[l]]].tvalues != NULL) {
		if(_mt_ignore_duplicates != 0) {
			LM_NOTICE("prefix already allocated [%.*s/%.*s]\n",
					sp->len, sp->s, svalue->len, svalue->s);
			return 1;
		} else if (_mt_allow_duplicates == 0) {
			LM_ERR("prefix already allocated [%.*s/%.*s]\n",
					sp->len, sp->s, svalue->len, svalue->s);
			return -1;
		}
	}

	tvalues = (mt_is_t *)shm_malloc(sizeof(mt_is_t));
	if (tvalues == NULL) {
		LM_ERR("no more shm mem for tvalue\n");
		return -1;
	}
	memset(tvalues, 0, sizeof(mt_is_t));

	if (pt->type == MT_TREE_IVAL) {
		tvalues->tvalue.n = ivalue;
	} else { /* pt->type == MT_TREE_SVAL or MT_TREE_DW */
		tvalues->tvalue.s.s = (char*)shm_malloc((svalue->len+1)*sizeof(char));
		if (tvalues->tvalue.s.s == NULL) {
			LM_ERR("no more shm mem for string\n");
			return -1;
		}
		tvalues->tvalue.s.len = svalue->len;
		pt->memsize +=  (svalue->len+1)*sizeof(char);
		pt->nritems++;
		strncpy(tvalues->tvalue.s.s, svalue->s, svalue->len);
		tvalues->tvalue.s.s[svalue->len] = '\0';
	}
	tvalues->next = itn0[_mt_char_table[(unsigned int)sp->s[l]]].tvalues;
	itn0[_mt_char_table[(unsigned int)sp->s[l]]].tvalues = tvalues;

	mt_node_set_payload(&itn0[_mt_char_table[(unsigned int)sp->s[l]]],
			pt->type);
	return 0;
}

m_tree_t* mt_get_tree(str *tname)
{
	m_tree_t *it;
	int ret;

	if(_ptree==NULL || *_ptree==NULL)
		return NULL;

	if( tname==NULL || tname->s==NULL)
	{
		LM_ERR("bad parameters\n");
		return NULL;
	}

	it = *_ptree;
	/* search the tree for the asked tname */
	while(it!=NULL)
	{
		ret = str_strcmp(&it->tname, tname);
		if(ret>0)
			return NULL;
		if(ret==0)
			return it;
		it = it->next;
	}

	return it;
}

m_tree_t* mt_get_first_tree()
{
	if(_ptree==NULL || *_ptree==NULL)
		return NULL;
	return *_ptree;
}


is_t* mt_get_tvalue(m_tree_t *pt, str *tomatch, int *len)
{
	int l;
	mt_node_t *itn;
	is_t *tvalue;

	if(pt==NULL || tomatch==NULL || tomatch->s==NULL || len == NULL)
	{
		LM_ERR("bad parameters\n");
		return NULL;
	}

	l = 0;
	itn = pt->head;
	tvalue = NULL;

	while(itn!=NULL && l < tomatch->len && l < MT_MAX_DEPTH)
	{
		/* check validity */
		if(_mt_char_table[(unsigned int)tomatch->s[l]]==255)
		{
			LM_DBG("not matching char at %d in [%.*s]\n",
					l, tomatch->len, tomatch->s);
			return NULL;
		}

		if(itn[_mt_char_table[(unsigned int)tomatch->s[l]]].tvalues!=NULL)
		{
			tvalue = &itn[_mt_char_table[(unsigned int)tomatch->s[l]]].tvalues->tvalue;
		}

		itn = itn[_mt_char_table[(unsigned int)tomatch->s[l]]].child;
		l++;	
	}

	*len = l;

	return tvalue;
}

int mt_add_tvalues(struct sip_msg *msg, m_tree_t *pt, str *tomatch)
{
        int l, n;
	mt_node_t *itn;
	int_str val, values_avp_name;
	unsigned short values_name_type;
	mt_is_t *tvalues;

	if (pt == NULL || tomatch == NULL || tomatch->s == NULL) {
		LM_ERR("bad parameters\n");
		return -1;
	}

	if (pv_get_avp_name(msg, &pv_values.pvp, &values_avp_name,
				&values_name_type) < 0) {
		LM_ERR("cannot get values avp name\n");
		return -1;
	}

	destroy_avps(values_name_type, values_avp_name, 1);

	l = n = 0;
	itn = pt->head;

	while (itn != NULL && l < tomatch->len && l < MT_MAX_DEPTH) {
		/* check validity */
		if(_mt_char_table[(unsigned int)tomatch->s[l]]==255) {
			LM_ERR("invalid char at %d in [%.*s]\n",
					l, tomatch->len, tomatch->s);
			return -1;
		}
		tvalues = itn[_mt_char_table[(unsigned int)tomatch->s[l]]].tvalues;
		while (tvalues != NULL) {
			if (pt->type == MT_TREE_IVAL) {
				val.n = tvalues->tvalue.n;
				LM_DBG("adding avp <%.*s> with value <i:%d>\n",
						values_avp_name.s.len, values_avp_name.s.s, val.n);
				add_avp(values_name_type, values_avp_name, val);
			} else {  /* pt->type == MT_TREE_SVAL */
				val.s = tvalues->tvalue.s;
				LM_DBG("adding avp <%.*s> with value <s:%.*s>\n",
						values_avp_name.s.len, values_avp_name.s.s, val.s.len,
						val.s.s);
				add_avp(values_name_type|AVP_VAL_STR, values_avp_name, val);
			}
			n++;
			tvalues = tvalues->next;
		}

		itn = itn[_mt_char_table[(unsigned int)tomatch->s[l]]].child;
		l++;	
	}

	if (n > 0)
	        return 0;
	else
	        return -1;
}

int mt_match_prefix(struct sip_msg *msg, m_tree_t *it,
		str *tomatch, int mode)
{
	int l, len, n;
	int i, j, k = 0;
	mt_node_t *itn;
	is_t *tvalue;
	int_str dstid_avp_name;
	unsigned short dstid_name_type;
	int_str weight_avp_name;
	unsigned short weight_name_type;
	int_str avp_value;
	mt_dw_t *dw;
	pv_value_t val;

#define MT_MAX_DST_LIST	64
	unsigned int tmp_list[2*(MT_MAX_DST_LIST+1)];

	if(it==NULL || tomatch == NULL
			|| tomatch->s == NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	l = len = 0;
	n = 0;
	if ((it->type==MT_TREE_SVAL) || (it->type==MT_TREE_IVAL)) {
		if (mode == 2) 
			return mt_add_tvalues(msg, it, tomatch);
		tvalue = mt_get_tvalue(it, tomatch, &k);
		if (tvalue == NULL) {
			LM_DBG("no match for: %.*s\n", tomatch->len, tomatch->s);
			return -1;
		}
		memset(&val, 0, sizeof(pv_value_t));
		if (it->type==MT_TREE_SVAL) {
			val.flags = PV_VAL_STR;
			val.rs = tvalue->s;
			if(pv_value.setf(msg, &pv_value.pvp, (int)EQ_T, &val)<0) {
				LM_ERR("setting PV failed\n");
				return -2;
			}
		} else {
			val.flags = PV_VAL_INT;
			val.ri = tvalue->n;
			if(pv_value.setf(msg, &pv_value.pvp, (int)EQ_T, &val)<0) {
				LM_ERR("setting PV failed\n");
				return -2;
			}
		}
		return 0;
	}

	if(it->type!=MT_TREE_DW)
		return -1; /* wrong tree type */

	if(pv_get_avp_name(msg, &pv_dstid.pvp, &dstid_avp_name,
				&dstid_name_type)<0)
	{
		LM_ERR("cannot get dstid avp name\n");
		return -1;
	}
	if(pv_get_avp_name(msg, &pv_weight.pvp, &weight_avp_name,
				&weight_name_type)<0)
	{
		LM_ERR("cannot get weight avp name\n");
		return -1;
	}

	itn = it->head;
	memset(tmp_list, 0, sizeof(unsigned int)*2*(MT_MAX_DST_LIST+1));

	while(itn!=NULL && l < tomatch->len && l < MT_MAX_DEPTH)
	{
		/* check validity */
		if(_mt_char_table[(unsigned int)tomatch->s[l]]==255)
		{
			LM_ERR("invalid char at %d in [%.*s]\n",
					l, tomatch->len, tomatch->s);
			return -1;
		}

		if(itn[_mt_char_table[(unsigned int)tomatch->s[l]]].tvalues!=NULL)
		{
			dw = (mt_dw_t*)itn[_mt_char_table[(unsigned int)tomatch->s[l]]].data;
			while(dw) {
				tmp_list[2*n]=dw->dstid;
				tmp_list[2*n+1]=dw->weight;
				n++;
				if(n==MT_MAX_DST_LIST)
					break;
				dw = dw->next;
			}
			len = l+1;
		}
		if(n==MT_MAX_DST_LIST)
			break;

		itn = itn[_mt_char_table[(unsigned int)tomatch->s[l]]].child;
		l++;	
	}

	if(n==0)
		return -1; /* no match */
	/* invalidate duplicated dstid, keeping longest match */
	for(i=(n-1); i>0; i--)
	{
		if (tmp_list[2*i]!=0)
		{
			for(j=0; j<i; j++)
				if(tmp_list[2*i]==tmp_list[2*j])
					tmp_list[2*j] = 0;
		}
	}
	/* sort the table -- bubble sort -- reverse order */
	for (i = (n - 1); i >= 0; i--)
	{
		for (j = 1; j <= i; j++)
		{
			if (tmp_list[2*(j-1)+1] < tmp_list[2*j+1])
			{
				tmp_list[2*MT_MAX_DST_LIST]   = tmp_list[2*(j-1)];
				tmp_list[2*MT_MAX_DST_LIST+1] = tmp_list[2*(j-1)+1];
				tmp_list[2*(j-1)]   = tmp_list[2*j];
				tmp_list[2*(j-1)+1] = tmp_list[2*j+1];
				tmp_list[2*j]   = tmp_list[2*MT_MAX_DST_LIST];
				tmp_list[2*j+1] = tmp_list[2*MT_MAX_DST_LIST+1];
			}
		}
	}
	/* add as avp */
	for(i=0; i<n; i++)
	{
		if(tmp_list[2*i]!=0)
		{
			avp_value.n = (int)tmp_list[2*i+1];
			add_avp(weight_name_type, weight_avp_name, avp_value);
			avp_value.n = (int)tmp_list[2*i];
			add_avp(dstid_name_type, dstid_avp_name, avp_value);
		}
	}

	return 0;
}

void mt_free_node(mt_node_t *pn, int type)
{
	int i;
	mt_is_t *tvalues, *next;

	if(pn==NULL)
		return;

	for(i=0; i<MT_NODE_SIZE; i++) {
		tvalues = pn[i].tvalues;
		while (tvalues != NULL) {
			if ((type == MT_TREE_SVAL) && (tvalues->tvalue.s.s != NULL)) {
				shm_free(tvalues->tvalue.s.s);
				tvalues->tvalue.s.s   = NULL;
				tvalues->tvalue.s.len = 0;
			}
			next = tvalues->next;
			shm_free(tvalues);
			tvalues = next;
		}
		if(type==MT_TREE_DW)
			mt_node_unset_payload(&pn[i], type);
		if(pn[i].child!=NULL) {
			mt_free_node(pn[i].child, type);
			pn[i].child = NULL;
		}
	}
	shm_free(pn);
	pn = NULL;

	return;
}

void mt_free_tree(m_tree_t *pt)
{
	if(pt == NULL)
		return;

	if(pt->head!=NULL) 
		mt_free_node(pt->head, pt->type);
	if(pt->next!=NULL)
		mt_free_tree(pt->next);
	if(pt->dbtable.s!=NULL)
		shm_free(pt->dbtable.s);
	if(pt->tname.s!=NULL)
		shm_free(pt->tname.s);

	shm_free(pt);
	pt = NULL;
	return;
}

int mt_print_node(mt_node_t *pn, char *code, int len, int type)
{
	int i;
	mt_is_t *tvalues;

	if(pn==NULL || code==NULL || len>=MT_MAX_DEPTH)
		return 0;

	for(i=0; i<MT_NODE_SIZE; i++)
	{
		code[len]=mt_char_list.s[i];
		tvalues = pn[i].tvalues;
		while (tvalues != NULL) {
			if (type == MT_TREE_IVAL) {
				LM_INFO("[%.*s] [i:%d]\n",	len+1, code, tvalues->tvalue.n);
			} else if (tvalues->tvalue.s.s != NULL) {
				LM_INFO("[%.*s] [s:%.*s]\n",
						len+1, code, tvalues->tvalue.s.len, tvalues->tvalue.s.s);
			}
			tvalues = tvalues->next;
		}
		mt_print_node(pn[i].child, code, len+1, type);
	}

	return 0;
}

static char mt_code_buf[MT_MAX_DEPTH+1];
int mt_print_tree(m_tree_t *pt)
{
	int len;

	if(pt == NULL)
	{
		LM_DBG("tree is empty\n");
		return 0;
	}

	LM_INFO("[%.*s]\n", pt->tname.len, pt->tname.s);
	len = 0;
	mt_print_node(pt->head, mt_code_buf, len, pt->type);
	return mt_print_tree(pt->next);
}

int mt_node_set_payload(mt_node_t *node, int type)
{
	param_t *list;
	param_t *it;
	param_hooks_t hooks;
	str s;
	mt_dw_t *dwl;
	mt_dw_t *dw;

	if(type!=MT_TREE_DW)
		return 0;
	s = node->tvalues->tvalue.s;
	if(s.s[s.len-1]==';')
		s.len--;
	if(parse_params(&s, CLASS_ANY, &hooks, &list)<0)
	{
		LM_ERR("cannot parse tvalue payload [%.*s]\n", s.len, s.s);
		return -1;
	}
	dwl = NULL;
	for(it=list; it; it=it->next)
	{
		dw = (mt_dw_t*)shm_malloc(sizeof(mt_dw_t));
		if(dw==NULL)
		{
			LM_ERR("no more shm\n");
			goto error;
		}
		memset(dw, 0, sizeof(mt_dw_t));
		str2int(&it->name, &dw->dstid);
		str2int(&it->body, &dw->weight);
		dw->next = dwl;
		dwl = dw;
	}
	node->data = (void*)dwl;
	free_params(list);
	return 0;
error:
	while(dwl)
	{
		dw=dwl;
		dwl=dwl->next;
		shm_free(dwl);
	}
	free_params(list);
	return -1;
}

int mt_node_unset_payload(mt_node_t *node, int type)
{
	mt_dw_t *dwl;
	mt_dw_t *dw;

	if(type!=MT_TREE_DW)
		return 0;
	dwl = (mt_dw_t*)node->data;
	while(dwl)
	{
		dw=dwl;
		dwl=dwl->next;
		shm_free(dw);
	}
	node->data = NULL;
	return 0;
}

int mt_table_spec(char* val)
{
	param_t* params_list = NULL;
	param_hooks_t phooks;
	param_t *pit=NULL;
	m_tree_t tmp;
	m_tree_t *it, *prev, *ndl;
	str s;

	if(val==NULL)
		return -1;

	if(!shm_initialized())
	{
		LM_ERR("shm not intialized - cannot define mtree now\n");
		return 0;
	}

	s.s = val;
	s.len = strlen(s.s);
	if(s.s[s.len-1]==';')
		s.len--;
	if (parse_params(&s, CLASS_ANY, &phooks, &params_list)<0)
		return -1;
	memset(&tmp, 0, sizeof(m_tree_t));
	for (pit = params_list; pit; pit=pit->next)
	{
		if (pit->name.len==4
				&& strncasecmp(pit->name.s, "name", 4)==0) {
			tmp.tname = pit->body;
		} else if(pit->name.len==4
				&& strncasecmp(pit->name.s, "type", 4)==0) {
			str2sint(&pit->body, &tmp.type);
		} else if(pit->name.len==5
				&& strncasecmp(pit->name.s, "multi", 5)==0) {
			str2sint(&pit->body, &tmp.multi);
		}  else if(pit->name.len==7
				&& strncasecmp(pit->name.s, "dbtable", 7)==0) {
			tmp.dbtable = pit->body;
		}
	}
	if(tmp.tname.s==NULL)
	{
		LM_ERR("invalid mtree name\n");
		goto error;
	}
	if(tmp.dbtable.s==NULL)
	{
		LM_INFO("no table name - default mtree\n");
		tmp.dbtable.s = "mtree";
		tmp.dbtable.len = 5;
	}
	if ((tmp.type != 0) && (tmp.type != 1) && (tmp.type != 2)) {
		LM_ERR("unknown tree type <%d>\n", tmp.type);
		goto error;
	}
	if ((tmp.multi != 0) && (tmp.multi != 1)) {
		LM_ERR("unknown multi value <%d>\n", tmp.multi);
		goto error;
	}
	
	/* check for same tree */
	if(_ptree == 0)
	{
		/* tree list head in shm */
		_ptree = (m_tree_t**)shm_malloc( sizeof(m_tree_t*) );
		if (_ptree==0)
		{
			LM_ERR("out of shm mem for ptree\n");
			goto error;
		}
		*_ptree=0;
	}
	it = *_ptree;
	prev = NULL;
	/* search the it position before which to insert new tvalue */
	while(it!=NULL && str_strcmp(&it->tname, &tmp.tname)<0)
	{	
		prev = it;
		it = it->next;
	}

	/* found */
	if(it!=NULL && str_strcmp(&it->tname, &tmp.tname)==0)
	{
		LM_ERR("duplicate tree with name [%s]\n", tmp.tname.s);
		goto error; 
	}
	/* add new tname*/
	if(it==NULL || str_strcmp(&it->tname, &tmp.tname)>0)
	{
		LM_DBG("adding new tname [%s]\n", tmp.tname.s);

		ndl = mt_init_tree(&tmp.tname, &tmp.dbtable, tmp.type,
				   tmp.multi);
		if(ndl==NULL)
		{
			LM_ERR("no more shm memory\n");
			goto error; 
		}

		ndl->next = it;

		/* new tvalue must be added as first element */
		if(prev==NULL)
			*_ptree = ndl;
		else
			prev->next=ndl;

	}

	free_params(params_list);
	return 0;
error:
	free_params(params_list);
	return -1;
}

m_tree_t *mt_add_tree(m_tree_t **dpt, str *tname, str *dbtable, int type,
		      int multi)
{
	m_tree_t *it = NULL;
	m_tree_t *prev = NULL;
	m_tree_t *ndl = NULL;

	if(dpt==NULL)
		return NULL;

	it = *dpt;
	prev = NULL;
	/* search the it position before which to insert new tvalue */
	while(it!=NULL && str_strcmp(&it->tname, tname)<0)
	{
		prev = it;
		it = it->next;
	}

	if(it!=NULL && str_strcmp(&it->tname, tname)==0)
	{
		return it;
	}
	/* add new tname*/
	if(it==NULL || str_strcmp(&it->tname, tname)>0)
	{
		LM_DBG("adding new tname [%s]\n", tname->s);

		ndl = mt_init_tree(tname, dbtable, type, multi);
		if(ndl==NULL)
		{
			LM_ERR("no more shm memory\n");
			return NULL;
		}

		ndl->next = it;

		/* new tvalue must be added as first element */
		if(prev==NULL)
			*dpt = ndl;
		else
			prev->next=ndl;
	}
	return ndl;
}

void mt_destroy_trees(void)
{
	if (_ptree!=NULL)
	{
		if (*_ptree!=NULL)
			mt_free_tree(*_ptree);
		shm_free(_ptree);
		_ptree = NULL;
	}
}

int mt_defined_trees(void)
{
	if (_ptree!=NULL && *_ptree!=NULL)
		return 1;
	return 0;
}

int mt_mi_add_tvalues(struct mi_node *rpl, m_tree_t *pt, str *tomatch)
{
	int l;
	mt_node_t *itn;
	mt_is_t *tvalues;
	struct mi_attr* attr= NULL;
	struct mi_node *node = NULL;

	if (pt == NULL || tomatch == NULL || tomatch->s == NULL) {
		LM_ERR("bad parameters\n");
		return -1;
	}

	l = 0;
	itn = pt->head;

	while (itn != NULL && l < tomatch->len && l < MT_MAX_DEPTH) {
		/* check validity */
		if(_mt_char_table[(unsigned int)tomatch->s[l]]==255) {
			LM_ERR("invalid char at %d in [%.*s]\n",
					l, tomatch->len, tomatch->s);
			return -1;
		}
		tvalues = itn[_mt_char_table[(unsigned int)tomatch->s[l]]].tvalues;
		while (tvalues != NULL) {
			node = add_mi_node_child(rpl, 0, "MT", 2, 0, 0);
			if(node == NULL)
				return -1;
			if( add_mi_attr(node, MI_DUP_VALUE, "TNAME", 5,
					pt->tname.s, pt->tname.len) == NULL)
				return -1;
			if (add_mi_attr(node, MI_DUP_VALUE, "TPREFIX", 7,
						tomatch->s, l+1) == NULL)
				return -1;
			if (pt->type == MT_TREE_IVAL) {
				attr = addf_mi_attr(node, MI_DUP_VALUE, "TVALUE", 6,
						"%d", tvalues->tvalue.n);
			} else {  /* pt->type == MT_TREE_SVAL */
				attr = add_mi_attr(node, MI_DUP_VALUE, "TVALUE", 6,
						tvalues->tvalue.s.s, tvalues->tvalue.s.len);
			}
			tvalues = tvalues->next;
			if (attr == NULL)
				return -1;
		}

		itn = itn[_mt_char_table[(unsigned int)tomatch->s[l]]].child;
		l++;
	}

	if (node == NULL) return -1;

	return 0;
}

int mt_mi_match_prefix(struct mi_node *rpl, m_tree_t *it,
		str *tomatch, int mode)
{
	int l, len, n;
	int i, j;
	mt_node_t *itn;
	is_t *tvalue;
	mt_dw_t *dw;
	int tprefix_len = 0;

	struct mi_node *node;
	struct mi_attr* attr= NULL;

#define MT_MAX_DST_LIST	64
	unsigned int tmp_list[2*(MT_MAX_DST_LIST+1)];

	if(it==NULL || tomatch == NULL
			|| tomatch->s == NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	l = len = 0;
	n = 0;
	if ((it->type==MT_TREE_SVAL) || (it->type==MT_TREE_IVAL)) {
		if (mode == 2)
			return mt_mi_add_tvalues(rpl, it, tomatch);
		tvalue = mt_get_tvalue(it, tomatch, &tprefix_len);
		if (tvalue == NULL) {
			LM_DBG("no match for: %.*s\n", tomatch->len, tomatch->s);
			return -1;
		}
		if (tvalue) {
			node = add_mi_node_child(rpl, 0, "MT", 2, 0, 0);
			if(node == NULL)
				return -1;
			if( add_mi_attr(node, MI_DUP_VALUE, "TNAME", 5,
					it->tname.s, it->tname.len) == NULL)
				return -1;
			if (add_mi_attr(node, MI_DUP_VALUE, "TPREFIX", 7,
						tomatch->s, tprefix_len) == NULL)
				return -1;
			if (it->type==MT_TREE_SVAL) {
				attr = add_mi_attr(node, MI_DUP_VALUE, "TVALUE", 6,
										tvalue->s.s, tvalue->s.len);
			} else {
				attr = addf_mi_attr(node, MI_DUP_VALUE, "TVALUE", 6,
										"%d", tvalue->n);
			}
			if (attr == NULL)
				return -1;
		}
		return 0;
	}

	if(it->type!=MT_TREE_DW)
		return -1; /* wrong tree type */

	itn = it->head;
	memset(tmp_list, 0, sizeof(unsigned int)*2*(MT_MAX_DST_LIST+1));

	while(itn!=NULL && l < tomatch->len && l < MT_MAX_DEPTH)
	{
		/* check validity */
		if(_mt_char_table[(unsigned int)tomatch->s[l]]==255)
		{
			LM_ERR("invalid char at %d in [%.*s]\n",
					l, tomatch->len, tomatch->s);
			return -1;
		}

		if(itn[_mt_char_table[(unsigned int)tomatch->s[l]]].tvalues!=NULL)
		{
			dw = (mt_dw_t*)itn[_mt_char_table[(unsigned int)tomatch->s[l]]].data;
			while(dw) {
				tmp_list[2*n]=dw->dstid;
				tmp_list[2*n+1]=dw->weight;
				n++;
				if(n==MT_MAX_DST_LIST)
					break;
				dw = dw->next;
			}
			len = l+1;
		}
		if(n==MT_MAX_DST_LIST)
			break;

		itn = itn[_mt_char_table[(unsigned int)tomatch->s[l]]].child;
		l++;
	}

	if(n==0)
		return -1; /* no match */
	/* invalidate duplicated dstid, keeping longest match */
	for(i=(n-1); i>0; i--)
	{
		if (tmp_list[2*i]!=0)
		{
			for(j=0; j<i; j++)
				if(tmp_list[2*i]==tmp_list[2*j])
					tmp_list[2*j] = 0;
		}
	}
	/* sort the table -- bubble sort -- reverse order */
	for (i = (n - 1); i >= 0; i--)
	{
		for (j = 1; j <= i; j++)
		{
			if (tmp_list[2*(j-1)+1] < tmp_list[2*j+1])
			{
				tmp_list[2*MT_MAX_DST_LIST]   = tmp_list[2*(j-1)];
				tmp_list[2*MT_MAX_DST_LIST+1] = tmp_list[2*(j-1)+1];
				tmp_list[2*(j-1)]   = tmp_list[2*j];
				tmp_list[2*(j-1)+1] = tmp_list[2*j+1];
				tmp_list[2*j]   = tmp_list[2*MT_MAX_DST_LIST];
				tmp_list[2*j+1] = tmp_list[2*MT_MAX_DST_LIST+1];
			}
		}
	}

	/* add as attributes */
	for(i=0; i<n; i++)
	{
		if(tmp_list[2*i]!=0)
		{
			node = add_mi_node_child(rpl, 0, "MT", 2, 0, 0);
			if(node == NULL)
				return -1;
			if( add_mi_attr(node, MI_DUP_VALUE, "TNAME", 5,
					it->tname.s, it->tname.len) == NULL)
				return -1;
			if (add_mi_attr(node, MI_DUP_VALUE, "TPREFIX", 7,
						tomatch->s, len) == NULL)
				return -1;
			if(addf_mi_attr(node, MI_DUP_VALUE, "WEIGHT", 6,
					"%d", (int)tmp_list[2*i+1]) == NULL)
				return -1;

			if (addf_mi_attr(node, MI_DUP_VALUE, "DSTID", 5,
					"%d", (int)tmp_list[2*i]) == NULL)
				return -1;

		}
	}

	return 0;
}

int mt_rpc_add_tvalues(rpc_t* rpc, void* ctx, m_tree_t *pt, str *tomatch)
{
	int l;
	mt_node_t *itn;
	mt_is_t *tvalues;
	void *vstruct = NULL;
	str prefix = *tomatch;

	if (pt == NULL || tomatch == NULL || tomatch->s == NULL) {
		LM_ERR("bad parameters\n");
		return -1;
	}

	l = 0;
	itn = pt->head;

	while (itn != NULL && l < tomatch->len && l < MT_MAX_DEPTH) {
		/* check validity */
		if(_mt_char_table[(unsigned int)tomatch->s[l]]==255) {
			LM_ERR("invalid char at %d in [%.*s]\n",
					l, tomatch->len, tomatch->s);
			return -1;
		}
		tvalues = itn[_mt_char_table[(unsigned int)tomatch->s[l]]].tvalues;
		while (tvalues != NULL) {
			prefix.len = l+1;
			if (rpc->add(ctx, "{", &vstruct) < 0) {
				rpc->fault(ctx, 500, "Internal error adding struct");
				return -1;
			}
			if(rpc->struct_add(vstruct, "S", "PREFIX", &prefix) < 0) {
				rpc->fault(ctx, 500, "Internal error adding prefix");
				return -1;
			}
			if (pt->type == MT_TREE_IVAL) {
				if(rpc->struct_add(vstruct, "d", "TVALUE", tvalues->tvalue.n) < 0 ) {
					rpc->fault(ctx, 500, "Internal error adding tvalue");
					return -1;
				}
			} else {  /* pt->type == MT_TREE_SVAL */
				if(rpc->struct_add(vstruct, "S", "TVALUE", &tvalues->tvalue.s) < 0 ) {
					rpc->fault(ctx, 500, "Internal error adding tvalue");
					return -1;
				}
			}
			tvalues = tvalues->next;
		}

		itn = itn[_mt_char_table[(unsigned int)tomatch->s[l]]].child;
		l++;
	}

	if (vstruct == NULL) return -1;

	return 0;
}

int mt_rpc_match_prefix(rpc_t* rpc, void* ctx, m_tree_t *it,
		str *tomatch, int mode)
{
	int l, len, n;
	int i, j;
	mt_node_t *itn;
	is_t *tvalue;
	mt_dw_t *dw;
	int tprefix_len = 0;
	str prefix = *tomatch;
	void *vstruct = NULL;

#define MT_MAX_DST_LIST	64
	unsigned int tmp_list[2*(MT_MAX_DST_LIST+1)];

	if(it==NULL || tomatch == NULL
			|| tomatch->s == NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	if (rpc->add(ctx, "S", &it->tname) < 0) {
		rpc->fault(ctx, 500, "Internal error adding tname");
		return -1;
	}

	l = len = 0;
	n = 0;
	if ((it->type==MT_TREE_SVAL) || (it->type==MT_TREE_IVAL)) {
		if (mode == 2)
			return mt_rpc_add_tvalues(rpc, ctx, it, tomatch);
		tvalue = mt_get_tvalue(it, tomatch, &tprefix_len);
		if (tvalue == NULL) {
			LM_DBG("no match for: %.*s\n", tomatch->len, tomatch->s);
			return -1;
		}
		if (tvalue) {
			prefix.len = tprefix_len;
			if (rpc->add(ctx, "{", &vstruct) < 0) {
				rpc->fault(ctx, 500, "Internal error adding struct");
				return -1;
			}
			if (rpc->struct_add(vstruct, "S", "PREFIX", &prefix) < 0) {
				rpc->fault(ctx, 500, "Internal error adding prefix");
				return -1;
			}
			if (it->type==MT_TREE_SVAL) {
				if(rpc->struct_add(vstruct, "S", "TVALUE", &tvalue->s) < 0 ) {
					rpc->fault(ctx, 500, "Internal error adding tvalue");
					return -1;
				}
			} else {
				if(rpc->struct_add(vstruct, "d", "TVALUE", tvalue->n) < 0 ) {
					rpc->fault(ctx, 500, "Internal error adding tvalue");
					return -1;
				}
			}
		}
		return 0;
	}

	if(it->type!=MT_TREE_DW)
		return -1; /* wrong tree type */

	itn = it->head;
	memset(tmp_list, 0, sizeof(unsigned int)*2*(MT_MAX_DST_LIST+1));

	while(itn!=NULL && l < tomatch->len && l < MT_MAX_DEPTH)
	{
		/* check validity */
		if(_mt_char_table[(unsigned int)tomatch->s[l]]==255)
		{
			LM_ERR("invalid char at %d in [%.*s]\n",
					l, tomatch->len, tomatch->s);
			return -1;
		}

		if(itn[_mt_char_table[(unsigned int)tomatch->s[l]]].tvalues!=NULL)
		{
			dw = (mt_dw_t*)itn[_mt_char_table[(unsigned int)tomatch->s[l]]].data;
			while(dw) {
				tmp_list[2*n]=dw->dstid;
				tmp_list[2*n+1]=dw->weight;
				n++;
				if(n==MT_MAX_DST_LIST)
					break;
				dw = dw->next;
			}
			len = l+1;
		}
		if(n==MT_MAX_DST_LIST)
			break;

		itn = itn[_mt_char_table[(unsigned int)tomatch->s[l]]].child;
		l++;
	}

	if(n==0)
		return -1; /* no match */
	/* invalidate duplicated dstid, keeping longest match */
	for(i=(n-1); i>0; i--)
	{
		if (tmp_list[2*i]!=0)
		{
			for(j=0; j<i; j++)
				if(tmp_list[2*i]==tmp_list[2*j])
					tmp_list[2*j] = 0;
		}
	}
	/* sort the table -- bubble sort -- reverse order */
	for (i = (n - 1); i >= 0; i--)
	{
		for (j = 1; j <= i; j++)
		{
			if (tmp_list[2*(j-1)+1] < tmp_list[2*j+1])
			{
				tmp_list[2*MT_MAX_DST_LIST]   = tmp_list[2*(j-1)];
				tmp_list[2*MT_MAX_DST_LIST+1] = tmp_list[2*(j-1)+1];
				tmp_list[2*(j-1)]   = tmp_list[2*j];
				tmp_list[2*(j-1)+1] = tmp_list[2*j+1];
				tmp_list[2*j]   = tmp_list[2*MT_MAX_DST_LIST];
				tmp_list[2*j+1] = tmp_list[2*MT_MAX_DST_LIST+1];
			}
		}
	}

	prefix.len = len;

	/* add as attributes */
	for(i=0; i<n; i++)
	{
		if(tmp_list[2*i]!=0)
		{
			if (rpc->add(ctx, "{", &vstruct) < 0) {
				rpc->fault(ctx, 500, "Internal error adding struct");
				return -1;
			}
			if (rpc->struct_add(vstruct, "S", "PREFIX", &prefix) < 0) {
				rpc->fault(ctx, 500, "Internal error adding prefix");
				return -1;
			}
			if(rpc->add(vstruct, "dd",
					"WEIGHT", tmp_list[2*i+1],
					"DSTID", tmp_list[2*i]) < 0 ) {
				rpc->fault(ctx, 500, "Internal error adding weight/dstid");
				return -1;
			}
		}
	}

	return 0;
}
