/*
 * $Id: dtrie.c 5237 2008-11-21 10:17:10Z henningw $
 *
 * Copyright (C) 2008 1&1 Internet AG
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

/**
 * \file
 * \brief Trie datastructure with utility functions
 *
 * Provides a generic trie datastructure and utility functions to
 * initialize and manage individual nodes. Its optimized towards
 * the usecase of a matching tree that contains only digits, e.g.
 * for LCR or blacklist modules. Nevertheless it also supports the
 * matching of characters when configured correctly. For normal
 * digit only matching you need to use a branches parameter of
 * 10, when you use 128, the complete standard ascii charset is
 * available for matching. The trie is setup in shared memory.
 * - Module: \ref carrierroute
 * - Module: \ref userblacklist
 */


#include "dtrie.h"

#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"


struct dtrie_node_t *dtrie_init(const unsigned int branches)
{
	struct dtrie_node_t *root;

	root = shm_malloc(sizeof(struct dtrie_node_t));
	if (root == NULL) {
		SHM_MEM_ERROR;
		return NULL;
	}
	LM_DBG("allocate %lu bytes for root at %p\n",
			(long unsigned)sizeof(struct dtrie_node_t), root);
	memset(root, 0, sizeof(struct dtrie_node_t));

	root->child = shm_malloc(sizeof(struct dtrie_node_t *) * branches);
	if (root->child == NULL) {
		shm_free(root);
		SHM_MEM_ERROR;
		return NULL;
	}
	LM_DBG("allocate %lu bytes for %d root children pointer at %p\n",
			(long unsigned)sizeof(struct dtrie_node_t *) * branches,
			branches, root->child);
	memset(root->child, 0, sizeof(struct dtrie_node_t *) * branches);

	return root;
}


void dtrie_delete(struct dtrie_node_t *root, struct dtrie_node_t *node,
		dt_delete_func_t delete_payload, const unsigned int branches)
{
	unsigned int i;
	if (node==NULL) return;

	for (i=0; i<branches; i++) {
		dtrie_delete(root, node->child[i], delete_payload, branches);
		node->child[i] = NULL;
	}

	if (delete_payload) {
		delete_payload(node->data);
	}

	node->data = NULL;

	if (node != root) {
		LM_DBG("free node at %p\n", node);
		shm_free(node->child);
		node->child = NULL;
		shm_free(node);
	}
}


void dtrie_destroy(struct dtrie_node_t **root, dt_delete_func_t delete_payload, const unsigned int branches)
{
	if ((root!=NULL) && (*root!=NULL)) {
		dtrie_delete(*root, *root, delete_payload, branches);
		LM_DBG("free root at %p\n", root);
		shm_free((*root)->child);
		shm_free(*root);
		*root = NULL;
	}
}


void dtrie_clear(struct dtrie_node_t *root, dt_delete_func_t delete_payload,
		const unsigned int branches)
{
	dtrie_delete(root, root, delete_payload, branches);
}


int dtrie_insert(struct dtrie_node_t *root, const char *number, const unsigned int numberlen,
		void *data, const unsigned int branches)
{
	struct dtrie_node_t *node = root;
	unsigned char digit, i=0;

	while (i<numberlen) {
		if (branches==10) {
			digit = number[i] - '0';
			if (digit>9) {
				LM_ERR("cannot insert non-numerical character\n");
				return -1;
			}
		} else {
			digit = number[i];
			if (digit>127) {
				LM_ERR("cannot insert extended ascii character\n");
				return -1;
			}
		}

		if (node->child[digit] == NULL) {
			node->child[digit] = shm_malloc(sizeof(struct dtrie_node_t));
			if(node->child[digit] == NULL ){
				SHM_MEM_ERROR;
				return -1;
			}

			LM_DBG("allocate %lu bytes for node at %p\n", (long unsigned)sizeof(struct dtrie_node_t), node->child[digit]);
			memset(node->child[digit], 0, sizeof(struct dtrie_node_t));

			node->child[digit]->child = shm_malloc(sizeof(struct dtrie_node_t *) * branches);
			if(node->child[digit]->child == NULL){
				SHM_MEM_ERROR;
				shm_free(node->child[digit]);
				node->child[digit] = NULL;
				return -1;
			}
			LM_DBG("allocate %lu bytes for %d root children pointer at %p\n",
					(long unsigned)sizeof(struct dtrie_node_t *) * branches,
					branches, node->child[digit]->child);
			memset(node->child[digit]->child, 0, sizeof(struct dtrie_node_t *) * branches);
		}
		node = node->child[digit];
		i++;
	}
	node->data = data;
	return 0;
}


unsigned int dtrie_size(const struct dtrie_node_t *root, const unsigned int branches)
{
	unsigned int i, sum = 0;

	if (root == NULL) return 0;

	for (i=0; i<branches; i++) {
		sum += dtrie_size(root->child[i], branches);
	}

	return sum+1;
}


unsigned int dtrie_loaded_nodes(const struct dtrie_node_t *root, const unsigned int branches)
{
	unsigned int i, sum = 0;

	if (root == NULL) return 0;

	for (i=0; i<branches; i++) {
		sum += dtrie_loaded_nodes(root->child[i], branches);
	}

	if (root->data != NULL) sum++;

	return sum;
}


unsigned int dtrie_leaves(const struct dtrie_node_t *root, const unsigned int branches)
{
	unsigned int i, sum = 0, leaf = 1;

	for (i=0; i<branches; i++) {
		if (root->child[i]) {
			sum += dtrie_leaves(root->child[i], branches);
			leaf = 0;
		}
	}

	return sum+leaf;
}


void **dtrie_longest_match(struct dtrie_node_t *root, const char *number,
		const unsigned int numberlen, int *nmatchptr, const unsigned int branches)
{
	struct dtrie_node_t *node = root;
	unsigned char digit, i = 0;
	void **ret = NULL;

	if (nmatchptr) *nmatchptr=-1;
	if (node->data != NULL) {
		if (nmatchptr) *nmatchptr=0;
		ret = &node->data;
	}
	while (i<numberlen) {
		if (branches==10) {
			digit = number[i] - '0';
			if (digit>9) return ret;
		} else {
			digit = number[i];
			if (digit>127) return ret;
		}
		
		if (node->child[digit] == NULL) return ret;
		node = node->child[digit];
		i++;
		if (node->data != NULL) {
			if (nmatchptr) *nmatchptr=i;
			ret = &node->data;
		}
	}

	return ret;
}


void **dtrie_contains(struct dtrie_node_t *root, const char *number,
		const unsigned int numberlen, const unsigned int branches)
{
	int nmatch;
	void **ret;
	ret = dtrie_longest_match(root, number, numberlen, &nmatch, branches);

	if (nmatch == numberlen) return ret;
	return NULL;
}
