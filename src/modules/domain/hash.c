/*
 * Hash functions for cached domain table
 *
 * Copyright (C) 2002-2012 Juha Heinanen
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

/* Check if domain exists in hash table */
int hash_table_lookup (str *domain, str *did, struct attr_list **attrs)
{
	struct domain_list *np;

	for (np = (*hash_table)[dom_hash(domain)]; np != NULL; np = np->next) {
		if ((np->domain.len == domain->len) && 
		    (strncasecmp(np->domain.s, domain->s, domain->len) == 0)) {
		    *did = np->did;
		    *attrs = np->attrs;
		    return 1;
		}
	}

	return -1;
}

/* Add did attribute to hash table */
int hash_table_attr_install (struct domain_list **hash_table, str* did,
			     str *name, short type, int_str *val)
{
    struct attr_list *attr;
    struct domain_list *np;

    attr = (struct attr_list *)shm_malloc(sizeof(struct attr_list));
    if (attr == NULL) {
	LM_ERR("no shm memory left for attribute\n");
	return -1;
    }
    attr->name.s = (char *)shm_malloc(name->len);
    if (attr->name.s == NULL) {
	LM_ERR("no shm memory left for attribute name\n");
	shm_free(attr);
	return -1;
    }
    memcpy(attr->name.s, name->s, name->len);
    attr->name.len = name->len;
    attr->type = type;
    attr->val.n = val->n;
    attr->val.s = val->s;
    if (type == 2) {
	attr->val.s.s = (char *)shm_malloc(val->s.len);
	if (attr->val.s.s == NULL) {
	    LM_ERR("no shm memory left for attribute value\n");
	    shm_free(attr->name.s);
	    shm_free(attr);
	}
	memcpy(attr->val.s.s, val->s.s, val->s.len);
	attr->val.s.len = val->s.len;
    }
    attr->next = NULL;

    np = hash_table[DOM_HASH_SIZE];
    while (np) {
	if ((np->did.len == did->len) && 
	    (strncasecmp(np->did.s, did->s, did->len) == 0)) {
	    if (np->attrs) attr->next = np->attrs;
	    np->attrs = attr;
	    return 1;
	}
	np = np->next;
    }
    np = (struct domain_list *)shm_malloc(sizeof(struct domain_list));
    if (np == NULL) {
	LM_ERR("no shm memory left for domain list\n");
	if (type == 2) shm_free(attr->name.s);
	shm_free(attr);
	return -1;
    }
    np->did.s = (char *)shm_malloc(did->len);
    if (np->did.s == NULL) {
	LM_ERR("no shm memory left for did\n");
	if (type == 2) shm_free(attr->name.s);
	shm_free(attr);
	shm_free(np);
	return -1;
    }
    memcpy(np->did.s, did->s, did->len);
    np->did.len = did->len;
    np->attrs = attr;
    np->next = hash_table[DOM_HASH_SIZE];
    hash_table[DOM_HASH_SIZE] = np;
	
    return 1;
}

/* Add domain to hash table */
int hash_table_install (struct domain_list **hash_table, str* did, str *domain)
{
        struct domain_list *np, *dl;
	unsigned int hash_val;
	    
	np = (struct domain_list *) shm_malloc(sizeof(*np));
	if (np == NULL) {
		LM_ERR("no shared memory for hash table entry\n");
		return -1;
	}

	np->did.len = did->len;
	np->did.s = (char *)shm_malloc(did->len);
	if (np->did.s == NULL) {
		LM_ERR("no shared memeory for did\n");
	        shm_free(np);
		return -1;
	}
	(void)memcpy(np->did.s, did->s, did->len);

	np->domain.len = domain->len;
	np->domain.s = (char *) shm_malloc(domain->len);
	if (np->domain.s == NULL) {
		LM_ERR("no shared memory for domain\n");
	        shm_free(np);
		return -1;
	}
	(void)strncpy(np->domain.s, domain->s, domain->len);

	np->attrs = NULL;
	dl = hash_table[DOM_HASH_SIZE];
	while (dl) {
	    if ((dl->did.len == did->len) && 
		(strncasecmp(dl->did.s, did->s, did->len) == 0)) {
		np->attrs = dl->attrs;
		break;
	    }
	    dl = dl->next;
	}
	hash_val = dom_hash(&np->domain);
	np->next = hash_table[hash_val];
	hash_table[hash_val] = np;

	return 1;
}

int hash_table_mi_print(struct domain_list **hash_table, struct mi_node* rpl)
{
	int i;
	struct domain_list *np;
	struct attr_list *ap;
	struct mi_node *dnode, *node;

	if(hash_table==0) return -1;
	for (i = 0; i < DOM_HASH_SIZE; i++) {
		np = hash_table[i];
		while (np) {
			dnode = add_mi_node_child(rpl, 0, "domain", 6, 
						  np->domain.s, np->domain.len);
			if (dnode == 0) return -1;
			node = add_mi_node_child(dnode, 0, "did", 3, 
						 np->did.s, np->did.len);
			if (node == 0) return -1;
			np = np->next;
		}
	}
	np = hash_table[DOM_HASH_SIZE];
	while (np) {
	    dnode = add_mi_node_child(rpl, 0, "did", 3, 
				      np->did.s, np->did.len);
	    if (dnode == 0) return -1;
	    ap = np->attrs;
	    while (ap) {
		node = add_mi_node_child(dnode, 0, "attr", 4, 
					 ap->name.s, ap->name.len);
		if (node == 0) return -1;
		ap = ap->next;
	    }
	    np = np->next;
	}
	return 0;
}

/* Free contents of hash table */
void hash_table_free (struct domain_list **hash_table)
{
    int i;
    struct domain_list *np, *next;
    struct attr_list *ap, *next_ap;

    if (hash_table == 0) return;

    for (i = 0; i < DOM_HASH_SIZE; i++) {
	np = hash_table[i];
	while (np) {
	    shm_free(np->did.s);
	    shm_free(np->domain.s);
	    next = np->next;
	    shm_free(np);
	    np = next;
	}
	hash_table[i] = NULL;
    }

    np = hash_table[DOM_HASH_SIZE];
    while (np) {
	shm_free(np->did.s);
	ap = np->attrs;
	while (ap) {
	    shm_free(ap->name.s);
	    if (ap->type == 2) shm_free(ap->val.s.s);
	    next_ap = ap->next;
	    shm_free(ap);
	    ap = next_ap;
	}
	np = np->next;
    }

    hash_table[DOM_HASH_SIZE] = NULL;
    return;
}
