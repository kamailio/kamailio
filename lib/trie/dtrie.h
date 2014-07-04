/*
 * $Id: dtrie.h 5237 2008-11-21 10:17:10Z henningw $
 *
 * Copyright (C) 2008 1&1 Internet AG
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

#ifndef _DTRIE_H_
#define _DTRIE_H_


/*! Trie node */
struct dtrie_node_t {
	struct dtrie_node_t **child; /*!< children */
	void *data; /*!< custom data */
};


/*! Function signature for destroying the payload. First parameter is the payload. */
typedef void(*dt_delete_func_t)(void *);


/*!
 * \brief Allocates memory for the root node and initializes it
 * \param branches number of branches in the trie
 * \return pointer to an initialized root node on success, NULL otherwise.
 */
struct dtrie_node_t *dtrie_init(const unsigned int branches);


/*!
 * \brief Deletes a subtree and frees memory including node
 * \param root root node of the whole tree
 * \param node root of the subtree
 * \param delete_payload pointer to a function for deleting payload. If NULL, it will not be used.
 * \param branches number of branches in the trie
 * \note Memory for the root node is not freed.
 */
void dtrie_delete(struct dtrie_node_t *root, struct dtrie_node_t *node,
		dt_delete_func_t delete_payload, const unsigned int branches);


/*!
 * \brief Deletes the whole tree and frees the memory including the root node
 * \param root root node
 * \param delete_payload pointer to a function for deleting payload. If NULL, it will not be used.
 * \param branches number of branches in the trie
 */
void dtrie_destroy(struct dtrie_node_t **root, dt_delete_func_t delete_payload,
		const unsigned int branches);


/*!
 * \brief Deletes everything but the root node
 *
 * Deletes everything but the root node, the root node is new initialized.
 * This could be used to create an empty tree like after dtrie_init. It
 * also deletes eventual payload on all nodes including the root node.
 * \param root root node
 * \param delete_payload pointer to a function for deleting payload. If NULL, it will not be used.
 * \param branches number of branches in the trie
 */
void dtrie_clear(struct dtrie_node_t *root, dt_delete_func_t delete_payload,
		const unsigned int branches);


/*!
 * \brief Insert a number with a corresponding id
 *
 * Insert a number with a corresponding id. Nodes are created if necessary
 * and the node after the last digit is marked with the given id.
 * \param root root node
 * \param number inserted number string
 * \param numberlen number of individual numbers in number
 * \param data pointer to some custom data
 * \param branches number of branches in the trie
 * \return 0 on success, -1 otherwise.
 */
int dtrie_insert(struct dtrie_node_t *root, const char *number, const unsigned int numberlen,
		void *data, const unsigned int dtrie_size);


/*!
 * \brief Returns the number of nodes in the given tree
 * \param root root node
 * \param branches number of branches in the trie
 * \return number of nodes in tree, at least 1
 */
unsigned int dtrie_size(const struct dtrie_node_t *root, const unsigned int branches);


/*!
 * \brief Returns the number of nodes in the given tree that holds custom data.
 *
 * Returns the number of nodes in the given tree that are loaded with data (data != NULL).
 * \param root root node
 * \param branches number of branches in the trie
 * \return number of nodes in the tree with custom data
 */
unsigned int dtrie_loaded_nodes(const struct dtrie_node_t *root, const unsigned int branches);


/*!
 * \brief Returns the number of leaf nodes in the given tree
 *
 * Returns the number of leaf nodes in the given tree.
 * On leaf nodes the leaf flag is set, on other nodes it is cleared.
 * \param root root node
 * \param branches number of branches in the trie
 * \return number of leaf nodes
 */
unsigned int dtrie_leaves(const struct dtrie_node_t *root, const unsigned int branches);


/*!
 *\brief Find the longest prefix match of number in root.
 *
 * Find the longest prefix match of number in root.
 * Set *data according to value in the tree.
 * \param root root node
 * \param number matched prefix
 * \param numberlen length of number
 * \param nmatchptr if not NULL store the number of matched digits or -1 if not found.
 * \param branches number of branches in the trie
 * \return the address of the pointer in the tree node if number is found in root, NULL if the number is not found.
 */
void **dtrie_longest_match(struct dtrie_node_t *root, const char *number,
		const unsigned int numberlen, int *nmatchptr, const unsigned int branches);


/*!
 * \brief Check if the trie contains a number
 * \param root root node
 * \param number matched prefix
 * \param numberlen length of number
 * \param branches number of branches in the trie
 * \return the address of the pointer in the tree node if number is found in root, NULL if the number is not found.
 */
void **dtrie_contains(struct dtrie_node_t *root, const char *number,
		const unsigned int numberlen, const unsigned int branches);


#endif
