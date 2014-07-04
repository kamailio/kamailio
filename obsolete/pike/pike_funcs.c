/* 
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * --------
 *  2003-03-11  converted to the new locking interface: locking.h --
 *               major changes (andrei)
 *  2005-05-02  flags field added to node stucture -better sync between timer
 *              and worker processes; some races eliminated (bogdan)
 */



#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
//#include <arpa/inet.h>

#include "../../mem/shm_mem.h"
#include "../../locking.h"
#include "../../timer.h"
#include "../../ip_addr.h"
#include "../../resolve.h"
#include "ip_tree.h"
#include "pike_funcs.h"
#include "timer.h"




extern gen_lock_t*       timer_lock;
extern struct list_link* timer;
extern int               timeout;



void print_timer_list(struct list_link *head)
{
	struct list_link *ll;

	DBG("DEBUG:pike:print_timer_list --->\n");
	for ( ll=head->next ; ll!=head; ll=ll->next) {
		DBG("\t %p [byte=%x](expires=%d)\n",
			ll, ll2ipnode(ll)->byte, ll2ipnode(ll)->expires);
	}
}


int pike_check_req(struct sip_msg *msg, char *foo, char *bar)
{
	struct ip_node *node;
	struct ip_node *father;
	unsigned char flags;
	struct ip_addr* ip;
	int update_timer;
	

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

	update_timer=(int)(long)foo;
	/* first lock the proper tree branch and mark the IP with one more hit*/
	lock_tree_branch( ip->u.addr[0] );
	node = mark_node( ip->u.addr, ip->len, &father, &flags);
	if (node==0) {
		/* even if this is an error case, we return true in script to avoid
		 * considering the IP as marked (bogdan) */
		return 1;
	}

	DBG("DEBUG:pike_check_req: src IP [%s],node=%p; hits=[%d,%d],[%d,%d] "
		"node_flags=%d func_flags=%d\n",
		ip_addr2a( ip ), node,
		node->hits[PREV_POS],node->hits[CURR_POS],
		node->leaf_hits[PREV_POS],node->leaf_hits[CURR_POS],
		node->flags, flags);

	/* update the timer */
	lock_get(timer_lock);
	if ( flags&NEW_NODE ) {
		/* put this node into the timer list and remove its
		   father only if this has one kid and is not a LEAF_NODE*/
		node->expires =  get_ticks() + timeout;
		DBG("DEBUG:pike:pike_check_req: expires: %d, get_ticks: %d, timeout: %d", node->expires, node->expires-timeout, timeout);
		append_to_timer( timer, &(node->timer_ll) );
		node->flags |= NODE_INTIMER_FLAG;
		if (father) {
			DBG("DEBUG:pike_check_req: father %p: flags=%d kids->next=%p\n",
				father,father->flags,father->kids->next);
			if (!(father->flags&NODE_IPLEAF_FLAG) && !father->kids->next){
				/* debug */
				assert( has_timer_set(&(father->timer_ll))
					&& (father->flags&(NODE_EXPIRED_FLAG|NODE_INTIMER_FLAG)) );
				/* if the node is maked as expired by timer, let the timer
				 * to finish and remove the node */
				if ( !(father->flags&NODE_EXPIRED_FLAG) ) {
					remove_from_timer( timer, &(father->timer_ll) );
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
			/* if node exprired, ignore the current hit and let is
			 * expire in timer process */
			if (!update_timer && !(flags&NO_UPDATE) &&
							!(node->flags&NODE_EXPIRED_FLAG) ) {
				node->expires = get_ticks() + timeout;
				update_in_timer( timer, &(node->timer_ll) );
			}
		} else {
			/* debug */
			assert( !has_timer_set(&(node->timer_ll)) 
				&& !(node->flags&(NODE_INTIMER_FLAG|NODE_EXPIRED_FLAG)) );
			/* debug */
			assert( !(node->flags&NODE_IPLEAF_FLAG) && node->kids );
		}
	}
	/*print_timer_list( timer );*/ /* debug*/
	lock_release(timer_lock);

	unlock_tree_branch( ip->u.addr[0] );
	/*print_tree( 0 );*/ /* debug */

	if (flags&RED_NODE) {
		LOG(L_WARN,"DEBUG:pike_check_req: ALARM - TOO MANY HITS on "
			"%s !!\n", ip_addr2a( ip ) );
		return -1;
	}
	return 1;
}



void clean_routine(unsigned int ticks , void *param)
{
	static unsigned char mask[32];  /* 256 positions mask */
	struct list_link head;
	struct list_link *ll;
	struct ip_node   *dad;
	struct ip_node   *node;
	int i;

	/* DBG("DEBUG:pike:clean_routine:  entering (%d)\n",ticks); */
	/* before locking check first if the list is not empty and if can
	 * be at least one element removed */
	if ( is_list_empty( timer )) return; /* quick exit */

	/* get the expired elements */
	lock_get( timer_lock );
	/* check again for empty list */
	if (is_list_empty(timer) || (ll2ipnode(timer->next)->expires>ticks )){
		lock_release( timer_lock );
		return;
	}
	check_and_split_timer( timer, ticks, &head, mask);
	/*print_timer_list(timer);*/ /* debug */
	lock_release( timer_lock );
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
		for( ll=head.next ; ll!=&head ; ) {
			node = ll2ipnode( ll );
			ll = ll->next;
			/* skip nodes from a different branch */
			if (node->branch!=i)
				continue;

			/* unlink the node -> the list will get shorter and it will be
			 * faster for the next branches to process it */
			ll->prev->prev->next = ll;
			ll->prev = ll->prev->prev;
			node->expires = 0;
			node->timer_ll.prev = node->timer_ll.next = 0;
			if ( node->flags&NODE_EXPIRED_FLAG )
				node->flags &= ~NODE_EXPIRED_FLAG;
			else
				continue;

			/* process the node */
			DBG("DEBUG:pike:clean_routine: clean node %p (kids=%p;"
				"hits=[%d,%d];leaf=[%d,%d])\n", node,node->kids,
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
				/* if the node has no prev, means its a top branch node -> just
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
							lock_get(timer_lock);
							dad->expires = get_ticks() + timeout;
							assert( !has_timer_set(&(dad->timer_ll)) );
							append_to_timer( timer, &(dad->timer_ll));
							dad->flags |= NODE_INTIMER_FLAG;
							lock_release(timer_lock);
						} else {
							assert( has_timer_set(&(dad->timer_ll)) );
						}
					}
				}
				DBG("DEBUG:pike:clean_routine: rmv node %p[%d] \n",
					node,node->byte);
				/* del the node */
				remove_node( node);
			}
		} /* for all expired elements */
		unlock_tree_branch( i );
	} /* for all branches */
}




void refresh_node( struct ip_node *node)
{
	for( ; node ; node=node->next ) {
		node->hits[PREV_POS] = node->hits[CURR_POS];
		node->hits[CURR_POS] = 0;
		node->leaf_hits[PREV_POS] = node->leaf_hits[CURR_POS];
		node->leaf_hits[CURR_POS] = 0;
		if (node->kids)
			refresh_node( node->kids );
	}
}




void swap_routine( unsigned int ticks, void *param)
{
	struct ip_node *node;
	int i;

	/* DBG("DEBUG:pike:swap_routine:  entering \n"); */
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



