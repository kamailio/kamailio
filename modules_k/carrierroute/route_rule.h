/*
 * $Id$
 *
 * Copyright (C) 2007 1&1 Internet AG
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
 *
 * @author: Jonas Appel <jonas dot appel at schlund dot de>
 *
 * @date Mo May 21 2007
 *
 * Copyright: 2007 1 & 1 Internet AG
 *
 * @brief contains the functions to manage routing rule
 * data
 *
 */

#ifndef SP_ROUTE_ROUTE_RULE_H
#define SP_ROUTE_ROUTE_RULE_H

#include "route.h"

/**
 * Adds a route rule to rt
 *
 * @param rt the current route tree node
 * @param full_prefix the whole scan prefix
 * @param max_locdb the number of locdbs
 * @param prob the weight of the rule
 * @param rewrite_hostpart the rewrite_host of the rule
 * @param strip the number of digits to be stripped off userpart before prepending prefix
 * @param rewrite_local_prefix the rewrite prefix
 * @param rewrite_local_suffix the rewrite suffix
 * @param status the status of the rule
 * @param hash_index the hash index of the rule
 * @param comment a comment for the route rule
 *
 * @return 0 on success, -1 on failure
 *
 * @see add_route_to_tree()
 */
int add_route_rule(struct route_tree_item * route_tree, const char * prefix,
                   int max_locdb, double prob, const char * rewrite_hostpart, int strip,
                   const char * rewrite_local_prefix, const char * rewrite_local_suffix,
                   int status, int hash_index, const char * comment);

/**
 * Destroys route rule rr by freeing all its memory.
 *
 * @param rr route rule to be destroyed
 */
void destroy_route_rule(struct route_rule * rr);

/**
 * Fixes the route rules by creating an array for accessing
 * route rules by hash index directly
 *
 * @param rd route data to be fixed
 *
 * @return 0 on success, -1 on failure
 */
int rule_fixup(struct rewrite_data * rd);

#endif
