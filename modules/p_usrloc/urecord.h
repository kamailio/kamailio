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
 *  \brief USRLOC - Usrloc record structure
 *  \ingroup usrloc
 */


#ifndef URECORD_H
#define URECORD_H


#include <stdio.h>
#include <time.h>
#include "hslot.h"
#include "../../str.h"
#include "../../qvalue.h"
#include "ucontact.h"
#include "../usrloc/usrloc.h"

/*!
 * \brief Create and initialize new record structure
 * \param _dom domain name
 * \param _aor address of record
 * \param _r pointer to the new record
 * \return 0 on success, negative on failure
 */
int new_urecord(str* _dom, str* _aor, urecord_t** _r);


/*!
 * \brief Free all memory used by the given structure
 *
 * Free all memory used by the given structure.
 * The structure must be removed from all linked
 * lists first
 * \param _r freed record list
 */
void free_urecord(urecord_t* _r);


/*!
 * \brief Print a record, useful for debugging
 * \param _f print output
 * \param _r printed record
 */
void print_urecord(FILE* _f, urecord_t* _r);


/*!
 * \brief Add a new contact in memory
 *
 * Add a new contact in memory, contacts are ordered by:
 * 1) q value, 2) descending modification time
 * \param _r record this contact belongs to
 * \param _c contact
 * \param _ci contact information
 * \return pointer to new created contact on success, 0 on failure
 */
ucontact_t* mem_insert_ucontact(urecord_t* _r, str* _c, ucontact_info_t* _ci);


/*!
 * \brief Remove the contact from lists in memory
 * \param _r record this contact belongs to
 * \param _c removed contact
 */
void mem_remove_ucontact(urecord_t* _r, ucontact_t* _c);


/*!
 * \brief Remove contact in memory from the list and delete it
 * \param _r record this contact belongs to
 * \param _c deleted contact
 */
void mem_delete_ucontact(urecord_t* _r, ucontact_t* _c);


/*!
 * \brief Run timer functions depending on the db_mode setting.
 *
 * Helper function that run the appropriate timer function, depending
 * on the db_mode setting.
 * \param _r processed record
 */
void timer_urecord(urecord_t* _r);


/*!
 * \brief Delete a record from the database
 * \param _r deleted record
 * \return 0 on success, -1 on failure
 */
int db_delete_urecord(struct udomain* _d, urecord_t* _r);

/*!
 * \brief Create and insert new contact into urecord
 * \param _r record into the new contact should be inserted
 * \param _contact contact string
 * \param _ci contact information
 * \param _c new created contact
 * \return 0 on success, -1 on failure
 */
int insert_ucontact(urecord_t* _r, str* _contact,
           ucontact_info_t* _ci, ucontact_t** _c);


/*!
 * \brief Delete ucontact from urecord
 * \param _r record where the contact belongs to
 * \param _c deleted contact
 * \return 0 on success, -1 on failure
 */
int delete_ucontact(urecord_t* _r, struct ucontact* _c);


/*!
 * \brief Get pointer to ucontact with given contact
 * \param _r record where to search the contacts
 * \param _c contact string
 * \param _callid callid
 * \param _path path 
 * \param _cseq CSEQ number
 * \param _co found contact
 * \return 0 - found, 1 - not found, -1 - invalid found, 
 * -2 - found, but to be skipped (same cseq)
 */
int get_ucontact(urecord_t* _r, str* _c, str* _callid, str* _path,
           int _cseq,
           struct ucontact** _co);

/* ===== Module interface ======== */


/*!
 * \brief Release urecord previously obtained through get_urecord
 * \warning Failing to calls this function after get_urecord will
 * result in a memory leak when the DB_ONLY mode is used. When
 * the records is later deleted, e.g. with delete_urecord, then
 * its not necessary, as this function already releases the record.
 * \param _r released record
 */
void release_urecord(urecord_t* _r);

/*!
 * \brief Get pointer to ucontact with given contact
 * \param _r record where to search the contacts
 * \param _c contact string
 * \param _ci contact info (callid, cseq, instance, ...)
 * \param _co found contact
 * \return 0 - found, 1 - not found, -1 - invalid found, 
 * -2 - found, but to be skipped (same cseq)
 */
int get_ucontact_by_instance(urecord_t* _r, str* _c, ucontact_info_t* _ci,
		ucontact_t** _co);

#endif
