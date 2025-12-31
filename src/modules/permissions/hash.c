/*
 * Hash functions for cached trusted and address tables
 *
 * Copyright (C) 2003-2012 Juha Heinanen
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

#include <sys/types.h>
#include <regex.h>
#include "parse_config.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/parser/parse_from.h"
#include "../../core/ut.h"
#include "../../core/hashes.h"
#include "../../core/usr_avp.h"
#include "../../core/ip_addr.h"
#include "../../core/pvar.h"
#include "hash.h"
#include "trusted.h"
#include "address.h"

#define perm_hash(_s) core_hash(&(_s), 0, PERM_HASH_SIZE)


/* tag AVP specs */
static avp_flags_t tag_avp_type;
static avp_name_t tag_avp;

extern int perm_peer_tag_mode;


extern int _perm_max_subnets;
extern int _perm_subnet_match_mode;

#define PERM_MAX_SUBNETS _perm_max_subnets

/*
 * Parse and set tag AVP specs
 */
int init_tag_avp(str *tag_avp_param)
{
	pv_spec_t avp_spec;
	avp_flags_t avp_flags;

	if(tag_avp_param->s && tag_avp_param->len > 0) {
		if(pv_parse_spec(tag_avp_param, &avp_spec) == 0
				|| avp_spec.type != PVT_AVP) {
			LM_ERR("malformed or non "
				   "AVP %.*s peer_tag_avp definition\n",
					tag_avp_param->len, tag_avp_param->s);
			return -1;
		}
		if(pv_get_avp_name(0, &avp_spec.pvp, &tag_avp, &avp_flags) != 0) {
			LM_ERR("[%.*s]- invalid "
				   "peer_tag_avp AVP definition\n",
					tag_avp_param->len, tag_avp_param->s);
			return -1;
		}
		tag_avp_type = avp_flags;
	} else {
		tag_avp.n = 0;
	}
	return 0;
}


/*
 * Gets tag avp specs
 */
void get_tag_avp(int_str *tag_avp_p, int *tag_avp_type_p)
{
	*tag_avp_p = tag_avp;
	*tag_avp_type_p = tag_avp_type;
}


/**
 * De-allocation helpers.
 */
void trusted_table_free_row_lock(gen_lock_t *row_lock)
{
	if(!row_lock) {
		LM_ERR("Nothing to clean! Empty lock.\n");
		return;
	}
	lock_destroy(row_lock);
	lock_dealloc(row_lock);
	return;
}
void trusted_table_free_entry(struct trusted_list *entry)
{
	if(!entry)
		return;

	/* clean the content */
	if(entry->src_ip.s)
		shm_free(entry->src_ip.s);
	if(entry->pattern)
		shm_free(entry->pattern);
	if(entry->ruri_pattern)
		shm_free(entry->ruri_pattern);
	if(entry->tag.s)
		shm_free(entry->tag.s);

	/* free entry itself */
	shm_free(entry);

	return;
}

/**
 * Must be used with acquired lock.
 */
void trusted_table_free_entries(struct trusted_list *given_entry)
{
	struct trusted_list *entry, *last_entry;

	if(!given_entry)
		return;

	entry = given_entry;
	while(entry)
	{
		last_entry = entry;
		entry = entry->next;
		trusted_table_free_entry(last_entry);
		last_entry = NULL;
	}

	return;
}


/**
 * Destroy hash table content.
 * Locks can be left intact,
 * e.g. in case this is just a table re-initialization.
 */
void trusted_table_free_buckets(struct trusted_hash_table *trusted_table, bool free_locks,
		bool keep_dummy_head)
{
	if(!trusted_table) {
		LM_ERR("Nothing to de-allocate! Empty trusted table pointer.");
		return;
	}

	unsigned int orig_size = trusted_table->size;

	for(int i = 0; i < orig_size; i++)
	{
		if(!trusted_table->row_locks[i]) {
			LM_ERR("Absent table row lock[%d], cannot acquire it and de-allocate members.\n", i);
			continue;
		} else {
			lock_get(trusted_table->row_locks[i]);
		}

		/* now clean the bucket */
		if(!trusted_table->row_entry_list[i]) {
			LM_DBG("Table bucket[%d] is already empty, cannot de-allocate its content.\n", i);
		} else {
			/* in case of bucket re-init, leave the dummy head (list is never NULL when using) */
			if (keep_dummy_head) {
				struct trusted_list *dummy = trusted_table->row_entry_list[i];
				trusted_table_free_entries(dummy->next);
				dummy->next = NULL;
				/* ensure that dummy is always having no real values */
				trusted_table->row_entry_list[i]->pattern = NULL;
				trusted_table->row_entry_list[i]->ruri_pattern = NULL;
				trusted_table->row_entry_list[i]->next = NULL;
				trusted_table->row_entry_list[i]->priority = 0;

				/* don't decrement the size,
				 * because in fact the bucket's size is still the same (as during init) */
			} else {
				/* free entries in the bucket */
				trusted_table_free_entries(trusted_table->row_entry_list[i]);
				trusted_table->row_entry_list[i] = NULL;
				/* track real table size */
				trusted_table->size--;
			}
		}

		lock_release(trusted_table->row_locks[i]);

		/* and accordingly a lock */
		if (free_locks) {
			trusted_table_free_row_lock(trusted_table->row_locks[i]);
			trusted_table->row_locks[i] = NULL;
		}
	}
}


/**
 * Clean trusted table (frees all previously allocated shm).
 */
int trusted_table_destroy(struct trusted_hash_table *trusted_table)
{
	if(!trusted_table) {
		LM_ERR("Nothing to de-allocate! Empty trusted table pointer.");
		return -1;
	}

	/* de-allocate row locks */
	if(!trusted_table->row_locks) {
		LM_ERR("Empty row locks, cannot acquire it and de-allocate trusted table members.\n");
		shm_free(trusted_table);
		return -1;
	}

	/* destroy hashtable content */
	trusted_table_free_buckets(trusted_table, true, false);

	/* clean entries list */
	if(!trusted_table->row_entry_list) {
		/* not really critical */
		LM_ERR("Cannot clean the row entries list, it's NULL.\n");
	} else {
		shm_free(trusted_table->row_entry_list);
		trusted_table->row_entry_list = NULL;
	}

	/* destroy trusted table row locks list */
	shm_free(trusted_table->row_locks);
	trusted_table->row_locks = NULL;

	/* destroy trusted table itself */
	shm_free(trusted_table);

	return 0;
}


int trusted_table_reinit(struct trusted_hash_table * trusted_table,
		unsigned int hash_table_size)
{
	DBG("re-initializing table.\n");

	trusted_table_free_buckets(trusted_table, false, true);
	if (trusted_table_init(trusted_table, PERM_HASH_SIZE)) {
		LM_ERR("failed to re-initialize the new trusted table.\n");
		return -1;
	}
	return 0;
}


/**
 * Initialize main members and locks.
 */
int trusted_table_init(struct trusted_hash_table * trusted_table,
		unsigned int hash_table_size)
{
	if(!trusted_table) {
		LM_ERR("Cannot initialize this table, empty trusted table pointer.");
		return -1;
	}

	DBG("init trusted_table size = %d\n", hash_table_size);

	for(unsigned int i = 0; i < hash_table_size; i++)
	{
		/* init locks (can be already in place, e.g. re-init) */
		if (!trusted_table->row_locks[i]) {
			trusted_table->row_locks[i] = lock_alloc();

			if(!trusted_table->row_locks[i]) {
				LM_ERR("no shared memory left to allocate a row lock[%d] for this trusted table\n", i);
				trusted_table_destroy(trusted_table);
				return -1;
			}
			if(!lock_init(trusted_table->row_locks[i])) {
				LM_ERR("failed to initialize a row lock[%d] for this trusted table\n", i);
				trusted_table_destroy(trusted_table);
				return -1;
			}
		}

		/* initialize each trusted_list entry (dummy head can already be in place, e.g. re-init) */
		if (!trusted_table->row_entry_list[i]) {
			trusted_table->row_entry_list[i] = shm_malloc(sizeof(struct trusted_list));
			if(!trusted_table->row_entry_list[i]) {
				LM_ERR("no shared memory left to allocate trusted list entry[%d] for this trusted table\n", i);
				trusted_table_destroy(trusted_table);
				return -1;
			}
			memset(trusted_table->row_entry_list[i], 0, sizeof(struct trusted_list));

			/* track real table size */
			trusted_table->size++;
		}

		/* make sure to keep bucket's head initialized with empty values */
		trusted_table->row_entry_list[i]->pattern = NULL;
		trusted_table->row_entry_list[i]->ruri_pattern = NULL;
		trusted_table->row_entry_list[i]->next = NULL;
		trusted_table->row_entry_list[i]->priority = 0;
	}

	LM_DBG("successfully initialized trusted table\n");
	return 0;
}


/**
 * Allocate space (shm) for the table.
 */
struct trusted_hash_table * trusted_table_allocate(unsigned int hash_table_size)
{
	/* init the main hash table structure */
	struct trusted_hash_table * trusted_table = shm_malloc(sizeof(struct trusted_hash_table));
	if(!trusted_table) {
		LM_ERR("no shared memory left to create this trusted table\n");
		return NULL;
	}
	memset(trusted_table, 0, sizeof(struct trusted_hash_table));

	/* init the row locks for it */
	trusted_table->row_locks = shm_malloc(hash_table_size * sizeof(gen_lock_t *));
	if(!trusted_table->row_locks) {
		LM_ERR("no shared memory left to create row locks for this trusted table\n");
		trusted_table_destroy(trusted_table);
		return NULL;
	}
	memset(trusted_table->row_locks, 0, hash_table_size * sizeof(gen_lock_t *));

	/* allocate space for the list */
	trusted_table->row_entry_list = shm_malloc(hash_table_size * sizeof(struct trusted_list *));
	if(!trusted_table->row_entry_list) {
		LM_ERR("no shared memory left to create entry list for this trusted table\n");
		trusted_table_destroy(trusted_table);
		return NULL;
	}
	memset(trusted_table->row_entry_list, 0, hash_table_size * sizeof(struct trusted_list *));

	LM_DBG("successfully allocated new trusted table\n");
	return trusted_table;
}


/*
 * Create and initialize a hash table
 */
struct trusted_list **new_hash_table(void)
{
	struct trusted_list **ptr;

	/* Initializing hash tables and hash table variable */
	ptr = (struct trusted_list **)shm_malloc(
			sizeof(struct trusted_list *) * PERM_HASH_SIZE);
	if(!ptr) {
		LM_ERR("no shm memory for hash table\n");
		return 0;
	}

	memset(ptr, 0, sizeof(struct trusted_list *) * PERM_HASH_SIZE);
	return ptr;
}


/*
 * Release all memory allocated for a hash table
 */
void free_hash_table(struct trusted_list **table)
{
	if(!table)
		return;

	empty_hash_table(table);
	shm_free(table);
}



/*
 * Add <src_ip, proto, pattern, ruri_pattern, tag, priority> into hash table, where proto is integer
 * representation of string argument proto.
 */
int hash_table_insert(struct trusted_hash_table *hash_table, char *src_ip, char *proto,
		char *pattern, char *ruri_pattern, char *tag, int priority)
{
	struct trusted_list *cur_entry, *prev_entry;
	struct trusted_list *new_entry = NULL;

	unsigned int hash_index;

	new_entry = (struct trusted_list *)shm_malloc(sizeof(*new_entry));
	if(new_entry == NULL) {
		LM_ERR("cannot allocate shm memory for new table entry\n");
		return -1;
	}

	if(strcasecmp(proto, "any") == 0) {
		new_entry->proto = PROTO_NONE;
	} else if(strcasecmp(proto, "udp") == 0) {
		new_entry->proto = PROTO_UDP;
	} else if(strcasecmp(proto, "tcp") == 0) {
		new_entry->proto = PROTO_TCP;
	} else if(strcasecmp(proto, "tls") == 0) {
		new_entry->proto = PROTO_TLS;
	} else if(strcasecmp(proto, "sctp") == 0) {
		new_entry->proto = PROTO_SCTP;
	} else if(strcasecmp(proto, "ws") == 0) {
		new_entry->proto = PROTO_WS;
	} else if(strcasecmp(proto, "wss") == 0) {
		new_entry->proto = PROTO_WSS;
	} else if(strcasecmp(proto, "none") == 0) {
		shm_free(new_entry);
		return 1;
	} else {
		LM_CRIT("unknown protocol\n");
		shm_free(new_entry);
		return -1;
	}

	/* source IP */
	new_entry->src_ip.len = strlen(src_ip);
	new_entry->src_ip.s = (char *)shm_malloc(new_entry->src_ip.len + 1);
	if(new_entry->src_ip.s == NULL) {
		LM_CRIT("cannot allocate shm memory for src_ip string\n");
		shm_free(new_entry);
		return -1;
	}
	(void)strncpy(new_entry->src_ip.s, src_ip, new_entry->src_ip.len);
	new_entry->src_ip.s[new_entry->src_ip.len] = 0;

	/* pattern */
	if(pattern) {
		new_entry->pattern = (char *)shm_malloc(strlen(pattern) + 1);
		if(new_entry->pattern == NULL) {
			LM_CRIT("cannot allocate shm memory for pattern string\n");
			shm_free(new_entry->src_ip.s);
			shm_free(new_entry);
			return -1;
		}
		(void)strcpy(new_entry->pattern, pattern);
	} else {
		new_entry->pattern = 0;
	}

	/* R-URI pattern */
	if(ruri_pattern) {
		new_entry->ruri_pattern = (char *)shm_malloc(strlen(ruri_pattern) + 1);
		if(new_entry->ruri_pattern == NULL) {
			LM_CRIT("cannot allocate shm memory for ruri_pattern string\n");
			shm_free(new_entry->src_ip.s);
			shm_free(new_entry);
			return -1;
		}
		(void)strcpy(new_entry->ruri_pattern, ruri_pattern);
	} else {
		new_entry->ruri_pattern = 0;
	}

	/* tag */
	if(tag) {
		new_entry->tag.len = strlen(tag);
		new_entry->tag.s = (char *)shm_malloc((new_entry->tag.len) + 1);
		if(new_entry->tag.s == NULL) {
			LM_CRIT("cannot allocate shm memory for pattern or ruri_pattern "
					"string\n");
			shm_free(new_entry->src_ip.s);
			shm_free(new_entry->pattern);
			shm_free(new_entry->ruri_pattern);
			shm_free(new_entry);
			return -1;
		}
		(void)strcpy(new_entry->tag.s, tag);
	} else {
		new_entry->tag.len = 0;
		new_entry->tag.s = 0;
	}

	/* priority */
	new_entry->priority = priority;

	/* determinde the indexation */
	hash_index = perm_hash(new_entry->src_ip);

	/* ensure to have a correct index within boundaries */
	if (hash_index >= hash_table->size) {
		trusted_table_free_entry(new_entry);
		LM_ERR("hash index out of bounds [%d]\n", hash_index);
		return -1;
	}

	/* now new entry is ready for insertion */

	/* lock while working */
	if(hash_table->row_locks[hash_index]) {
		lock_get(hash_table->row_locks[hash_index]);
	} else {
		trusted_table_free_entry(new_entry);
		LM_ERR("Cannot acquire the bucket lock, hash table slot[%d]\n", hash_index);
		return -1;
	}

	/* check whether the bucket was pre-allocated per index */
	if(!hash_table->row_entry_list[hash_index]) {
		trusted_table_free_entry(new_entry);
		LM_ERR("Non-initialized bucket, hash table slot[%d]\n", hash_index);
		lock_release(hash_table->row_locks[hash_index]);
		return -1;
	}

	new_entry->next = NULL; // for the meanwhile
	cur_entry = hash_table->row_entry_list[hash_index];

	while(cur_entry)
	{
		/* check always if already added before */
		if(STR_EQ(cur_entry->src_ip, new_entry->src_ip) && /* compare source ip */
			/* compare From patern (both equal or both null) */
			((!cur_entry->pattern && !new_entry->pattern) ||
			(cur_entry->pattern && new_entry->pattern &&
			strcmp(cur_entry->pattern, new_entry->pattern) == 0)) &&
			/* compare ruri patern (both equal or both null) */
			((!cur_entry->ruri_pattern && !new_entry->ruri_pattern) ||
			(cur_entry->ruri_pattern && new_entry->ruri_pattern &&
			strcmp(cur_entry->ruri_pattern, new_entry->ruri_pattern) == 0)) &&
			/* compare protocol */
			cur_entry->proto == new_entry->proto)
		{
			lock_release(hash_table->row_locks[hash_index]);
			trusted_table_free_entry(new_entry);
			LM_NOTICE("source IP = '%.*s', was already added before, ingore new entry.\n",
					cur_entry->src_ip.len, cur_entry->src_ip.s);
			return 0;
			/* TODO: we should actually return -1, but the caller is quite strict! */
		}

		/* stop by the first entry with a lower priority */
		if (cur_entry->priority < new_entry->priority)
			break;

		/* find the next available slot in the list */
		prev_entry = cur_entry;
		cur_entry = cur_entry->next;
	}

	/* insert new_entry in the right position */
	if (prev_entry == NULL) {
		/* head */
		new_entry->next = hash_table->row_entry_list[hash_index];
		hash_table->row_entry_list[hash_index] = new_entry;
	} else {
		/* after the last higher priority and before the lower priority */
		new_entry->next = prev_entry->next;
		prev_entry->next = new_entry;
	}

	lock_release(hash_table->row_locks[hash_index]);

	return 0;
}


/*
 * Check if an entry exists in hash table that has given src_ip and protocol
 * value and pattern that matches to From URI.  If an entry exists and tag_avp
 * has been defined, tag of the entry is added as a value to tag_avp.
 * Returns number of matches or -1 if none matched.
 */
int match_hash_table(struct trusted_hash_table *trusted_table, struct sip_msg *msg,
		char *src_ip_c_str, int proto, char *from_uri)
{
	LM_DBG("match_hash_table src_ip: %s, proto: %d, uri: %s\n", src_ip_c_str,
			proto, from_uri);
	str ruri;
	char ruri_string[MAX_URI_SIZE + 1];
	regex_t preg;
	struct trusted_list *np;
	str src_ip;
	int_str val;
	int count = 0;
	unsigned int hash_index;
	struct trusted_list **table = NULL;

	if (!trusted_table) {
		LM_ERR("empty table used for comparison.\n");
		return -1;
	}

	/* detect source IP */
	if (!src_ip_c_str) {
		LM_ERR("empty source IP given\n");
		return -1;
	}
	src_ip.s = src_ip_c_str;
	src_ip.len = strlen(src_ip.s);

	if(IS_SIP(msg)) {
		ruri = msg->first_line.u.request.uri;
		if(ruri.len > MAX_URI_SIZE) {
			LM_ERR("message has Request URI too large\n");
			return -1;
		}
		memcpy(ruri_string, ruri.s, ruri.len);
		ruri_string[ruri.len] = (char)0;
	}

	/* determinde the indexation */
	hash_index = perm_hash(src_ip);

	if(trusted_table->row_locks[hash_index]) {
		lock_get(trusted_table->row_locks[hash_index]);
	} else {
		LM_ERR("Cannot acquire the bucket lock, hash table slot[%d]\n", hash_index);
		return -1;
	}

	if(!trusted_table->row_entry_list[hash_index])
	{
		LM_ERR("Non-initialized bucket, hash table slot[%d]\n", hash_index);
		lock_release(trusted_table->row_locks[hash_index]);
		return -1;
	}
	table = trusted_table->row_entry_list;

	for(np = table[hash_index]; np != NULL; np = np->next) {
		if((np->src_ip.len == src_ip.len)
				&& (strncmp(np->src_ip.s, src_ip.s, src_ip.len) == 0)
				&& ((np->proto == PROTO_NONE) || (proto == PROTO_NONE)
						|| (np->proto == proto))) {

			LM_DBG("match_hash_table: %d, %s, %s, %s\n", np->proto,
					(np->pattern ? np->pattern : "null"),
					(np->ruri_pattern ? np->ruri_pattern : "null"),
					(np->tag.s ? np->tag.s : "null"));

			if(IS_SIP(msg)) {
				if(np->pattern) {
					if(regcomp(&preg, np->pattern, REG_NOSUB)) {
						LM_ERR("invalid regular expression\n");
						continue;
					}
					if(regexec(&preg, from_uri, 0, (regmatch_t *)0, 0)) {
						regfree(&preg);
						continue;
					}
					regfree(&preg);
				}
				if(np->ruri_pattern) {
					if(regcomp(&preg, np->ruri_pattern, REG_NOSUB)) {
						LM_ERR("invalid regular expression\n");
						continue;
					}
					if(regexec(&preg, ruri_string, 0, (regmatch_t *)0, 0)) {
						regfree(&preg);
						continue;
					}
					regfree(&preg);
				}
			}
			/* Found a match */
			if(tag_avp.n && np->tag.s) {
				val.s = np->tag;
				if(add_avp(tag_avp_type | AVP_VAL_STR, tag_avp, val) != 0) {
					LM_ERR("setting of tag_avp failed\n");
					lock_release(trusted_table->row_locks[hash_index]);
					return -1;
				}
			}
			if(!perm_peer_tag_mode) {
				lock_release(trusted_table->row_locks[hash_index]);
				return 1;
			}
			count++;
		}
	}

	lock_release(trusted_table->row_locks[hash_index]);
	if(!count)
		return -1;
	else
		return count;
}


/*! \brief
 * RPC interface :: Print trusted entries stored in hash table
 */
int hash_table_rpc_print(struct trusted_hash_table *trusted_table, rpc_t *rpc, void *c)
{
	int i;
	struct trusted_list *np = NULL;
	void *th;
	void *ih;

	if (!trusted_table) {
		LM_ERR("empty table used for comparison.\n");
		return -1;
	}

	if(rpc->add(c, "{", &th) < 0) {
		rpc->fault(c, 500, "Internal error creating rpc");
		return -1;
	}

	for(i = 0; i < PERM_HASH_SIZE; i++) {

		if (trusted_table->row_locks[i]) {
			lock_get(trusted_table->row_locks[i]);
		} else {
			LM_ERR("Cannot acquire the bucket lock, hash table slot[%d]\n", i);
			continue;
		}

		if (!trusted_table->row_entry_list[i])
		{
			LM_WARN("Non-initialized bucket, hash table slot[%d]\n", i);
			lock_release(trusted_table->row_locks[i]);
			continue;
		}

		np = trusted_table->row_entry_list[i];

		while(np) {
			if(rpc->struct_add(th, "d{", "table", i, "item", &ih) < 0) {
				rpc->fault(c, 500, "Internal error creating rpc ih");
				lock_release(trusted_table->row_locks[i]);
				return -1;
			}

			if(rpc->struct_add(ih, "s", "ip", np->src_ip.s) < 0) {
				rpc->fault(c, 500, "Internal error creating rpc data (ip)");
				lock_release(trusted_table->row_locks[i]);
				return -1;
			}
			if(rpc->struct_add(ih, "dsssd", "proto", np->proto, "pattern",
					   np->pattern ? np->pattern : "NULL", "ruri_pattern",
					   np->ruri_pattern ? np->ruri_pattern : "NULL", "tag",
					   np->tag.len ? np->tag.s : "NULL", "priority",
					   np->priority)
					< 0)
			{
				rpc->fault(c, 500, "Internal error creating rpc data");
				lock_release(trusted_table->row_locks[i]);
				return -1;
			}
			np = np->next;
		}
		lock_release(trusted_table->row_locks[i]);
	}
	return 0;
}

/*
 * Free contents of hash table, it doesn't destroy the
 * hash table itself
 */
void empty_hash_table(struct trusted_list **table)
{
	int i;
	struct trusted_list *np, *next;

	for(i = 0; i < PERM_HASH_SIZE; i++) {
		np = table[i];
		while(np) {
			if(np->src_ip.s)
				shm_free(np->src_ip.s);
			if(np->pattern)
				shm_free(np->pattern);
			if(np->ruri_pattern)
				shm_free(np->ruri_pattern);
			if(np->tag.s)
				shm_free(np->tag.s);
			next = np->next;
			shm_free(np);
			np = next;
		}
		table[i] = 0;
	}
}


/*
 * Create and initialize an address hash table
 */
struct addr_list **new_addr_hash_table(void)
{
	struct addr_list **ptr;

	/* Initializing hash tables and hash table variable */
	ptr = (struct addr_list **)shm_malloc(
			sizeof(struct addr_list *) * PERM_HASH_SIZE);
	if(!ptr) {
		LM_ERR("no shm memory for hash table\n");
		return 0;
	}

	memset(ptr, 0, sizeof(struct addr_list *) * PERM_HASH_SIZE);
	return ptr;
}


/*
 * Release all memory allocated for a hash table
 */
void free_addr_hash_table(struct addr_list **table)
{
	if(!table)
		return;

	empty_addr_hash_table(table);
	shm_free(table);
}


/*
 * Add <grp, ip_addr, port> into hash table
 */
int addr_hash_table_insert(struct addr_list **table, unsigned int grp,
		ip_addr_t *addr, unsigned int port, str *tagv)
{
	struct addr_list *np;
	unsigned int hash_val;
	str addr_str;
	int len;

	len = sizeof(struct addr_list);
	if(tagv != NULL && tagv->s != NULL) {
		len += tagv->len + 1;
	}

	np = (struct addr_list *)shm_malloc(len);
	if(np == NULL) {
		LM_ERR("no shm memory for table entry\n");
		return -1;
	}

	memset(np, 0, len);

	np->grp = grp;
	memcpy(&np->addr, addr, sizeof(ip_addr_t));
	np->port = port;
	if(tagv != NULL && tagv->s != NULL) {
		np->tag.s = (char *)np + sizeof(struct addr_list);
		np->tag.len = tagv->len;
		memcpy(np->tag.s, tagv->s, tagv->len);
		np->tag.s[np->tag.len] = '\0';
	}

	addr_str.s = (char *)addr->u.addr;
	addr_str.len = 4;
	hash_val = perm_hash(addr_str);
	np->next = table[hash_val];
	table[hash_val] = np;

	return 1;
}


/*
 * Check if an entry exists in hash table that has given group, ip_addr, and
 * port.  Port 0 in hash table matches any port.
 */
int match_addr_hash_table(struct addr_list **table, unsigned int group,
		ip_addr_t *addr, unsigned int port)
{
	struct addr_list *np;
	str addr_str;
	avp_value_t val;

	addr_str.s = (char *)addr->u.addr;
	addr_str.len = 4;

	for(np = table[perm_hash(addr_str)]; np != NULL; np = np->next) {
		if((np->grp == group) && ((np->port == 0) || (np->port == port))
				&& ip_addr_cmp(&np->addr, addr)) {

			if(tag_avp.n && np->tag.s) {
				val.s = np->tag;
				if(add_avp(tag_avp_type | AVP_VAL_STR, tag_avp, val) != 0) {
					LM_ERR("setting of tag_avp failed\n");
					return -1;
				}
			}

			return 1;
		}
	}

	return -1;
}


/*
 * Check if an ip_addr/port entry exists in hash table in any group.
 * Returns first group in which ip_addr/port is found.
 * Port 0 in hash table matches any port.
 */
int find_group_in_addr_hash_table(
		struct addr_list **table, ip_addr_t *addr, unsigned int port)
{
	struct addr_list *np;
	str addr_str;
	avp_value_t val;

	addr_str.s = (char *)addr->u.addr;
	addr_str.len = 4;

	for(np = table[perm_hash(addr_str)]; np != NULL; np = np->next) {
		if(((np->port == 0) || (np->port == port))
				&& ip_addr_cmp(&np->addr, addr)) {

			if(tag_avp.n && np->tag.s) {
				val.s = np->tag;
				if(add_avp(tag_avp_type | AVP_VAL_STR, tag_avp, val) != 0) {
					LM_ERR("setting of tag_avp failed\n");
					return -1;
				}
			}

			return np->grp;
		}
	}

	return -1;
}


/*! \brief
 * RPC: Print addresses stored in hash table
 */
int addr_hash_table_rpc_print(struct addr_list **table, rpc_t *rpc, void *c)
{
	int i;
	void *th;
	void *ih;
	struct addr_list *np;


	for(i = 0; i < PERM_HASH_SIZE; i++) {
		np = table[i];
		while(np) {
			if(rpc->add(c, "{", &th) < 0) {
				rpc->fault(c, 500, "Internal error creating rpc");
				return -1;
			}

			if(rpc->struct_add(
					   th, "dd{", "table", i, "group", np->grp, "item", &ih)
					< 0) {
				rpc->fault(c, 500, "Internal error creating rpc ih");
				return -1;
			}

			if(rpc->struct_add(ih, "s", "ip", ip_addr2a(&np->addr)) < 0) {
				rpc->fault(c, 500, "Internal error creating rpc data (ip)");
				return -1;
			}
			if(rpc->struct_add(ih, "ds", "port", np->port, "tag",
					   np->tag.len ? np->tag.s : "NULL")
					< 0) {
				rpc->fault(c, 500, "Internal error creating rpc data");
				return -1;
			}
			np = np->next;
		}
	}
	return 0;
}


/*
 * Free contents of hash table, it doesn't destroy the
 * hash table itself
 */
void empty_addr_hash_table(struct addr_list **table)
{
	int i;
	struct addr_list *np, *next;

	for(i = 0; i < PERM_HASH_SIZE; i++) {
		np = table[i];
		while(np) {
			next = np->next;
			shm_free(np);
			np = next;
		}
		table[i] = 0;
	}
}


/*
 * Create and initialize a subnet table
 */
struct subnet *new_subnet_table(void)
{
	struct subnet *ptr;

	/* subnet record [PERM_MAX_SUBNETS] contains in its grp field
	 * the number of subnet records in the subnet table */
	ptr = (struct subnet *)shm_malloc(
			sizeof(struct subnet) * (PERM_MAX_SUBNETS + 1));
	if(!ptr) {
		LM_ERR("no shm memory for subnet table\n");
		return 0;
	}
	memset(ptr, 0, sizeof(struct subnet) * (PERM_MAX_SUBNETS + 1));
	return ptr;
}


/*
 * Add <grp, subnet, mask, port, tag> into subnet table so that table is
 * kept in increasing ordered according to grp.
 */
int subnet_table_insert(struct subnet *table, unsigned int grp,
		ip_addr_t *subnet, unsigned int mask, unsigned int port, str *tagv)
{
	int i;
	unsigned int count;
	str tags;

	count = table[PERM_MAX_SUBNETS].grp;

	if(count == PERM_MAX_SUBNETS) {
		LM_CRIT("subnet table is full\n");
		return 0;
	}

	if(tagv == NULL || tagv->s == NULL) {
		tags.s = NULL;
		tags.len = 0;
	} else {
		tags.len = tagv->len;
		tags.s = (char *)shm_malloc(tags.len + 1);
		if(tags.s == NULL) {
			LM_ERR("No more shared memory\n");
			return 0;
		}
		memcpy(tags.s, tagv->s, tags.len);
		tags.s[tags.len] = '\0';
	}

	i = count - 1;

	while((i >= 0) && (table[i].grp > grp)) {
		table[i + 1] = table[i];
		i--;
	}

	table[i + 1].grp = grp;
	memcpy(&table[i + 1].subnet, subnet, sizeof(ip_addr_t));
	table[i + 1].port = port;
	table[i + 1].mask = mask;
	table[i + 1].tag = tags;

	table[PERM_MAX_SUBNETS].grp = count + 1;

	return 1;
}


/*
 * Check if an entry exists in subnet table that matches given group, ip_addr,
 * and port.  Port 0 in subnet table matches any port.
 */
int match_subnet_table(struct subnet *table, unsigned int grp, ip_addr_t *addr,
		unsigned int port)
{
	unsigned int count, i;
	avp_value_t val;
	int best_idx = -1;
	unsigned int best_mask = 0;

	count = table[PERM_MAX_SUBNETS].grp;

	i = 0;
	while((i < count) && (table[i].grp < grp))
		i++;

	if(i == count)
		return -1;

	while((i < count) && (table[i].grp == grp)) {
		if(((table[i].port == port) || (table[i].port == 0))
				&& (ip_addr_match_net(addr, &table[i].subnet, table[i].mask)
						== 0)) {
			if(table[i].mask > best_mask) {
				best_mask = table[i].mask;
				best_idx = i;
			}
			if(_perm_subnet_match_mode == 0) {
				/* use the first match */
				break;
			}
		}
		i++;
	}

	if(best_idx >= 0) {
		if(tag_avp.n && table[best_idx].tag.s) {
			val.s = table[best_idx].tag;
			if(add_avp(tag_avp_type | AVP_VAL_STR, tag_avp, val) != 0) {
				LM_ERR("setting of tag_avp failed\n");
				return -1;
			}
		}
		return 1;
	}

	return -1;
}


/*
 * Check if an entry exists in subnet table that matches given ip_addr,
 * and port.  Port 0 in subnet table matches any port.  Return group of
 * first match or -1 if no match is found.
 */
int find_group_in_subnet_table(
		struct subnet *table, ip_addr_t *addr, unsigned int port)
{
	unsigned int count, i;
	avp_value_t val;
	int best_idx = -1;
	unsigned int best_mask = 0;

	count = table[PERM_MAX_SUBNETS].grp;

	i = 0;
	while(i < count) {
		if(((table[i].port == port) || (table[i].port == 0))
				&& (ip_addr_match_net(addr, &table[i].subnet, table[i].mask)
						== 0)) {
			if(table[i].mask > best_mask) {
				best_mask = table[i].mask;
				best_idx = i;
			}
			if(_perm_subnet_match_mode == 0) {
				/* use the first match */
				break;
			}
		}
		i++;
	}

	if(best_idx >= 0) {
		if(tag_avp.n && table[best_idx].tag.s) {
			val.s = table[best_idx].tag;
			if(add_avp(tag_avp_type | AVP_VAL_STR, tag_avp, val) != 0) {
				LM_ERR("setting of tag_avp failed\n");
				return -1;
			}
		}
		return table[best_idx].grp;
	}

	return -1;
}


/*! \brief
 * RPC interface :: Print subnet entries stored in hash table
 */
int subnet_table_rpc_print(struct subnet *table, rpc_t *rpc, void *c)
{
	int i;
	int count;
	void *th;
	void *ih;

	count = table[PERM_MAX_SUBNETS].grp;

	for(i = 0; i < count; i++) {
		if(rpc->add(c, "{", &th) < 0) {
			rpc->fault(c, 500, "Internal error creating rpc");
			return -1;
		}

		if(rpc->struct_add(
				   th, "dd{", "id", i, "group", table[i].grp, "item", &ih)
				< 0) {
			rpc->fault(c, 500, "Internal error creating rpc ih");
			return -1;
		}

		if(rpc->struct_add(ih, "s", "ip", ip_addr2a(&table[i].subnet)) < 0) {
			rpc->fault(c, 500, "Internal error creating rpc data (subnet)");
			return -1;
		}
		if(rpc->struct_add(ih, "dds", "mask", table[i].mask, "port",
				   table[i].port, "tag",
				   (table[i].tag.s == NULL) ? "" : table[i].tag.s)
				< 0) {
			rpc->fault(c, 500, "Internal error creating rpc data");
			return -1;
		}
	}
	return 0;
}


/*
 * Empty contents of subnet table
 */
void empty_subnet_table(struct subnet *table)
{
	int i;
	table[PERM_MAX_SUBNETS].grp = 0;
	for(i = 0; i < PERM_MAX_SUBNETS; i++) {
		if(table[i].tag.s != NULL) {
			shm_free(table[i].tag.s);
			table[i].tag.s = NULL;
			table[i].tag.len = 0;
		}
	}
}


/*
 * Release memory allocated for a subnet table
 */
void free_subnet_table(struct subnet *table)
{
	int i;
	if(!table)
		return;
	for(i = 0; i < PERM_MAX_SUBNETS; i++) {
		if(table[i].tag.s != NULL) {
			shm_free(table[i].tag.s);
			table[i].tag.s = NULL;
			table[i].tag.len = 0;
		}
	}

	shm_free(table);
}

/*
 * Create and initialize a domain_name table
 */
struct domain_name_list **new_domain_name_table(void)
{
	struct domain_name_list **ptr;

	/* Initializing hash tables and hash table variable */
	ptr = (struct domain_name_list **)shm_malloc(
			sizeof(struct domain_name_list *) * PERM_HASH_SIZE);
	if(!ptr) {
		LM_ERR("no shm memory for hash table\n");
		return 0;
	}

	memset(ptr, 0, sizeof(struct domain_name *) * PERM_HASH_SIZE);
	return ptr;
}


/*
 * Free contents of hash table, it doesn't destroy the
 * hash table itself
 */
void empty_domain_name_table(struct domain_name_list **table)
{
	int i;
	struct domain_name_list *np, *next;

	for(i = 0; i < PERM_HASH_SIZE; i++) {
		np = table[i];
		while(np) {
			next = np->next;
			shm_free(np);
			np = next;
		}
		table[i] = 0;
	}
}

/*
 * Release all memory allocated for a hash table
 */
void free_domain_name_table(struct domain_name_list **table)
{
	if(!table)
		return;

	empty_domain_name_table(table);
	shm_free(table);
}


/*
 * Check if an entry exists in hash table that has given group, domain_name, and
 * port.  Port 0 in hash table matches any port.
 */
int match_domain_name_table(struct domain_name_list **table, unsigned int group,
		str *domain_name, unsigned int port)
{
	struct domain_name_list *np;
	avp_value_t val;

	for(np = table[perm_hash(*domain_name)]; np != NULL; np = np->next) {
		if((np->grp == group) && ((np->port == 0) || (np->port == port))
				&& np->domain.len == domain_name->len
				&& strncmp(np->domain.s, domain_name->s, domain_name->len)
						   == 0) {

			if(tag_avp.n && np->tag.s) {
				val.s = np->tag;
				if(add_avp(tag_avp_type | AVP_VAL_STR, tag_avp, val) != 0) {
					LM_ERR("setting of tag_avp failed\n");
					return -1;
				}
			}

			return 1;
		}
	}

	return -1;
}


/*
 * Check if a domain_name/port entry exists in hash table in any group.
 * Returns first group in which ip_addr/port is found.
 * Port 0 in hash table matches any port.
 */
int find_group_in_domain_name_table(
		struct domain_name_list **table, str *domain_name, unsigned int port)
{
	struct domain_name_list *np;

	for(np = table[perm_hash(*domain_name)]; np != NULL; np = np->next) {
		if(((np->port == 0) || (np->port == port))
				&& np->domain.len == domain_name->len
				&& strncmp(np->domain.s, domain_name->s, domain_name->len)
						   == 0) {
			return np->grp;
		}
	}

	return -1;
}


/*
 * Add <grp, domain_name, port> into hash table
 */
int domain_name_table_insert(struct domain_name_list **table, unsigned int grp,
		str *domain_name, unsigned int port, str *tagv)
{
	struct domain_name_list *np;
	unsigned int hash_val;
	int len;

	len = sizeof(struct domain_name_list) + domain_name->len;
	if(tagv != NULL && tagv->s != NULL) {
		len += tagv->len + 1;
	}

	np = (struct domain_name_list *)shm_malloc(len);
	if(np == NULL) {
		LM_ERR("no shm memory for table entry\n");
		return -1;
	}

	memset(np, 0, len);

	np->grp = grp;
	np->domain.s = (char *)np + sizeof(struct domain_name_list);
	memcpy(np->domain.s, domain_name->s, domain_name->len);
	np->domain.len = domain_name->len;
	np->port = port;
	if(tagv != NULL && tagv->s != NULL) {
		np->tag.s =
				(char *)np + sizeof(struct domain_name_list) + domain_name->len;
		np->tag.len = tagv->len;
		memcpy(np->tag.s, tagv->s, np->tag.len);
		np->tag.s[np->tag.len] = '\0';
	}

	LM_DBG("** Added domain name: %.*s\n", np->domain.len, np->domain.s);

	hash_val = perm_hash(*domain_name);
	np->next = table[hash_val];
	table[hash_val] = np;

	return 1;
}


/*! \brief
 * RPC: Print addresses stored in hash table
 */
int domain_name_table_rpc_print(
		struct domain_name_list **table, rpc_t *rpc, void *c)
{
	int i;
	void *th;
	void *ih;
	struct domain_name_list *np;


	if(rpc->add(c, "{", &th) < 0) {
		rpc->fault(c, 500, "Internal error creating rpc");
		return -1;
	}

	for(i = 0; i < PERM_HASH_SIZE; i++) {
		np = table[i];
		while(np) {
			if(rpc->struct_add(
					   th, "dd{", "table", i, "group", np->grp, "item", &ih)
					< 0) {
				rpc->fault(c, 500, "Internal error creating rpc ih");
				return -1;
			}

			if(rpc->struct_add(ih, "S", "domain_name", &np->domain) < 0) {
				rpc->fault(c, 500, "Internal error creating rpc data (ip)");
				return -1;
			}
			if(rpc->struct_add(ih, "ds", "port", np->port, "tag",
					   np->tag.len ? np->tag.s : "NULL")
					< 0) {
				rpc->fault(c, 500, "Internal error creating rpc data");
				return -1;
			}
			np = np->next;
		}
	}
	return 0;
}
