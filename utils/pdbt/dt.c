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

#include "dt.h"
#include "dtm.h"
#include "carrier.h"
#include "log.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>




struct dt_node_t *dt_init()
{
	struct dt_node_t *root;

	root = malloc(sizeof(struct dt_node_t));
	if (root == NULL) {
		LERR("dt_init() cannot allocate memory for dt_node_t.\n");
		return NULL;
	}

	memset(root, 0, sizeof(struct dt_node_t));

	return root;
}




void dt_delete(struct dt_node_t *root, struct dt_node_t *node)
{
	int i;
	if (node==NULL) return;

	for (i=0; i<10; i++) {
		dt_delete(root, node->child[i]);
		node->child[i] = NULL;
	}

	if (node != root) free(node);
}




void dt_destroy(struct dt_node_t **root)
{
	if ((root != NULL) && (*root != NULL)) {
		dt_delete(*root, *root);
		free(*root);
		*root = NULL;
	}
}




void dt_clear(struct dt_node_t *root)
{
	dt_delete(root, root);
	memset(root, 0, sizeof(struct dt_node_t));
}




int dt_insert(struct dt_node_t *root, const char *number, int numberlen, carrier_t carrier)
{
	struct dt_node_t *node = root;
	int i=0;
	unsigned int digit;

	while (i<numberlen) {
		digit = number[i] - '0';
		if (digit>9) {
			LERR("dt_insert() cannot insert non-numerical number\n");
			return -1;
		}
		if (node->child[digit] == NULL) {
			node->child[digit] = malloc(sizeof(struct dt_node_t));
#ifdef DT_MEM_ASSERT
			assert(node->child[digit] != NULL);
#else
			if (node->child[digit] == NULL) {
				LERR("dt_insert() cannot allocate memory\n");
				return -1;
			}
#endif
			/* non-mapping intermediary nodes carry the special carrier id 0 */
			memset(node->child[digit], 0, sizeof(struct dt_node_t));
		}
		node = node->child[digit];

		i++;
	}

	node->carrier = carrier;
	return 0;
}




int dt_size(struct dt_node_t *root)
{
	int i;
	int sum = 0;

	if (root == NULL) return 0;

	for (i=0; i<10; i++) {
		sum += dt_size(root->child[i]);
	}
	return sum+1;
}




int dt_loaded_nodes(struct dt_node_t *root) {
	int i;
	int sum = 0;

	if (root == NULL) return 0;

	for (i=0; i<10; i++) {
		sum += dt_loaded_nodes(root->child[i]);
	}

	if (root->carrier > 0) sum++;

	return sum;
}




int dt_leaves(struct dt_node_t *root)
{
	int i;
	int sum = 0;
	int leaf = 1;

	for (i=0; i<10; i++) {
		if (root->child[i]) {
			sum += dt_leaves(root->child[i]);
			leaf = 0;
		}
	}

	return sum+leaf;
}




int dt_longest_match(struct dt_node_t *root, const char *number, int numberlen, carrier_t *carrier)
{
	struct dt_node_t *node = root;
	int nmatch = -1;
	int i=0;
	unsigned int digit;

	if (node->carrier > 0) {
		nmatch=0;
		*carrier = node->carrier;
	}
	while (i<numberlen) {
		digit = number[i] - '0';
		if (digit>9) return nmatch;
		if (node->child[digit] == NULL) return nmatch;
		node = node->child[digit];
		i++;
		if (node->carrier > 0) {
			nmatch=i;
			*carrier = node->carrier;
		}
	}

	return nmatch;
}




int dt_contains(struct dt_node_t *root, const char *number, int numberlen, carrier_t *carrier)
{
  return (dt_longest_match(root, number, numberlen, carrier) == numberlen);
}




/*
 Returns the carrier if all children have the same carrier,
 0 otherwise.
 */
carrier_t dt_allcce(struct dt_node_t *root) {
	int i;
	carrier_t ret = 0;

	/* determine single child carrier */
	for (i=0; i<10; i++) {
		if (root->child[i] == NULL)
			return 0;
		else if (root->child[i]->carrier > 0) {
			ret=root->child[i]->carrier;
			break;
		}
	}
	if (ret==0) return 0;

	/* check if all children share the same carrier */
	for (i=0; i<10; i++) {
		if ((root->child[i] == NULL) || (root->child[i]->carrier != ret)) return 0;
	}

	return ret;
}



/*
 Cuts off tree branches which share a common carrier id with a node located
 more upwards in the tree. To do so, the subtree starting at root will be
 traversed recursively and according leaf nodes deleted.
 Nodes potentially eligible for removal will be marked by the special carrier
 id `0' (also denoting a [non-mapping] intermediary node). In order to decide
 whether a node is eligible or not, the lastly observed carrier id will be
 maintained in lastcarrier and transferred over to each recursion.
 */
int dt_optimize_leaf(struct dt_node_t *root, carrier_t lastcarrier)
{
	struct dt_node_t *node = root;
	carrier_t currentcarrier;
	int deleteret;
	int delete;
	carrier_t allcce;
	int i;

	if (node == NULL) return 0;

	if (node->carrier == lastcarrier) {
		node->carrier = 0;	    /* this is a node sharing carrier id */
	}

	if (node->carrier>0) currentcarrier=node->carrier;  /* new common carrier id starts at this node */
	else currentcarrier=lastcarrier;		    /* carry over common carrier id */

	/* 
	 Nodes with children sharing the same carrier may be generalized into a common node (prefix). 
	 Note that the code in the following if-statement is the reason why dt_optimize() calls this function
	 multiple times.
	 */
	if ((allcce=dt_allcce(node))) {
		/*
		 generalization requires having an intermediary parent node or a common carrier id between
		 all children and the current node 
		 */
		if ((node->carrier == 0) || (node->carrier < 0 && allcce == currentcarrier)) {
			currentcarrier=allcce;
			node->carrier=currentcarrier;
			for(i=0; i<10; i++) {
				/* 
				 Negative carrier ids mark children eligible for generalization into a parent
				 node. Carrier id 0 cannot be used because it could ambiguously refer to an
				 intermediary parent node, thereby rendering differentiation of such nodes and
				 generalized ones impossible and, in turn, overwriting mappings to existing
				 carrier ids.
				 When optimization completes, negative carrier ids will be modified to 0 to
				 make sure other functions operating on the tree do not get confused. (See
				 dt_clear_negatives() for details.)
				 */
				node->child[i]->carrier = -allcce;
			}
		}
	}

	/* preliminarily assume leaf nodes without carrier to be eligible for removal */
	if (node->carrier <= 0) deleteret=1;
	else deleteret=0;

	/* optimize children */
	for (i=0; i<10; i++) {
		delete=dt_optimize_leaf(node->child[i], currentcarrier);
		if (delete) {
			dt_delete(node, node->child[i]);
			node->child[i] = NULL;
		}
		/* this is no leaf node ==> revert removal assumption */
		if (node->child[i]) deleteret = 0;
	}

	return deleteret;
}



/*
 Replaces negative carrier ids in the given, root-based
 subtree by zeros to comply with other functions which may
 not expect negative carrier ids. (See also dt_optimize_leaf().)
 */
void dt_clear_negatives(struct dt_node_t *root)
{
    struct dt_node_t *node = root;
    int i;

    if (node == NULL) return;

    for (i=0; i<10; i++) {
	    dt_clear_negatives(node->child[i]);
    }

    if (node->carrier < 0) node->carrier = 0;
}



void dt_optimize(struct dt_node_t *root)
{
	int size;
	int oldsize = 0;
	
	size=dt_size(root);

	/*
	 optimization gradually trims leaf nodes in each invocation
	 of dt_optimize_leaf() ==> keep calling this function until
	 the size of the tree stabilizes
	 */
	while (size!=oldsize) {
		dt_optimize_leaf(root, 0);
		oldsize=size;
		size=dt_size(root);
	}

	/* turn negative carrier ids into zero's  */
	dt_clear_negatives(root);
}
