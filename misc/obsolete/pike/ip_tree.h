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
 *  2004-07-28  s/lock_set_t/gen_lock_set_t/ because of a type conflict
 *              on darwin (andrei)
 *  2004-11-05  adaptiv init lock (bogdan)
 *  2005-06-02  flags added to ip_node structure (bogdan)
 */

#ifndef _IP_TREE_H
#define _IP_TREE_H


#include <stdio.h>
#include "../../locking.h"
#include "timer.h"


#define NEW_NODE   (1<<0)
#define RED_NODE   (1<<1)
#define NO_UPDATE  (1<<2)

#define MAX_IP_BRANCHES 256

#define PREV_POS 0
#define CURR_POS 1


#define NODE_EXPIRED_FLAG  (1<<0)
#define NODE_INTIMER_FLAG  (1<<1)
/**
 *   Node marked as IPLEAF is final node for IP address. Does not matter if IPv4 or IPv6.
 *   This node could not be leaf of tree, it is a leaf node of complete IP address.
 */
#define NODE_IPLEAF_FLAG   (1<<2)

struct ip_node
{
	unsigned int      expires;
	unsigned int      leaf_hits[2];
	unsigned int      hits[2];
	unsigned char     byte;
	unsigned char     branch;
	volatile unsigned short    flags;
	struct list_link  timer_ll;
	struct ip_node    *prev;
	struct ip_node    *next;
	struct ip_node    *kids;
};

typedef enum {
	NODE_STATUS_OK    = 0,
	NODE_STATUS_WARM  = 1,
	NODE_STATUS_HOT   = 2,
	NODE_STATUS_ALL   = 3	/** used for status matching */
} node_status_t;
node_status_t node_status(struct ip_node *node);
extern char *node_status_array[];

struct ip_tree
{
	struct entry {
		struct ip_node *node;
		int            lock_idx;
	} entries[MAX_IP_BRANCHES];
	unsigned int     max_hits;
	gen_lock_set_t  *entry_lock_set;
};


#define ll2ipnode(ptr) \
	((struct ip_node*)((char *)(ptr)-\
		(unsigned long)(&((struct ip_node*)0)->timer_ll)))


int   init_ip_tree(unsigned int);
void  destroy_ip_tree();
unsigned int get_max_hits();

struct ip_node* mark_node( unsigned char *ip, int ip_len,
		struct ip_node **father, unsigned char *flag);
void  remove_node(struct ip_node *node);

void print_tree( FILE *f);

void lock_tree_branch(unsigned char b);
void unlock_tree_branch(unsigned char b);
struct ip_node* get_tree_branch(unsigned char b);



#endif
