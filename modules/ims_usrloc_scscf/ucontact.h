/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 *
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 *
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus.
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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
 *
 *! \file
 *  \brief USRLOC - Usrloc contact structure
 *  \ingroup usrloc
 */


#ifndef UCONTACT_H
#define UCONTACT_H


#include <stdio.h>
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


/* ====== Module interface ====== */

/*!
 * \brief Update ucontact with new values
 * \param _r record the contact belongs to
 * \param _c updated contact
 * \param _ci new contact informations
 * \return 0 on success, -1 on failure
 */
int update_ucontact(struct impurecord* _r, ucontact_t* _c, ucontact_info_t* _ci);

/*!
 * \brief Setting contact expires to now in memory
 * \param _c contact
  * \return 0 on success, -1 on failure
 */
int mem_expire_ucontact(ucontact_t* _c);

/*!
 * \brief Setting ucontact expires to now
 * \param _r record the contact belongs to
 * \param _c updated contact
 * \return 0 on success, -1 on failure
 */
int expire_ucontact(struct impurecord* _r, ucontact_t* _c);

int remove_dialog_data_from_contact(ucontact_t* _c, unsigned int h_entry, unsigned int h_id);

int add_dialog_data_to_contact(ucontact_t* _c, unsigned int h_entry, unsigned int h_id);

void release_ucontact(struct ucontact* _c);

#endif
