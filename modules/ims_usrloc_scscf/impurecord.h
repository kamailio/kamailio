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
 */


#ifndef IMPURECORD_H
#define IMPURECORD_H


#include <stdio.h>
#include <time.h>
#include "hslot.h"
#include "../../str.h"
#include "../../qvalue.h"
#include "ucontact.h"
#include "usrloc.h"

struct hslot; /*!< Hash table slot */

/*!
 * \brief Create and initialize new record structure
 * \param _dom domain name
 * \param _aor address of record
 * \param _r pointer to the new record
 * \return 0 on success, negative on failure
 */
int new_impurecord(str* _dom, str* public_identity, int reg_state, int barring, ims_subscription** s, str* ccf1, str* ccf2, str* ecf1, str* ecf2, impurecord_t** _r);


/*!
 * \brief Free all memory used by the given structure
 *
 * Free all memory used by the given structure.
 * The structure must be removed from all linked
 * lists first
 * \param _r freed record list
 */
void free_impurecord(impurecord_t* _r);


/*!
 * \brief Print a record, useful for debugging
 * \param _f print output
 * \param _r printed record
 */
void print_impurecord(FILE* _f, impurecord_t* _r);


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
ucontact_t* mem_insert_ucontact(impurecord_t* _r, str* _c, ucontact_info_t* _ci);


/*!
 * \brief Remove the contact from lists in memory
  * \param _c contact to remove
 */
void mem_remove_ucontact(ucontact_t* _c);


/*!
 * \brief Remove contact in memory from the list and delete it
 * \param _c contact to delete
 */
void mem_delete_ucontact(ucontact_t* _c);


/*!
 * \brief Run timer functions depending on the db_mode setting.
 *
 * Helper function that run the appropriate timer function, depending
 * on the db_mode setting.
 * \param _r processed record
 */
void timer_impurecord(impurecord_t* _r);


/* ===== Module interface ======== */


//int update_user_profile(ims_subscription* s, int assignment_type);

//impurecord_t* update_impurecord(enum pi_reg_states *reg_state,ims_subscription **s, str *ccf1, str *ccf2, str *ecf1, str *ecf2);

void free_ims_subscription_data(ims_subscription *s);


/*!
 * \brief Create and insert new contact into impurecord
 * \param _r record into the new contact should be inserted
 * \param _contact contact string
 * \param _ci contact information
 * \param _c new created contact
 * \return 0 on success, -1 on failure
 */
int insert_ucontact(impurecord_t* _r, str* _contact,
		ucontact_info_t* _ci, ucontact_t** _c);


/*!
 * \brief Delete ucontact from impurecord
 * \param _r record where the contact belongs to
 * \param _c deleted contact
 * \return 0 on success, -1 on failure
 */
int delete_ucontact(struct ucontact* _c);


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
int get_ucontact(impurecord_t* _r, str* _c, str* _callid, str* _path,
		int _cseq,
		struct ucontact** _co);

int update_impurecord(struct udomain* _d, str* public_identity, int reg_state, int send_sar_on_delete, int barring, int is_primary, ims_subscription** s, str* ccf1, str* ccf2, str* ecf1, str* ecf2, struct impurecord** _r);

int link_contact_to_impu(impurecord_t* impu, ucontact_t* contact, int write_to_db);
int unlink_contact_from_impu(impurecord_t* impu, ucontact_t* contact, int write_to_db);

#endif

