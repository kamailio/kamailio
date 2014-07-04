/*
 * Header file for hash table functions
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

#ifndef _HASH_H
#define _HASH_H

#include <stdio.h>
#include "uid_domain_mod.h"
#include "domain.h"

#define HASH_SIZE 128

/*
 * Hash table entry
 */
struct hash_entry {
	str key;                  /* Hash key */
	domain_t* domain;         /* Pointer to the domain structure */
	struct hash_entry* next;  /* Next element in hash table colision slot */
};


/*
 * Generate hash table, use domain names as hash keys
 */
int gen_domain_table(struct hash_entry** table, domain_t* list);


/*
 * Lookup key in the table
 */
int hash_lookup(domain_t** d, struct hash_entry** table, str* key);


/*
 * Generate hash table, use did as hash key
 */
int gen_did_table(struct hash_entry** table, domain_t* list);


/*
 * Free memory allocated for entire hash table
 */
void free_table(struct hash_entry** table);

#endif /* _HASH_H */
