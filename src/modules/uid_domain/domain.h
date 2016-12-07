/*
 * Header file for domain table relates functions
 *
 * Copyright (C) 2002-2003 Juha Heinanen
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version
 *
 * sip-router is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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
	DOMAIN_DISABLED  = (1 << 0), /* Domain has been disabled and should not be
								  * loaded from the database */
	DOMAIN_CANONICAL = (1 << 1) /* Canonical domain name (to be used in user
								 * interfaces a.s.o.) */
};


/*
 * This structure represents a virtual domain within SER Each virtual domain
 * is identified by unique domain ID.  Each domain can have several domain
 * names (also called aliases
 */
typedef struct domain {
	str did;             /* Unique domain ID */
	int n;               /* Number of domain names */
	str* domain;         /* Array of all domains associated with did */
	unsigned int* flags; /* Flags of each domain in the domain array */
	avp_list_t attrs;    /* List of domain attributes */
	struct domain* next; /* Next domain in the list */
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


/* Retrieve did directly from database, without using memory cache. Use 0 as
 * the value of first parameter if you only want to know whether the entry is
 * in the database. The function returns 1 if there is such entry, 0 if not,
 * and -1 on error.  The result is allocated using pkg_malloc and must be
 * freed.
 */
int db_get_did(str* did, str* domain);

/* Check if the domain name given in the parameter is one
 * of the locally configured domain names.
 * Returns 1 if yes and -1 otherwise
 */
typedef int (*is_domain_local_f)(str* domain);
int is_domain_local(str* domain);


#endif /* _DOMAIN_H */
