/*
 * Header file for PIKE MI functions
 *
 * Copyright (C) 2006 Voice Sistem SRL
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

#include "ip_tree.h"
#include "pike_mi.h"

#define IPv6_LEN 16
#define IPv4_LEN 4
#define MAX_IP_LEN IPv6_LEN


static struct ip_node *ip_stack[MAX_IP_LEN];


static inline void print_ip_stack( int level, struct mi_node *node)
{
	if (level==IPv6_LEN) {
		/* IPv6 */
		addf_mi_node_child( node, 0, 0, 0,
			"%x%x:%x%x:%x%x:%x%x:%x%x:%x%x:%x%x:%x%x",
			ip_stack[0]->byte,  ip_stack[1]->byte,
			ip_stack[2]->byte,  ip_stack[3]->byte,
			ip_stack[4]->byte,  ip_stack[5]->byte,
			ip_stack[6]->byte,  ip_stack[7]->byte,
			ip_stack[8]->byte,  ip_stack[9]->byte,
			ip_stack[10]->byte, ip_stack[11]->byte,
			ip_stack[12]->byte, ip_stack[13]->byte,
			ip_stack[14]->byte, ip_stack[15]->byte );
	} else if (level==IPv4_LEN) {
		/* IPv4 */
		addf_mi_node_child( node, 0, 0, 0, "%d.%d.%d.%d",
			ip_stack[0]->byte,
			ip_stack[1]->byte,
			ip_stack[2]->byte,
			ip_stack[3]->byte );
	} else {
		LM_CRIT("leaf node at depth %d!!!\n", level);
		return;
	}
}


static void print_red_ips( struct ip_node *ip, int level, struct mi_node *node)
{
	struct ip_node *foo;

	if (level==MAX_IP_LEN) {
		LM_CRIT("tree deeper than %d!!!\n", MAX_IP_LEN);
		return;
	}
	ip_stack[level] = ip;

	/* is the node marked red? */
	if ( ip->flags&NODE_ISRED_FLAG)
		print_ip_stack(level+1,node);

	/* go through all kids */
	foo = ip->kids;
	while(foo){
		print_red_ips( foo, level+1, node);
		foo = foo->next;
	}

}



/*
  Syntax of "pike_list" :
    no nodes
*/
struct mi_root* mi_pike_list(struct mi_root* cmd_tree, void* param)
{
	struct mi_root* rpl_tree;
	struct ip_node *ip;
	int i;

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==0)
		return 0;

	for( i=0 ; i<MAX_IP_BRANCHES ; i++ ) {

		if (get_tree_branch(i)==0)
			continue;

		lock_tree_branch(i);

		if ( (ip=get_tree_branch(i))!=NULL )
			print_red_ips( ip, 0, &rpl_tree->node );

		unlock_tree_branch(i);
	}

	return rpl_tree;
}


