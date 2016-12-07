/*
 * $Id$
 *
 * Header file for allow_trusted hash table functions
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

#ifndef _TRUSTED_HASH_H
#define _TRUSTED_HASH_H

#include <stdio.h>
#include "../../parser/msg_parser.h"
#include "../../rpc.h"
#include "../../str.h"

#define HASH_SIZE 128

/*
 * Structure stored in the hash table
 */
struct trusted_list {
	str src_ip;                 /* Source IP of SIP message */
	int proto;                  /* Protocol -- UDP, TCP, TLS, or SCTP */
	char *pattern;              /* Pattern matching From header field */
	struct trusted_list *next;  /* Next element in the list */
};


/*
 * Create and initialize a hash table
 */
struct trusted_list** new_hash_table(void);


/*
 * Release all memory allocated for a hash table
 */
void free_hash_table(struct trusted_list** table);


/*
 * Destroy a hash table
 */
void destroy_hash_table(struct trusted_list** table);


/* 
 * Add <src_ip, proto, pattern> into hash table, where proto is integer
 * representation of string argument proto.
 */
int hash_table_insert(struct trusted_list** hash_table, char* src_ip, char* proto, char* pattern);


/* 
 * Check if an entry exists in hash table that has given src_ip and protocol
 * value and pattern that matches to From URI.
 */
int match_hash_table(struct trusted_list** table, struct sip_msg* msg);


/* 
 * Print domains stored in hash table 
 */
void hash_table_print(struct trusted_list** hash_table, rpc_t* rpc, void* ctx);


/* 
 * Empty hash table
 */
void empty_hash_table(struct trusted_list** hash_table);


#endif /* _TRUSTED_HASH_H */
