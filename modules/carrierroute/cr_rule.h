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
 * \file cr_rule.h
 * \brief Contains the functions to manage routing rule data.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#ifndef CR_RULE_H
#define CR_RULE_H

#include "../../str.h"
#include "../../flags.h"


/*! list of rules */
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
	str prefix; /*!< The prefix for which the route is valid */
	int status; /*!< The status of the route rule, only useful when using prime number hashing */
	struct route_rule_p_list * backed_up; /*!< indicates if the rule is already backup route for another */
	struct route_rule_p_list * backup; /*!< if not NULL, it points to a route rule which shall be used instead (only used if status is 0) */
	int hash_index; /*!< The hash index of the route rule, only useful when using prime number hashing */
	struct route_rule * next; /*!< A pointer to the next route rule */
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
 * Destroys route rule rr by freeing all its memory.
 *
 * @param rr route rule to be destroyed
 */
void destroy_route_rule(struct route_rule * rr);


/**
 * Try to find a matching route_flags struct in rt and return it, add it if not found.
 *
 * @param rf_head pointer to the head of the route flags list, might be changed during insert
 * @param flags user defined flags
 * @param mask mask for user defined flags
 *
 * @return pointer to the route_flags struct on success, NULL on failure.
 *
 */
struct route_flags * add_route_flags(struct route_flags **rf_head, const flag_t flags, const flag_t mask);


/**
 * Destroys route_flags in shared memory by freing all its memory.
 *
 * @param rf route_flags struct to be destroyed
 */
void destroy_route_flags(struct route_flags *rf);


/**
 * Adds a failure route rule to rule list. prefix, host, reply_code, and comment
 * must not contain NULL pointers.
 *
 * @param frr_head pointer to the head of the failure route rule list, might be changed during insert
 * @param prefix the whole scan prefix
 * @param host the hostname last tried
 * @param reply_code the reply code 
 * @param flags user defined flags
 * @param mask mask for user defined flags
 * @param next_domain continue routing with this domain
 * @param comment a comment for the route rule
 *
 * @return pointer to the failure_route_rul struct on success, NULL on failure.
 *
 * @see add_failure_route_to_tree()
 */
struct failure_route_rule *add_failure_route_rule(struct failure_route_rule **frr_head,
		const str * prefix, const str * host, const str * reply_code,
		flag_t flags, flag_t mask, const int next_domain, const str * comment);


/**
 * Destroys failure route rule frr by freeing all its memory.
 *
 * @param frr route rule to be destroyed
 */
void destroy_failure_route_rule(struct failure_route_rule * frr);

struct route_rule * find_rule_by_hash(struct route_flags * rf, int hash);

struct route_rule * find_rule_by_host(struct route_flags * rf, str * host);

int add_backup_rule(struct route_rule * rule, struct route_rule * backup);

int remove_backed_up(struct route_rule * rule);

struct route_rule * find_auto_backup(struct route_flags * rf, struct route_rule * rule);

#endif
