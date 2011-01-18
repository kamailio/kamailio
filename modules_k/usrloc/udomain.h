/*
 * $Id$ 

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
#include "../../lib/kcore/statistics.h"
#include "../../locking.h"
#include "../../str.h"
#include "../../lib/srdb1/db.h"
#include "urecord.h"
#include "hslot.h"
#include "usrloc.h"

struct hslot;   /*!< Hash table slot */
struct urecord; /*!< Usrloc record */


/*! \brief
 * The structure represents a usrloc domain
 */
struct udomain {
	str* name;                 /*!< Domain name (NULL terminated) */
	int size;                  /*!< Hash table size */
	struct hslot* table;       /*!< Hash table - array of collision slots */
	/* statistics */
	stat_var *users;           /*!< no of registered users */
	stat_var *contacts;        /*!< no of registered contacts */
	stat_var *expires;         /*!< no of expires */
};


/*!
 * \brief Create a new domain structure
 * \param  _n is pointer to str representing name of the domain, the string is
 * not copied, it should point to str structure stored in domain list
 * \param _s is hash table size
 * \param _d new created domain
 * \return 0 on success, -1 on failure
 */
int new_udomain(str* _n, int _s, udomain_t** _d);


/*!
 * \brief Free all memory allocated for the domain
 * \param _d freed domain
 */
void free_udomain(udomain_t* _d);


/*!
 * \brief Print udomain, debugging helper function
 */
void print_udomain(FILE* _f, udomain_t* _d);


/*!
 * \brief Load all records from a udomain
 *
 * Load all records from a udomain, useful to populate the
 * memory cache on startup.
 * \param _c database connection
 * \param _d loaded domain
 * \return 0 on success, -1 on failure
 */
int preload_udomain(db1_con_t* _c, udomain_t* _d);


/*!
 * \brief performs a dummy query just to see if DB is ok
 * \param con database connection
 * \param d domain
 */
int testdb_udomain(db1_con_t* con, udomain_t* d);


/*!
 * \brief Timer function to cleanup expired contacts, DB_ONLY db_mode
 * \param _d cleaned domain
 * \return 0 on success, -1 on failure
 */
int db_timer_udomain(udomain_t* _d);


/*!
 * \brief Run timer handler for given domain
 * \param _d domain
 */
void mem_timer_udomain(udomain_t* _d);


/*!
 * \brief Insert a new record into domain in memory
 * \param _d domain the record belongs to
 * \param _aor address of record
 * \param _r new created record
 * \return 0 on success, -1 on failure
 */
int mem_insert_urecord(udomain_t* _d, str* _aor, struct urecord** _r);


/*!
 * \brief Remove a record from domain in memory
 * \param _d domain the record belongs to
 * \param _r deleted record
 */
void mem_delete_urecord(udomain_t* _d, struct urecord* _r);

/*!
 * \brief  Get lock for a slot
 * \param _d domain
 * \param i slot number
 */
void lock_ulslot(udomain_t* _d, int i);

/*!
 * \brief Release lock for a slot
 * \param _d domain
 * \param i slot number
 */
void unlock_ulslot(udomain_t* _d, int i);

/* ===== module interface ======= */





#endif
