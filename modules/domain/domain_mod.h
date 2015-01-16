/*
 * Domain module headers
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


#ifndef DOMAIN_MOD_H
#define DOMAIN_MOD_H


#include "../../lib/srdb1/db.h"
#include "../../str.h"
#include "../../usr_avp.h"


/*
 * Constants
 */
#define DOM_HASH_SIZE 128

/* flags for param source for is_domain_local() */
#define PARAM_SOURCE_NONE  (0)
#define PARAM_SOURCE_AVP   (1<<0)
#define PARAM_SOURCE_RURI  (1<<1)
#define PARAM_SOURCE_FROM  (1<<2)


/*
 * Type definitions
 */
struct domain_list {
    str domain;
    str did;
    struct attr_list *attrs;
    struct domain_list *next;
};

struct attr_list {
    str name;
    short type;
    int_str val;
    struct attr_list *next;
};

typedef struct param_source {
	int source;       /* One of PARAM_SOURCE_XXX from above */

	int avp_type;     /* If source is an avp, the avp type else 0 */
	int_str avp_name; /* If source is an avp, the avp name else NULL */
} param_source;

/*
 * Module parameters variables
 */
extern str db_url;
extern str domain_table;	/* Domain table name */
extern str domain_attrs_table;	/* Domain attributes table name */
extern str did_col;   	        /* Domain id column name */
extern str domain_col;   	/* Domain column name */
extern str name_col;   	        /* Attribute name column name */
extern str type_col;   	        /* Attribute type column name */
extern str value_col;  	        /* Attribute value column name */

/*
 * Other module variables
 */
extern struct domain_list **hash_table_1; /* Hash table for domains */
extern struct domain_list **hash_table_2; /* Hash table for domains */
extern struct domain_list ***hash_table;  /* Current hash table */
extern gen_lock_t *reload_lock;

#endif /* DOMAIN_MOD_H */
