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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "../../core/dprint.h"
#include "../../core/mem/shm_mem.h"
#include "ip_tree.h"



static pike_ip_tree_t*  pike_root = 0;


static inline pike_ip_node_t* prv_get_tree_branch(unsigned char b)
{
	return pike_root->entries[b].node;
}


/* locks a tree branch */
static inline void prv_lock_tree_branch(unsigned char b)
{
	lock_set_get(pike_root->entry_lock_set, pike_root->entries[b].lock_idx);
}



/* unlocks a tree branch */
static inline void prv_unlock_tree_branch(unsigned char b)
{
	lock_set_release(pike_root->entry_lock_set, pike_root->entries[b].lock_idx);
}


/* wrapper functions */
pike_ip_node_t* get_tree_branch(unsigned char b)
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
		LM_INFO("probing %d set size\n", *size);
		/* create a lock set */
		lset = lock_set_alloc( *size );
		if (lset==0) {
			LM_INFO("cannot get %d locks\n", *size);
			continue;
		}
		/* init lock set */
		if (lock_set_init(lset)==0) {
			LM_INFO("cannot init %d locks\n", *size);
			lock_set_dealloc( lset );
			lset = 0;
			continue;
		}
		/* alloc and init succesfull */
		break;
	}

	if (*size==0) {
		LM_ERR("cannot get a lock set\n");
		return 0;
	}
	return lset;
}



/* Builds and Inits a new IP tree */
int init_ip_tree(int maximum_hits)
{
	int size;
	int i;

	/* create the pike_root */
	pike_root = (pike_ip_tree_t*)shm_malloc(sizeof(pike_ip_tree_t));
	if (pike_root==0) {
		LM_ERR("shm malloc failed\n");
		goto error;
	}
	memset(pike_root, 0, sizeof(pike_ip_tree_t));

	/* init lock set */
	size = MAX_IP_BRANCHES;
	pike_root->entry_lock_set = init_lock_set( &size );
	if (pike_root->entry_lock_set==0) {
		LM_ERR("failed to create locks\n");
		goto error;
	}
	/* assign to each branch a lock */
	for(i=0;i<MAX_IP_BRANCHES;i++) {
		pike_root->entries[i].node = 0;
		pike_root->entries[i].lock_idx = i % size;
	}

	pike_root->max_hits = maximum_hits;

	return 0;
error:
	if (pike_root) {
		shm_free(pike_root);
		pike_root = NULL;
	}
	return -1;
}

unsigned int get_max_hits()
{
	return (pike_root != 0) ? pike_root->max_hits : -1;
}

/* destroy an ip_node and all nodes under it; the nodes must be first removed
 * from any other lists/timers */
static inline void destroy_ip_node(pike_ip_node_t *node)
{
	pike_ip_node_t *foo, *bar;

	foo = node->kids;
	while (foo){
		bar = foo;
		foo = foo->next;
		destroy_ip_node(bar);
	}

	shm_free(node);
}



/* destroy and free the IP tree */
void destroy_ip_tree(void)
{
	int i;

	if (pike_root==0)
		return;

	/* destroy and free the lock set */
	if (pike_root->entry_lock_set) {
		lock_set_destroy(pike_root->entry_lock_set);
		lock_set_dealloc(pike_root->entry_lock_set);
	}

	/* destroy all the nodes */
	for(i=0;i<MAX_IP_BRANCHES;i++)
		if (pike_root->entries[i].node)
			destroy_ip_node(pike_root->entries[i].node);

	shm_free( pike_root );
	pike_root = 0;

	return;
}



/* builds a new ip_node corresponding to a byte value */
static inline pike_ip_node_t *new_ip_node(unsigned char byte)
{
	pike_ip_node_t *new_node;

	new_node = (pike_ip_node_t*)shm_malloc(sizeof(pike_ip_node_t));
	if (!new_node) {
		LM_ERR("no more shm mem\n");
		return 0;
	}
	memset( new_node, 0, sizeof(pike_ip_node_t));
	new_node->byte = byte;
	return new_node;
}



/* splits from the current node (dad) a new child */
pike_ip_node_t *split_node(pike_ip_node_t* dad, unsigned char byte)
{
	pike_ip_node_t *new_node;

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
	( (_node)->hits[PREV_POS]>=pike_root->max_hits>>2 ||\
		(_node)->hits[CURR_POS]>=pike_root->max_hits>>2 ||\
		(((_node)->hits[PREV_POS]+(_node)->hits[CURR_POS])>>1)>=\
			pike_root->max_hits>>2 )

#define is_hot_leaf(_node) \
	( (_node)->leaf_hits[PREV_POS]>=pike_root->max_hits ||\
		(_node)->leaf_hits[CURR_POS]>=pike_root->max_hits ||\
		(((_node)->leaf_hits[PREV_POS]+(_node)->leaf_hits[CURR_POS])>>1)>=\
			pike_root->max_hits )

#define is_warm_leaf(_node) \
	( (_node)->hits[CURR_POS]>=pike_root->max_hits>>2 )

#define MAX_TYPE_VAL(_x) \
	(( (1<<(8*sizeof(_x)-1))-1 )|( (1<<(8*sizeof(_x)-1)) ))


int is_node_hot_leaf(pike_ip_node_t *node)
{
	return is_hot_leaf(node);
}

/*! \brief Used by the rpc function */
char *node_status_array[] = {"", "WARM", "HOT", "ALL"};
pike_node_status_t node_status(pike_ip_node_t *node)
{
	if ( is_hot_leaf(node) )
		return NODE_STATUS_HOT;

	if ( is_warm_leaf(node) )
		return NODE_STATUS_WARM;

	return NODE_STATUS_OK;
}



/* mark with one more hit the given IP address - */
pike_ip_node_t* mark_node(unsigned char *ip,int ip_len,
							pike_ip_node_t **father,unsigned char *flag)
{
	pike_ip_node_t *node;
	pike_ip_node_t *kid;
	int    byte_pos;

	kid = pike_root->entries[ ip[0] ].node;
	node = NULL;
	byte_pos = 0;

	LM_DBG("search on branch %d (top=%p)\n", ip[0],kid);
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

	LM_DBG("only first %d were matched!\n",byte_pos);
	*flag = 0;
	*father = 0;

	/* what have we found? */
	if (byte_pos==ip_len) {
		/* we found the entire address */
		node->flags |= NODE_IPLEAF_FLAG;
		/* increment it, but be careful not to overflow the value */
		if(node->leaf_hits[CURR_POS]<MAX_TYPE_VAL(node->leaf_hits[CURR_POS])-1)
			node->leaf_hits[CURR_POS]++;
		/* becoming red node? */
		if ( (node->flags&NODE_ISRED_FLAG)==0 ) {
			if (is_hot_leaf(node) ) {
				*flag |= RED_NODE|NEWRED_NODE;
				node->flags |= NODE_ISRED_FLAG;
			}
		} else {
			*flag |= RED_NODE;
		}
	} else if (byte_pos==0) {
		/* we hit an empty branch in the IP tree */
		assert(node==0);
		/* add a new node containing the start byte of the IP address */
		if ( (node=new_ip_node(ip[0]))==0)
			return 0;
		node->hits[CURR_POS] = 1;
		node->branch = ip[0];
		*flag = NEW_NODE ;
		/* set this node as pike_root of the branch starting with first byte of IP*/
		pike_root->entries[ ip[0] ].node = node;
	} else{
		/* only a non-empty prefix of the IP was found */
		if ( node->hits[CURR_POS]<MAX_TYPE_VAL(node->hits[CURR_POS])-1 )
			node->hits[CURR_POS]++;
		if ( is_hot_non_leaf(node) ) {
			/* we have to split the node */
			*flag = NEW_NODE ;
			LM_DBG("splitting node %p [%d]\n",node,node->byte);
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
void remove_node(pike_ip_node_t *node)
{
	LM_DBG("destroying node %p\n",node);
	/* is it a branch pike_root node? (these nodes have no prev (father)) */
	if (node->prev==0) {
		assert(pike_root->entries[node->byte].node==node);
		pike_root->entries[node->byte].node = 0;
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

static void print_node(pike_ip_node_t *node,int sp, FILE *f)
{
	pike_ip_node_t *foo;

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
