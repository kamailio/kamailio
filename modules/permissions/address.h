/*
 * Header file for address.c implementing allow_address function
 *
 * Copyright (C) 2006 Juha Heinanen
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

#ifndef ADDRESS_H
#define ADDRESS_H
		
#include "../../parser/msg_parser.h"


/* Pointer to current address hash table pointer */
extern struct addr_list ***addr_hash_table; 


/* Pointer to current subnet table */
extern struct subnet **subnet_table; 


/* Pointer to current domain name table */
extern struct domain_name_list ***domain_list_table;

/*
 * Initialize data structures
 */
int init_addresses(void);


/*
 * Open database connection if necessary
 */
int mi_init_addresses(void);


/*
 * Reload address table to new hash table and when done, make new hash table
 * current one.
 */
int reload_address_table(void);

/*
 * Wrapper to reload addr table from mi or rpc
 * we need to open the db_handle
 */
int reload_address_table_cmd(void);

/*
 * Close connections and release memory
 */
void clean_addresses(void);


/*
 * Checks if an entry exists in cached address table that belongs to a
 * given address group and has given ip address and port.  Port value
 * 0 in cached address table matches any port.
 */
int allow_address(struct sip_msg* _msg, char* _addr_group, char* _addr_sp,
		  char* _port_sp);


/*
 * allow_source_address("group") equals to allow_address("group", "$si", "$sp")
 * but is faster.
 */
int allow_source_address(struct sip_msg* _msg, char* _addr_group, char* _str2);


/*
 * Checks if source address/port is found in cached address or
 * subnet table in any group. If yes, returns that group. If not returns -1.
 * Port value 0 in cached address and group table matches any port.
 */
int allow_source_address_group(struct sip_msg* _msg, char* _str1, char* _str2);


/*
 * Checks if address/port is found in cached address or
 * subnet table in any group. If yes, returns that group. If not returns -1.
 * Port value 0 in cached address and group table matches any port.
 */
int allow_address_group(struct sip_msg* _msg, char* _addr, char* _port);

#endif /* ADDRESS_H */
