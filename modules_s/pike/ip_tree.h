/* 
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef _IP_TREE_H
#define _IP_TREE_H

#include "../../mem/shm_mem.h"
#include "timer.h"

#define ip_malloc shm_malloc
#define ip_free   shm_free

#define NEW_NODE  1
#define LEAF_NODE 2
#define RED_NODE  4


struct ip_node
{
	struct pike_timer_link  tl;
	unsigned char           byte;
	unsigned short          leaf_hits;
	unsigned short          hits;
	struct ip_node          *children;
	struct ip_node          *prev;
	struct ip_node          *next;
};


struct ip_node* init_ip_tree(int);
struct ip_node* add_node(struct ip_node *root, unsigned char *ip,int ip_len,
										struct ip_node **father,char *flag);
void            remove_node(struct ip_node* root, struct ip_node *node);
void            destroy_ip_tree(struct ip_node *root);





#endif
