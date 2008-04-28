/*
 * $Id$
 *
 * Copyright (C) 2007 1&1 Internet AG
 *
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
 */

#include "dt.h"
#include <assert.h>
#include "../../mem/shm_mem.h"


int dt_init(struct dt_node_t **root)
{
	*root = shm_malloc(sizeof(struct dt_node_t));
	if (!(*root)) {
		LM_CRIT("out of private memory\n");
		return -1;
	}
	memset(*root, 0, sizeof(struct dt_node_t));

	return 0;
}


void dt_delete(struct dt_node_t *root, struct dt_node_t *node)
{
	int i;
	if (!node) return;

	for (i = 0; i < 10; i++) {
		dt_delete(root, node->child[i]);
		node->child[i] = NULL;
	}

	if (node != root) shm_free(node);
}


void dt_destroy(struct dt_node_t **root)
{
	if (*root) {
		dt_delete(*root, *root);
		shm_free(*root);
		*root = NULL;
	}
}


void dt_clear(struct dt_node_t *root)
{
	dt_delete(root, root);
	memset(root, 0, sizeof(struct dt_node_t));
}


void dt_insert(struct dt_node_t *root, const char *number, char whitelist)
{
	struct dt_node_t *node = root;

	int i = 0;
	unsigned int digit;
	while (number[i] != 0) {
		digit = number[i] - '0';
		if (digit > 9) {
			LM_ERR("cannot insert non-numerical number\n");
			return;
		}
		if (!node->child[digit]) {
			node->child[digit] = shm_malloc(sizeof(struct dt_node_t));
			assert(node->child[digit] != NULL);
			memset(node->child[digit], 0, sizeof(struct dt_node_t));
		}
		node = node->child[digit];
		i++;
	}

	node->leaf = 1;
	node->whitelist = whitelist;
}


/**
 * Find the longest match of number in root.
 * Set *whitelist according to value in dtree.
 * \return the number of matched digits, in case no match is found, return -1.
 */
int dt_longest_match(struct dt_node_t *root, const char *number, char *whitelist)
{
	struct dt_node_t *node = root;
	int nmatch = -1;
	int i = 0;

	if (node->leaf == 1) {
		nmatch = 0;
		*whitelist = node->whitelist;
	}
	unsigned int digit;
	while (number[i] != 0) {
		digit = number[i] - '0';
		if (!node->child[digit]) return nmatch;
		node = node->child[digit];
		i++;
    if (node->leaf == 1) {
			nmatch=i;
			*whitelist = node->whitelist;
		}
	}

	return nmatch;
}
