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
 *  \brief USRLOC - List of registered domains
 *  \ingroup usrloc
 */


#ifndef DLIST_H
#define DLIST_H

#include <stdio.h>
#include "../../core/str.h"
#include "usrloc.h"
#include "udomain.h"

/*!
 * List of all domains registered with usrloc
 */
typedef struct dlist {
	str name;            /*!< Name of the domain (null terminated) */
	udomain_t* d;        /*!< Payload */
	struct dlist* next;  /*!< Next element in the list */
} dlist_t;

/*! \brief Global list of all registered domains */
extern dlist_t *_ksr_ul_root;


/*!
 * \brief Registers a new domain with usrloc
 *
 * Registers a new domain with usrloc. If the domain exists,
 * a pointer to existing structure will be returned, otherwise
 * a new domain will be created
 * \param _n domain name
 * \param _d new created domain
 * \return 0 on success, -1 on failure
 */
int register_udomain(const char* _n, udomain_t** _d);


/*!
 * \brief Free all allocated memory for domains
 */
void free_all_udomains(void);


/*!
 * \brief Print all domains, just for debugging
 * \param _f output file
 */
void print_all_udomains(FILE* _f);


/*!
 * \brief Run timer handler of all domains
 * \return 0 if all timer return 0, != 0 otherwise
 */
int synchronize_all_udomains(int istart, int istep);


/*!
 * \brief Get all contacts from the usrloc, in partitions if wanted
 *
 * Return list of all contacts for all currently registered
 * users in all domains. The caller must provide buffer of
 * sufficient length for fitting all those contacts. In the
 * case when buffer was exhausted, the function returns
 * estimated amount of additional space needed, in this
 * case the caller is expected to repeat the call using
 * this value as the hint.
 *
 * Information is packed into the buffer as follows:
 *
 * +------------+----------+-----+------+-----+
 * |contact1.len|contact1.s|sock1|flags1|path1|
 * +------------+----------+-----+------+-----+
 * |contact2.len|contact2.s|sock2|flags2|path1|
 * +------------+----------+-----+------+-----+
 * |..........................................|
 * +------------+----------+-----+------+-----+
 * |contactN.len|contactN.s|sockN|flagsN|pathN|
 * +------------+----------+-----+------+-----+
 * |000000000000|
 * +------------+
 *
 * \param buf target buffer
 * \param len length of buffer
 * \param flags contact flags
 * \param part_idx part index
 * \param part_max maximal part
 * \param options options
 * \return 0 on success, positive if buffer size was not sufficient, negative on failure
 */
int get_all_ucontacts(void *buf, int len, unsigned int flags,
		unsigned int part_idx, unsigned int part_max, int options);


/*!
 * \brief Find and return usrloc domain
 *
 * \param _n domain name
 * \param _d usrloc domain (location table)
 * \return 0 on success, -1 on failure
 */
 int get_udomain(const char* _n, udomain_t** _d);

/*!
 * \brief Loops through all domains summing up the number of users
 * \return the number of users, could be zero
 */
unsigned long get_number_of_users(void);


/*!
 * \brief Find a particular domain, small wrapper around find_dlist
 * \param _d domain name
 * \param _p pointer to domain if found
 * \return 1 if domain was found, 0 otherwise
 */
int find_domain(str* _d, udomain_t** _p);


/*!
 * \brief Set the value for max partition
 */
void ul_set_max_partition(unsigned int m);

int ul_db_clean_udomains(void);

#endif
