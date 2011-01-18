/*
 * $Id: urecord.h 5241 2008-11-21 12:52:25Z henningw $ 
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



#endif
