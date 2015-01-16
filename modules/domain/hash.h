/*
 * Header file for hash table functions
 *
 * Copyright (C) 2002-2012 Juha Heinanen
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


#ifndef _DOM_HASH_H_
#define _DOM_HASH_H_

#include <stdio.h>
#include "domain_mod.h"
#include "../../lib/kmi/mi.h"

int hash_table_install (struct domain_list **hash_table, str *did, str *domain);
int hash_table_attr_install (struct domain_list **hash_table, str* did,
			     str *name, short type, int_str *val);
int hash_table_lookup (str *domain, str *did, struct attr_list **attrs);
int hash_table_mi_print(struct domain_list **hash_table, struct mi_node* rpl);
void hash_table_free (struct domain_list **hash_table);

#endif
