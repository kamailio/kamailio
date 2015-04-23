/*
 * $Id$
 *
 * Header file for trusted.c implementing allow_trusted function
 *
 * Copyright (C) 2003-2008 Juha Heinanen
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

#ifndef TRUSTED_H
#define TRUSTED_H
		
#include "../../parser/msg_parser.h"


extern struct trusted_list ***hash_table;     /* Pointer to current hash table pointer */
extern struct trusted_list **hash_table_1;   /* Pointer to hash table 1 */
extern struct trusted_list **hash_table_2;   /* Pointer to hash table 2 */


/*
 * Initialize data structures
 */
int init_trusted(void);


/*
 * Open database connections if necessary
 */
int init_child_trusted(int rank);


/*
 * Open database connections if necessary
 */
int mi_init_trusted(void);


/*
 * Reload trusted table to new hash table and when done, make new hash table
 * current one.
 */
int reload_trusted_table(void);


/*
 * Close connections and release memory
 */
void clean_trusted(void);


/*
 * Check if request comes from trusted ip address with matching from URI
 */
int allow_trusted(struct sip_msg* _msg, char* _s1, char* _s2);


/*
 * Checks based on request's source address, protocol, and From URI
 * if request can be trusted without authentication.
 */
int allow_trusted_0(struct sip_msg* _msg, char* str1, char* str2);


/*
 * Checks based on source address and protocol given in pvar arguments and
 * and requests's From URI, if request can be trusted without authentication.
 */
int allow_trusted_2(struct sip_msg* _msg, char* _src_ip_sp, char* _proto_sp);


int reload_trusted_table_cmd(void);

#endif /* TRUSTED_H */
