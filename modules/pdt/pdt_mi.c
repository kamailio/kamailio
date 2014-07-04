/**
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2005 Voice Sistem SRL (Voice-System.RO)
 * Copyright (C) 2011 Elena-Ramona Modroiu
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef PDT_NO_MI

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "../../lib/srdb1/db_op.h"
#include "../../lib/kmi/mi.h"
#include "../../sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../parser/parse_uri.h"
#include "../../timer.h"
#include "../../ut.h"
#include "../../locking.h"
#include "../../action.h"
#include "../../mod_fix.h"
#include "../../parser/parse_from.h"

#include "pdtree.h"

static int  mi_child_init(void);

static struct mi_root* pdt_mi_reload(struct mi_root*, void* param);
static struct mi_root* pdt_mi_list(struct mi_root*, void* param);

int pdt_mi_init(void);

static mi_export_t mi_cmds[] = {
	{ "pdt_reload",  pdt_mi_reload,  0,  0,  mi_child_init },
	{ "pdt_list",    pdt_mi_list,    0,  0,  0 },
	{ 0, 0, 0, 0, 0}
};


/**
 * mi init function
 */
int pdt_init_mi(char *mod)
{
	if(register_mi_mod(mod, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}
	return 0;
}

/**
 * mi and worker process init
 */
static int mi_child_init(void)
{
	/* db handler initialization */
	if(pdt_init_db()<0)
	{
		LM_ERR("failed to connect to database\n");
		return -1;
	}

	return 0;
}


/**
 * "pdt_reload" syntax :
 * \n
 */
static struct mi_root* pdt_mi_reload(struct mi_root *cmd_tree, void *param)
{
	/* re-loading all information from database */
	if(pdt_load_db()!=0)
	{
		LM_ERR("cannot re-load info from database\n");	
		goto error;
	}
	
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);

error:
	return init_mi_tree( 500, "Failed to reload",16);
}


int pdt_print_mi_node(pdt_node_t *pt, struct mi_node* rpl, char *code,
		int len, str *sdomain, str *sd, str *sp)
{
	int i;
	struct mi_node* node = NULL;
	struct mi_attr* attr= NULL;
	str *cl;

	if(pt==NULL || len>=PDT_MAX_DEPTH)
		return 0;
	
	cl = pdt_get_char_list();

	for(i=0; i<cl->len; i++)
	{
		code[len]=cl->s[i];
		if(pt[i].domain.s!=NULL)
		{
			if((sp->s==NULL && sd->s==NULL)
				|| (sp->s==NULL && (sd->s!=NULL && pt[i].domain.len==sd->len
						&& strncasecmp(pt[i].domain.s, sd->s, sd->len)==0)) 
				|| (sd->s==NULL && (len+1>=sp->len
						&& strncmp(code, sp->s, sp->len)==0))
				|| ((sp->s!=NULL && len+1>=sp->len
						&& strncmp(code, sp->s, sp->len)==0)
						&& (sd->s!=NULL && pt[i].domain.len>=sd->len
						&& strncasecmp(pt[i].domain.s, sd->s, sd->len)==0)))
			{
				node = add_mi_node_child(rpl, 0, "PDT", 3, 0, 0);
				if(node == NULL)
					goto error;

				attr = add_mi_attr(node, MI_DUP_VALUE, "SDOMAIN", 7,
						sdomain->s, sdomain->len);
				if(attr == NULL)
					goto error;
				attr = add_mi_attr(node, MI_DUP_VALUE, "PREFIX", 6,
							code, len+1);
				if(attr == NULL)
					goto error;
						
				attr = add_mi_attr(node, MI_DUP_VALUE,"DOMAIN", 6,
							pt[i].domain.s, pt[i].domain.len);
				if(attr == NULL)
					goto error;
			}
		}
		if(pdt_print_mi_node(pt[i].child, rpl, code, len+1, sdomain, sd, sp)<0)
			goto error;
	}
	return 0;
error:
	return -1;
}

/**
 * "pdt_list" syntax :
 *    sdomain
 *    prefix
 *    domain
 *
 * 	- '.' (dot) means NULL value and will match anything
 * 	- the comparison operation is 'START WITH' -- if domain is 'a' then
 * 	  all domains starting with 'a' are listed
 *
 * 	  Examples
 * 	  pdt_list o 2 .    - lists the entries where sdomain is starting with 'o', 
 * 	                      prefix is starting with '2' and domain is anything
 * 	  
 * 	  pdt_list . 2 open - lists the entries where sdomain is anything, prefix 
 * 	                      starts with '2' and domain starts with 'open'
 */

struct mi_root* pdt_mi_list(struct mi_root* cmd_tree, void* param)
{
	str sd, sp, sdomain;
	pdt_tree_t *pt;
	struct mi_node* node = NULL;
	unsigned int i= 0;
	struct mi_root* rpl_tree = NULL;
	struct mi_node* rpl = NULL;
	static char code_buf[PDT_MAX_DEPTH+1];
	int len;
	str *cl;
	pdt_tree_t **ptree;

	ptree = pdt_get_ptree();

	if(ptree==NULL)
	{
		LM_ERR("empty domain list\n");
		return init_mi_tree( 500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
	}

	cl = pdt_get_char_list();

	/* read sdomain */
	sdomain.s = 0;
	sdomain.len = 0;
	sp.s = 0;
	sp.len = 0;
	sd.s = 0;
	sd.len = 0;
	node = cmd_tree->node.kids;
	if(node != NULL)
	{
		sdomain = node->value;
		if(sdomain.s == NULL || sdomain.len== 0)
			return init_mi_tree( 404, "domain not found", 16);

		if(*sdomain.s=='.')
			sdomain.s = 0;

		/* read prefix */
		node = node->next;
		if(node != NULL)
		{
			sp= node->value;
			if(sp.s== NULL || sp.len==0 || *sp.s=='.')
				sp.s = NULL;
			else {
				while(sp.s!=NULL && i!=sp.len)
				{
					if(strpos(cl->s, sp.s[i]) < 0)
					{
						LM_ERR("bad prefix [%.*s]\n", sp.len, sp.s);
						return init_mi_tree( 400, "bad prefix", 10);
					}
					i++;
				}
			}

			/* read domain */
			node= node->next;
			if(node != NULL)
			{
				sd= node->value;
				if(sd.s== NULL || sd.len==0 || *sd.s=='.')
					sd.s = NULL;
			}
		}
	}

	rpl_tree = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	if(rpl_tree == NULL)
		return 0;
	rpl = &rpl_tree->node;

	if(*ptree==0)
		return rpl_tree;

	pt = *ptree;
	
	while(pt!=NULL)
	{
		if(sdomain.s==NULL || 
			(sdomain.s!=NULL && pt->sdomain.len>=sdomain.len && 
			 strncmp(pt->sdomain.s, sdomain.s, sdomain.len)==0))
		{
			len = 0;
			if(pdt_print_mi_node(pt->head, rpl, code_buf, len, &pt->sdomain,
						&sd, &sp)<0)
				goto error;
		}
		pt = pt->next;
	}
	
	return rpl_tree;

error:
	free_mi_tree(rpl_tree);
	return 0;
}

#endif
