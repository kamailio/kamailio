/*
 * Prefix Route Module - tree search implementation
 *
 * Copyright (C) 2007 Alfred E. Heggestad
 * Copyright (C) 2008 Telio Telecom AS
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
/*! \file
 * \brief Prefix Route Module - tree search implementation
 * \ingroup prefix_route
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "../../atomic_ops.h"
#include "../../mem/shm_mem.h"
#include "../../str.h"
#include "../../lock_alloc.h"
#include "../../lock_ops.h"
#include "tree.h"


enum {
	DIGITS = 10
};


/** Defines a route item in the prefix tree */
struct tree_item {
	struct tree_item *digits[DIGITS];  /**< Child items for each digit */
	char name[16];                     /**< Route name (for dump)      */
	int route;                         /**< Valid route number if >0   */
};


/** Defines a locked prefix tree */
struct tree {
	struct tree_item *root;  /**< Root item of tree    */
	atomic_t refcnt;         /**< Reference counting   */
};


/* Local variables */
static struct tree **shared_tree = NULL;
static gen_lock_t* shared_tree_lock;


/**
 * Allocate and initialize a new tree item
 */
struct tree_item *tree_item_alloc(void)
{
	struct tree_item *root;
	int i;

	root = (struct tree_item *)shm_malloc(sizeof(*root));
	if (NULL == root) {
		LOG(L_CRIT, "tree_item_alloc: shared memory alloc failed\n");
		return NULL;
	}

	for (i=0; i<DIGITS; i++)
		root->digits[i] = NULL;

	root->route = 0;

	return root;
}


/**
 * Flush tree item
 */
void tree_item_free(struct tree_item *item)
{
	int i;

	if (NULL == item)
		return;

	for (i=0; i<DIGITS; i++) {
		tree_item_free(item->digits[i]);
	}

	shm_free(item);
}


/**
 * Add a route prefix rule to the tree
 */
int tree_item_add(struct tree_item *root, const char *prefix,
		  const char *route, int route_ix)
{
	struct tree_item *item;
	const char *p;
	int err;

	if (NULL == root || NULL == prefix || route_ix <= 0)
		return -1;

	item = root;
	for (p = prefix; '\0' != *p; p++) {
		int digit;

		if (!isdigit(*p))
			continue;

		digit = *p - '0';

		/* exist? */
		if (!item->digits[digit]) {
			item->digits[digit] = tree_item_alloc();
			if (!item->digits[digit]) {
				LOG(L_CRIT, "tree_item_add: alloc failed\n");
				err = -1;
				goto out;
			}
		}

		item = item->digits[digit];
	}

	if (NULL == item) {
		LOG(L_CRIT, "tree_item_add: internal error (no item)\n");
		err = -1;
		goto out;
	}

	if (item->route > 0) {
		LOG(L_ERR, "tree_item_add: prefix %s already set to %s\n",
		    prefix, item->name);
	}

	/* Set route number for the tree item */
	item->route = route_ix;

	/* Copy the route name (used in tree dump) */
	strncpy(item->name, route, sizeof(item->name)-1);
	item->name[sizeof(item->name)-1] = '\0';

	err = 0;

 out:
	return err;
}


/**
 * Get route number from username
 */
int tree_item_get(const struct tree_item *root, const str *user)
{
	const struct tree_item *item;
	const char *p, *pmax;
	int route = 0;

	if (NULL == root || NULL == user || NULL == user->s || !user->len)
		return -1;

	pmax = user->s + user->len;
	item = root;
	for (p = user->s; p < pmax ; p++) {
		int digit;

		if (!isdigit(*p)) {
			continue;
		}

		digit = *p - '0';

		/* Update route with best match so far */
		if (item->route > 0) {
			route = item->route;
		}

		/* exist? */
		if (NULL == item->digits[digit]) {
			break;
		}

		item = item->digits[digit];
	}

	return route;
}


/**
 * Print one tree item to a file handle
 */
void tree_item_print(const struct tree_item *item, FILE *f, int level)
{
	int i;

	if (NULL == item || NULL == f)
		return;

	if (item->route > 0) {
		fprintf(f, " \t--> route[%s] ", item->name);
	}

	for (i=0; i<DIGITS; i++) {
		int j;

		if (!item->digits[i]) {
			continue;
		}

		fputc('\n', f);
		for (j=0; j<level; j++)
			fputc(' ', f);

		fprintf(f, "%d ", i);
		tree_item_print(item->digits[i], f, level+1);
	}
}


/**
 * Allocate a new tree structure
 */
static struct tree *tree_alloc(void)
{
	struct tree *tree;

	tree = (struct tree *)shm_malloc(sizeof(*tree));
	if (NULL == tree)
		return NULL;

	tree->root    = NULL;
	atomic_set(&tree->refcnt, 0);

	return tree;
}


/**
 * Flush the tree
 */
static void tree_flush(struct tree *tree)
{
	if (NULL == tree)
		return;

	/* Wait for old tree to be released */
	for (;;) {
		const int refcnt = atomic_get(&tree->refcnt);

		if (refcnt <= 0)
			break;

		LOG(L_NOTICE, "prefix_route: tree_flush: waiting refcnt=%d\n",
		    refcnt);

		usleep(100000);
	};

	tree_item_free(tree->root);
	shm_free(tree);
}


/**
 * Access the shared tree and optionally increment/decrement the
 * reference count.
 */
static struct tree *tree_get(void)
{
	struct tree *tree;

	lock_get(shared_tree_lock);
	tree = *shared_tree;
	lock_release(shared_tree_lock);

	return tree;
}


static struct tree *tree_ref(void)
{
	struct tree *tree;

	lock_get(shared_tree_lock);
	tree = *shared_tree;
	atomic_inc(&tree->refcnt);
	lock_release(shared_tree_lock);

	return tree;
}


struct tree *tree_deref(struct tree *tree)
{
	if (tree)
		atomic_dec(&tree->refcnt);
	return tree;
}


int tree_init(void)
{
	/* Initialize lock */
	shared_tree_lock = lock_alloc();
	if (NULL == shared_tree_lock) {
		return -1;
	}
	lock_init(shared_tree_lock);

	/* Pointer to global tree must be in shared memory */
	shared_tree = (struct tree **)shm_malloc(sizeof(*shared_tree));
	if (NULL == shared_tree) {
		lock_destroy(shared_tree_lock);
		lock_dealloc(shared_tree_lock);
		shared_tree_lock=0;
		return -1;
	}

	*shared_tree = NULL;

	return 0;
}


void tree_close(void)
{
	if (shared_tree)
		tree_flush(tree_get());
	shared_tree = NULL;
	if (shared_tree_lock) {
		lock_destroy(shared_tree_lock);
		lock_dealloc(shared_tree_lock);
		shared_tree_lock=0;
	}
}


int tree_swap(struct tree_item *root)
{
	struct tree *new_tree, *old_tree;

	new_tree = tree_alloc();
	if (NULL == new_tree)
		return -1;

	new_tree->root = root;

	/* Save old tree */
	old_tree = tree_get();

	/* Critical - swap trees */
	lock_get(shared_tree_lock);
	*shared_tree = new_tree;
	lock_release(shared_tree_lock);

	/* Flush old tree */
	tree_flush(old_tree);

	return 0;
}


int tree_route_get(const str *user)
{
	struct tree *tree;
	int route;

	/* Find match in tree */
	tree = tree_ref();
	if (NULL == tree) {
		return -1;
	}

	route = tree_item_get(tree->root, user);
	tree_deref(tree);

	return route;
}


void tree_print(FILE *f)
{
	struct tree *tree;

	tree = tree_ref();

	fprintf(f, "Prefix route tree:\n");

	if (tree) {
		fprintf(f, " reference count: %d\n",
			atomic_get(&tree->refcnt));
		tree_item_print(tree->root, f, 0);
	}
	else {
		fprintf(f, " (no tree)\n");
	}

	tree_deref(tree);
}
