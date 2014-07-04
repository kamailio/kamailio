/*
 * $Id$
 *
 * List of registered domains
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifndef DLIST_H
#define DLIST_H

#include <stdio.h>
#include "udomain.h"
#include "../../str.h"


/*
 * List of all domains registered with usrloc
 */
typedef struct dlist {
	str name;            /* Name of the domain */
	udomain_t* d;        /* Payload */
	struct dlist* next;  /* Next element in the list */
} dlist_t;


extern dlist_t* root;

/*
 * Function registers a new domain with usrloc
 * if the domain exists, pointer to existing structure
 * will be returned, otherwise a new domain will be
 * created
 */
typedef int (*register_udomain_t)(const char* _n, udomain_t** _d);
int register_udomain(const char* _n, udomain_t** _d);


/*
 * Free all registered domains
 */
void free_all_udomains(void);


/*
 * Just for debugging
 */
void print_all_udomains(FILE* _f);


/*
 * Called from timer
 */
int synchronize_all_udomains(void);


/*
 * Get contacts to all registered users
 */
typedef int  (*get_all_ucontacts_t) (void* buf, int len, unsigned int flags);
int get_all_ucontacts(void *, int, unsigned int);


/*
 * Find a particular domain
 */
int find_domain(str* _d, udomain_t** _p);


#endif /* UDLIST_H */
