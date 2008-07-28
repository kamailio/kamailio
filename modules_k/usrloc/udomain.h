/* 
 * $Id$ 
 *
 * Usrloc domain structure
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * History:
 * --------
 *  2003-03-11  changed to new locking scheme: locking.h (andrei)
 */


/*! \file
 *  \brief USRLOC - Usrloc domain structure
 *  \ingroup usrloc
 */

#ifndef UDOMAIN_H
#define UDOMAIN_H


#include <stdio.h>
#include "../../statistics.h"
#include "../../locking.h"
#include "../../str.h"
#include "../../db/db.h"
#include "urecord.h"
#include "hslot.h"


struct hslot;   /*!< Hash table slot */
struct urecord; /*!< Usrloc record */


/*! \brief
 * The structure represents a usrloc domain
 */
typedef struct udomain {
	str* name;                 /*!< Domain name (NULL terminated) */
	int size;                  /*!< Hash table size */
	struct hslot* table;       /*!< Hash table - array of collision slots */
	/* statistics */
	stat_var *users;           /*!< no of registered users */
	stat_var *contacts;        /*!< no of registered contacts */
	stat_var *expires;         /*!< no of expires */
} udomain_t;


/*! \brief
 * Create a new domain structure
 * _n is pointer to str representing
 * name of the domain, the string is
 * not copied, it should point to str
 * structure stored in domain list
 * _s is hash table size
 */
int new_udomain(str* _n, int _s, udomain_t** _d);


/*! \brief
 * Free all memory allocated for
 * the domain
 */
void free_udomain(udomain_t* _d);


/*! \brief
 * Just for debugging
 */
void print_udomain(FILE* _f, udomain_t* _d);


/*! \brief
 * Load data from a database
 */
int preload_udomain(db_con_t* _c, udomain_t* _d);


/*! \brief
 * Check the DB validity of a domain
 */
int testdb_udomain(db_con_t* con, udomain_t* d);


/*! \brief
 * Timer handler for given domain (db_only)
 */
int db_timer_udomain(udomain_t* _d);


/*! \brief
 * Timer handler for given domain
 */
int mem_timer_udomain(udomain_t* _d);


/*! \brief
 * Insert record into domain
 */
int mem_insert_urecord(udomain_t* _d, str* _aor, struct urecord** _r);


/*! \brief
 * Delete a record
 */
void mem_delete_urecord(udomain_t* _d, struct urecord* _r);


/*! \brief
 * Get lock
 */
typedef void (*lock_udomain_t)(udomain_t* _d, str *_aor);
void lock_udomain(udomain_t* _d, str *_aor);


/*! \brief
 * Release lock
 */
typedef void (*unlock_udomain_t)(udomain_t* _d, str *_aor);
void unlock_udomain(udomain_t* _d, str *_aor);


void lock_ulslot(udomain_t* _d, int i);
void unlock_ulslot(udomain_t* _d, int i);

/* ===== module interface ======= */


/*! \brief
 * Create and insert a new record
 */
typedef int (*insert_urecord_t)(udomain_t* _d, str* _aor, struct urecord** _r);
int insert_urecord(udomain_t* _d, str* _aor, struct urecord** _r);


/*! \brief
 * Obtain a urecord pointer if the urecord exists in domain
 */
typedef int  (*get_urecord_t)(udomain_t* _d, str* _a, struct urecord** _r);
int get_urecord(udomain_t* _d, str* _aor, struct urecord** _r);


/*! \brief
 * Delete a urecord from domain
 */
typedef int  (*delete_urecord_t)(udomain_t* _d, str* _a, struct urecord* _r);
int delete_urecord(udomain_t* _d, str* _aor, struct urecord* _r);


#endif /* UDOMAIN_H */
