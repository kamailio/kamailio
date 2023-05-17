/*
 * Copyright (C) 2003-2008 Sippy Software, Inc., http://www.sippysoft.com
 * Copyright (C) 2014-2015 Sipwise GmbH, http://www.sipwise.com
 * Copyright (C) 2020 Mojtaba Esfandiari.S, Nasim-Telecom
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
 *
 */

#ifndef LRKPROXY_HASH_H
#define LRKPROXY_HASH_H

#include "../../core/str.h"
#include "../../core/locking.h"


/* table entry */
struct lrkproxy_hash_entry
{
	str src_ipv4;  //media ip address of initiator call in INVITE SIP message.
	str dst_ipv4;  //media ip address of selected node in 200Ok SIP message.
	str snat_ipv4; //change media ip address to selected node.
	str dnat_ipv4; //change media ip address to orgin destination party.
	str src_port;  //media port of initiator call in INVITE SIP message
	str dst_port;  //media port of selected node in 200Ok SIP message.
	str snat_port; //change media port to selected node.
	str dnat_port; //change media port to orgin destination party.

	str callid;				// call callid
	str viabranch;			// call viabranch
	struct lrkp_node *node; // call selected node

	unsigned int tout;				  // call timeout
	struct lrkproxy_hash_entry *next; // call next
};

/* table */
struct lrkproxy_hash_table
{
	struct lrkproxy_hash_entry *
			*row_entry_list; // vector of size pointers to entry
	gen_lock_t **row_locks;	 // vector of size pointers to locks
	unsigned int *
			row_totals; // vector of size numbers of entries in the hashtable rows
	unsigned int size; // hash table size
};


int lrkproxy_hash_table_init(int hsize);
int lrkproxy_hash_table_destroy();
int lrkproxy_hash_table_insert(
		str callid, str viabranch, struct lrkproxy_hash_entry *value);
int lrkproxy_hash_table_remove(str callid, str viabranch, enum lrk_operation);
struct lrkproxy_hash_entry *lrkproxy_hash_table_lookup(
		str callid, str viabranch);
//struct lrkproxy_hash_entry *lrkproxy_hash_table_lookup(str callid, str viabranch, enum lrk_operation);
//struct lrkp_node *lrkproxy_hash_table_lookup(str callid, str viabranch, enum lrk_operation);
//void lrkproxy_hash_table_print();
//unsigned int lrkproxy_hash_table_total();

void lrkproxy_hash_table_free_entry(struct lrkproxy_hash_entry *entry);
void lrkproxy_hash_table_free_row_entry_list(
		struct lrkproxy_hash_entry *row_entry_list);

int lrkproxy_hash_table_sanity_checks();

#endif //LRKPROXY_HASH_H
