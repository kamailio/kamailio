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
 *
 */

/*! \file
 *  \brief USRLOC - Usrloc contact structure
 *  \ingroup usrloc
 */


#ifndef UCONTACT_H
#define UCONTACT_H


#include <stdio.h>
#include "../../xavp.h"
#include "usrloc.h"


/*! \brief ancient time used for marking the contacts forced to expired */
#define UL_EXPIRED_TIME 10


/*!
 * \brief Create a new contact structure
 * \param _dom domain
 * \param _aor address of record
 * \param _contact contact string
 * \param _ci contact informations
 * \return new created contact on success, 0 on failure
 */
ucontact_t* new_ucontact(str* _dom, str* _aor, str* _contact,
		ucontact_info_t* _ci);


/*!
 * \brief Free all memory associated with given contact structure
 * \param _c freed contact
 */
void free_ucontact(ucontact_t* _c);


/*!
 * \brief Print contact, for debugging purposes only
 * \param _f output file
 * \param _c printed contact
 */
void print_ucontact(FILE* _f, ucontact_t* _c);


/*!
 * \brief Update existing contact in memory with new values
 * \param _c contact
 * \param _ci contact informations
 * \return 0
 */
int mem_update_ucontact(ucontact_t* _c, ucontact_info_t *_ci);


/* ===== State transition functions - for write back cache scheme ======== */

/*!
 * \brief Update state of the contact if we are using write-back scheme
 * \param _c updated contact
 */
void st_update_ucontact(ucontact_t* _c);


/*!
 * \brief Update state of the contact
 * \param _c updated contact
 * \return 1 if the contact should be deleted from memory immediately, 0 otherwise
 */
int st_delete_ucontact(ucontact_t* _c);


/*!
 * \brief Called when the timer is about to delete an expired contact
 * \param _c expired contact
 * \return 1 if the contact should be removed from the database and 0 otherwise
 */
int st_expired_ucontact(ucontact_t* _c);


/*!
 * \brief Called when the timer is about flushing the contact, updates contact state
 * \param _c flushed contact
 * \return 1 if the contact should be inserted, 2 if update and 0 otherwise
 */
int st_flush_ucontact(ucontact_t* _c);


/* ==== Database related functions ====== */

/*!
 * \brief Insert contact into the database
 * \param _c inserted contact
 * \return 0 on success, -1 on failure
 */
int db_insert_ucontact(ucontact_t* _c);


/*!
 * \brief Update contact in the database
 * \param _c updated contact
 * \return 0 on success, -1 on failure
 */
int db_update_ucontact(ucontact_t* _c);


/*!
 * \brief Delete contact from the database
 * \param _c deleted contact
 * \return 0 on success, -1 on failure
 */
int db_delete_ucontact(ucontact_t* _c);

/* ====== Module interface ====== */

/*!
 * \brief Update ucontact with new values
 * \param _r record the contact belongs to
 * \param _c updated contact
 * \param _ci new contact informations
 * \return 0 on success, -1 on failure
 */
int update_ucontact(struct urecord* _r, ucontact_t* _c, ucontact_info_t* _ci);

/* ====== per contact attributes ====== */

/*!
 * \brief Load all location attributes from a udomain
 *
 * Load all location attributes from a udomain, useful to populate the
 * memory cache on startup.
 * \param _dname loaded domain name
 * \param _user sip username
 * \param _domain sip domain
 * \param _ruid usrloc record unique id
 * \return 0 on success, -1 on failure
 */
int uldb_delete_attrs(str* _dname, str *_user, str *_domain, str *_ruid);

/*!
 * \brief Insert contact attributes into the database
 * \param _dname loaded domain name
 * \param _user sip username
 * \param _domain sip domain
 * \param _ruid record unique id
 * \param _xhead head of xavp list
 * \return 0 on success, -1 on failure
 */
int uldb_insert_attrs(str *_dname, str *_user, str *_domain,
        str *_ruid, sr_xavp_t *_xhead);
#endif
