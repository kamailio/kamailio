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
 * \file cr_domain.h
 * \brief Contains the functions to manage routing domains.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#ifndef CR_DOMAIN_H
#define CR_DOMAIN_H

#include "../../str.h"
#include "../../flags.h"
#include "../../lib/trie/dtrie.h"


/**
 * The struct for the domain.
 * Contains the head of each prefix tree.
 */
struct domain_data_t {
	int id; /*!< the numerical id of the routing tree */
	str * name; /*!< the name of the routing tree. This points to the name in domain_map to avoid duplication. */
	struct dtrie_node_t * tree; /*!< the root node of the routing tree. Payload is of type (struct route_flags *) */
	struct dtrie_node_t * failure_tree; /*!< the root node of the failure routing tree. Payload is of type (struct failure_route_rule *) */
};


/**
 * Create a new domain in shared memory and set it up.
 *
 * @param domain_id the id of the domain
 * @param domain_name the name of the domain
 *
 * @return a pointer to the newly allocated domain data or NULL on
 * error, in which case it LOGs an error message.
 */
struct domain_data_t * create_domain_data(int id, str * domain);


/**
 * Destroys the given domain and frees the used memory.
 *
 * @param domain_data the to the structure to be destroyed.
 */
void destroy_domain_data(struct domain_data_t *domain_data);


/**
 * Adds the given route information to the prefix tree identified by
 * node. scan_prefix identifies the number for which the information
 * is. The rewrite_* parameters define what to do in case of a match.
 * prob gives the probability with which this rule applies if there are
 * more than one for a given prefix.
 *
 * @param node the root of the routing tree
 * @param scan_prefix the prefix for which to add the rule (must not contain non-digits)
 * @param flags user defined flags
 * @param mask mask for user defined flags
 * @param full_prefix the whole scan prefix
 * @param max_targets the number of targets
 * @param prob the weight of the rule
 * @param rewrite_hostpart the rewrite_host of the rule
 * @param strip the number of digits to be stripped off userpart before prepending prefix
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
 * @return 0 on success, -1 on failure
 *
 * @see add_route()
 */
int add_route_to_tree(struct dtrie_node_t *node, const str * scan_prefix,
		flag_t flags, flag_t mask, const str * full_prefix, int max_targets, double prob,
		const str * rewrite_hostpart, int strip, const str * rewrite_local_prefix,
		const str * rewrite_local_suffix, int status, int hash_index, 
		int backup, int * backed_up, const str * comment);


/**
 * Adds the given failure route information to the failure prefix tree identified by
 * failure_node. scan_prefix, host, reply_code, flags identifies the number for which
 * the information is and the next_domain parameters defines where to continue
 * routing in case of a match.
 *
 * @param failure_node the root of the failure routing tree
 * @param scan_prefix the prefix for which to add the rule (must not contain non-digits)
 * @param full_prefix the whole scan prefix
 * @param host the hostname last tried
 * @param reply_code the reply code 
 * @param flags user defined flags
 * @param mask mask for user defined flags
 * @param next_domain continue routing with this domain id
 * @param comment a comment for the route rule
 *
 * @return 0 on success, -1 on failure
 *
 * @see add_route()
 */
int add_failure_route_to_tree(struct dtrie_node_t * failure_node, const str * scan_prefix,
		const str * full_prefix, const str * host, const str * reply_code,
		const flag_t flags, const flag_t mask, const int next_domain, const str * comment);


/**
 * Compares the IDs of two domain data structures.
 * A NULL pointer is always greater than any ID.
 *
 * @return -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
 */
int compare_domain_data(const void *v1, const void *v2);


#endif
