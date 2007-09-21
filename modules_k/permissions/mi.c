/*
 *
 * Permissions MI functions
 *
 * Copyright (C) 2006 Juha Heinanen
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2006-10-16  created (juhe)
 */


#include "../../dprint.h"
#include "address.h"
#include "trusted.h"
#include "hash.h"
#include "mi.h"


/*
 * MI function to reload trusted table
 */
struct mi_root* mi_trusted_reload(struct mi_root *cmd_tree, void *param)
{
    if (reload_trusted_table () == 1) {
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
    } else {
	return init_mi_tree( 400, "Trusted table reload failed", 26);
    }
}


/*
 * MI function to print trusted entries from current hash table
 */
struct mi_root* mi_trusted_dump(struct mi_root *cmd_tree, void *param)
{
	struct mi_root* rpl_tree;

	if (hash_table==NULL)
		return init_mi_tree( 500, "Trusted-module not in use", 24 );

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==NULL) return 0;

	if(hash_table_mi_print(*hash_table, &rpl_tree->node)< 0) {
		LM_ERR("failed to add a node\n");
		free_mi_tree(rpl_tree);
		return 0;
	}

	return rpl_tree;
}


/*
 * MI function to reload address table
 */
struct mi_root* mi_address_reload(struct mi_root *cmd_tree, void *param)
{
    if (reload_address_table () == 1) {
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
    } else {
	return init_mi_tree( 400, "Address table reload failed", 36);
    }
}


/*
 * MI function to print address entries from current hash table
 */
struct mi_root* mi_address_dump(struct mi_root *cmd_tree, void *param)
{
    struct mi_root* rpl_tree;
    
    rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
    if (rpl_tree==NULL) return 0;
    
    if(addr_hash_table_mi_print(*addr_hash_table, &rpl_tree->node) <  0) {
	LM_ERR("failed to add a node\n");
	free_mi_tree(rpl_tree);
	return 0;
    }

    return rpl_tree;
}


/*
 * MI function to print subnets from current subnet table
 */
struct mi_root* mi_subnet_dump(struct mi_root *cmd_tree, void *param)
{
    struct mi_root* rpl_tree;
    
    rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
    if (rpl_tree==NULL) return 0;
    
    if(subnet_table_mi_print(*subnet_table, &rpl_tree->node) <  0) {
	LM_ERR("failed to add a node\n");
	free_mi_tree(rpl_tree);
	return 0;
    }

    return rpl_tree;
}
