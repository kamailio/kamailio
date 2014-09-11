/*
 * $Id$
 *
 * Hash functions for cached trusted table
 *
 * Copyright (C) 2003 Juha Heinanen
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <sys/types.h>
#include <regex.h>
#include "../../mem/shm_mem.h"
#include "../../parser/parse_from.h"
#include "../../ut.h"
#include "trusted_hash.h"


/*
 * Create and initialize a hash table
 */
struct trusted_list** new_hash_table(void)
{
	struct trusted_list** ptr;

	     /* Initializing hash tables and hash table variable */
	ptr = (struct trusted_list **)shm_malloc(sizeof(struct trusted_list*) * HASH_SIZE);
	if (!ptr) {
		LOG(L_ERR, "new_hash_table(): No memory for hash table\n");
		return 0;
	}

	memset(ptr, 0, sizeof(struct trusted_list*) * HASH_SIZE);
	return ptr;
}


/*
 * Release all memory allocated for a hash table
 */
void free_hash_table(struct trusted_list** table)
{
	if (table) {
	        empty_hash_table(table);
	}

	shm_free(table);
}


/* 
 * String hash function 
 */
static unsigned int hash(str* src_ip)
{
	char *p;
	unsigned int h = 0;
	unsigned int len;
	unsigned int i;
	
	p = src_ip->s;
	len = src_ip->len;
	
	for (i = 0; i < len; i++) {
		h = ( h << 5 ) - h + *(p + i);
	}

	return h % HASH_SIZE;
}


/* 
 * Add <src_ip, proto, pattern> into hash table, where proto is integer
 * representation of string argument proto.
 */
int hash_table_insert(struct trusted_list** hash_table, char* src_ip, char* proto, char* pattern)
{
	struct trusted_list *np;
	unsigned int hash_val;

	np = (struct trusted_list *) shm_malloc(sizeof(*np));
	if (np == NULL) {
		LOG(L_CRIT, "hash_table_insert(): Cannot allocate memory for table entry\n");
		return -1;
	}

	np->src_ip.len = strlen(src_ip);
	np->src_ip.s = (char *) shm_malloc(np->src_ip.len);

	if (np->src_ip.s == NULL) {
		LOG(L_CRIT, "hash_table_insert(): Cannot allocate memory for src_ip string\n");
		return -1;
	}

	(void) strncpy(np->src_ip.s, src_ip, np->src_ip.len);

	if (strcmp(proto, "any") == 0) {
		np->proto = PROTO_NONE;
	} else if (strcmp(proto, "udp") == 0) {
		np->proto = PROTO_UDP;
	} else if (strcmp(proto, "tcp") == 0) {
		np->proto = PROTO_TCP;
	} else if (strcmp(proto, "tls") == 0) {
		np->proto = PROTO_TLS;
	} else if (strcmp(proto, "sctp") == 0) {
		np->proto = PROTO_SCTP;
	} else {
		LOG(L_CRIT, "hash_table_insert(): Unknown protocol '%s'\n", proto);
		return -1;
	}
		
	np->pattern = (char *) shm_malloc(strlen(pattern)+1);
	if (np->pattern == NULL) {
		LOG(L_CRIT, "hash_table_insert(): Cannot allocate memory for pattern string\n");
		return -1;
	}
	(void) strcpy(np->pattern, pattern);

	hash_val = hash(&(np->src_ip));
	np->next = hash_table[hash_val];
	hash_table[hash_val] = np;

	return 1;
}


/* 
 * Check if an entry exists in hash table that has given src_ip and protocol
 * value and pattern that matches to From URI.
 */
int match_hash_table(struct trusted_list** table, struct sip_msg* msg)
{
	str uri;
	char uri_string[MAX_URI_SIZE + 1];
	regex_t preg;
	struct trusted_list *np;
	str src_ip;

	src_ip.s = ip_addr2a(&msg->rcv.src_ip);
	src_ip.len = strlen(src_ip.s);

	if (parse_from_header(msg) < 0) return -1;
	uri = get_from(msg)->uri;
	if (uri.len > MAX_URI_SIZE) {
		LOG(L_ERR, "match_hash_table(): From URI too large\n");
		return -1;
	}
	memcpy(uri_string, uri.s, uri.len);
	uri_string[uri.len] = (char)0;

	for (np = table[hash(&src_ip)]; np != NULL; np = np->next) {
		if ((np->src_ip.len == src_ip.len) && 
		    (strncasecmp(np->src_ip.s, src_ip.s, src_ip.len) == 0) &&
		    ((np->proto == PROTO_NONE) || (np->proto == msg->rcv.proto))) {
			if (regcomp(&preg, np->pattern, REG_NOSUB)) {
				LOG(L_ERR, "match_hash_table(): Error in regular expression\n");
				return -1;
			}
			if (regexec(&preg, uri_string, 0, (regmatch_t *)0, 0)) {
				regfree(&preg);
			} else {
				regfree(&preg);
				return 1;
			}
		}
	}
	return -1;
}


/* 
 * Print domains stored in hash table 
 */
void hash_table_print(struct trusted_list** hash_table, rpc_t* rpc, void* c)
{
	void* st;
	int i;
	struct trusted_list *np;

	for (i = 0; i < HASH_SIZE; i++) {
		np = hash_table[i];
		while (np) {
			if (rpc->add(c, "{", &st) < 0) return;
			rpc->struct_add(st, "Sds", "src_ip", &np->src_ip, "proto", np->proto, "pattern", np->pattern);
			np = np->next;
		}
	}
}

/* 
 * Free contents of hash table, it doesn't destroy the
 * hash table itself
 */
void empty_hash_table(struct trusted_list **hash_table)
{
	int i;
	struct trusted_list *np, *next;

	for (i = 0; i < HASH_SIZE; i++) {
		np = hash_table[i];
		while (np) {
			shm_free(np->src_ip.s);
			shm_free(np->pattern);
			next = np->next;
			shm_free(np);
			np = next;
		}
		hash_table[i] = 0;
	}
}
