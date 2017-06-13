/*
 * Hash functions for cached address tables
 *
 * Copyright (C) 2003-2012 Juha Heinanen
 * Copyright (C) 2017 Alexandr Dubovikov
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

#include <sys/types.h>
#include <regex.h>
//#include "parse_config.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/ut.h"
#include "../../core/hashes.h"
#include "../../core/ip_addr.h"
#include "../../core/pvar.h"
#include "hash.h"
#include "address.h"

#define perm_hash(_s)  core_hash( &(_s), 0, PERM_HASH_SIZE)


extern int peer_tag_mode;


extern int _perm_max_subnets;

#define PERM_MAX_SUBNETS _perm_max_subnets

/*
 * Create and initialize an address hash table
 */
struct addr_list** new_addr_hash_table(void)
{
	struct addr_list** ptr;

	/* Initializing hash tables and hash table variable */
	ptr = (struct addr_list **)shm_malloc
		(sizeof(struct addr_list*) * PERM_HASH_SIZE);
	if (!ptr) {
		LM_ERR("no shm memory for hash table\n");
		return 0;
	}

	memset(ptr, 0, sizeof(struct addr_list*) * PERM_HASH_SIZE);
	return ptr;
}


/*
 * Release all memory allocated for a hash table
 */
void free_addr_hash_table(struct addr_list** table)
{
	if (!table)
		return;

	empty_addr_hash_table(table);
	shm_free(table);
}


/*
 * Add <trust, ip_addr, port> into hash table
 */
int addr_hash_table_insert(struct addr_list** table, unsigned int trust,
		ip_addr_t *addr, unsigned int port, char *tagv)
{
	struct addr_list *np;
	unsigned int hash_val;
	str addr_str;
	int len;

	len = sizeof(struct addr_list);
	if(tagv!=NULL)
		len += strlen(tagv) + 1;

	np = (struct addr_list *) shm_malloc(len);
	if (np == NULL) {
		LM_ERR("no shm memory for table entry\n");
		return -1;
	}

	memset(np, 0, len);

	np->trust = trust;
	memcpy(&np->addr, addr, sizeof(ip_addr_t));
	np->port = port;
	if(tagv!=NULL)
	{
		np->tag.s = (char*)np + sizeof(struct addr_list);
		np->tag.len = strlen(tagv);
		strcpy(np->tag.s, tagv);
	}

	addr_str.s = (char*)addr->u.addr;
	addr_str.len = 4;
	hash_val = perm_hash(addr_str);
	np->next = table[hash_val];
	table[hash_val] = np;

	return 1;
}


/*
 * Check if an entry exists in hash table that has given trust, ip_addr, and
 * port.  Port 0 in hash table matches any port.
 */
int match_addr_hash_table(struct addr_list** table, unsigned int trust,
		ip_addr_t *addr, unsigned int port)
{
	struct addr_list *np;
	str addr_str;

	addr_str.s = (char*)addr->u.addr;
	addr_str.len = 4;

	for (np = table[perm_hash(addr_str)]; np != NULL; np = np->next) {
		if ( (np->trust == trust)
				&& ((np->port == 0) || (np->port == port))
				&& ip_addr_cmp(&np->addr, addr)) {

			return 1;
		}
	}

	return -1;
}


/*
 * Check if an ip_addr/port entry exists in hash table in any trust.
 * Returns first trust in which ip_addr/port is found.
 * Port 0 in hash table matches any port.
 */
int find_trust_in_addr_hash_table(struct addr_list** table,
		ip_addr_t *addr, unsigned int port)
{
	struct addr_list *np;
	str addr_str;

	addr_str.s = (char*)addr->u.addr;
	addr_str.len = 4;

	for (np = table[perm_hash(addr_str)]; np != NULL; np = np->next) {
		if (((np->port == 0) || (np->port == port))
				&& ip_addr_cmp(&np->addr, addr)) {

			return np->trust;
		}
	}

	return -1;
}


/*! \brief
 * RPC: Print addresses stored in hash table
 */
int addr_hash_table_rpc_print(struct addr_list** table, rpc_t* rpc, void* c)
{
	int i;
	void* th;
	void* ih;
	struct addr_list *np;


	for (i = 0; i < PERM_HASH_SIZE; i++) {
		np = table[i];
		while (np) {
			if (rpc->add(c, "{", &th) < 0)
			{
				rpc->fault(c, 500, "Internal error creating rpc");
				return -1;
			}

			if(rpc->struct_add(th, "dd{",
						"table", i,
						"trust", np->trust,
						"item", &ih) < 0)
			{
				rpc->fault(c, 500, "Internal error creating rpc ih");
				return -1;
			}

			if(rpc->struct_add(ih, "s", "ip", ip_addr2a(&np->addr)) < 0)
			{
				rpc->fault(c, 500, "Internal error creating rpc data (ip)");
				return -1;
			}
			if(rpc->struct_add(ih, "ds", "port",  np->port,
						"tag",  np->tag.len ? np->tag.s : "NULL") < 0)
			{
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

	for (i = 0; i < PERM_HASH_SIZE; i++) {
		np = table[i];
		while (np) {
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
struct subnet* new_subnet_table(void)
{
	struct subnet* ptr;

	/* subnet record [PERM_MAX_SUBNETS] contains in its trust field
	 * the number of subnet records in the subnet table */
	ptr = (struct subnet *)shm_malloc
		(sizeof(struct subnet) * (PERM_MAX_SUBNETS + 1));
	if (!ptr) {
		LM_ERR("no shm memory for subnet table\n");
		return 0;
	}
	memset(ptr, 0, sizeof(struct subnet) * (PERM_MAX_SUBNETS + 1));
	return ptr;
}


/*
 * Add <trust, subnet, mask, port, tag> into subnet table so that table is
 * kept in increasing ordered according to trust.
 */
int subnet_table_insert(struct subnet* table, unsigned int trust,
		ip_addr_t *subnet, unsigned int mask,
		unsigned int port, char *tagv)
{
	int i;
	unsigned int count;
	str tags;

	count = table[PERM_MAX_SUBNETS].trust;

	if (count == PERM_MAX_SUBNETS) {
		LM_CRIT("subnet table is full\n");
		return 0;
	}

	if(tagv==NULL)
	{
		tags.s = NULL;
		tags.len = 0;
	} else {
		tags.len = strlen(tagv);
		tags.s = (char*)shm_malloc(tags.len+1);
		if(tags.s==NULL)
		{
			LM_ERR("No more shared memory\n");
			return 0;
		}
		strcpy(tags.s, tagv);
	}

	i = count - 1;

	while ((i >= 0) && (table[i].trust > trust)) {
		table[i + 1] = table[i];
		i--;
	}

	table[i + 1].trust = trust;
	memcpy(&table[i + 1].subnet, subnet, sizeof(ip_addr_t));
	table[i + 1].port = port;
	table[i + 1].mask = mask;
	table[i + 1].tag = tags;

	table[PERM_MAX_SUBNETS].trust = count + 1;

	return 1;
}


/*
 * Check if an entry exists in subnet table that matches given trust, ip_addr,
 * and port.  Port 0 in subnet table matches any port.
 */
int match_subnet_table(struct subnet* table, unsigned int trust,
		ip_addr_t *addr, unsigned int port)
{
	unsigned int count, i;

	count = table[PERM_MAX_SUBNETS].trust;

	i = 0;
	while ((i < count) && (table[i].trust < trust))
		i++;

	if (i == count) return -1;

	while ((i < count) && (table[i].trust == trust)) {
		if (((table[i].port == port) || (table[i].port == 0))
				&& (ip_addr_match_net(addr, &table[i].subnet, table[i].mask)==0))
		{		
			return 1;
		}
		i++;
	}

	return -1;
}


/*
 * Check if an entry exists in subnet table that matches given ip_addr,
 * and port.  Port 0 in subnet table matches any port.  Return trust of
 * first match or -1 if no match is found.
 */
int find_trust_in_subnet_table(struct subnet* table,
		ip_addr_t *addr, unsigned int port)
{
	unsigned int count, i;

	count = table[PERM_MAX_SUBNETS].trust;

	i = 0;
	while (i < count) {
		if ( ((table[i].port == port) || (table[i].port == 0))
				&& (ip_addr_match_net(addr, &table[i].subnet, table[i].mask)==0))
		{
			return table[i].trust;
		}
		i++;
	}

	return -1;
}


/*! \brief
 * RPC interface :: Print subnet entries stored in hash table
 */
int subnet_table_rpc_print(struct subnet* table, rpc_t* rpc, void* c)
{
	int i;
	int count;
	void* th;
	void* ih;

	count = table[PERM_MAX_SUBNETS].trust;

	for (i = 0; i < count; i++) {
		if (rpc->add(c, "{", &th) < 0)
		{
			rpc->fault(c, 500, "Internal error creating rpc");
			return -1;
		}

		if(rpc->struct_add(th, "dd{",
					"id", i,
					"trust", table[i].trust,
					"item", &ih) < 0)
		{
			rpc->fault(c, 500, "Internal error creating rpc ih");
			return -1;
		}

		if(rpc->struct_add(ih, "s", "ip", ip_addr2a(&table[i].subnet)) < 0)
		{
			rpc->fault(c, 500, "Internal error creating rpc data (subnet)");
			return -1;
		}
		if(rpc->struct_add(ih, "dds", "mask", table[i].mask,
					"port", table[i].port,
					"tag",  (table[i].tag.s==NULL)?"":table[i].tag.s) < 0)
		{
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
	table[PERM_MAX_SUBNETS].trust = 0;
	for(i=0; i<PERM_MAX_SUBNETS; i++)
	{
		if(table[i].tag.s!=NULL)
		{
			shm_free(table[i].tag.s);
			table[i].tag.s = NULL;
			table[i].tag.len =0;
		}
	}
}


/*
 * Release memory allocated for a subnet table
 */
void free_subnet_table(struct subnet* table)
{
	int i;
	if (!table)
		return;
	for(i=0; i<PERM_MAX_SUBNETS; i++)
	{
		if(table[i].tag.s!=NULL)
		{
			shm_free(table[i].tag.s);
			table[i].tag.s = NULL;
			table[i].tag.len =0;
		}
	}

	shm_free(table);
}

