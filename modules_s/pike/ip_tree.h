/* 
 * $Id$
 *
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
struct ip_node* add_node(struct ip_node *root, char *ip,int ip_len,
										struct ip_node **father,char *flag);
void            remove_node(struct ip_node* root, struct ip_node *node);
void            destroy_ip_tree(struct ip_node *root);





#endif
