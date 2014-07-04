/*
 * Hash functions for cached domain table
 *
 * Copyright (C) 2002-2003 Juha Heinanen
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version
 *
 * sip-router is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "uid_domain_mod.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "hash.h"


/*
 * String hash function
 */
static unsigned int calc_hash(str *key)
{
	char *p;
	unsigned int h, len, i;

	h = 0;
	p = key->s;
	len = key->len;

	for (i = 0; i < len; i++) {
		h = ( h << 5 ) - h + *(p + i);
	}

	return h % HASH_SIZE;
}


/*
 * Create new hash_entry structure from given key and domain
 */
static struct hash_entry* new_hash_entry(str* key, domain_t* domain)
{
	struct hash_entry* e;

	if (!key || !domain) {
		ERR("Invalid parameter value\n");
		return 0;
	}

	e = (struct hash_entry*)shm_malloc(sizeof(struct hash_entry));
	if (!e) {
		ERR("Not enough memory left\n");
		return 0;
	}
	e->key = *key;
	e->domain = domain;
	e->next = 0;
	return e;
}


/*
 * Release all memory allocated for given hash_entry structure
 */
static void free_hash_entry(struct hash_entry* e)
{
	if (e) shm_free(e);
}


/*
 * Free memory allocated for entire hash table
 */
void free_table(struct hash_entry** table)
{
	struct hash_entry* e;
	int i;

	if (!table) return;

	for(i = 0; i < HASH_SIZE; i++) {
		while(table[i]) {
			e = table[i];
			table[i] = table[i]->next;
			free_hash_entry(e);
		}
	}
}


/*
 * Generate hash table, use domain names as hash keys
 */
int gen_domain_table(struct hash_entry** table, domain_t* list)
{
	struct hash_entry* e;
	unsigned int slot;
	int i;

	if (!table) {
		ERR("Invalid parameter value\n");
		return -1;
	}

	while(list) {
		for(i = 0; i < list->n; i++) {
			e = new_hash_entry(&list->domain[i], list);
			if (!e) goto error;
			slot = calc_hash(&list->domain[i]);
			e->next = table[slot];
			table[slot] = e;
		}

		list = list->next;
	}
	return 0;

 error:
	free_table(table);
	return -1;
}


/*
 * Generate hash table, use did as hash key
 */
int gen_did_table(struct hash_entry** table, domain_t* list)
{
	unsigned int slot;
	struct hash_entry* e;

	if (!table) {
		ERR("Invalid parameter value\n");
		return -1;
	}

	while(list) {
		e = new_hash_entry(&list->did, list);
		if (!e) goto error;
		slot = calc_hash(&list->did);
		e->next = table[slot];
		table[slot] = e;
		list = list->next;
	}
	return 0;
 error:
	free_table(table);
	return -1;
}


/*
 * Lookup key in the table
 */
int hash_lookup(domain_t** d, struct hash_entry** table, str* key)
{
	struct hash_entry* np;

	for (np = table[calc_hash(key)]; np != NULL; np = np->next) {
		if ((np->key.len == key->len) &&
			(strncmp(np->key.s, key->s, key->len) == 0)) {
			if (d) *d = np->domain;
			return 1;
		}
	}
	if (d) *d = 0;
	return -1;
}
