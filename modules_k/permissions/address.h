/*
 * Header file for address.c implementing allow_address function
 *
 * Copyright (C) 2006 Juha Heinanen
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef ADDRESS_H
#define ADDRESS_H
		
#include "../../parser/msg_parser.h"


/* Pointer to current address hash table pointer */
extern struct addr_list ***addr_hash_table; 


/* Pointer to current subnet table */
extern struct subnet **subnet_table; 


/*
 * Initialize data structures
 */
int init_addresses(void);


/*
 * Open database connection if necessary
 */
int mi_init_addresses();


/*
 * Reload address table to new hash table and when done, make new hash table
 * current one.
 */
int reload_address_table(void);


/*
 * Close connections and release memory
 */
void clean_addresses(void);


/*
 * Sets address group to be used by subsequent allow_address() tests.
 */
int set_address_group(struct sip_msg* _msg, char* _addr_group, char* _str2);


/*
 * Checks if an entry exists in cached address table that belongs to
 * pre-assigned group (default 0) and has ip address and port given in pseudo
 * variable parameters.  Port value 0 in cached address table matches
 * any port.
 */
int allow_address(struct sip_msg* _msg, char* _addr, char* _port);


/*
 * allow_source_address(group) equals to
 * set_address_group(group); allow_address("$si", "$sp");
 * but is faster.
 */
int allow_source_address(struct sip_msg* _msg, char* _addr_group, char* _str2);


#endif /* ADDRESS_H */
