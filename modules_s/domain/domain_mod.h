/*
 * $Id$
 *
 * Domain module headers
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef DOMAIN_MOD_H
#define DOMAIN_MOD_H


#include "../../db/db.h"


/*
 * Constants
 */
#define HASH_SIZE 128

/*
 * Type definitions
 */
struct domain_list {
	char *domain;
	struct domain_list *next;
};

/*
 * Module parameters variables
 */
extern char* db_url;        /* Database URL */
extern int db_mode;         /* Database usage mode: 0 = no cache, 1 = cache */
extern char* domain_table;  /* Domain table name */
extern char* domain_col;    /* Domain column name */


/*
 * Other module variables
 */
extern db_con_t* db_handle; /* Database connection handle */
extern struct domain_list **hash_table_1; /* Hash table for domains */
extern struct domain_list **hash_table_2; /* Hash table for domains */
extern struct domain_list ***hash_table;  /* Current hash table */


#endif /* DOMAIN_MOD_H */
