/*
 * Header file for trusted and address hash table functions
 *
 * Copyright (C) 2003-2006 Juha Heinanen
 *
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

#ifndef _PERM_HASH_H_
#define _PERM_HASH_H_

#include <stdio.h>
#include "../../core/str.h"
#include "../../core/rpc.h"
#include "../../core/ip_addr.h"

#define PERM_HASH_SIZE 128


/*
 * Structure stored in address hash table
 */
struct addr_list {
	unsigned int trust;
	ip_addr_t addr;
	unsigned int port;
	str tag;
	struct addr_list *next;  /* Next element in the list */
};


/*
 * Create and initialize a hash table
 */
struct addr_list** new_addr_hash_table(void);


/*
 * Release all memory allocated for a hash table
 */
void free_addr_hash_table(struct addr_list** table);


/*
 * Destroy a hash table
 */
void destroy_addr_hash_table(struct addr_list** table);


/*
 * Add <trust, ip_addr, port> into hash table
 */
int addr_hash_table_insert(struct addr_list** hash_table, unsigned int trust,
		ip_addr_t *addr, unsigned int port, char *tagv);


/*
 * Check if an entry exists in hash table that has given trust, ip_addr, and
 * port.  Port 0 in hash table matches any port.
 */
int match_addr_hash_table(struct addr_list** table, unsigned int trust,
		ip_addr_t *addr, unsigned int port);


/*
 * Checks if an ip_addr/port entry exists in address hash table in any trust.
 * Port 0 in hash table matches any port.   Returns trust of the first match
 * or -1 if no match is found.
 */
int find_trust_in_addr_hash_table(struct addr_list** table,
		ip_addr_t *addr, unsigned int port);


/*
 * Print addresses stored in hash table
 */
void addr_hash_table_print(struct addr_list** hash_table, FILE* reply_file);
int addr_hash_table_rpc_print(struct addr_list** table, rpc_t* rpc, void* c);


/*
 * Empty hash table
 */
void empty_addr_hash_table(struct addr_list** hash_table);


/*
 * Structure used to store a subnet
 */
struct subnet {
	unsigned int trust;        /* address trust, subnet count in last record */
	ip_addr_t  subnet;       /* IP subnet in host byte order with host bits shifted out */
	unsigned int port;       /* port or 0 */
	unsigned int mask;       /* how many bits belong to network part */
	str tag;
};


/*
 * Create a subnet table
 */
struct subnet* new_subnet_table(void);


/*
 * Check if an entry exists in subnet table that matches given trust, ip_addr,
 * and port.  Port 0 in subnet table matches any port.
 */
int match_subnet_table(struct subnet* table, unsigned int trust,
		ip_addr_t *addr, unsigned int port);


/*
 * Checks if an entry exists in subnet table that matches given ip_addr,
 * and port.  Port 0 in subnet table matches any port.  Returns trust of
 * the first match or -1 if no match is found.
 */
int find_trust_in_subnet_table(struct subnet* table,
		ip_addr_t *addr, unsigned int port);

/*
 * Empty contents of subnet table
 */
void empty_subnet_table(struct subnet *table);


/*
 * Release memory allocated for a subnet table
 */
void free_subnet_table(struct subnet* table);


/*
 * Add <grp, subnet, mask, port> into subnet table so that table is
 * kept ordered according to subnet, port, trust.
 */
int subnet_table_insert(struct subnet* table, unsigned int trust,
			ip_addr_t *subnet, unsigned int mask,
			unsigned int port, char *tagv);


/*
 * Print subnets stored in subnet table
 */
void subnet_table_print(struct subnet* table, FILE* reply_file);
int subnet_table_rpc_print(struct subnet* table, rpc_t* rpc, void* c);


#endif /* _PERM_HASH_H_ */
