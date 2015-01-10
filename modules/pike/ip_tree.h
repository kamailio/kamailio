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

#ifndef _IP_TREE_H
#define _IP_TREE_H


#include <stdio.h>
#include "../../locking.h"
#include "timer.h"


#define NEW_NODE    (1<<0)
#define RED_NODE    (1<<1)
#define NEWRED_NODE (1<<2)
#define NO_UPDATE   (1<<3)

#define MAX_IP_BRANCHES 256

#define PREV_POS 0
#define CURR_POS 1


#define NODE_EXPIRED_FLAG  (1<<0)
#define NODE_INTIMER_FLAG  (1<<1)
#define NODE_IPLEAF_FLAG   (1<<2)
#define NODE_ISRED_FLAG    (1<<3)

struct ip_node
{
	unsigned int      expires;
	unsigned short    leaf_hits[2];
	unsigned short    hits[2];
	unsigned char     byte;
	unsigned char     branch;
	volatile unsigned short    flags;
	struct list_link  timer_ll;
	struct ip_node    *prev;
	struct ip_node    *next;
	struct ip_node    *kids;
};


struct ip_tree
{
	struct entry {
		struct ip_node *node;
		int            lock_idx;
	} entries[MAX_IP_BRANCHES];
	unsigned short   max_hits;
	gen_lock_set_t  *entry_lock_set;
};


#define ll2ipnode(ptr) \
	((struct ip_node*)((char *)(ptr)-\
		(unsigned long)(&((struct ip_node*)0)->timer_ll)))


int    init_ip_tree(int);
void   destroy_ip_tree(void);
struct ip_node* mark_node( unsigned char *ip, int ip_len,
			struct ip_node **father, unsigned char *flag);
void   remove_node(struct ip_node *node);
int is_node_hot_leaf(struct ip_node *node);

void lock_tree_branch(unsigned char b);
void unlock_tree_branch(unsigned char b);
struct ip_node* get_tree_branch(unsigned char b);

typedef enum {
        NODE_STATUS_OK    = 0,
        NODE_STATUS_WARM  = 1,
        NODE_STATUS_HOT   = 2,
        NODE_STATUS_ALL   = 3   /** used for status matching */
} node_status_t;
node_status_t node_status(struct ip_node *node);
extern char *node_status_array[];
unsigned int get_max_hits();

void print_tree( FILE *f);


#endif
