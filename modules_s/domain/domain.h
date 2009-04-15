/* domain.h v 0.1 2002/12/27
 *
 * Header file for domain table relates functions
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


#ifndef _DOMAIN_H
#define _DOMAIN_H

#include <time.h>
#include <stdio.h>
#include "../../str.h"
#include "../../usr_avp.h"


/* 
 * Flags stored in flags column and their meaning 
 */
enum domain_flags {
	DOMAIN_DISABLED  = (1 << 0), /* Domain has been disabled and should not be loaded from the database */
	DOMAIN_CANONICAL = (1 << 1) /* Canonical domain name (to be used in user interfaces a.s.o.) */
};


/*
 * This structure represents a virtual domain within SER
 * Each virtual domain is identified by unique domain ID.
 * Each domain can have several domain names (also called
 * aliases
 */
typedef struct domain {
	str did;                /* Unique domain ID */
	int n;                  /* Number of domain names */
	str* domain;            /* Array of all domains associated with did */
	unsigned int* flags;    /* Flags of each domain in the domain array */
	avp_list_t attrs;       /* List of domain attributes */
	struct domain* next;    /* Next domain in the list */
} domain_t;


/*
 * Create domain list from domain table
 */
int load_domains(domain_t** dest);

/* 
 * Load domain attributes from database 
 */
int db_load_domain_attrs(domain_t* dest);


/*
 * Release all memory allocated for entire domain list
 */
void free_domain_list(domain_t* list);

typedef int (*domain_get_did_t)(str* did, str* domain);

#endif /* _DOMAIN_H */
