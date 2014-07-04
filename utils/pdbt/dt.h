/*
 * Copyright (C) 2009 1&1 Internet AG
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * sip-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _DT_H_
#define _DT_H_




#include "common.h" 




struct dt_node_t {
	struct dt_node_t *child[10];
	carrier_t carrier;
};




/*
 Allocates memory for the root node and initializes it.
 Returns a pointer to an initialized root node on success, NULL otherwise.
*/
struct dt_node_t *dt_init();

/*
 Deletes a subtree and frees memory including node.
 Memory for the root node is not freed.
*/
void dt_delete(struct dt_node_t *root, struct dt_node_t *node);

/*
 Deletes the whole tree and frees the memory including the root node.
*/
void dt_destroy(struct dt_node_t **root);

/*
 Deletes everything but the root node.
 The root node is initialized.
 This will make an empty tree like after dt_init.
*/
void dt_clear(struct dt_node_t *root);

/*
 Inserts a number with a corresponding carrier id.
 Nodes are created if necessary and the node after the last
 digit is marked with the given carrier id.
 Returns 0 on success, -1 otherwise.
*/
int dt_insert(struct dt_node_t *root, const char *number, int numberlen, carrier_t carrier);

/*
 Returns the number of nodes in the given tree.
*/
int dt_size(struct dt_node_t *root);

/*
 Returns the number of nodes in the given tree that are marked
 with a carrier id (carrier>0).
*/
int dt_loaded_nodes(struct dt_node_t *root);

/*
 Returns the number of leaf nodes in the given tree.
 On leaf nodes the leaf flag is set, on other nodes it is cleared.
*/
int dt_leaves(struct dt_node_t *root);

/*
 Finds the longest prefix match of number in root.
 Sets *carrier according to value in dtree.
 Return the number of matched digits.
 In case no (partial) match is found, return -1 (i.e,
 not even the very first digit could be prefixed).
*/
int dt_longest_match(struct dt_node_t *root, const char *number, int numberlen, carrier_t *carrier);

/*
 Returns 1 if number is found in root and  set *carrier
 according to value in dtree.
 Returns 0 if the number is not found.
*/
int dt_contains(struct dt_node_t *root, const char *number, int numberlen, carrier_t *carrier);

/*
 Optimizes the tree by means of compression. Effectively,
 this reduces the tree to a set of nodes representing shortest
 prefixes (as opposed to [likely complete] phone numbers).
*/
void dt_optimize(struct dt_node_t *root);




#endif
