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
 *
 */



#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "../../core/mem/shm_mem.h"
#include "../../core/locking.h"
#include "../../core/timer.h"
#include "../../core/ip_addr.h"
#include "../../core/resolve.h"
#include "../../core/counters.h"
#include "../../core/mod_fix.h"
#include "ip_tree.h"
#include "pike_funcs.h"
#include "timer.h"




extern gen_lock_t*       pike_timer_lock;
extern pike_list_link_t* pike_timer;
extern int               pike_timeout;
extern int               pike_log_level;

counter_handle_t blocked;

void pike_counter_init()
{
	counter_register(&blocked, "pike", "blocked_ips", 0, 0, 0, "Counter of blocked IP addresses", 0);
}



int pike_check_ipaddr(sip_msg_t *msg, ip_addr_t* ip)
{
	pike_ip_node_t *node;
	pike_ip_node_t *father;
	unsigned char flags;

	/* first lock the proper tree branch and mark the IP with one more hit*/
	lock_tree_branch( ip->u.addr[0] );
	node = mark_node( ip->u.addr, ip->len, &father, &flags);
	if (node==0) {
		unlock_tree_branch( ip->u.addr[0] );
		/* even if this is an error case, we return true in script to avoid
		 * considering the IP as marked (bogdan) */
		return 1;
	}

	LM_DBG("src IP [%s],node=%p; hits=[%d,%d],[%d,%d] node_flags=%d"
		" func_flags=%d\n", ip_addr2a( ip ), node,
		node->hits[PREV_POS],node->hits[CURR_POS],
		node->leaf_hits[PREV_POS],node->leaf_hits[CURR_POS],
		node->flags, flags);

	/* update the timer */
	lock_get(pike_timer_lock);
	if ( flags&NEW_NODE ) {
		/* put this node into the timer list and remove its
		 * father only if this has one kid and is not a LEAF_NODE*/
		node->expires =  get_ticks() + pike_timeout;
		append_to_timer( pike_timer, &(node->timer_ll) );
		node->flags |= NODE_INTIMER_FLAG;
		if (father) {
			LM_DBG("father %p: flags=%d kids->next=%p\n",
				father,father->flags,father->kids->next);
			if (!(father->flags&NODE_IPLEAF_FLAG) && !father->kids->next){
				/* debug */
				assert( has_timer_set(&(father->timer_ll))
					&& (father->flags&(NODE_EXPIRED_FLAG|NODE_INTIMER_FLAG)) );
				/* if the node is maked as expired by timer, let the timer
				 * to finish and remove the node */
				if ( !(father->flags&NODE_EXPIRED_FLAG) ) {
					remove_from_timer( pike_timer, &(father->timer_ll) );
					father->flags &= ~NODE_INTIMER_FLAG;
				} else {
					father->flags &= ~NODE_EXPIRED_FLAG;
				}
			}
		}
	} else {
		/* update the timer -> in timer can be only nodes
		 * as IP-leaf(complete address) or tree-leaf */
		if (node->flags&NODE_IPLEAF_FLAG || node->kids==0) {
			/* tree leafs which are not potential red nodes are not update in
			 * order to make them to expire */
			/* debug */
			assert( has_timer_set(&(node->timer_ll))
				&& (node->flags&(NODE_EXPIRED_FLAG|NODE_INTIMER_FLAG)) );
			/* if node expired, ignore the current hit and let is
			 * expire in timer process */
			if ( !(flags&NO_UPDATE) && !(node->flags&NODE_EXPIRED_FLAG) ) {
				node->expires = get_ticks() + pike_timeout;
				update_in_timer( pike_timer, &(node->timer_ll) );
			}
		} else {
			/* debug */
			assert( !has_timer_set(&(node->timer_ll))
				&& !(node->flags&(NODE_INTIMER_FLAG|NODE_EXPIRED_FLAG)) );
			/* debug */
			assert( !(node->flags&NODE_IPLEAF_FLAG) && node->kids );
		}
	}
	/*print_timer_list( pike_timer );*/ /* debug*/
	lock_release(pike_timer_lock);

	unlock_tree_branch( ip->u.addr[0] );
	/*print_tree( 0 );*/ /* debug */

	if (flags&RED_NODE) {
		if (flags&NEWRED_NODE) {
			LM_GEN1( pike_log_level,
				"PIKE - BLOCKing ip %s, node=%p\n",ip_addr2a(ip),node);
			counter_inc(blocked);
			return -2;
		}
		return -1;
	}
	return 1;
}


int pike_check_req(sip_msg_t *msg)
{
	ip_addr_t* ip;


#ifdef _test
	/* get the ip address from second via */
	if (parse_headers(msg, HDR_VIA1_F, 0)!=0 )
		return -1;
	if (msg->via1==0 )
		return -1;
	/* convert from string to ip_addr */
	ip = str2ip( &msg->via1->host );
	if (ip==0)
		return -1;
#else
	ip = &(msg->rcv.src_ip);
#endif

	return pike_check_ipaddr(msg, ip);
}

int w_pike_check_req(sip_msg_t *msg, char *foo, char *bar)
{
	return pike_check_req(msg);
}

int pike_check_ip(sip_msg_t *msg, str *strip)
{
	ip_addr_t* ip;

	if(strip==NULL || strip->len<=0) {
		return -1;
	}

	ip = str2ip(strip);
	if (ip==0) {
		LM_ERR("failed to parse ip address: %.*s\n", strip->len, strip->s);
		return -1;
	}

	return pike_check_ipaddr(msg, ip);
}

int w_pike_check_ip(sip_msg_t *msg, char *pip, char *bar)
{
	str strip;

	if(fixup_get_svalue(msg, (gparam_t*)pip, &strip)<0) {
		LM_ERR("failed to get the ip parameter\n");
		return -1;
	}
	return pike_check_ip(msg, &strip);
}

void clean_routine(unsigned int ticks , void *param)
{
	static unsigned char mask[32];  /* 256 positions mask */
	pike_list_link_t head;
	pike_list_link_t *ll;
	pike_ip_node_t   *dad;
	pike_ip_node_t   *node;
	int i;

	/* LM_DBG("entering (%d)\n",ticks); */
	/* before locking check first if the list is not empty and if can
	 * be at least one element removed */
	if (pike_timer==0 || is_list_empty( pike_timer )) return; /* quick exit */

	memset(&head, 0, sizeof(pike_list_link_t));
	/* get the expired elements */
	lock_get( pike_timer_lock );
	/* check again for empty list */
	if (is_list_empty(pike_timer) || (ll2ipnode(pike_timer->next)->expires>ticks )){
		lock_release( pike_timer_lock );
		return;
	}
	check_and_split_timer( pike_timer, ticks, &head, mask);
	/*print_timer_list(pike_timer);*/ /* debug */
	lock_release( pike_timer_lock );
	/*print_tree( 0 );*/  /*debug*/

	/* got something back? */
	if ( is_list_empty(&head) )
		return;

	/* process what we got -> don't forget to lock the tree!! */
	for(i=0;i<MAX_IP_BRANCHES;i++) {
		/* if no element from this branch -> skip it */
		if ( ((mask[i>>3])&(1<<(i&0x07)))==0 )
			continue;

		lock_tree_branch( i );
		for( ll=head.next; ll!=&head && ll!=NULL; ) {
			node = ll2ipnode( ll );
			ll = ll->next;
			/* skip nodes from a different branch */
			if (node->branch!=i)
				continue;

			/* unlink the node -> the list will get shorter and it will be
			 * faster for the next branches to process it */
			if(ll) {
				ll->prev->prev->next = ll;
				ll->prev = ll->prev->prev;
			}
			node->expires = 0;
			node->timer_ll.prev = node->timer_ll.next = 0;
			if ( node->flags&NODE_EXPIRED_FLAG )
				node->flags &= ~NODE_EXPIRED_FLAG;
			else
				continue;

			/* process the node */
			LM_DBG("clean node %p (kids=%p; hits=[%d,%d];leaf=[%d,%d])\n",
				node,node->kids,
				node->hits[PREV_POS],node->hits[CURR_POS],
				node->leaf_hits[PREV_POS],node->leaf_hits[CURR_POS]);
			/* if it's a node, leaf for an ipv4 address inside an
			 * ipv6 address -> just remove it from timer it will be deleted
			 * only when all its kids will be deleted also */
			if (node->kids) {
				assert( node->flags&NODE_IPLEAF_FLAG );
				node->flags &= ~NODE_IPLEAF_FLAG;
				node->leaf_hits[CURR_POS] = 0;
			} else {
				/* if the node has no prev, means it is a top branch node -> just
				 * removed and destroy it */
				if (node->prev!=0) {
					/* if this is the last kid, we have to put the father
					 * into timer list */
					if (node->prev->kids==node && node->next==0) {
						/* this is the last kid node */
						dad = node->prev;
						/* put it in the list only if it's not an IP leaf
						 * (in this case, it's already there) */
						if ( !(dad->flags&NODE_IPLEAF_FLAG) ) {
							lock_get(pike_timer_lock);
							dad->expires = get_ticks() + pike_timeout;
							assert( !has_timer_set(&(dad->timer_ll)) );
							append_to_timer(pike_timer, &(dad->timer_ll));
							dad->flags |= NODE_INTIMER_FLAG;
							lock_release(pike_timer_lock);
						} else {
							assert( has_timer_set(&(dad->timer_ll)) );
						}
					}
				}
				LM_DBG("rmv node %p[%d] \n", node,node->byte);
				/* del the node */
				remove_node( node);
			}
		} /* for all expired elements */
		unlock_tree_branch( i );
	} /* for all branches */
}




static inline void refresh_node( pike_ip_node_t *node)
{
	for( ; node ; node=node->next ) {
		node->hits[PREV_POS] = node->hits[CURR_POS];
		node->hits[CURR_POS] = 0;
		node->leaf_hits[PREV_POS] = node->leaf_hits[CURR_POS];
		node->leaf_hits[CURR_POS] = 0;
		if ( node->flags&NODE_ISRED_FLAG && !is_node_hot_leaf(node) ) {
			node->flags &= ~(NODE_ISRED_FLAG);
			LM_GEN1( pike_log_level,"PIKE - UNBLOCKing node %p\n",node);
		}
		if (node->kids)
			refresh_node( node->kids );
	}
}




void swap_routine( unsigned int ticks, void *param)
{
	pike_ip_node_t *node;
	int i;

	/* LM_DBG("entering \n"); */
	for(i=0;i<MAX_IP_BRANCHES;i++) {
		node = get_tree_branch(i);
		if (node) {
			lock_tree_branch( i );
			node = get_tree_branch(i); /* again, to avoid races */
			if (node) refresh_node( node );
			unlock_tree_branch( i );
		}
	}
}



