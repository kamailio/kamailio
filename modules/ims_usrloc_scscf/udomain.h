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
#include "impurecord.h"
#include "hslot.h"
#include "usrloc.h"

struct hslot; /*!< Hash table slot */
struct impurecord; /*!< Usrloc record */

/*! \brief
 * The structure represents a usrloc domain
 */
struct udomain {
    str* name; /*!< Domain name (NULL terminated) */
    int size; /*!< Hash table size */
    struct hslot* table; /*!< Hash table - array of collision slots */
    /* statistics */
    stat_var *users; /*!< no of registered users */
    stat_var *contacts; /*!< no of registered contacts */
    stat_var *expires; /*!< no of expires */
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
/*!
 * \brief Run timer handler for given domain
 * \param _d domain
 */
void mem_timer_udomain(udomain_t* _d);


int mem_insert_impurecord(struct udomain* _d, str* public_identity, int reg_state, int barring, ims_subscription** s, str* ccf1, str* ccf2, str* ecf1, str* ecf2, struct impurecord** _r);


/*!
 * \brief Remove a record from domain in memory
 * \param _d domain the record belongs to
 * \param _r deleted record
 */
void mem_delete_impurecord(udomain_t* _d, struct impurecord* _r);


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

void lock_contact_slot(str* contact_uri);
void unlock_contact_slot(str* contact_uri);
void lock_contact_slot_i(int i);
void unlock_contact_slot_i(int i);

/* ===== module interface ======= */


/*!
 * \brief Create and insert a new record
 * \param _d domain to insert the new record
 * \param _aor address of the record
 * \param _r new created record
 * \return return 0 on success, -1 on failure
 */
int insert_impurecord(struct udomain* _d, str* public_identity, int reg_state, int barring,
        ims_subscription** s, str* ccf1, str* ccf2, str* ecf1, str* ecf2,
        struct impurecord** _r);


/*!
 * \brief Obtain a impurecord pointer if the impurecord exists in domain
 * \param _d domain to search the record
 * \param _aor address of record
 * \param _r new created record
 * \return 0 if a record was found, 1 if nothing could be found
 */
int get_impurecord(udomain_t* _d, str* _aor, struct impurecord** _r);


/*!
 * \brief Delete a impurecord from domain
 * \param _d domain where the record should be deleted
 * \param _aor address of record
 * \param _r deleted record
 * \return 0 on success, -1 if the record could not be deleted
 */
int delete_impurecord(udomain_t* _d, str* _aor, struct impurecord* _r);


//get all IMPUs as string from a subscription related to an impurecord. apply filter for barring (assumed to be called with lock on impurec)
//barring-1 get all barred
//barring-0 get all unbarred
//barring-(-1) get all records
int get_impus_from_subscription_as_string(udomain_t* _d, impurecord_t* impu_rec, int barring, str** impus, int* num_impus);


#endif
