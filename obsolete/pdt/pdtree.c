/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2005 Voice Sistem SRL (Voice-System.RO)
 *
 * This file is part of SIP Express Router.
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
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-sistem.ro
 *
 * History:
 * -------
 * 2005-01-25  first tree version (ramona)
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../mem/mem.h"

#include "pdtree.h"

pdt_tree_t* pdt_init_tree()
{
	pdt_tree_t *pt = NULL;

	pt = (pdt_tree_t*)pkg_malloc(sizeof(pdt_tree_t));
	if(pt==NULL)
	{
		LOG(L_ERR, "pdt_init_tree:ERROR: no more pkg memory\n");
		return NULL;
	}
	memset(pt, 0, sizeof(pdt_tree_t));
	
	pt->head = (pdt_node_t*)pkg_malloc(PDT_NODE_SIZE*sizeof(pdt_node_t));
	if(pt->head == NULL)
	{
		pkg_free(pt);
		LOG(L_ERR, "pdt_init_tree:ERROR: no more pkg mem\n");
		return NULL;
	}
	memset(pt->head, 0, PDT_NODE_SIZE*sizeof(pdt_node_t));
	
	return pt;
}

int pdt_add_to_tree(pdt_tree_t *pt, str *sp, str *sd)
{
	int l;
	pdt_node_t *itn, *itn0;
	
	if(pt==NULL || sp==NULL || sp->s==NULL
			|| sd==NULL || sd->s==NULL)
	{
		LOG(L_ERR, "pdt_add_to_tree:ERROR: bad parameters\n");
		return -1;
	}

	if(sp->len>=PDT_MAX_DEPTH)
	{
		LOG(L_ERR, "pdt_add_to_tree:ERROR: max prefix len exceeded\n");
		return -1;
	}
	
	l = 0;
	itn0 = pt->head;
	itn = itn0[(sp->s[l]-'0')%PDT_NODE_SIZE].child;

	while(l < sp->len-1)
	{
		if(sp->s[l] < '0' || sp->s[l] > '9')
		{
			LOG(L_ERR,
			"pdt_add_to_tree:ERROR: invalid char %d in prefix [%c (0x%x)]\n",
				l, sp->s[l], sp->s[l]);
			return -1;			
		}
		
		if(itn == NULL)
		{
			itn = (pdt_node_t*)pkg_malloc(PDT_NODE_SIZE*sizeof(pdt_node_t));
			if(itn == NULL)
			{
				LOG(L_ERR, "pdt_add_to_tree: no more pkg mem\n");
				return -1;
			}
			memset(itn, 0, PDT_NODE_SIZE*sizeof(pdt_node_t));
			itn0[(sp->s[l]-'0')%PDT_NODE_SIZE].child = itn;
		}
		l++;	
		itn0 = itn;
		itn = itn0[(sp->s[l]-'0')%PDT_NODE_SIZE].child;
	}

	if(itn0[(sp->s[l]-'0')%PDT_NODE_SIZE].domain.s!=NULL)
	{
		LOG(L_ERR, "pdt_add_to_tree:ERROR: prefix alredy allocated\n");
		return -1;
	}

	itn0[(sp->s[l]-'0')%PDT_NODE_SIZE].domain.s
			= (char*)pkg_malloc((sd->len+1)*sizeof(char));
	if(itn0[(sp->s[l]-'0')%PDT_NODE_SIZE].domain.s==NULL)
	{
		LOG(L_ERR, "pdt_add_to_tree:ERROR: no more pkg mem!\n");
		return -1;
	}
	strncpy(itn0[(sp->s[l]-'0')%PDT_NODE_SIZE].domain.s,
			sd->s, sd->len);
	itn0[(sp->s[l]-'0')%PDT_NODE_SIZE].domain.len = sd->len;
	itn0[(sp->s[l]-'0')%PDT_NODE_SIZE].domain.s[sd->len] = '\0';
	
	return 0;
}

int pdt_remove_from_tree(pdt_tree_t *pt, str *sp)
{
	int l;
	pdt_node_t *itn;

	if(pt==NULL || sp==NULL || sp->s==NULL || sp->s<=0)
	{
		LOG(L_ERR, "pdt_remove_from_tree:ERROR: bad parameters\n");
		return -1;
	}
	
	l = 1;
	itn = pt->head;
	
	while(itn!=NULL && l < sp->len && l < PDT_MAX_DEPTH)
	{
		itn = itn[(sp->s[l-1]-'0')%PDT_NODE_SIZE].child;
		l++;	
	}

	if(itn!=NULL && l==sp->len
			&& itn[(sp->s[l-1]-'0')%PDT_NODE_SIZE].domain.s!=NULL)
	{
		DBG("pdt_remove_from_tree: deleting <%.*s>\n",
				itn[(sp->s[l-1]-'0')%PDT_NODE_SIZE].domain.len,
				itn[(sp->s[l-1]-'0')%PDT_NODE_SIZE].domain.s);
		pkg_free(itn[(sp->s[l-1]-'0')%PDT_NODE_SIZE].domain.s);
		itn[(sp->s[l-1]-'0')%PDT_NODE_SIZE].domain.s   = NULL;
		itn[(sp->s[l-1]-'0')%PDT_NODE_SIZE].domain.len = 0;
	}

	/* todo: should free the node if no child and prefix inside */
	
	return 0;
}

str* pdt_get_domain(pdt_tree_t *pt, str *sp, int *plen)
{
	int l, len;
	pdt_node_t *itn;
	str *domain;

	if(pt==NULL || sp==NULL || sp->s==NULL)
	{
		LOG(L_ERR, "pdt_get_domain:ERROR: bad parameters\n");
		return NULL;
	}
	
	l = len = 0;
	itn = pt->head;
	domain = NULL;

	while(itn!=NULL && l < sp->len && l < PDT_MAX_DEPTH)
	{
		if(itn[(sp->s[l]-'0')%PDT_NODE_SIZE].domain.s!=NULL)
		{
			domain = &itn[(sp->s[l]-'0')%PDT_NODE_SIZE].domain;
			len = l+1;
		}
		
		itn = itn[(sp->s[l]-'0')%PDT_NODE_SIZE].child;
		l++;	
	}
	
	if(plen!=NULL)
		*plen = len;
	
	return domain;
	
}

void pdt_free_node(pdt_node_t *pn)
{
	int i;
	if(pn==NULL)
		return;

	for(i=0; i<PDT_NODE_SIZE; i++)
	{
		if(pn[i].domain.s!=NULL)
		{
			pkg_free(pn[i].domain.s);
			pn[i].domain.s   = NULL;
			pn[i].domain.len = 0;
		}
		pdt_free_node(pn[i].child);

		pn[i].child = NULL;
	}

	pkg_free(pn);
	pn = NULL;

	return;
}

void pdt_free_tree(pdt_tree_t *pt)
{
	if(pt == NULL)
	{
		LOG(L_INFO, "pdt_free_tree: bad parameters\n");
		return;
	}

	pdt_free_node(pt->head);
	pkg_free(pt);
	pt = NULL;
	return;
}

int pdt_print_node(pdt_node_t *pn, char *code, int len)
{
	int i;

	if(pn==NULL || code==NULL || len>=PDT_MAX_DEPTH)
		return 0;
	
	for(i=0; i<PDT_NODE_SIZE; i++)
	{
		code[len] = '0' + (char)i;
		if(pn[i].domain.s!=NULL)
			DBG("pdt_print_node: [%.*s] [%.*s]\n",
				len+1, code, pn[i].domain.len, pn[i].domain.s);
		pdt_print_node(pn[i].child, code, len+1);
	}

	return 0;
}

int pdt_print_tree(pdt_tree_t *pt)
{
	static char code_buf[PDT_MAX_DEPTH+1];
	int len;

	if(pt == NULL)
	{
		LOG(L_ERR, "pdt_remove_from_tree:ERROR: bad parameters\n");
		return -1;
	}

	len = 0;
	return pdt_print_node(pt->head, code_buf, len);
}


