/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "../usrloc/usrloc.h"
#include "urecord.h"
#include "hslot.h"


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
	/* for ul_db_layer */
	int dbt;                   /* type of the database */
	db1_con_t * dbh;            /* database handle */
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
 * \brief Timer function to cleanup expired contacts, DB_ONLY db_mode
 * \param _d cleaned domain
 * \return 0 on success, -1 on failure
 */
/*
int db_timer_udomain(udomain_t* _d);
*/


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

/*! \brief
 * Timer handler for given domain
 */
void lock_udomain(udomain_t* _d, str *_aor);


/*!
 * \brief Release lock for a domain
 * \param _d domain
 * \param _aor address of record, uses as hash source for the lock slot
 */
void unlock_udomain(udomain_t* _d, str *_aor);



/*!
 * \brief Create and insert a new record
 * \param _d domain to insert the new record
 * \param _aor address of the record
 * \param _r new created record
 * \return return 0 on success, -1 on failure
 */
int insert_urecord(udomain_t* _d, str* _aor, struct urecord** _r);


/*!
 * \brief Obtain a urecord pointer if the urecord exists in domain
 * \param _d domain to search the record
 * \param _aor address of record
 * \param _r new created record
 * \return 0 if a record was found, 1 if nothing could be found
 */
int get_urecord(udomain_t* _d, str* _aor, struct urecord** _r);


/*!
 * \brief Obtain a urecord pointer if the urecord exists in domain (lock slot)
 * \param _d domain to search the record
 * \param _aorhash hash id for address of record
 * \param _ruid record internal unique id
 * \param _r store pointer to location record
 * \param _c store pointer to contact structure
 * \return 0 if a record was found, 1 if nothing could be found
 */
int get_urecord_by_ruid(udomain_t* _d, unsigned int _aorhash,
		str *_ruid, struct urecord** _r, struct ucontact** _c);

/*!
 * \brief Delete a urecord from domain
 * \param _d domain where the record should be deleted
 * \param _aor address of record
 * \param _r deleted record
 * \return 0 on success, -1 if the record could not be deleted
 */
int delete_urecord(udomain_t* _d, str* _aor, struct urecord* _r);


#endif
