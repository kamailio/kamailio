/*
 * $Id$
 *
 * Copyright (C) 2007-2008 1&1 Internet AG
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
 */

/**
 * \file carrier_tree.h
 * \brief Contains the functions to manage carrier tree data.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#ifndef SP_ROUTE_CARRIER_TREE_H
#define SP_ROUTE_CARRIER_TREE_H

#include <sys/types.h>
#include "../../str.h"
#include "route.h"



/**
 * initialises the routing data, i.e. it binds the data loader
 * initialises the global data pointer
 *
 * @param source data source, can be db or file
 *
 * @return 0 on success, -1 on failure
 */
int init_route_data(const char * source);

/**
 * Loads the routing data into the routing tree and sets the
 * global_data pointer to the new data. The old_data is removed
 * when it is not locked anymore.
 *
 * @return 0 on success, -1 on failure
 */
int prepare_route_tree(void);

/**
 * Increases lock counter and returns a pointer to the
 * current routing data
 *
 * @return pointer to the global routing data on success,
 * NULL on failure
*/
struct rewrite_data * get_data(void);

/**
 * decrements the lock counter of the routing data
 *
 * @param data data to be released
 */
void release_data(struct rewrite_data *data);

/**
 * Create a new carrier tree in shared memory and set it up.
 *
 * @param tree the name of the carrier tree
 * @param carrier_id the id
 * @param id the domain id of the carrier tree
 * @param trees number of route_tree entries
 *
 * @return a pointer to the newly allocated route tree or NULL on
 * error, in which case it LOGs an error message.
 */
struct carrier_tree * create_carrier_tree(const str * tree, int carrier_id, int id, int trees);

/**
 * Adds the given route information to the route tree identified by
 * domain. scan_prefix identifies the number for which the information
 * is and the rewrite_* parameters define what to do in case of a match.
 * prob gives the probability with which this rule applies if there are
 * more than one for a given prefix.
 *
 * @param rd the route data to which the route shall be added
 * @param carrier_id the carrier id of the route to be added
 * @param domain the routing domain of the new route
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
int add_route(struct rewrite_data * rd, int carrier_id,
              const str * domain, const str * scan_prefix, flag_t flags, flag_t mask, int max_targets,
              double prob, const str * rewrite_hostpart, int strip, const str * rewrite_local_prefix,
              const str * rewrite_local_suffix, int status, int hash_index, int backup, int * backed_up,
              const str * comment);

/**
 * Adds the given failure route information to the failure route tree identified by
 * domain. scan_prefix, host, reply_code and flags identifies the number for which
 * the information is and the next_domain parameter defines where to continue routing
 * in case of a match.
 *
 * @param rd the route data to which the route shall be added
 * @param carrier_id the carrier id of the route to be added
 * @param domain the routing domain of the new route
 * @param scan_prefix the number prefix
 * @param host the hostname last tried
 * @param reply_code the reply code 
 * @param flags user defined flags
 * @param mask mask for user defined flags
 * @param next_domain continue routing with this domain
 * @param comment a comment for the failure route rule
 *
 * @return 0 on success, -1 on error in which case it LOGs a message.
 */
int add_failure_route(struct rewrite_data * rd, int carrier_id, const str * domain,
											const str * scan_prefix, const str * host, const str * reply_code,
											flag_t flags, flag_t mask, const str * next_domain, const str * comment);

/**
 * Tries to add a tree to the tree map. If the given tree doesn't
 * exist, it is added. Otherwise, nothing happens.
 *
 * @param tree the tree to be added
 *
 * @return values: on succcess the numerical index of the given tree,
 * -1 on failure
 */
int add_tree(const str * tree, int carrier_id);

/**
 * Searches for the ID for a Carrier-Name
 *
 * @param tree the carrier, we are looking for
 *
 * @return values: on succcess the id of for this carrier,
 * -1 on failure
 */
int find_tree(str tree);

/**
 * adds a carrier tree for the given carrier
 *
 * @param carrier the carrier name of desired routing tree
 * @param carrier_id the id of the carrier
 * @param rd route data to be searched
 * @param trees number of route_tree entries
 *
 * @return a pointer to the root node of the desired routing tree,
 * NULL on failure
 */
struct carrier_tree * add_carrier_tree(const str * carrier, int carrier_id, struct rewrite_data * rd, int trees);

/**
 * returns the routing tree for the given domain, if domain's tree
 * doesnt exist, it will be created. If the trees are completely
 * filled and a not existing domain shall be added, an error is
 * returned
 *
 * @param carrier_id the id of the desired carrier tree
 * @param rd route data to be searched
 *
 * @return a pointer to the root node of the desired routing tree,
 * NULL on failure
 */
struct carrier_tree * get_carrier_tree(int carrier_id, struct rewrite_data * rd);

/**
 * Frees the routing data
 */
void destroy_route_data();

/**
 * Destroys the complete routing tree data.
 *
 * @param data route data to be destroyed
 */
void destroy_rewrite_data(struct rewrite_data *data);

#endif
