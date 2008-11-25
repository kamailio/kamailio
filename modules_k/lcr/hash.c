/*
 * Header file for hash table functions
 *
 * Copyright (C) 2008 Juha Heinanen
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


#include "../../mem/shm_mem.h"
#include "lcr_mod.h"

#define lcr_hash(_s) core_hash( _s, 0, lcr_hash_size_param)

/* Add lcr entry into hash table */
int lcr_hash_table_insert(struct lcr_info **hash_table,
			  unsigned short prefix_len, char *prefix,
			  unsigned short from_uri_len, char *from_uri,
			  pcre *from_uri_re, unsigned int grp_id,
			  unsigned short first_gw, unsigned short priority)
{
    struct lcr_info *lcr;
    str prefix_str;
    unsigned int hash_val;

    lcr = (struct lcr_info *)shm_malloc(sizeof(struct lcr_info));
    if (lcr == NULL) {
	LM_ERR("Cannot allocate memory for lcr hash table entry\n");
	return 0;
    }
    memset(lcr, 0, sizeof(struct lcr_info));

    lcr->prefix_len = prefix_len;
    if (prefix_len) {
	memcpy(lcr->prefix, prefix, prefix_len);
    }
    lcr->from_uri_len = from_uri_len;
    if (from_uri_len) {
	memcpy(lcr->from_uri, from_uri, from_uri_len);
	(lcr->from_uri)[from_uri_len] = '\0';
	lcr->from_uri_re = from_uri_re;
    }
    lcr->grp_id = grp_id;
    lcr->first_gw = first_gw;
    lcr->priority = priority;
    
    prefix_str.len = prefix_len;
    prefix_str.s = prefix;

    hash_val = lcr_hash(&prefix_str);
    lcr->next = hash_table[hash_val];
    hash_table[hash_val] = lcr;
    
    LM_DBG("inserted prefix <%.*s>, from_uri <%.*s>, grp_id <%u>, "
	   "priority <%u> into index <%u>\n",
	   prefix_len, prefix, from_uri_len, from_uri, grp_id,
	   priority, hash_val);

    return 1;
}


/* 
 * Return pointer to lcr hash table entry to which given prefix hashes to.
 */
struct lcr_info *lcr_hash_table_lookup(struct lcr_info **hash_table,
				       unsigned short prefix_len, char *prefix)
{
    str prefix_str;

    LM_DBG("looking for <%.*s>\n", prefix_len, prefix);

    prefix_str.len = prefix_len;
    prefix_str.s = prefix;

    return (hash_table)[lcr_hash(&prefix_str)];
}


/* Free contents of lcr hash table */
void lcr_hash_table_contents_free(struct lcr_info **hash_table)
{
    int i;
    struct lcr_info *lcr_rec, *next;
	
    if (hash_table == 0)
	return;

    for (i = 0; i <= lcr_hash_size_param; i++) {
	lcr_rec = hash_table[i];
	while (lcr_rec) {
	    LM_DBG("freeing lcr hash table prefix <%.*s> grp_id <%u>\n",
		   lcr_rec->prefix_len, lcr_rec->prefix, lcr_rec->grp_id);
	    if (lcr_rec->from_uri_re) {
		shm_free(lcr_rec->from_uri_re);
	    }
	    next = lcr_rec->next;
	    shm_free(lcr_rec);
	    lcr_rec = next;
	}
	hash_table[i] = NULL;
    }
}
