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

#include "../../core/parser/msg_parser.h"


/* Pointer to current address hash table pointer */
extern struct addr_list ***perm_addr_table;


/* Pointer to current subnet table */
extern struct subnet **perm_subnet_table;


/* Pointer to current domain name table */
extern struct domain_name_list ***perm_domain_table;

/*
 * Initialize data structures
 */
int init_addresses(void);


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


int allow_address(sip_msg_t *_msg, int addr_group, str *ips, int port);

/*
 * Checks if an entry exists in cached address table that belongs to a
 * given address group and has given ip address and port.  Port value
 * 0 in cached address table matches any port.
 */
int w_allow_address(struct sip_msg *_msg, char *_addr_group, char *_addr_sp,
		char *_port_sp);


int allow_source_address(sip_msg_t *_msg, int addr_group);

/*
 * w_allow_source_address("group") equals to allow_address("group", "$si", "$sp")
 * but is faster.
 */
int w_allow_source_address(
		struct sip_msg *_msg, char *_addr_group, char *_str2);


/*
 * Checks if source address/port is found in cached address or
 * subnet table in any group. If yes, returns that group. If not returns -1.
 * Port value 0 in cached address and group table matches any port.
 */
int allow_source_address_group(struct sip_msg *_msg, char *_str1, char *_str2);

int ki_allow_source_address_group(sip_msg_t *_msg);

/*
 * Checks if address/port is found in cached address or
 * subnet table in any group. If yes, returns that group. If not returns -1.
 * Port value 0 in cached address and group table matches any port.
 */
int allow_address_group(struct sip_msg *_msg, char *_addr, char *_port);

int ki_allow_address_group(sip_msg_t *_msg, str *_addr, int _port);

#endif /* ADDRESS_H */
