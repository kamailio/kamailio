/*
 * $Id$
 *
 * Hash functions for cached domain table
 *
 * Copyright (C) 2002-2008 Juha Heinanen
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "../../dprint.h"
#include "../../ut.h"
#include "../../hashes.h"
#include "../../mem/shm_mem.h"
#include "../../lib/kmi/mi.h"
#include "domain_mod.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define dom_hash(_s)  core_case_hash( _s, 0, DOM_HASH_SIZE)


/* Add domain to hash table */
int hash_table_install (struct domain_list **hash_table, char *domain)
{
	struct domain_list *np;
	unsigned int hash_val;

	np = (struct domain_list *) shm_malloc(sizeof(*np));
	if (np == NULL) {
		LM_ERR("Cannot allocate memory for hash table entry\n");
		return -1;
	}

	np->domain.len = strlen(domain);
	np->domain.s = (char *) shm_malloc(np->domain.len);
	if (np->domain.s == NULL) {
		LM_ERR("Cannot allocate memory for domain string\n");
	        shm_free(np);
		return -1;
	}
	(void) strncpy(np->domain.s, domain, np->domain.len);

	hash_val = dom_hash(&np->domain);
	np->next = hash_table[hash_val];
	hash_table[hash_val] = np;

	return 1;
}


/* Check if domain exists in hash table */
int hash_table_lookup (str *domain)
{
	struct domain_list *np;

	for (np = (*hash_table)[dom_hash(domain)]; np != NULL; np = np->next) {
		if ((np->domain.len == domain->len) && 
		    (strncasecmp(np->domain.s, domain->s, domain->len) == 0)) {
			return 1;
		}
	}

	return -1;
}


int hash_table_mi_print(struct domain_list **hash_table, struct mi_node* rpl)
{
	int i;
	struct domain_list *np;
	struct mi_node* node;

	if(hash_table==0)
		return -1;
	for (i = 0; i < DOM_HASH_SIZE; i++) {
		np = hash_table[i];
		while (np) {
			node = add_mi_node_child(rpl, 0, 0, 0, 
					np->domain.s, np->domain.len);
			if(node == 0)
				return -1;

			np = np->next;
		}
	}
	return 0;
}

/* Free contents of hash table */
void hash_table_free (struct domain_list **hash_table)
{
	int i;
	struct domain_list *np, *next;
	
	if(hash_table==0)
		return;

	for (i = 0; i < DOM_HASH_SIZE; i++) {
		np = hash_table[i];
		while (np) {
			shm_free(np->domain.s);
			next = np->next;
			shm_free(np);
			np = next;
		}
		hash_table[i] = NULL;
	}
}
