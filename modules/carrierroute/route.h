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
 * \file
 * \brief Contains the functions to manage routing rule data.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#ifndef SP_ROUTE_ROUTE_H
#define SP_ROUTE_ROUTE_H

#include <sys/types.h>

#include "../../str.h"
#include "../../locking.h"
#include "../../flags.h"

struct route_rule_p_list;

/**
 * Second stage of processing: Try to map the end of the user part of the URI
 * to a given suffix. Then rewrite with given parameters.
 */
struct route_rule {
	int dice_to; /*!< prob * DICE_MAX */
	double prob; /*!< The probability for that rule, only useful when using crc32 hashing */
	double orig_prob; /*!< The original probability for that rule, only useful when using crc32 hashing */
	str host; /*!< The new target host for the request */
	int strip; /*!< the number of digits to be stripped off from uri befor prepending prefix */
	str local_prefix; /*!< the pefix to be attached to the new destination */
	str local_suffix; /*!< the suffix to be appended to the localpart of the new destination */
	str comment; /*!< A comment for the route rule */
	str prefix; /*!< The prefix for which the route ist valid */
	int status; /*!< The status of the route rule, only useful when using prime number hashing */
	struct route_rule_p_list * backed_up; /*!< indicates if the rule is already backup route for another */
	struct route_rule_p_list * backup; /*!< if not NULL, it points to a route rule which shall be used instead (only used if status is 0) */
	int hash_index; /*!< The hash index of the route rule, only useful when using prime number hashing */
	struct route_rule * next; /*!< A pointer to the next route rule */
};

/**
 * Second stage of processing: Try to map the end of the user part of the URI
 * to a given suffix. Then rewrite with given parameters.
 */
struct failure_route_rule {
	str host; /*!< The new target host for the request */
	str comment; /*!< A comment for the route rule */
	str prefix; /*!< The prefix for which the route ist valid */
	str reply_code;  /*!< The reply code for which the route ist valid */
	int next_domain;  /*!< The domain id where to continue routing */
	flag_t flags;  /*!< The flags for which the route ist valid */
	flag_t mask;  /*!< The mask for the flags field */
	struct failure_route_rule * next; /*!< A pointer to the next route rule */
};

/**
 * list of routing rules with hash index
 */
struct route_rule_p_list {
	struct route_rule * rr;
	int hash_index;
	struct route_rule_p_list * next;
};

/**
 * Use route rules only if message flags match stored mask/flags.
 */
struct route_flags {
	flag_t flags;  /*!< The flags for which the route ist valid */
	flag_t mask;  /*!< The mask for the flags field */
	struct route_rule * rule_list; /*!< Each node MAY contain a rule list */
	struct route_rule ** rules; /*!< The array points to the rules in order of hash indices */
	int rule_num; /*!< The number of rules */
	int dice_max; /*!< The DICE_MAX value for the rule set, calculated by rule_fixup */
	int max_targets; /*!< upper edge of hashing via prime number algorithm, must be eqal to rule_num */
	struct route_flags * next; /*!< A pointer to the next route flags struct */
};

/**
 * First stage of processing: The actual route tree.
 * Take one digit after another off the user part of the uri until the pointer
 * for the digit is NULL.
 * Note: We can only handle digits right now, ie., no letters or symbols.
 * Seems okay since this is for PSTN routing.
 */
struct route_tree_item {
	struct route_tree_item * nodes[10]; /*!< The Array points to child nodes if present */
	struct route_flags *flag_list;
};

/**
 * First stage of processing: The actual route tree. Take one digit after
 * another of the user part of the uri until the pointer for the digit is NULL.
 * Note: We can only handle digits right now, ie., no letters or symbols.
 * Seems okay since this is for PSTN routing.
 */
struct failure_route_tree_item {
	struct failure_route_tree_item * nodes[10]; /*!< The Array points to child nodes if present */
	struct failure_route_rule * rule_list; /*!< Each node MAY contain a failure rule list */
};

/**
 * The head of each route tree.
 */
struct route_tree {
	int id; /*!< the numerical id of the routing tree */
	str name; /*!< the name of the routing tree */
	struct route_tree_item * tree; /*!< the root node of the routing tree */
	struct failure_route_tree_item * failure_tree; /*!< the root node of the failure routing tree */
};

/**
 * The struct for a carrier routing tree.
 */
struct carrier_tree {
	struct route_tree ** trees; /*!< array of route trees */
	size_t tree_num; /*!< number of routing trees/domains */
	str name; /*!< name of the carrier */
	int id; /*!< id of the carrier */
	int index; /*!< index of the tree */
};

/**
 * contains all routing trees.
 */
struct rewrite_data {
	struct carrier_tree ** carriers; /*!< array of carrier trees */
	size_t tree_num; /*!< number of carrier trees */
	int default_carrier_index;
	int proc_cnt; /*!< a ref counter for the shm data */
	gen_lock_t lock; /*!< lock for ref counter updates */
};

/**
 * used to map routing domain names to numbers for
 * faster access.
 */
struct route_map {
	str name; /*!< name of the routing domain */
	int no; /*!< number of domain */
	struct route_map * next; /*!< pointer to the next element */
};

/**
 * used to map carrier tree names to numbers for
 * faster access.
 */
struct tree_map {
	str name; /*!< name of the carrier tree */
	int id; /*!< id of the carrier tree */
	int no; /*!< number of carrier array index for rewrite_data.trees  */
	struct tree_map * next; /*!< pointer to the next element */
};


#endif
