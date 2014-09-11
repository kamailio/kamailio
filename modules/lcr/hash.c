/*
 * Header file for hash table functions
 *
 * Copyright (C) 2008-2012 Juha Heinanen
 *
 * This file is part of SIP Router, a free SIP server.
 *
 * SIP Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP Router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file
 * \brief SIP Router LCR :: Header file for hash table functions
 * \ingroup lcr
 * Module: \ref lcr
 */

#include "../../mem/shm_mem.h"
#include "../../hashes.h"
#include "lcr_mod.h"

#define rule_hash(_s) core_hash(_s, 0, lcr_rule_hash_size_param)

/* Add lcr entry into hash table */
int rule_hash_table_insert(struct rule_info **hash_table,
			   unsigned int lcr_id, unsigned int rule_id,
			   unsigned short prefix_len, char *prefix,
			   unsigned short from_uri_len, char *from_uri,
			   pcre *from_uri_re, unsigned short request_uri_len,
			   char *request_uri, pcre *request_uri_re,
			   unsigned short stopper)
{
    struct rule_info *rule;
    str prefix_str;
    unsigned int hash_val;
    struct rule_id_info *rid;

    rule = (struct rule_info *)shm_malloc(sizeof(struct rule_info));
    if (rule == NULL) {
	LM_ERR("no shm memory for rule hash table entry\n");
	if (from_uri_re) shm_free(from_uri_re);
	if (request_uri_re) shm_free(request_uri_re);
	return 0;
    }
    memset(rule, 0, sizeof(struct rule_info));

    rule->rule_id = rule_id;
    rule->prefix_len = prefix_len;
    if (prefix_len) {
	memcpy(rule->prefix, prefix, prefix_len);
    }
    rule->from_uri_len = from_uri_len;
    if (from_uri_len) {
	memcpy(rule->from_uri, from_uri, from_uri_len);
	(rule->from_uri)[from_uri_len] = '\0';
	rule->from_uri_re = from_uri_re;
    }
    rule->request_uri_len = request_uri_len;
    if (request_uri_len) {
	memcpy(rule->request_uri, request_uri, request_uri_len);
	(rule->request_uri)[request_uri_len] = '\0';
	rule->request_uri_re = request_uri_re;
    }
    rule->stopper = stopper;
    rule->targets = (struct target *)NULL;

    prefix_str.len = rule->prefix_len;
    prefix_str.s = rule->prefix;

    hash_val = rule_hash(&prefix_str);
    rule->next = hash_table[hash_val];
    hash_table[hash_val] = rule;
    
    LM_DBG("inserted rule_id <%u>, prefix <%.*s>, from_uri <%.*s>, "
	   "request_uri <%.*s>, stopper <%u>, into index <%u>\n",
	   rule_id, prefix_len, prefix, from_uri_len, from_uri,
	   request_uri_len, request_uri, stopper, hash_val);

    /* Add rule_id info to rule_id hash table */
    rid = (struct rule_id_info *)pkg_malloc(sizeof(struct rule_id_info));
    if (rid == NULL) {
	LM_ERR("no pkg memory for rule_id hash table entry\n");
	return 0;
    }
    memset(rid, 0, sizeof(struct rule_id_info));
    rid->rule_id = rule_id;
    rid->rule_addr = rule;
    hash_val = rule_id % lcr_rule_hash_size_param;
    rid->next = rule_id_hash_table[hash_val];
    rule_id_hash_table[hash_val] = rid;
    LM_DBG("inserted rule_id <%u> addr <%p> into rule_id hash table "
	   "index <%u>\n", rule_id, rule, hash_val);

    return 1;
}


/* Find gw table index with gw_id */
int get_gw_index(struct gw_info *gws, unsigned int gw_id,
		 unsigned short *gw_index)
{
    unsigned short gw_count, i;

    gw_count = gws[0].ip_addr.u.addr32[0];

    for (i = 1; i <= gw_count; i++) {
	if (gws[i].gw_id == gw_id) {
	    *gw_index = i;
	    return 1;
	}
    }
    return 0;
}


/* Insert target into hash table rule */
int rule_hash_table_insert_target(struct rule_info **hash_table,
				   struct gw_info *gws,
				   unsigned int rule_id, unsigned int gw_id,
				   unsigned int priority, unsigned int weight)
{
    unsigned short gw_index;
    struct target *target;
    struct rule_id_info *rid;

    target = (struct target *)shm_malloc(sizeof(struct target));
    if (target == NULL) {
	LM_ERR("cannot allocate memory for rule target\n");
	return 0;
    }

    if (get_gw_index(gws, gw_id, &gw_index) == 0) {
	LM_DBG("could not find (disabled) gw with id <%u>\n", gw_id);
	shm_free(target);
	return 2;
    }

    target->gw_index = gw_index;
    target->priority = priority;
    target->weight = weight;

    rid = rule_id_hash_table[rule_id % lcr_rule_hash_size_param];
    while (rid) {
	if (rid->rule_id == rule_id) {
	    target->next = rid->rule_addr->targets;
	    rid->rule_addr->targets = target;
	    LM_DBG("found rule with id <%u> and addr <%p>\n",
		    rule_id, rid->rule_addr);
	    return 1;
	}
	rid = rid->next;
    }

    LM_DBG("could not find (disabled) rule with id <%u>\n", rule_id);
    shm_free(target);
    return 2;
}


/* 
 * Return pointer to lcr hash table entry to which given prefix hashes to.
 */
struct rule_info *rule_hash_table_lookup(struct rule_info **hash_table,
					 unsigned short prefix_len,
					 char *prefix)
{
    str prefix_str;

    prefix_str.len = prefix_len;
    prefix_str.s = prefix;

    return (hash_table)[rule_hash(&prefix_str)];
}


/* Free contents of lcr hash table */
void rule_hash_table_contents_free(struct rule_info **hash_table)
{
    int i;
    struct rule_info *r, *next_r;
    struct target *t, *next_t;
	
    if (hash_table == 0)
	return;

    for (i = 0; i <= lcr_rule_hash_size_param; i++) {
	r = hash_table[i];
	while (r) {
	    if (r->from_uri_re) {
		shm_free(r->from_uri_re);
	    }
	    if (r->request_uri_re)
		shm_free(r->request_uri_re);
	    t = r->targets;
	    while (t) {
		next_t = t->next;
		shm_free(t);
		t = next_t;
	    }
	    next_r = r->next;
	    shm_free(r);
	    r = next_r;
	}
	hash_table[i] = NULL;
    }
}

/* Free contents of rule_id hash table */
void rule_id_hash_table_contents_free()
{
    int i;
    struct rule_id_info *r, *next_r;
	
    if (rule_id_hash_table == 0)
	return;

    for (i = 0; i < lcr_rule_hash_size_param; i++) {
	r = rule_id_hash_table[i];
	while (r) {
	    next_r = r->next;
	    pkg_free(r);
	    r = next_r;
	}
	rule_id_hash_table[i] = NULL;
    }
}
