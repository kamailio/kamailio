/*
 * $Id: dlist.h 5160 2008-11-03 17:51:22Z henningw $
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
 *
 * History:
 * ========
 * 2006-11-28 Added get_number_of_users() (Jeffrey Magder - SOMA Networks)
 * 2007-09-12 added partitioning support for fetching all ul contacts
 *            (bogdan)
 */

/*! \file
 *  \brief USRLOC - List of registered domains
 *  \ingroup usrloc
 */


#ifndef DLIST_H
#define DLIST_H

#include <stdio.h>
#include "udomain.h"
#include "../../str.h"


/*!
 * List of all domains registered with usrloc
 */
struct domain_list_item {
	str name;
	udomain_t domain;
	struct domain_list_item*next;
};

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
typedef int (*register_udomain_t)(const char* _n, udomain_t** _d);
int register_udomain(const char* _n, udomain_t** _d);


/*!
 * \brief Free all allocated memory for domains
 */
void free_all_udomains(void);

typedef int  (*get_all_ucontacts_t) (void* buf, int len, unsigned int flags,
              unsigned int part_idx, unsigned int part_max);

int get_all_ucontacts(void *, int, unsigned int,
                unsigned int part_idx, unsigned int part_max);

/*!
 * \brief Run timer handler of all domains
 * \return 0 if all timer return 0, != 0 otherwise
 */
int synchronize_all_udomains(void);

/*!
 * \brief Loops through all domains summing up the number of users
 * \return the number of users, could be zero
 */
unsigned long get_number_of_users(void);



#endif
