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

/**
 * \file cr_data.h
 * \brief Contains the functions to manage routing data.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#ifndef CR_DATA_H
#define CR_DATA_H

#include <sys/types.h>
#include "../../locking.h"
#include "../../flags.h"
#include "cr_map.h"


/**
 * contains all routing data.
 */
struct route_data_t {
	struct name_map_t * carrier_map; /*!< holds the map between carrier names and numbers */
	struct name_map_t * domain_map; /*!< holds the map between domain names and numbers */
	struct carrier_data_t ** carriers; /*!< array of carriers */
	size_t carrier_num; /*!< number of carriers */
	size_t first_empty_carrier; /*!< the index of the first empty entry in carriers */
	size_t domain_num; /*!< total number of different domains */
	int default_carrier_id;
	int proc_cnt; /*!< a ref counter for the shm data */
	gen_lock_t lock; /*!< lock for ref counter updates */
};

/**
 * initialises the routing data, initialises the global data pointer
 *
 * @return 0 on success, -1 on failure
 */
int init_route_data(void);


/**
 * Frees the routing data
 */
void destroy_route_data(void);


/**
 * Clears the complete routing data.
 *
 * @param data route data to be cleared
 */
void clear_route_data(struct route_data_t *data);


/**
 * adds a carrier_data struct for given carrier
 *
 * @param rd route data to be searched
 * @param carrier_data the carrier data struct to be inserted
 *
 * @return 0 on success, -1 on failure
 */
int add_carrier_data(struct route_data_t * rd, struct carrier_data_t * carrier_data);


/**
 * Loads the routing data into the routing trees and sets the
 * global_data pointer to the new data. The old_data is removed
 * when it is not locked anymore.
 *
 * @return 0 on success, -1 on failure
 */
int reload_route_data(void);


/**
 * Increases lock counter and returns a pointer to the
 * current routing data
 *
 * @return pointer to the global routing data on success,
 * NULL on failure
*/
struct route_data_t * get_data(void);


/**
 * decrements the lock counter of the routing data
 *
 * @param data data to be released
 */
void release_data(struct route_data_t *data);


/**
 * Returns the carrier data for the given id by doing a binary search.
 * @note The carrier array must be sorted!
 *
 * @param rd route data to be searched
 * @param carrier_id the id of the desired carrier
 *
 * @return a pointer to the desired carrier data, NULL if not found.
 */
struct carrier_data_t *get_carrier_data(struct route_data_t * rd, int carrier_id);


/**
 * Adds the given route information to the routing domain identified by
 * domain. scan_prefix identifies the number for which the information
 * is and the rewrite_* parameters define what to do in case of a match.
 * prob gives the probability with which this rule applies if there are
 * more than one for a given prefix.
 *
 * @param rd the route data to which the route shall be added
 * @param carrier_id the carrier id of the route to be added
 * @param domain_id the routing domain id of the new route
 * @param scan_prefix the number prefix
 * @param flags user defined flags
 * @param mask mask for user defined flags
 * @param max_targets the number of targets
 * @param prob the weight of the rule
 * @param strip the number of digits to be stripped off userpart before prepending prefix
 * @param rewrite_hostpart the rewrite_host of the rule
 * @param rewrite_local_prefix the rewrite prefix
 * @param rewrite_local_suffix the rewrite suffix
 * @param status the status of the rule
 * @param hash_index the hash index of the rule
 * @param backup indicates if the route is backed up by another. only
                 useful if status==0, if set, it is the hash value
                 of another rule
  * @param backed_up an -1-termintated array of hash indices of the route
                    for which this route is backup
 * @param comment a comment for the route rule
 *
 * @return 0 on success, -1 on error in which case it LOGs a message.
 */
int add_route(struct route_data_t * rd, int carrier_id,
		int domain_id, const str * scan_prefix, flag_t flags, flag_t mask, int max_targets,
		double prob, const str * rewrite_hostpart, int strip, const str * rewrite_local_prefix,
		const str * rewrite_local_suffix, int status, int hash_index, int backup, int * backed_up,
		const str * comment);


/**
 * Adds the given failure route information to the failure routing domain identified by
 * domain. scan_prefix, host, reply_code and flags identifies the number for which
 * the information is and the next_domain parameter defines where to continue routing
 * in case of a match.
 *
 * @param rd the route data to which the route shall be added
 * @param carrier_id the carrier id of the route to be added
 * @param domain_id the routing domain id of the new route
 * @param scan_prefix the number prefix
 * @param host the hostname last tried
 * @param reply_code the reply code 
 * @param flags user defined flags
 * @param mask for user defined flags
 * @param next_domain_id continue routing with this domain id
 * @param comment a comment for the failure route rule
 *
 * @return 0 on success, -1 on error in which case it LOGs a message.
 */
int add_failure_route(struct route_data_t * rd, int carrier_id, int domain_id,
		const str * scan_prefix, const str * host, const str * reply_code,
		flag_t flags, flag_t mask, int next_domain_id, const str * comment);


/**
 * Fixes the route rules by creating an array for accessing
 * route rules by hash index directly
 *
 * @param rd route data to be fixed
 *
 * @return 0 on success, -1 on failure
 */
int rule_fixup(struct route_data_t * rd);


#endif
