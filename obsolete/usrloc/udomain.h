/* 
 * $Id$ 
 *
 * Usrloc domain structure
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
/*
 * History:
 * --------
 *  2003-03-11  changed to new locking scheme: locking.h (andrei)
 *  2006-11-23 switched to fixed hash size (andrei)
 */


#ifndef UDOMAIN_H
#define UDOMAIN_H


#include <stdio.h>
#include "../../locking.h"
#include "../../str.h"
#include "../../lib/srdb2/db.h"
#include "urecord.h"
#include "hslot.h"


/* udomain hash size, for best performance always use a 2^k value
 * good values 8192: 9% increase over 4096, 16384: 5% inc. over 8192,
 *  32768 4-5% inc over 16384 */
#define UDOMAIN_HASH_SIZE	16384

struct hslot;   /* Hash table slot */
struct urecord; /* Usrloc record */


/*
 * The structure represents a usrloc domain
 */
typedef struct udomain {
	str* name;                     /* Domain name */
	int users;                     /* Number of registered users */
	int expired;                   /* Number of expired contacts */
	int db_cmd_idx;                /* Index into db_cmd arrays */
	struct hslot* table;           /* Hash table - array of collision slots */
	struct {                       /* Linked list of all elements in the domain */
		int n;                 /* Number of element in the linked list */
		struct urecord* first; /* First element in the list */
		struct urecord* last;  /* Last element in the list */
	} d_ll;
	gen_lock_t lock;                /* lock variable */
} udomain_t;


/*
 * Create a new domain structure
 * _n is pointer to str representing
 * name of the domain, the string is
 * not copied, it should point to str
 * structure stored in domain list
 */
int new_udomain(str* _n, udomain_t** _d);


/*
 * Free all memory allocated for
 * the domain
 */
void free_udomain(udomain_t* _d);


/*
 * Just for debugging
 */
void print_udomain(FILE* _f, udomain_t* _d);


/*
 * Load data from a database
 */
int preload_udomain(udomain_t* _d);


/*
 * Timer handler for given domain
 */
int timer_udomain(udomain_t* _d);


/*
 * Insert record into domain
 */
int mem_insert_urecord(udomain_t* _d, str* _uid, struct urecord** _r);


/*
 * Delete a record
 */
void mem_delete_urecord(udomain_t* _d, struct urecord* _r);


/*
 * Get lock
 */
typedef void (*lock_udomain_t)(udomain_t* _d);
void lock_udomain(udomain_t* _d);


/*
 * Release lock
 */
typedef void (*unlock_udomain_t)(udomain_t* _d);
void unlock_udomain(udomain_t* _d);


/* ===== module interface ======= */


/*
 * Create and insert a new record
 */
typedef int (*insert_urecord_t)(udomain_t* _d, str* _uid, struct urecord** _r);
int insert_urecord(udomain_t* _d, str* _uid, struct urecord** _r);


/*
 * Obtain a urecord pointer if the urecord exists in domain
 */
typedef int  (*get_urecord_t)(udomain_t* _d, str* _uid, struct urecord** _r);
int get_urecord(udomain_t* _d, str* _uid, struct urecord** _r);


/*
 * Delete a urecord from domain
 */
typedef int  (*delete_urecord_t)(udomain_t* _d, str* _uid);
int delete_urecord(udomain_t* _d, str* _uid);


#endif /* UDOMAIN_H */
