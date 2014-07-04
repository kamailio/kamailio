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
 *  2004-11-05: adaptiv init lock (bogdan)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "ip_tree.h"



static struct ip_tree*  root = 0;


static inline struct ip_node* prv_get_tree_branch(unsigned char b)
{
	return root->entries[b].node;
}


/* locks a tree branch */
static inline void prv_lock_tree_branch(unsigned char b)
{
	lock_set_get( root->entry_lock_set, root->entries[b].lock_idx);
}



/* unlocks a tree branch */
static inline void prv_unlock_tree_branch(unsigned char b)
{
	lock_set_release( root->entry_lock_set, root->entries[b].lock_idx);
}


/* wrapper functions */
struct ip_node* get_tree_branch(unsigned char b)
{
	return prv_get_tree_branch(b);
}
void lock_tree_branch(unsigned char b)
{
	prv_lock_tree_branch(b);
}
void unlock_tree_branch(unsigned char b)
{
	prv_unlock_tree_branch(b);
}


/* size must be a power of 2  */
static gen_lock_set_t* init_lock_set(int *size)
{
	gen_lock_set_t *lset;

	lset=0; /* kill warnings */
	for( ; *size ; *size=((*size)>>1) ) {
		LOG(L_INFO,"INFO:pike:init_lock_set: probing %d set size\n",*size);
		/* create a lock set */
		lset = lock_set_alloc( *size );
		if (lset==0) {
			LOG(L_INFO,"INFO:pike:init_lock_set: cannot get %d locks\n",
				*size);
			continue;
		}
		/* init lock set */
		if (lock_set_init(lset)==0) {
			LOG(L_INFO,"INFO:pike:init_lock_set: cannot init %d locks\n",
				*size);
			lock_set_dealloc( lset );
			lset = 0;
			continue;
		}
		/* alloc and init succesfull */
		break;
	}

	if (*size==0) {
		LOG(L_ERR,"ERROR:pike:init_lock_set: cannot get a lock set\n");
		return 0;
	}
	return lset;
}



/* Builds and Inits a new IP tree */
int init_ip_tree(unsigned int maximum_hits)
{
	int size;
	int i;

	/* create the root */
	root = (struct ip_tree*)shm_malloc(sizeof(struct ip_tree));
	if (root==0) {
		LOG(L_ERR,"ERROR:pike:init_ip_tree: shm malloc failed\n");
		goto error;
	}
	memset( root, 0, sizeof(struct ip_tree));

	/* init lock set */
	size = MAX_IP_BRANCHES;
	root->entry_lock_set = init_lock_set( &size );
	if (root->entry_lock_set==0) {
		LOG(L_ERR,"ERROR:pike:init_ip_tree: failed to create locks\n");
		goto error;
	}
	/* assign to each branch a lock */
	for(i=0;i<MAX_IP_BRANCHES;i++) {
		root->entries[i].node = 0;
		root->entries[i].lock_idx = i % size;
		/*DBG("DEBUG:pike:pike_ip_tree: branch %d takes lock index %d\n",
			i, root->entries[i].lock_idx);*/
	}

	root->max_hits = maximum_hits;

	return 0;
error:
	if (root)
		shm_free(root);
	return -1;
}

unsigned int get_max_hits() { return root != 0 ? root->max_hits : -1; }

/* destroy an ip_node and all nodes under it; the nodes must be first removed
 * from any other lists/timers */
static inline void destroy_ip_node(struct ip_node *node)
{
	struct ip_node *foo, *bar;

	foo = node->kids;
	while (foo){
		bar = foo;
		foo = foo->next;
		destroy_ip_node(bar);
	}

	shm_free(node);
}



/* destroy and free the IP tree */
void destroy_ip_tree()
{
	int i;

	if (root==0)
		return;

	/* destroy and free the lock set */
	if (root->entry_lock_set) {
		lock_set_destroy(root->entry_lock_set);
		lock_set_dealloc(root->entry_lock_set);
	}

	/* destroy all the nodes */
	for(i=0;i<MAX_IP_BRANCHES;i++)
		if (root->entries[i].node)
			destroy_ip_node(root->entries[i].node);

	shm_free( root );
	root = 0;

	return;
}



/* builds a new ip_node corresponding to a byte value */
static inline struct ip_node *new_ip_node(unsigned char byte)
{
	struct ip_node *new_node;

	new_node = (struct ip_node*)shm_malloc(sizeof(struct ip_node));
	if (!new_node) {
		LOG(L_ERR,"ERROR:pike:new_ip_node: no more shm mem\n");
		return 0;
	}
	memset( new_node, 0, sizeof(struct ip_node));
	new_node->byte = byte;
	return new_node;
}



/* splits from the current node (dad) a new child */
struct ip_node *split_node(struct ip_node* dad, unsigned char byte)
{
	struct ip_node *new_node;

	/* create a new node */
	if ( (new_node=new_ip_node(byte))==0 )
		return 0;
	/* the child node inherits a part of his father hits */
	if (dad->hits[CURR_POS]>=1)
		new_node->hits[CURR_POS] = (dad->hits[CURR_POS])-1;
	if (dad->leaf_hits[CURR_POS]>=1)
		new_node->leaf_hits[PREV_POS] = (dad->leaf_hits[PREV_POS])-1;
	/* link the child into father's kids list -> insert it at the beginning,
	 * is much faster */
	if (dad->kids) {
		dad->kids->prev = new_node;
		new_node->next = dad->kids;
	}
	dad->kids = new_node;
	new_node->branch = dad->branch;
	new_node->prev = dad;

	return new_node;
}


#define is_hot_non_leaf(_node) \
	( (_node)->hits[PREV_POS]>=root->max_hits>>2 ||\
	  (_node)->hits[CURR_POS]>=root->max_hits>>2 ||\
	  (((_node)->hits[PREV_POS]+(_node)->hits[CURR_POS])>>1)>=\
		 root->max_hits>>2 )

#define is_hot_leaf(_node) \
	( (_node)->leaf_hits[PREV_POS]>=root->max_hits ||\
	  (_node)->leaf_hits[CURR_POS]>=root->max_hits ||\
	  (((_node)->leaf_hits[PREV_POS]+(_node)->leaf_hits[CURR_POS])>>1)>=\
		 root->max_hits )

#define is_warm_leaf(_node) \
	( (_node)->hits[CURR_POS]>=root->max_hits>>2 )

#define MAX_TYPE_VAL(_x) \
	(( (1U<<(8*sizeof(_x)-1))-1 )|( (1U<<(8*sizeof(_x)-1)) ))

char *node_status_array[] = {"", "WARM", "HOT", "ALL"};
node_status_t node_status(struct ip_node *node)
{
	if ( is_hot_leaf(node) )
		return NODE_STATUS_HOT;

	if ( is_warm_leaf(node) )
		return NODE_STATUS_WARM;

	return NODE_STATUS_OK;
}

/* mark with one more hit the given IP address - */
struct ip_node* mark_node(unsigned char *ip,int ip_len,
							struct ip_node **father,unsigned char *flag)
{
	struct ip_node *node;
	struct ip_node *kid;
	int    byte_pos;

	kid = root->entries[ ip[0] ].node;
	node = 0;
	byte_pos = 0;

	DBG("DEBUG:pike:mark_node: search on branch %d (top=%p)\n", ip[0],kid);
	/* search into the ip tree the longest prefix matching the given IP */
	while (kid && byte_pos<ip_len) {
		while (kid && kid->byte!=(unsigned char)ip[byte_pos]) {
				kid = kid->next;
		}
		if (kid) {
			node = kid;
			kid = kid->kids;
			byte_pos++;
		}
	}

	DBG("DEBUG:pike:mark_node: Only first %d were matched!\n",byte_pos);
	*flag = 0;
	*father = 0;

	/* what have we found? */
	if (byte_pos==ip_len) {
		/* we found the entire address */
		node->flags |= NODE_IPLEAF_FLAG;
		/* increment it, but be careful not to overflow the value */
		if(node->leaf_hits[CURR_POS]<MAX_TYPE_VAL(node->leaf_hits[CURR_POS])-1)
			node->leaf_hits[CURR_POS]++;
		if ( is_hot_leaf(node) )
			*flag |= RED_NODE;
	} else if (byte_pos==0) {
		/* we hit an empty branch in the IP tree */
		assert(node==0);
		/* add a new node containing the start byte of the IP address */
		if ( (node=new_ip_node(ip[0]))==0)
			return 0;
		node->hits[CURR_POS] = 1;
		node->branch = ip[0];
		*flag = NEW_NODE ;
		/* set this node as root of the branch starting with first byte of IP*/
		root->entries[ ip[0] ].node = node;
	} else{
		/* only a non-empty prefix of the IP was found */
		if ( node->hits[CURR_POS]<MAX_TYPE_VAL(node->hits[CURR_POS])-1 )
			node->hits[CURR_POS]++;
		if ( is_hot_non_leaf(node) ) {
			/* we have to split the node */
			*flag = NEW_NODE ;
			DBG("DEBUG:pike:mark_node: splitting node %p [%d]\n",
				node,node->byte);
			*father = node;
			node = split_node(node,ip[byte_pos]);
		} else {
			/* to reduce memory usage, force to expire non-leaf nodes if they
			 * have just a few hits -> basically, don't update the timer for
			 * them the nr of hits is small */
			if ( !is_warm_leaf(node) )
				*flag = NO_UPDATE;
		}
	}

	return node;
}



/* remove and destroy a IP node along with its subtree */
void remove_node(struct ip_node *node)
{
	DBG("DEBUG:pike:remove_node: destroying node %p\n",node);
	/* is it a branch root node? (these nodes have no prev (father)) */
	if (node->prev==0) {
		assert(root->entries[node->byte].node==node);
		root->entries[node->byte].node = 0;
	} else {
		/* unlink it from kids list */
		if (node->prev->kids==node)
			/* it's the head of the list! */
			node->prev->kids = node->next;
		else
			/* it's somewhere in the list */
			node->prev->next = node->next;
		if (node->next)
			node->next->prev = node->prev;
	}

	/* destroy the node */
	node->next = node->prev = 0;
	destroy_ip_node(node);
}



void print_node(struct ip_node *node,int sp, FILE *f)
{
	struct ip_node *foo;

	/* print current node */
	if (!f) {
		DBG("[l%d] node %p; brh=%d byte=%d flags=%d, hits={%d,%d} , "
			"leaf_hits={%d,%d]\n",
			sp, node, node->branch, node->byte, node->flags,
			node->hits[PREV_POS],node->hits[CURR_POS],
			node->leaf_hits[PREV_POS],node->leaf_hits[CURR_POS]);
	} else {
		fprintf(f,"[l%d] node %p; brh=%d byte=%d flags=%d, hits={%d,%d} , "
			"leaf_hits={%d,%d]\n",
			sp, node, node->branch, node->byte, node->flags,
			node->hits[PREV_POS],node->hits[CURR_POS],
			node->leaf_hits[PREV_POS],node->leaf_hits[CURR_POS]);
	}

	/* print all the kids */
	foo = node->kids;
	while(foo){
		print_node(foo,sp+1,f);
		foo = foo->next;
	}
}



void print_tree(  FILE *f )
{
	int i;

	DBG("DEBUG:pike:print_tree: printing IP tree\n");
	for(i=0;i<MAX_IP_BRANCHES;i++) {
		if (prv_get_tree_branch(i)==0)
			continue;
		prv_lock_tree_branch(i);
		if (prv_get_tree_branch(i))
			print_node( prv_get_tree_branch(i), 0, f);
		prv_unlock_tree_branch(i);
	}
}

