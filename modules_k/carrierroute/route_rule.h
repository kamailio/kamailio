/*
 * $Id$
 *
 * Copyright (C) 2007-2008 1&1 Internet AG
 *
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/**
 * @file route_rule.h
 * @brief Contains the functions to manage routing rule data.
 */

#ifndef SP_ROUTE_ROUTE_RULE_H
#define SP_ROUTE_ROUTE_RULE_H

#include "route.h"

/**
 * Adds a route rule to rf
 *
 * @param rf the current route_flags struct
 * @param prefix the whole scan prefix
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
 * @param backed_up an NULL-termintated array of hash indices of the route
                    for which this route is backup
 * @param comment a comment for the route rule
 *
 * @return 0 on success, -1 on failure
 *
 * @see add_route_to_tree()
 */
int add_route_rule(struct route_flags *rf, const str * prefix,
		int max_targets, double prob, const str * rewrite_hostpart, int strip,
		const str * rewrite_local_prefix, const str * rewrite_local_suffix,
		int status, int hash_index, int backup, int * backed_up,
		const str * comment);

/**
 * Adds a failure route rule to rt
 *
 * @param failure_tree the current route tree node
 * @param prefix the whole scan prefix
 * @param host the hostname last tried
 * @param reply_code the reply code 
 * @param flags user defined flags
 * @param mask mask for user defined flags
 * @param next_domain continue routing with this domain id
 * @param comment a comment for the route rule
 *
 * @return 0 on success, -1 on failure
 *
 * @see add_failure_route_to_tree()
 */
int add_failure_route_rule(struct failure_route_tree_item * failure_tree,
		const str * prefix, const str * host, const str * reply_code,
		flag_t flags, flag_t mask, const int next_domain, const str * comment);

/**
 * Destroys route rule rr by freeing all its memory.
 *
 * @param rr route rule to be destroyed
 */
void destroy_route_rule(struct route_rule * rr);

/**
 * Destroys failure route rule rr by freeing all its memory.
 *
 * @param rr route rule to be destroyed
 */
void destroy_failure_route_rule(struct failure_route_rule * rr);

/**
 * Fixes the route rules by creating an array for accessing
 * route rules by hash index directly
 *
 * @param rd route data to be fixed
 *
 * @return 0 on success, -1 on failure
 */
int rule_fixup(struct rewrite_data * rd);

struct route_rule * find_rule_by_hash(struct route_flags * rf, int hash);

struct route_rule * find_rule_by_host(struct route_flags * rf, str * host);

int add_backup_route(struct route_rule * rule, struct route_rule * backup);

int remove_backed_up(struct route_rule * rule);

struct route_rule * find_auto_backup(struct route_flags * rf, struct route_rule * rule);

#endif
