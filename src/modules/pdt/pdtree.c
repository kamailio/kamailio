/**
 * $Id$
 *
 * Copyright (C) 2011 Elena-Ramona Modroiu (asipto.com)
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

#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../ut.h"

#include "pdtree.h"

//extern str pdt_char_list = {"1234567890*",11};
extern str pdt_char_list;

/**
 *
 */
pdt_tree_t* pdt_init_tree(str* sdomain)
{
	pdt_tree_t *pt = NULL;

	pt = (pdt_tree_t*)shm_malloc(sizeof(pdt_tree_t));
	if(pt==NULL)
	{
		LM_ERR("no more shm memory\n");
		return NULL;
	}
	memset(pt, 0, sizeof(pdt_tree_t));

	pt->sdomain.s = (char*)shm_malloc((1+sdomain->len)*sizeof(char));
	if(pt->sdomain.s==NULL)
	{
		shm_free(pt);
		LM_ERR("no more shm memory\n");
		return NULL;
	}
	memset(pt->sdomain.s, 0,1+sdomain->len );
	memcpy(pt->sdomain.s, sdomain->s, sdomain->len);
	pt->sdomain.len = sdomain->len;
	
	pt->head = (pdt_node_t*)shm_malloc(PDT_NODE_SIZE*sizeof(pdt_node_t));
	if(pt->head == NULL)
	{
		shm_free(pt->sdomain.s);
		shm_free(pt);
		LM_ERR("no more shm memory\n");
		return NULL;
	}
	memset(pt->head, 0, PDT_NODE_SIZE*sizeof(pdt_node_t));
	
	return pt;
}

/**
 *
 */
int add_to_tree(pdt_tree_t *pt, str *sp, str *sd)
{
	int l;
	pdt_node_t *itn, *itn0;
	
	if(pt==NULL || sp==NULL || sp->s==NULL
			|| sd==NULL || sd->s==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(sp->len>=PDT_MAX_DEPTH)
	{
		LM_ERR("max prefix len exceeded\n");
		return -1;
	}
	
	l = 0;
	itn0 = pt->head;
	itn = itn0[strpos(pdt_char_list.s,sp->s[l])%PDT_NODE_SIZE].child;

	while(l < sp->len-1)
	{
		if(strpos(pdt_char_list.s,sp->s[l]) < 0)
		{
			LM_ERR("invalid char %d in prefix [%c (0x%x)]\n",
				l, sp->s[l], sp->s[l]);
			return -1;			
		}
		
		if(itn == NULL)
		{
			itn = (pdt_node_t*)shm_malloc(PDT_NODE_SIZE*sizeof(pdt_node_t));
			if(itn == NULL)
			{
				LM_ERR("no more shm mem\n");
				return -1;
			}
			memset(itn, 0, PDT_NODE_SIZE*sizeof(pdt_node_t));
			itn0[strpos(pdt_char_list.s,sp->s[l])%PDT_NODE_SIZE].child = itn;
		}
		l++;	
		itn0 = itn;
		itn = itn0[strpos(pdt_char_list.s,sp->s[l])%PDT_NODE_SIZE].child;
	}

	if(itn0[strpos(pdt_char_list.s,sp->s[l])%PDT_NODE_SIZE].domain.s!=NULL)
	{
		LM_ERR("prefix already allocated [%.*s/[%.*s]\n",
			sp->len, sp->s, sd->len, sd->s);
		return -1;
	}

	itn0[strpos(pdt_char_list.s,sp->s[l])%PDT_NODE_SIZE].domain.s
			= (char*)shm_malloc((sd->len+1)*sizeof(char));
	if(itn0[strpos(pdt_char_list.s,sp->s[l])%PDT_NODE_SIZE].domain.s==NULL)
	{
		LM_ERR("no more shm mem!\n");
		return -1;
	}
	strncpy(itn0[strpos(pdt_char_list.s,sp->s[l])%PDT_NODE_SIZE].domain.s,
			sd->s, sd->len);
	itn0[strpos(pdt_char_list.s,sp->s[l])%PDT_NODE_SIZE].domain.len = sd->len;
	itn0[strpos(pdt_char_list.s,sp->s[l])%PDT_NODE_SIZE].domain.s[sd->len]
		= '\0';
	
	return 0;
}

/**
 *
 */
pdt_tree_t* pdt_get_tree(pdt_tree_t *pl, str *sdomain)
{
	pdt_tree_t *it;
			   
	if(pl==NULL)
		return NULL;

	if( sdomain==NULL || sdomain->s==NULL)
	{
		LM_ERR("bad parameters\n");
		return NULL;
	}

	it = pl;
	/* search the tree for the asked sdomain */
	while(it!=NULL && str_strcmp(&it->sdomain, sdomain)<0)
		it = it->next;

	if(it==NULL || str_strcmp(&it->sdomain, sdomain)>0)
		return NULL;
	
	return it;
}

/**
 *
 */
int pdt_add_to_tree(pdt_tree_t **dpt, str *sdomain, str *code, str *domain)
{
	pdt_tree_t *ndl, *it, *prev;

	if( sdomain==NULL || sdomain->s==NULL
			|| code==NULL || code->s==NULL
			|| domain==NULL || domain->s==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}
	
	ndl = NULL;
	
	it = *dpt;
	prev = NULL;
	/* search the it position before which to insert new domain */
	while(it!=NULL && str_strcmp(&it->sdomain, sdomain)<0)
	{	
		prev = it;
		it = it->next;
	}
//	printf("sdomain:%.*s\n", sdomain->len, sdomain->s);

	/* add new sdomain*/
	if(it==NULL || str_strcmp(&it->sdomain, sdomain)>0)
	{
		ndl = pdt_init_tree(sdomain);
		if(ndl==NULL)
		{
			LM_ERR("no more shm memory\n");
			return -1; 
		}

		if(add_to_tree(ndl, code, domain)<0)
		{
			LM_ERR("internal error!\n");
			return -1;
		}
		ndl->next = it;
		
		/* new domain must be added as first element */
		if(prev==NULL)
			*dpt = ndl;
		else
			prev->next=ndl;

	}
	else 
		/* add (prefix, code) to already present sdomain */
		if(add_to_tree(it, code, domain)<0)
		{
			LM_ERR("internal error!\n");
			return -1;
		}

	return 0;
}

/**
 *
 */
str* get_domain(pdt_tree_t *pt, str *sp, int *plen)
{
	int l, len;
	pdt_node_t *itn;
	str *domain;

	if(pt==NULL || sp==NULL || sp->s==NULL)
	{
		LM_ERR("bad parameters\n");
		return NULL;
	}
	
	l = len = 0;
	itn = pt->head;
	domain = NULL;

	while(itn!=NULL && l < sp->len && l < PDT_MAX_DEPTH)
	{
		/* check validity */
		if(strpos(pdt_char_list.s,sp->s[l]) < 0)
		{
			LM_ERR("invalid char at %d in [%.*s]\n", l, sp->len, sp->s);
			return NULL;
		}

		if(itn[strpos(pdt_char_list.s,sp->s[l])%PDT_NODE_SIZE].domain.s!=NULL)
		{
			domain = &itn[strpos(pdt_char_list.s,sp->s[l])%PDT_NODE_SIZE].domain;
			len = l+1;
		}
		
		itn = itn[strpos(pdt_char_list.s,sp->s[l])%PDT_NODE_SIZE].child;
		l++;	
	}
	
	if(plen!=NULL)
		*plen = len;
	
	return domain;
	
}

/**
 *
 */
str* pdt_get_domain(pdt_tree_t *pl, str* sdomain, str *code, int *plen)
{
	pdt_tree_t *it;
	int len;
	str *domain=NULL;

	if(pl==NULL || sdomain==NULL || sdomain->s==NULL || code == NULL
			|| code->s == NULL)
	{
		LM_INFO("bad parameters\n");
		return NULL;
	}

	it = pl;
	while(it!=NULL && str_strcmp(&it->sdomain, sdomain)<0)
		it = it->next;
	
	if(it==NULL || str_strcmp(&it->sdomain, sdomain)>0)
		return NULL;
	
	domain = get_domain(it, code, &len);
	if(plen!=NULL)
			*plen = len;
	return domain;
}

/**
 *
 */
void pdt_free_node(pdt_node_t *pn)
{
	int i;
	if(pn==NULL)
		return;

	for(i=0; i<PDT_NODE_SIZE; i++)
	{
		if(pn[i].domain.s!=NULL)
		{
			shm_free(pn[i].domain.s);
			pn[i].domain.s   = NULL;
			pn[i].domain.len = 0;
		}
		if(pn[i].child!=NULL)
		{
			pdt_free_node(pn[i].child);
			pn[i].child = NULL;
		}
	}
	shm_free(pn);
	pn = NULL;
	
	return;
}

/**
 *
 */
void pdt_free_tree(pdt_tree_t *pt)
{
	if(pt == NULL)
		return;

	if(pt->head!=NULL) 
		pdt_free_node(pt->head);
	if(pt->next!=NULL)
		pdt_free_tree(pt->next);
	if(pt->sdomain.s!=NULL)
		shm_free(pt->sdomain.s);
	
	shm_free(pt);
	pt = NULL;
	return;
}

/**
 *
 */
int pdt_print_node(pdt_node_t *pn, char *code, int len)
{
	int i;

	if(pn==NULL || code==NULL || len>=PDT_MAX_DEPTH)
		return 0;
	
	for(i=0; i<PDT_NODE_SIZE; i++)
	{
		code[len]=pdt_char_list.s[i];
		if(pn[i].domain.s!=NULL)
			LM_DBG("[%.*s] [%.*s]\n",
					len+1, code, pn[i].domain.len, pn[i].domain.s);
		pdt_print_node(pn[i].child, code, len+1);
	}

	return 0;
}

static char pdt_code_buf[PDT_MAX_DEPTH+1];
/**
 *
 */
int pdt_print_tree(pdt_tree_t *pt)
{
	int len;

	if(pt == NULL)
	{
		LM_DBG("tree is empty\n");
		return 0;
	}
	
	LM_DBG("[%.*s]\n", pt->sdomain.len, pt->sdomain.s);
	len = 0;
	pdt_print_node(pt->head, pdt_code_buf, len);
	return pdt_print_tree(pt->next);
}

/**
 *
 */
int pdt_check_pd_node(pdt_node_t *pn, str *sp, str *sd,
		char *code, int len)
{
	int i;
	int ret;
 
	if(pn==NULL || code==NULL || len>=PDT_MAX_DEPTH)
		return 0;
	ret = 0;
	for(i=0; i<PDT_NODE_SIZE; i++)
	{
		code[len]=pdt_char_list.s[i];
		if(pn[i].domain.s!=NULL)
		{
			/* we have a domain - check for duplicates */
			LM_DBG("[%.*s] [%.*s]\n",
					len+1, code, pn[i].domain.len, pn[i].domain.s);
			if(sp->len == len+1 &&
					strncmp(sp->s, code, sp->len)==0)
			{
				LM_DBG("duplicated prefix\n");
				return 1;
			}
			if(sd->len == pn[i].domain.len &&
					strncmp(sd->s, pn[i].domain.s, sd->len)==0)
			{
				LM_DBG("duplicated domain\n");
				return 1;
			}
		}
		ret = pdt_check_pd_node(pn[i].child, sp, sd, code, len+1);
		if(ret != 0)
			break;
	}

	return ret;
}

/* returns 
 * 1 if prefix or domain already exists 
 * 0 if prefix or domain does not exist
 * -1 if any error
 */
int pdt_check_pd(pdt_tree_t *pt, str* sdomain, str *sp, str *sd)
{
	int len;
	int ret;
	pdt_tree_t *it;
		
	if(pt==NULL || sp==NULL || sd==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	it = pt;
	while(it!=NULL)
	{
		if(it->sdomain.len==sdomain->len
				&& strncasecmp(it->sdomain.s, sdomain->s, sdomain->len)==0)
			break;
		it = it->next;
	}
	if(it == NULL)
		return 0;

	len = 0;
	ret = pdt_check_pd_node(it->head, sp, sd, pdt_code_buf, len);
	return ret;
}

