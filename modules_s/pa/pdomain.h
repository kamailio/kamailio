/*
 * Presence Agent, domain support
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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

#ifndef PDOMAIN_H
#define PDOMAIN_H


#include "hslot.h"
#include "presentity.h"
#include "../../fastlock.h"
#include "../../str.h"


typedef struct pdomain {
	str* name;
	int size;                 /* Hash table size */
	struct presentity* first; /* First presentity in the domain */
	struct presentity* last;  /* Last presentity in the domain */
	struct hslot* table;      /* Hash table for fast lookup */
	fl_lock_t lock;           /* Lock for the domain */
	int users;                /* Number of registered presentities */
	int expired;              /* Number of expired presentities */
} pdomain_t;


/*
 * Create a new domain structure
 * _n is pointer to str representing
 * name of the domain, the string is
 * not copied, it should point to str
 * structure stored in domain list
 * _s is hash table size
 */
int new_pdomain(str* _n, int _s, pdomain_t** _d);


/*
 * Free all memory allocated for
 * the domain
 */
void free_udomain(pdomain_t* _d);


/*
 * Just for debugging
 */
void print_udomain(FILE* _f, pdomain_t* _d);


/*
 * Timer handler for given domain
 */
int timer_udomain(pdomain_t* _d);


/*
 * Get lock
 */
void lock_pdomain(pdomain_t* _d);


/*
 * Release lock
 */
void unlock_pdomain(pdomain_t* _d);


/*
 * Find a presentity in domain
 */
int find_presentity(pdomain_t* _d, str* _to, struct presentity** _p);

/*
 * Add a presentity to domain
 */
void add_presentity(pdomain_t* _d, struct presentity* _p);


/*
 * Remove a presentity from domain
 */
void remove_presentity(pdomain_t* _d, struct presentity* _p);


#endif /* PDOMAIN_H */
