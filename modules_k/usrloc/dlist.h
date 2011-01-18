/*
 * $Id$
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
#include "../../str.h"
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
extern dlist_t* root;


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
int synchronize_all_udomains(void);


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


#endif
