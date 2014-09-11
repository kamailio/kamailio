/*
 * $Id$
 *
 * Copyright (C) 2007-2008 1&1 Internet AG
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file cr_carrier.h
 * \brief Contains the functions to manage carrier data.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#ifndef CR_CARRIER_H
#define CR_CARRIER_H

#include <sys/types.h>
#include "../../str.h"


/**
 * The struct for a carrier.
 */
struct carrier_data_t {
	int id; /*!< id of the carrier */
	str * name; /*!< name of the carrier. This points to the name in carrier_map to avoid duplication. */
	struct domain_data_t ** domains; /*!< array of routing domains */
	size_t domain_num; /*!< number of routing domains */
	size_t first_empty_domain; /*!< the index of the first empty entry in domains */
};


/**
 * Create a new carrier_data struct in shared memory and set it up.
 *
 * @param carrier_id id of carrier
 * @param carrier_name pointer to the name of the carrier
 * @param domains number of domains for that carrier
 *
 * @return a pointer to the newly allocated carrier data or NULL on
 * error, in which case it LOGs an error message.
 */
struct carrier_data_t * create_carrier_data(int carrier_id, str *carrier_name, int domains);


/**
 * Destroys the given carrier and frees the used memory.
 *
 * @param carrier_data the structure to be destroyed.
 */
void destroy_carrier_data(struct carrier_data_t *carrier_data);


/**
 * Adds a domain_data struct to the given carrier data structure at the given index.
 * Other etries are moved one position up to make space for the new one.
 *
 * @param carrier_data the carrier data struct where domain_data should be inserted
 * @param domain_data the domain data struct to be inserted
 * @param index the index where to insert the domain_data structure in the domain array
 *
 * @return 0 on success, -1 on failure
 */
int add_domain_data(struct carrier_data_t * carrier_data, struct domain_data_t * domain_data, int index);


/**
 * Returns the domain data for the given id by doing a binary search.
 * @note The domain array must be sorted!
 *
 * @param carrier_data carrier data to be searched
 * @param domain_id the id of desired domain
 *
 * @return a pointer to the desired domain data, NULL if not found.
 */
struct domain_data_t *get_domain_data(struct carrier_data_t * carrier_data, int domain_id);


/**
 * Compares the IDs of two carrier data structures.
 * A NULL pointer is always greater than any ID.
 *
 * @return -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
 */
int compare_carrier_data(const void *v1, const void *v2);


#endif
