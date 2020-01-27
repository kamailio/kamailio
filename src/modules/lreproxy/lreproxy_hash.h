/*
 * Copyright (C) 2019-2020 Mojtaba Esfandiari.S, Nasim-Telecom
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

#ifndef LREPROXY_HASH_H
#define LREPROXY_HASH_H

#include "../../core/str.h"
#include "../../core/locking.h"


/* table entry */
struct lreproxy_hash_entry {
    str src_ipv4;   //media ip address of initiator call in INVITE SIP message.
    str dst_ipv4;   //media ip address of selected node in 200Ok SIP message.
    str snat_ipv4;  //change media ip address to selected node.
    str dnat_ipv4;  //change media ip address to orgin destination party.
    str src_port;   //media port of initiator call in INVITE SIP message
    str dst_port;   //media port of selected node in 200Ok SIP message.
    str snat_port;  //change media port to selected node.
    str dnat_port;  //change media port to orgin destination party.

    str callid;				// call callid
    str viabranch;				// call viabranch
    struct lrep_node *node;			// call selected node

    unsigned int tout;			// call timeout
    struct lreproxy_hash_entry *next;	// call next
};

/* table */
struct lreproxy_hash_table {
    struct lreproxy_hash_entry **row_entry_list;	// vector of size pointers to entry
    gen_lock_t **row_locks;				// vector of size pointers to locks
    unsigned int *row_totals;			// vector of size numbers of entries in the hashtable rows
    unsigned int size;				// hash table size
};



int lreproxy_hash_table_init(int hsize);
int lreproxy_hash_table_destroy();
int lreproxy_hash_table_insert(str callid, str viabranch, struct lreproxy_hash_entry *value);
int lreproxy_hash_table_remove(str callid, str viabranch, enum lre_operation);
struct lreproxy_hash_entry *lreproxy_hash_table_lookup(str callid, str viabranch);


void lreproxy_hash_table_free_entry(struct lreproxy_hash_entry *entry);
void lreproxy_hash_table_free_row_entry_list(struct lreproxy_hash_entry *row_entry_list);

int lreproxy_hash_table_sanity_checks();

#endif //LREPROXY_HASH_H
