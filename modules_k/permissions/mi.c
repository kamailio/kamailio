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
#include "permissions.h"


/*
 * MI function to reload trusted table
 */
struct mi_root* mi_trusted_reload(struct mi_root *cmd_tree, void *param)
{
	if (hash_table==NULL)
		return init_mi_tree( 200, MI_SSTR(MI_OK));

    if (reload_trusted_table () == 1) {
	return init_mi_tree( 200, MI_SSTR(MI_OK));
    } else {
	return init_mi_tree( 400, MI_SSTR("Trusted table reload failed"));
    }
}


/*
 * MI function to print trusted entries from current hash table
 */
struct mi_root* mi_trusted_dump(struct mi_root *cmd_tree, void *param)
{
	struct mi_root* rpl_tree;

	if (hash_table==NULL)
		return init_mi_tree( 500, MI_SSTR("Trusted-module not in use"));

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
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
	return init_mi_tree( 200, MI_SSTR(MI_OK));
    } else {
	return init_mi_tree( 400, MI_SSTR("Address table reload failed"));
    }
}


/*
 * MI function to print address entries from current hash table
 */
struct mi_root* mi_address_dump(struct mi_root *cmd_tree, void *param)
{
    struct mi_root* rpl_tree;
    
    rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
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
    
    rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
    if (rpl_tree==NULL) return 0;
    
    if(subnet_table_mi_print(*subnet_table, &rpl_tree->node) <  0) {
	LM_ERR("failed to add a node\n");
	free_mi_tree(rpl_tree);
	return 0;
    }

    return rpl_tree;
}

#define MAX_FILE_LEN 128

/*
 * MI function to make allow_uri query.
 */
struct mi_root* mi_allow_uri(struct mi_root *cmd, void *param)
{
    struct mi_node *node;
    str *basenamep, *urip, *contactp;
    char basename[MAX_FILE_LEN + 1];
    char uri[MAX_URI_SIZE + 1], contact[MAX_URI_SIZE + 1]; 
    unsigned int allow_suffix_len;

    node = cmd->node.kids;
    if (node == NULL || node->next == NULL || node->next->next == NULL ||
	node->next->next->next != NULL)
	return init_mi_tree(400, MI_SSTR(MI_MISSING_PARM));
    
    /* look for base name */
    basenamep = &node->value;
    if (basenamep == NULL)
	return init_mi_tree(404, MI_SSTR("Basename is NULL"));
    allow_suffix_len = strlen(allow_suffix);
    if (basenamep->len + allow_suffix_len + 1 > MAX_FILE_LEN)
	return init_mi_tree(404, MI_SSTR("Basename is too long"));
    memcpy(basename, basenamep->s, basenamep->len);
    memcpy(basename + basenamep->len, allow_suffix, allow_suffix_len);
    basename[basenamep->len + allow_suffix_len] = 0;

    /* look for uri */
    urip = &node->next->value;
    if (urip == NULL)
	return init_mi_tree(404, MI_SSTR("URI is NULL"));
    if (urip->len > MAX_URI_SIZE)
	return init_mi_tree(404, MI_SSTR("URI is too long"));
    memcpy(uri, urip->s, urip->len);
    uri[urip->len] = 0;

    /* look for contact */
    contactp = &node->next->next->value;
    if (contactp == NULL)
	return init_mi_tree(404, MI_SSTR("Contact is NULL"));
    if (contactp->len > MAX_URI_SIZE)
	return init_mi_tree(404, MI_SSTR("Contact is too long"));
    memcpy(contact, contactp->s, contactp->len);
    contact[contactp->len] = 0;

    if (allow_test(basename, uri, contact) == 1) {
	return init_mi_tree(200, MI_SSTR(MI_OK));
    } else {
	return init_mi_tree(403, MI_SSTR("Forbidden"));
    }
}
