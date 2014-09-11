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
 * \file cr_domain.c
 * \brief Contains the functions to manage routing domains.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include <stdlib.h>
#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "cr_domain.h"
#include "cr_rule.h"
#include "carrierroute.h"


/**
 * Destroys route_flags list in shared memory by freing all its memory.
 *
 * @param data the start of the route_flags list to be destroyed
 */
static void destroy_route_flags_list(void *data) {
	struct route_flags *rf, *rf_tmp;

	rf=(struct route_flags *)(data);
	while (rf!=NULL) {
		rf_tmp = rf->next;
		destroy_route_flags(rf);
		rf = rf_tmp;
	}
}


/**
 * Destroys failure_route_rule list in shared memory by freing all its memory.
 *
 * @param data the start of the failure_route_rule list to be destroyed
 */
static void destroy_failure_route_rule_list(void *data) {
	struct failure_route_rule *rs, *rs_tmp;
	
	rs = (struct failure_route_rule *)(data);
	while (rs != NULL) {
		rs_tmp = rs->next;
		destroy_failure_route_rule(rs);
		rs = rs_tmp;
	}
}


/**
 * Create a new domain in shared memory and set it up.
 *
 * @param domain_id the id of the domain
 * @param domain_name the name of the domain
 *
 * @return a pointer to the newly allocated domain data or NULL on
 * error, in which case it LOGs an error message.
 */
struct domain_data_t * create_domain_data(int domain_id, str * domain_name) {
	struct domain_data_t * tmp;
	if ((tmp = shm_malloc(sizeof(struct domain_data_t))) == NULL) {
		SHM_MEM_ERROR;
		return NULL;
	}
	memset(tmp, 0, sizeof(struct domain_data_t));
	tmp->id = domain_id;
	tmp->name = domain_name;
	if ((tmp->tree = dtrie_init(cr_match_mode)) == NULL) {
		shm_free(tmp);
		return NULL;
	}
	if ((tmp->failure_tree = dtrie_init(cr_match_mode)) == NULL) {
		dtrie_destroy(&tmp->tree, NULL, cr_match_mode);
		shm_free(tmp);
		return NULL;
	}
	return tmp;
}


/**
 * Destroys the given domain and frees the used memory.
 *
 * @param domain_data the structure to be destroyed.
 */
void destroy_domain_data(struct domain_data_t *domain_data) {
	if (domain_data) {
		dtrie_destroy(&domain_data->tree, destroy_route_flags_list, cr_match_mode);
		dtrie_destroy(&domain_data->failure_tree, destroy_failure_route_rule_list,
				cr_match_mode);
		shm_free(domain_data);
	}
}


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
		int backup, int * backed_up, const str * comment) {
	void **ret;
	struct route_flags *rf;

	ret = dtrie_contains(node, scan_prefix->s, scan_prefix->len, cr_match_mode);

	rf = add_route_flags((struct route_flags **)ret, flags, mask);
	if (rf == NULL) {
		LM_ERR("cannot insert route flags into list\n");
		return -1;
	}

	if (ret == NULL) {
		/* node does not exist */
		if (dtrie_insert(node, scan_prefix->s, scan_prefix->len, rf, cr_match_mode) != 0) {
			LM_ERR("cannot insert route flags into d-trie\n");
			return -1;
		}
	}

	/* Now add rule to flags */
	return add_route_rule(rf, full_prefix, max_targets, prob, rewrite_hostpart, strip,
												rewrite_local_prefix, rewrite_local_suffix, status, hash_index,
												backup, backed_up, comment);
}


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
		const flag_t flags, const flag_t mask, const int next_domain, const str * comment) {
	void **ret;
	struct failure_route_rule *frr;

	ret = dtrie_contains(failure_node, scan_prefix->s, scan_prefix->len, cr_match_mode);

	frr = add_failure_route_rule((struct failure_route_rule **)ret, full_prefix, host, reply_code, flags, mask, next_domain, comment);
	if (frr == NULL) {
		LM_ERR("cannot insert failure route rule into list\n");
		return -1;
		}

	if (ret == NULL) {
		/* node does not exist */
		if (dtrie_insert(failure_node, scan_prefix->s, scan_prefix->len, frr, cr_match_mode) != 0) {
			LM_ERR("cannot insert failure route rule into d-trie\n");
			return -1;
		}
	}

	return 0;
}


/**
 * Compares the IDs of two domain data structures.
 * A NULL pointer is always greater than any ID.
 *
 * @return -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
 */
int compare_domain_data(const void *v1, const void *v2) {
  struct domain_data_t *d1 = *(struct domain_data_t * const *)v1;
	struct domain_data_t *d2 = *(struct domain_data_t * const *)v2;
	if (d1 == NULL) {
		if (d2 == NULL) return 0;
		else return 1;
	}
	else {
		if (d2 == NULL) return -1;
		else {
			if (d1->id < d2->id) return -1;
			else if (d1->id > d2->id) return 1;
			else return 0;
		}
	}
}
