/*
 * $Id$
 *
 * Hash functions for cached domain table
 *
 * Copyright (C) 2002-2003 Juha Heinanen
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "domain_mod.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


/* String hash function */
unsigned int hash (str *domain)
{
  char *p;
  unsigned int h=0;
  unsigned int len;
  unsigned int i;

  p = domain->s;
  len = domain->len;

  for (i = 0; i < len; i++) {
	  h = ( h << 5 ) - h + *(p + i);
  }

  return h % HASH_SIZE;
}


/* Add domain to hash table */
int hash_table_install (struct domain_list **hash_table, char *domain)
{
	struct domain_list *np;
	unsigned int hash_val;

	np = (struct domain_list *) shm_malloc(sizeof(*np));
	if (np == NULL) {
		LOG(L_CRIT, "hash_install(): Cannot allocate memory for table entry\n");
		return -1;
	}

	np->domain.len = strlen(domain);
	np->domain.s = (char *) shm_malloc(np->domain.len);
	if (np->domain.s == NULL) {
		LOG(L_CRIT, "hash_install(): Cannot allocate memory for domain string\n");
		return -1;
	}
	(void) strncpy(np->domain.s, domain, np->domain.len);

	hash_val = hash(&(np->domain));
	np->next = hash_table[hash_val];
	hash_table[hash_val] = np;

	return 1;
}


/* Check if domain exsist in hash table */
int hash_table_lookup (str *domain)
{
	struct domain_list *np;

	LOG(L_ERR, "looking hash entry %d for domain \'%.*s\'\n", hash(domain), domain->len, domain->s);

	for (np = (*hash_table)[hash(domain)]; np != NULL; np = np->next) {
		if ((np->domain.len == domain->len) && 
		    (strncasecmp(np->domain.s, domain->s, domain->len) == 0)) {
			return 1;
		}
	}
	return -1;
}


/* Print domains stored in hash table */
void hash_table_print (struct domain_list **hash_table, FILE *reply_file)
{
	int i;
	struct domain_list *np;

	for (i = 0; i < HASH_SIZE; i++) {
		np = hash_table[i];
		while (np) {
			fprintf(reply_file, "%4d %.*s\n", i, np->domain.len, np->domain.s);
			np = np->next;
		}
	}
}


/* Free contents of hash table */
void hash_table_free (struct domain_list **hash_table)
{
	int i;
	struct domain_list *np, *next;

	for (i = 0; i < HASH_SIZE; i++) {
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
