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
 * \file route_tree.c
 * \brief Contains the functions to manage routing tree data in a digital tree.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../ut.h"
#include "carrierroute.h"
#include "route.h"
#include "route_rule.h"
#include "load_data.h"


/**
 * holds the map between routing domain names and numbers
 */
struct route_map ** script_routes = NULL;

static void destroy_route_tree_item(struct route_tree_item *route_tree);

static void destroy_failure_route_tree_item(struct failure_route_tree_item *route_tree_item);

static struct route_tree_item * create_route_tree_item(void);

static struct failure_route_tree_item * create_failure_route_tree_item(void);

static int add_route_tree(struct carrier_tree * ct, struct route_tree * rt);


/**
 * returns the routing tree for the given domain, if domain's tree
 * doesnt exist, it will be created. If the trees are completely
 * filled and a not existing domain shall be added, an error is
 * returned
 *
 * @param domain the domain name of desired routing tree
 * @param rd route data to be searched
 *
 * @return a pointer to the desired routing tree,
 * NULL on failure
 */
struct route_tree * get_route_tree(const str * domain, struct carrier_tree * rd) {
	int i, id;
	struct route_tree * rt = NULL;
	if (!rd) {
		LM_ERR("NULL pointer in parameter\n");
		return NULL;
	}
	for (i=0; i<rd->tree_num; i++) {
		if (rd->trees[i] && rd->trees[i]->name.s) {
			if (str_strcmp(&rd->trees[i]->name, domain) == 0) {
				LM_INFO("found domain %.*s\n", rd->trees[i]->name.len, rd->trees[i]->name.s);
				return rd->trees[i];
			}
		}
	}
	LM_INFO("domain %.*s not found, add it\n", domain->len, domain->s);
	if ((id = add_domain(domain)) < 0) {
		LM_ERR("could not add domain\n");
		return NULL;
	}
	if ((rt = create_route_tree(domain, id)) == NULL) {
		return NULL;
	}
	if ((rt->tree = create_route_tree_item()) == NULL) {
		return NULL;
	}
	if ((rt->failure_tree = create_failure_route_tree_item()) == NULL) {
		return NULL;
	}
	if (add_route_tree(rd, rt) < 0) {
		LM_ERR("couldn't add route tree\n");
		destroy_route_tree(rt);
		return NULL;
	}
	LM_INFO("created route tree: %.*s, with id %i\n", rt->name.len, rt->name.s, rt->id);
	return rt;
}


static int add_route_tree(struct carrier_tree * ct, struct route_tree * rt) {
	int i;
	LM_INFO("tree %.*s has %ld trees\n",
			ct->name.len, ct->name.s, (long)ct->tree_num);
	for (i=0; i<ct->tree_num; i++) {
		LM_DBG("tree %p", ct->trees[i]);
		if (ct->trees[i] == 0) {
			ct->trees[i] = rt;
			return 0;
		}
	}
	return -1;
}


struct route_tree * get_route_tree_by_id(struct carrier_tree * ct, int id) {
	int i;
	LM_DBG("searching in carrier %.*s, id %d\n", ct->name.len, ct->name.s, ct->id);
	for (i=0; i<ct->tree_num; i++) {
		if (ct->trees[i]) {
			LM_DBG("tree %.*s, domain %.*s : %i\n", ct->name.len, ct->name.s, ct->trees[i]->name.len, ct->trees[i]->name.s, ct->trees[i]->id);
			if (ct->trees[i]->id == id) {
				return ct->trees[i];
			}
		}
	}
	return NULL;
}


/**
 * creates a routing tree node in shared memory and sets it up.
 *
 * @return a pointer to the newly allocated route tree item, NULL
 * on error, in which case it LOGs an error message.
 */
static struct route_tree_item * create_route_tree_item(void) {
	struct route_tree_item *ret;

	ret = (struct route_tree_item *)
	      shm_malloc(sizeof(struct route_tree_item));
	if (ret == NULL) {
		LM_ERR("out of shared memory while building route tree.\n");
	} else {
		memset(ret, 0, sizeof(struct route_tree_item));
	}
	return ret;
}


/**
 * creates a routing tree node in shared memory and sets it up.
 *
 * @return a pointer to the newly allocated route tree item, NULL
 * on error, in which case it LOGs an error message.
 */
static struct failure_route_tree_item * create_failure_route_tree_item(void) {
	struct failure_route_tree_item *ret;
	
	ret = (struct failure_route_tree_item *)
		shm_malloc(sizeof(struct failure_route_tree_item));
	if (ret == NULL) {
		LM_ERR("out of shared memory while building route tree.\n");
	} else {
		memset(ret, 0, sizeof(struct failure_route_tree_item));
	}
	return ret;
}


/**
 * Create a new route tree root in shared memory and set it up.
 *
 * @param domain the domain name of the route tree
 * @param id the domain id of the route tree
 *
 * @return a pointer to the newly allocated route tree or NULL on
 * error, in which case it LOGs an error message.
 */
struct route_tree * create_route_tree(const str * domain, int id) {
	struct route_tree * tmp;
	if ((tmp = shm_malloc(sizeof(struct route_tree))) == NULL) {
		LM_ERR("out of shared memory\n");
		return NULL;
	}
	memset(tmp, 0, sizeof(struct route_tree));
	if (shm_str_dup(&tmp->name, domain)!=0) {
		LM_ERR("cannot duplicate string\n");
		shm_free(tmp);
		return NULL;
	}
	tmp->id = id;
	return tmp;
}


/**
 * Try to find a matching route_flags struct in rt and return it, add it if not found.
 *
 * @param route_tree the current route tree node
 * @param flags user defined flags
 * @param mask mask for user defined flags
 *
 * @return the route_flags struct on success, NULL on failure.
 *
 */
struct route_flags * add_route_flags(struct route_tree_item * route_tree, const flag_t flags, const flag_t mask)
{
	struct route_flags *shm_rf;
	struct route_flags *prev_rf = NULL;
	struct route_flags *tmp_rf = NULL;

	/* search for matching route_flags struct */
	for (tmp_rf=route_tree->flag_list; tmp_rf!=NULL; tmp_rf=tmp_rf->next) {
		if ((tmp_rf->flags == flags) && (tmp_rf->mask == mask)) return tmp_rf;
	}

	/* not found, insert one */
	for (tmp_rf=route_tree->flag_list; tmp_rf!=NULL; tmp_rf=tmp_rf->next) {
		if (tmp_rf->mask < mask) break;
		prev_rf=tmp_rf;
	}

	if ((shm_rf = shm_malloc(sizeof(struct route_flags))) == NULL) {
		LM_ERR("out of shared memory\n");
		return NULL;
	}
	memset(shm_rf, 0, sizeof(struct route_flags));

	shm_rf->flags=flags;
	shm_rf->mask=mask;
	shm_rf->next=tmp_rf;
	
	if (prev_rf) prev_rf->next = shm_rf;
	else route_tree->flag_list = shm_rf;

	return shm_rf;
}


/**
 * Adds the given route information to the route tree identified by
 * route_tree. scan_prefix identifies the number for which the information
 * is and the rewrite_* parameters define what to do in case of a match.
 * prob gives the probability with which this rule applies if there are
 * more than one for a given prefix.
 *
 * Note that this is a recursive function. It strips off digits from the
 * beginning of scan_prefix and calls itself.
 *
 * @param route_tree the current route tree node
 * @param scan_prefix the prefix at the current position
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
int add_route_to_tree(struct route_tree_item * route_tree, const str * scan_prefix,
		flag_t flags, flag_t mask, const str * full_prefix, int max_targets, double prob,
		const str * rewrite_hostpart, int strip, const str * rewrite_local_prefix,
		const str * rewrite_local_suffix, int status, int hash_index, 
		int backup, int * backed_up, const str * comment) {
	str next_prefix;
	struct route_flags *rf;

	if (scan_prefix->len == 0) {
		rf = add_route_flags(route_tree, flags, mask);
		if (rf==NULL) {
			LM_ERR("cannot add route_flags struct to route_tree\n");
			return -1;
		}
		return add_route_rule(rf, full_prefix, max_targets, prob, rewrite_hostpart, strip,
		                      rewrite_local_prefix, rewrite_local_suffix, status, hash_index,
		                      backup, backed_up, comment);
	} else {
		if (route_tree->nodes[*scan_prefix->s - '0'] == NULL) {
			route_tree->nodes[*scan_prefix->s - '0']
			= create_route_tree_item();
			if (route_tree->nodes[*scan_prefix->s - '0'] == NULL) {
				return -1;
			}
		}
		next_prefix.s = scan_prefix->s + 1;
		next_prefix.len = scan_prefix->len - 1;
		return add_route_to_tree(route_tree->nodes[*scan_prefix->s - '0'],
		                         &next_prefix, flags, mask, full_prefix, max_targets, prob,
		                         rewrite_hostpart, strip, rewrite_local_prefix,
		                         rewrite_local_suffix, status, hash_index,
		                         backup, backed_up, comment);
	}
}


/**
 * Adds the given failure route information to the failure route tree identified by
 * route_tree. scan_prefix, host, reply_code, flags identifies the number for which
 * the information is and the next_domain parameters defines where to continue
 * routing in case of a match.
 *
 * Note that this is a recursive function. It strips off digits from the
 * beginning of scan_prefix and calls itself.
 *
 * @param failure_tree the current route tree node
 * @param scan_prefix the prefix at the current position
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
int add_failure_route_to_tree(struct failure_route_tree_item * failure_tree, const str * scan_prefix,
		const str * full_prefix, const str * host, const str * reply_code,
		const flag_t flags, const flag_t mask, const int next_domain, const str * comment) {
	str next_prefix;

	if (!scan_prefix || !scan_prefix->s || *scan_prefix->s == '\0') {
		return add_failure_route_rule(failure_tree, full_prefix, host, reply_code, flags, mask, next_domain, comment);
	} else {
		if (failure_tree->nodes[*scan_prefix->s - '0'] == NULL) {
			failure_tree->nodes[*scan_prefix->s - '0']
				= create_failure_route_tree_item();
			if (failure_tree->nodes[*scan_prefix->s - '0'] == NULL) {
				return -1;
			}
		}
		next_prefix.s = scan_prefix->s + 1;
		next_prefix.len = scan_prefix->len - 1;
		return add_failure_route_to_tree(failure_tree->nodes[*scan_prefix->s - '0'],
																		 &next_prefix, full_prefix, host, reply_code,
																		 flags, mask, next_domain, comment);
	}
}

/**
 * Destroy the route map by freeing its memory.
 */
void destroy_route_map(void) {
	struct route_map * tmp, *tmp2;
	if (script_routes) {
		tmp = *script_routes;
		while (tmp) {
			tmp2 = tmp;
			tmp = tmp->next;
			shm_free(tmp2);
		}
		*script_routes = NULL;
		shm_free(script_routes);
		script_routes = NULL;
	}
}

/**
 * Destroys route_tree by freeing all its memory.
 *
 * @param route_tree route tree to be destroyed
 */
void destroy_route_tree(struct route_tree *route_tree) {
	destroy_route_tree_item(route_tree->tree);
	destroy_failure_route_tree_item(route_tree->failure_tree);
	shm_free(route_tree->name.s);
	shm_free(route_tree);
	return;
}

/**
 * Destroys route_flags in shared memory by freing all its memory.
 *
 * @param rf route_flags struct to be destroyed
 */
static void destroy_route_flags(struct route_flags *rf) {
	struct route_rule *rs;
	struct route_rule *rs_tmp;

	if (rf->rules) {
		shm_free(rf->rules);
	}
	rs = rf->rule_list;
	while (rs != NULL) {
		rs_tmp = rs->next;
		destroy_route_rule(rs);
		rs = rs_tmp;
	}
	shm_free(rf);
}

/**
 * Destroys route_tree_item in shared memory by freing all its memory.
 *
 * @param route_tree_item route tree node to be destroyed
 */
static void destroy_route_tree_item(struct route_tree_item *route_tree_item) {
	int i;
	struct route_flags *rf;
	struct route_flags *rf_tmp;

	if (!route_tree_item) {
		LM_ERR("NULL pointer in parameter\n");
	}

	for (i = 0; i < 10; ++i) {
		if (route_tree_item->nodes[i] != NULL) {
			destroy_route_tree_item(route_tree_item->nodes[i]);
		}
	}

	rf=route_tree_item->flag_list;
	while (rf!=NULL) {
		rf_tmp = rf->next;
		destroy_route_flags(rf);
		rf = rf_tmp;
	}
}


/**
 * Destroys failure_route_tree_item in shared memory by freing all its memory.
 *
 * @param route_tree_item route tree node to be destroyed
 */
static void destroy_failure_route_tree_item(struct failure_route_tree_item *route_tree_item) {
	int i;
	struct failure_route_rule *rs;
	struct failure_route_rule *rs_tmp;
	
	if (!route_tree_item) {
		LM_ERR("NULL pointer in parameter\n");
	}
	
	for (i = 0; i < 10; ++i) {
		if (route_tree_item->nodes[i] != NULL) {
			destroy_failure_route_tree_item(route_tree_item->nodes[i]);
		}
	}
	rs = route_tree_item->rule_list;
	while (rs != NULL) {
		rs_tmp = rs->next;
		destroy_failure_route_rule(rs);
		rs = rs_tmp;
	}
	shm_free(route_tree_item);
	return;
}


/**
 * Tries to add a domain to the domain map. If the given domain doesn't
 * exist, it is added. Otherwise, nothing happens.
 *
 * @param domain the domain to be added
 *
 * @return values: on succcess the numerical index of the given domain,
 * -1 on failure
 */
int add_domain(const str * domain) {
	struct route_map * tmp, * prev = NULL;
	int id = 0;
	if (!script_routes) {
		if ((script_routes = shm_malloc(sizeof(struct route_map *))) == NULL) {
			LM_ERR("out of shared memory\n");
			return -1;
		}
		memset(script_routes, 0, sizeof(struct route_map *));
	}

	tmp = *script_routes;

	while (tmp) {
		if (str_strcmp(&tmp->name, domain) == 0) {
			return tmp->no;
		}
		id = tmp->no + 1;
		prev = tmp;
		tmp = tmp->next;
	}
	if ((tmp = shm_malloc(sizeof(struct route_map))) == NULL) {
		LM_ERR("out of shared memory\n");
		return -1;
	}
	memset(tmp, 0, sizeof(struct route_map));
	if (shm_str_dup(&tmp->name, domain) != 0) {
		LM_ERR("cannot duplicate string\n");
		shm_free(tmp);
		return -1;
	}
	tmp->no = id;
	if (!prev) {
		*script_routes = tmp;
	} else {
		prev->next = tmp;
	}
	LM_INFO("domain %.*s has id %i\n", domain->len, domain->s, id);
	return id;
}
