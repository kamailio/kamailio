/*
 * $Id$
 *
 * Copyright (C) 2005-2009 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * History:
 * ---------
 *  2005-02-20  first version (cristian)
 *  2005-02-27  ported to 0.9.0 (bogdan)
 */


#include <stdlib.h>
#include <stdio.h>

#include "../../str.h"
#include "../../mem/shm_mem.h"

#include "prefix_tree.h"
#include "routing.h"
#include "dr_time.h"

extern int inode;
extern int unode;



static inline int 
check_time(
		tmrec_t *time_rec
		)
{
	ac_tm_t att;

	/* shortcut: if there is no dstart, timerec is valid */
	if (time_rec->dtstart==0)
		return 1;

	memset( &att, 0, sizeof(att));

	/* set current time */
	if ( ac_tm_set_time( &att, time(0) ) )
		return 0;

	/* does the recv_time match the specified interval?  */
	if (check_tmrec( time_rec, &att, 0)!=0)
		return 0;

	return 1;
}


static inline rt_info_t*
internal_check_rt(
		ptree_node_t *ptn,
		unsigned int rgid
		)
{
	int i;
	int rg_pos=0;
	rg_entry_t* rg=NULL;
	rt_info_wrp_t* rtlw=NULL;

	if((NULL==ptn) || (NULL==ptn->rg))
		goto err_exit;
	rg_pos = ptn->rg_pos;
	rg=ptn->rg;
	for(i=0;(i<rg_pos) && (rg[i].rgid!=rgid);i++);
	if(i<rg_pos) {
		LM_DBG("found rgid %d (rule list %p)\n", 
				rgid, rg[i].rtlw);
		rtlw=rg[i].rtlw;
		while(rtlw!=NULL) {
			if(check_time(rtlw->rtl->time_rec))
				goto ok_exit;
			rtlw=rtlw->next;
		}
	}
err_exit:
	return NULL;

ok_exit:
	return rtlw?rtlw->rtl:0;
}


rt_info_t* 
check_rt(
	ptree_node_t *ptn,
	unsigned int rgid
	)
{
	return internal_check_rt( ptn, rgid);
}


rt_info_t*
get_prefix(
	ptree_t *ptree,
	str* prefix,
	unsigned int rgid
	)
{
	rt_info_t *rt = NULL;
	char *tmp=NULL;
	char local=0;
	int idx=0;

	if(NULL == ptree)
		goto err_exit;
	if(NULL == prefix)
		goto err_exit;
	tmp = prefix->s;
	/* go the tree down to the last digit in the 
	 * prefix string or down to a leaf */
	while(tmp< (prefix->s+prefix->len)) {
		if(NULL == tmp)
			goto err_exit;
		local=*tmp;
		if( !IS_DECIMAL_DIGIT(local) ) {
			/* unknown character in the prefix string */
			goto err_exit;
		}
		if( tmp == (prefix->s+prefix->len-1) ) {
			/* last digit in the prefix string */
			break;
		}
		idx = local -'0';
		if( NULL == ptree->ptnode[idx].next) {
			/* this is a leaf */
			break;
		}
		ptree = ptree->ptnode[idx].next;
		tmp++;
	}
	/* go in the tree up to the root trying to match the
	 * prefix */
	while(ptree !=NULL ) {
		if(NULL == tmp)
			goto err_exit;
		/* is it a real node or an intermediate one */
		idx = *tmp-'0';
		if(NULL != ptree->ptnode[idx].rg) {
			/* real node; check the constraints on the routing info*/
			if( NULL != (rt = internal_check_rt( &(ptree->ptnode[idx]), rgid)))
				break;
		}
		tmp--;
		ptree = ptree->bp;
	}
	return rt;

err_exit:
	return NULL;
}


pgw_t* 
get_pgw(
		pgw_t* pgw_l,
		long id
		)
{
	if(NULL==pgw_l)
		goto err_exit;
	while(NULL != pgw_l) {
		if(id == pgw_l->id) {
			return pgw_l;
		}
		pgw_l = pgw_l->next;
	}
err_exit:
	return NULL;
}


int 
add_prefix(
	ptree_t *ptree,
	str* prefix,
	rt_info_t *r,
	unsigned int rg
	) 
{
	char* tmp=NULL;
	int res = 0;
	if(NULL==ptree)
		goto err_exit;
	tmp = prefix->s;
	while(tmp < (prefix->s+prefix->len)) {
		if(NULL == tmp)
			goto err_exit;
		if( !IS_DECIMAL_DIGIT(*tmp) ) {
			/* unknown character in the prefix string */
			goto err_exit;
		}
		if( tmp == (prefix->s+prefix->len-1) ) {
			/* last digit in the prefix string */
			LM_DBG("adding info %p, %d at: "
				"%p (%d)\n", r, rg, &(ptree->ptnode[*tmp-'0']), *tmp-'0');
			res = add_rt_info(&(ptree->ptnode[*tmp-'0']), r,rg);
			if(res < 0 )
				goto err_exit;
			unode++;
			res = 1;
			goto ok_exit;
		}
		/* process the current digit in the prefix */
		if(NULL == ptree->ptnode[*tmp - '0'].next) {
			/* allocate new node */
			INIT_PTREE_NODE(ptree, ptree->ptnode[*tmp - '0'].next);
			inode+=10;
#if 0
			printf("new tree node: %p (bp: %p)\n", 
					ptree->ptnode[*tmp - '0'].next,
					ptree->ptnode[*tmp - '0'].next->bp
					);
#endif
		}
		ptree = ptree->ptnode[*tmp-'0'].next;
		tmp++; 
	}

ok_exit:
	return 0;

err_exit:
	return -1;
}

int 
del_tree(
		ptree_t* t
		)
{
	int i,j;
	if(NULL == t)
		goto exit;
	/* delete all the children */
	for(i=0; i< PTREE_CHILDREN; i++) {
		/* shm_free the rg array of rt_info */
		if(NULL!=t->ptnode[i].rg) {
			for(j=0;j<t->ptnode[i].rg_pos;j++) {
				/* if non intermediate delete the routing info */
				if(t->ptnode[i].rg[j].rtlw !=NULL)
					del_rt_list(t->ptnode[i].rg[j].rtlw);
			}
			shm_free(t->ptnode[i].rg);
		}
		/* if non leaf delete all the children */
		if(t->ptnode[i].next != NULL)
			del_tree(t->ptnode[i].next);
	}
	shm_free(t);
exit:
	return 0;
}

void
del_rt_list(
		rt_info_wrp_t *rwl
		)
{
	rt_info_wrp_t* t=rwl;
	while(rwl!=NULL) {
		t=rwl;
		rwl=rwl->next;
		if ( (--t->rtl->ref_cnt)==0)
			free_rt_info(t->rtl);
		shm_free(t);
	}
}

void
free_rt_info(
		rt_info_t *rl
		)
{
	if(NULL == rl)
		return;
	if(NULL!=rl->pgwl)
		shm_free(rl->pgwl);
	if(NULL!=rl->time_rec)
		tmrec_free(rl->time_rec);
	shm_free(rl);
	return;
}

void
print_rt(
		rt_info_t*rt
		)
{
	int i=0;
	if(NULL==rt)
		return;
	printf("priority:%d list of gw:\n", rt->priority);
	for(i=0;i<rt->pgwa_len;i++)
		if(NULL!=rt->pgwl[i].pgw) 
			printf("  id:%ld pri:%.*s ip:%.*s \n",
				rt->pgwl[i].pgw->id, 
				rt->pgwl[i].pgw->pri.len, rt->pgwl[i].pgw->pri.s,
				rt->pgwl[i].pgw->ip.len, rt->pgwl[i].pgw->ip.s);
}
