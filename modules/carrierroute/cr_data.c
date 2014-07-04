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
 * \file cr_data.c
 * \brief Contains the functions to manage routing data.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include <stdlib.h>
#include "../../mem/shm_mem.h"
#include "cr_data.h"
#include "carrierroute.h"
#include "cr_config.h"
#include "cr_db.h"
#include "cr_carrier.h"
#include "cr_domain.h"
#include "cr_rule.h"


/**
 * Pointer to the routing data.
 */
struct route_data_t ** global_data = NULL;


static int carrier_data_fixup(struct route_data_t * rd){
	int i;
	str tmp;
	tmp = default_tree;
	rd->default_carrier_id = -1;
	for(i=0; i<rd->carrier_num; i++){
		if(rd->carriers[i]){
			if(str_strcmp(rd->carriers[i]->name, &tmp) == 0){
				rd->default_carrier_id = rd->carriers[i]->id;
			}
		}
	}
	if(rd->default_carrier_id < 0){
		LM_ERR("default_carrier not found\n");
	}
	return 0;
}


/**
 * initialises the routing data, initialises the global data pointer
 *
 * @return 0 on success, -1 on failure
 */
int init_route_data(void) {
	if (global_data == NULL) {
		global_data = (struct route_data_t **)
		              shm_malloc(sizeof(struct route_data_t *));
		if (global_data == NULL) {
			SHM_MEM_ERROR;
			return -1;
		}
	}
	*global_data = NULL;
	return 0;
}


/**
 * Frees the routing data
 */
void destroy_route_data(void){
	struct route_data_t * rd = get_data();
	clear_route_data(rd);
	if(global_data){
		*global_data = NULL;
		shm_free(global_data);
		global_data = NULL;
	}
}


/**
 * Clears the complete routing data.
 *
 * @param data route data to be cleared
 */
void clear_route_data(struct route_data_t *data) {
	int i;

	if (data == NULL) {
		return;
	}
	if (data->carriers != NULL) {
		for (i = 0; i < data->carrier_num; ++i) {
			if (data->carriers[i] != NULL) {
				destroy_carrier_data(data->carriers[i]);
			}
		}
		shm_free(data->carriers);
	}
	if (data->carrier_map) {
		for (i = 0; i < data->carrier_num; ++i) {
			if (data->carrier_map[i].name.s) shm_free(data->carrier_map[i].name.s);
		}
		shm_free(data->carrier_map);
	}
	if (data->domain_map) {
		for (i = 0; i < data->domain_num; ++i) {
			if (data->domain_map[i].name.s) shm_free(data->domain_map[i].name.s);
		}
		shm_free(data->domain_map);
	}
	shm_free(data);
	return;
}


/**
 * adds a carrier_data struct for given carrier.
 *
 * @param rd route data to be searched
 * @param carrier_data the carrier data struct to be inserted
 *
 * @return 0 on success, -1 on failure
 */
int add_carrier_data(struct route_data_t * rd, struct carrier_data_t * carrier_data) {
	if (rd->first_empty_carrier >= rd->carrier_num) {
		LM_ERR("carrier array already full");
		return -1;
	}

	if (rd->carriers[rd->first_empty_carrier] != 0) {
		LM_ERR("invalid pointer in first empty carrier entry");
		return -1;
	}

	rd->carriers[rd->first_empty_carrier] = carrier_data;
	rd->first_empty_carrier++;
	return 0;
}


/**
 * Loads the routing data into the routing trees and sets the
 * global_data pointer to the new data. The old_data is removed
 * when it is not locked anymore.
 *
 * @return 0 on success, -1 on failure
 */
int reload_route_data(void) {
	struct route_data_t * old_data;
	struct route_data_t * new_data = NULL;
	int i;

	if ((new_data = shm_malloc(sizeof(struct route_data_t))) == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(new_data, 0, sizeof(struct route_data_t));

	switch (mode) {
	case CARRIERROUTE_MODE_DB:
		if (load_route_data_db(new_data) < 0) {
			LM_ERR("could not load routing data\n");
			goto errout;
		}
		break;
	case CARRIERROUTE_MODE_FILE:
		if (load_config(new_data) < 0) {
			LM_ERR("could not load routing data\n");
			goto errout;
		}
		break;
	default:
		LM_ERR("invalid mode");
		goto errout;
	}
	if (new_data == NULL) {
		LM_ERR("loading routing data failed (NULL pointer)");
		goto errout;
	}

	/* sort carriers by id for faster access */
	qsort(new_data->carriers, new_data->carrier_num, sizeof(new_data->carriers[0]), compare_carrier_data);

	/* sort domains by id for faster access */
	for (i=0; i<new_data->carrier_num; i++) {
		qsort(new_data->carriers[i]->domains, new_data->carriers[i]->domain_num, sizeof(new_data->carriers[i]->domains[0]), compare_domain_data);
	}

	if (rule_fixup(new_data) < 0) {
		LM_ERR("could not fixup rules\n");
		goto errout;
	}

	if (carrier_data_fixup(new_data) < 0){
		LM_ERR("could not fixup trees\n");
		goto errout;
	}

	new_data->proc_cnt = 0;

	if (*global_data == NULL) {
		*global_data = new_data;
	} else {
		old_data = *global_data;
		*global_data = new_data;
		i = 0;
		while (old_data->proc_cnt > 0) {
			LM_ERR("data is still locked after %i seconds\n", i);
			sleep_us(i*1000000);
			i++;
		}
		clear_route_data(old_data);
	}
	return 0;

 errout:
	clear_route_data(new_data);
	return -1;
}


/**
 * Increases lock counter and returns a pointer to the
 * current routing data
 *
 * @return pointer to the global routing data on success,
 * NULL on failure
*/
struct route_data_t * get_data(void) {
	struct route_data_t *ret;
	if (!global_data || !*global_data) {
		return NULL;
	}
	ret = *global_data;
	lock_get(&ret->lock);
	++ret->proc_cnt;
	lock_release(&ret->lock);
	if (ret == *global_data) {
		return ret;
	} else {
		lock_get(&ret->lock);
		--ret->proc_cnt;
		lock_release(&ret->lock);
		return NULL;
	}
}


/**
 * decrements the lock counter of the routing data
 *
 * @param data data to be released
 */
void release_data(struct route_data_t *data) {
	lock_get(&data->lock);
	--data->proc_cnt;
	lock_release(&data->lock);
}


/**
 * Returns the carrier data for the given id by doing a binary search.
 * @note The carrier array must be sorted!
 *
 * @param rd route data to be searched
 * @param carrier_id the id of the desired carrier
 *
 * @return a pointer to the desired carrier data, NULL if not found.
 */
struct carrier_data_t *get_carrier_data(struct route_data_t * rd, int carrier_id) {
	struct carrier_data_t **ret;
	struct carrier_data_t key;
	struct carrier_data_t *pkey = &key;

	if (!rd) {
		LM_ERR("NULL pointer in parameter\n");
		return NULL;
	}
	key.id = carrier_id;
	ret = bsearch(&pkey, rd->carriers, rd->carrier_num, sizeof(rd->carriers[0]), compare_carrier_data);
	if (ret) return *ret;
	return NULL;
}


typedef int (*cmpfunc_t)(const void *v1, const void *v2);


/**
 * Implements a binary search algorithm using the function cmpfunc
 * for comparison.
 *
 * @param base pointer to the beginning of the array
 * @param len length of array
 * @param elemsize size of array elements
 * @param key pointer to the key we are looking for
 * @param cmpfunc function to be used for comparison
 * @param index  If index is not NULL it is set to:
 *     -1 if an error occured,
 *     the index of the first entry equal to v
 *     or the index of the first entry greater than v in the case v was not found.
 *   Be careful: The index returned can be greater than the length of the array!
 *
 * @return -1 on error, 0 if the value was not found, 1 if it was found.
 */
static int binary_search(void *base, unsigned int len, int elemsize, void *key, cmpfunc_t cmpfunc, int *index) {
	int left, right, mid;

	if (index) *index=-1;
	if (!base) {
		LM_ERR("NULL pointer in parameter\n");
		return -1;
	}
	if (len == 0) {
		if (index) *index=0;
		return 0;
	}

	left=0;
	right=len-1;
	if (cmpfunc(base+elemsize*left, key) > 0) {
		LM_DBG("not found (out of left bound)\n");
		if (index) *index=0; /* not found, must be inserted at the beginning of array */
		return 0;
	}
	if (cmpfunc(base+elemsize*right, key) < 0) {
		LM_DBG("not found (out of right bound)\n");
		if (index) *index=len; /* not found, must be inserted at the end of array */
		return 0;
	}

	while (left < right) {
		mid = left + ((right - left) / 2);
		if (cmpfunc(base+elemsize*mid, key) < 0) left = mid + 1;
		else right = mid;
	}

	/* left == right here! */
	if (index) *index=left;
	if (cmpfunc(base+elemsize*left, key) == 0) return 1;
	else return 0;
}


/**
 * Returns the domain data for the given id by doing a binary search.
 * If not found, a new domain data structure is added.
 *
 * @param rd route data to used for name - id mapping
 * @param carrier_data carrier data to be searched
 * @param domain_id the id of desired domain
 *
 * @return a pointer to the desired domain data, NULL on error.
 */
static struct domain_data_t * get_domain_data_or_add(struct route_data_t * rd, struct carrier_data_t * carrier_data, int domain_id) {
	struct domain_data_t key;
	struct domain_data_t *pkey = &key;
	struct domain_data_t *domain_data = NULL;
	str *domain_name;
	int i;
	int res;

	if ((!rd) || (!carrier_data)) {
		LM_ERR("NULL pointer in parameter\n");
		return NULL;
	}

	key.id = domain_id;
	res = binary_search(carrier_data->domains, carrier_data->first_empty_domain, sizeof(struct domain_data_t *), &pkey, compare_domain_data, &i);
	if (res<0) {
		LM_ERR("error while searching for domain_id %d\n", domain_id);
		return NULL;
	}
	else if (res>0) {
		/* found domain id */
		domain_data = carrier_data->domains[i];
	}
	else {
		/* did not find domain id - insert new entry! */
		if ((domain_name = map_id2name(rd->domain_map, rd->domain_num, domain_id)) == NULL) {
			LM_ERR("could not find domain name for id %d\n", domain_id);
			return NULL;
		}
		if ((domain_data = create_domain_data(domain_id, domain_name)) == NULL) {
			LM_ERR("could not create new domain data\n");
			return NULL;
		}

		/* keep the array sorted! */
		if (add_domain_data(carrier_data, domain_data, i) < 0) {
			LM_ERR("could not add domain data\n");
			destroy_domain_data(domain_data);
			return NULL;
		}
		LM_INFO("added domain %d '%.*s' to carrier %d '%.*s'", domain_id, domain_name->len, domain_name->s, carrier_data->id, carrier_data->name->len, carrier_data->name->s);
	}

	return domain_data;
}


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
 * @return 0 on success, -1 on error in which case it LOGs a message.
 */
int add_route(struct route_data_t * rd, int carrier_id,
		int domain_id, const str * scan_prefix, flag_t flags, flag_t mask, int max_targets,
		double prob, const str * rewrite_hostpart, int strip,
		const str * rewrite_local_prefix, const str * rewrite_local_suffix,
		int status, int hash_index, int backup, int * backed_up, const str * comment) {
	struct carrier_data_t * carrier_data = NULL;
	struct domain_data_t * domain_data = NULL;
	LM_INFO("adding prefix %.*s, prob %f\n", scan_prefix->len, scan_prefix->s, prob);

	if ((carrier_data = get_carrier_data(rd, carrier_id)) == NULL) {
		LM_ERR("could not retrieve carrier data for carrier id %d\n", carrier_id);
		return -1;
	}

	if ((domain_data = get_domain_data_or_add(rd, carrier_data, domain_id)) == NULL) {
		LM_ERR("could not retrieve domain data\n");
		return -1;
	}

	LM_INFO("found carrier and domain, now adding route\n");
	return add_route_to_tree(domain_data->tree, scan_prefix, flags, mask, scan_prefix, max_targets, prob, rewrite_hostpart,
	                         strip, rewrite_local_prefix, rewrite_local_suffix, status,
	                         hash_index, backup, backed_up, comment);
}


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
		flag_t flags, flag_t mask, int next_domain_id, const str * comment) {
	struct carrier_data_t * carrier_data = NULL;
	struct domain_data_t * domain_data = NULL;
	LM_INFO("adding prefix %.*s, reply code %.*s\n", scan_prefix->len, scan_prefix->s, reply_code->len, reply_code->s);
		
	if (reply_code->len!=3) {
		LM_ERR("invalid reply_code '%.*s'!\n", reply_code->len, reply_code->s);
		return -1;
	}
	
	if ((carrier_data = get_carrier_data(rd, carrier_id)) == NULL) {
		LM_ERR("could not retrieve carrier data\n");
		return -1;
	}
	
	if ((domain_data = get_domain_data_or_add(rd, carrier_data, domain_id)) == NULL) {
		LM_ERR("could not retrieve domain data\n");
		return -1;
	}

	LM_INFO("found carrier and domain, now adding failure route\n");
	return add_failure_route_to_tree(domain_data->failure_tree, scan_prefix, scan_prefix, host, reply_code,
			flags, mask, next_domain_id, comment);
}


static int fixup_rule_backup(struct route_flags * rf, struct route_rule * rr){
	struct route_rule_p_list * rl;
	if(!rr->status && rr->backup){
		if((rr->backup->rr = find_rule_by_hash(rf, rr->backup->hash_index)) == NULL){
			LM_ERR("didn't find backup route\n");
			return -1;
		}
	}
	rl = rr->backed_up;
	while(rl){
		if((rl->rr = find_rule_by_hash(rf, rl->hash_index)) == NULL){
			LM_ERR("didn't find backed up route\n");
			return -1;
		}
		rl = rl->next;
	}
	return 0;
}


/**
 * Does the work for rule_fixup recursively.
 * First, it tries to set a pointer the rules with an existing hash index
 * at the marching array index. Afterward, remaining rules are populated
 * with incrementing hash indices.
 *
 * @param node the prefix tree node to be fixed up
 *
 * @return 0 on success, -1 on failure
 */
static int rule_fixup_recursor(struct dtrie_node_t *node) {
	struct route_rule * rr;
	struct route_flags * rf;
	int i, p_dice, ret = 0;

	for (rf=(struct route_flags *)(node->data); rf!=NULL; rf=rf->next) {
		p_dice = 0;
		if (rf->rule_list) {
			rr = rf->rule_list;
			rf->rule_num = 0;
			while (rr) {
				rf->rule_num++;
				rf->dice_max += rr->prob * DICE_MAX;
				rr = rr->next;
			}
			rr = rf->rule_list;
			while (rr) {
				rr->dice_to = (rr->prob * DICE_MAX) + p_dice;
				p_dice = rr->dice_to;
				rr = rr->next;
			}
			
			if (rf->rule_num != rf->max_targets) {
				LM_ERR("number of rules(%i) differs from max_targets(%i), maybe your config is wrong?\n", rf->rule_num, rf->max_targets);
				return -1;
			}
			if(rf->rules) {
				shm_free(rf->rules);
				rf->rules = NULL;
			}
			if ((rf->rules = shm_malloc(sizeof(struct route_rule *) * rf->rule_num)) == NULL) {
				SHM_MEM_ERROR;
				return -1;
			}
			memset(rf->rules, 0, sizeof(struct route_rule *) * rf->rule_num);
			for (rr = rf->rule_list; rr; rr = rr->next) {
				if (rr->hash_index) {
					if (rr->hash_index > rf->rule_num) {
						LM_ERR("too large hash index %i, max is %i\n", rr->hash_index, rf->rule_num);
						shm_free(rf->rules);
						return -1;
					}
					if (rf->rules[rr->hash_index - 1]) {
						LM_ERR("duplicate hash index %i\n", rr->hash_index);
						shm_free(rf->rules);
						return -1;
					}
					rf->rules[rr->hash_index - 1] = rr;
					LM_INFO("rule with host %.*s hash has hashindex %i.\n", rr->host.len, rr->host.s, rr->hash_index);
				}
			}
			
			rr = rf->rule_list;
			i=0;
			while (rr && i < rf->rule_num) {
				if (!rr->hash_index) {
					if (rf->rules[i]) {
						i++;
					} else {
						rf->rules[i] = rr;
						rr->hash_index = i + 1;
						LM_INFO("hashless rule with host %.*s hash, hash_index %i\n", rr->host.len, rr->host.s, i+1);
						rr = rr->next;
					}
				} else {
					rr = rr->next;
				}
			}
			if (rr) {
				LM_ERR("Could not populate rules: rr: %p\n", rr);
				return -1;
			}
			for(i=0; i<rf->rule_num; i++){
				ret += fixup_rule_backup(rf, rf->rules[i]);
			}
		}
	}

	for (i=0; i<cr_match_mode; i++) {
		if (node->child[i]) {
			ret += rule_fixup_recursor(node->child[i]);
		}
	}

	return ret;
}


/**
 * Fixes the route rules by creating an array for accessing
 * route rules by hash index directly
 *
 * @param rd route data to be fixed
 *
 * @return 0 on success, -1 on failure
 */
int rule_fixup(struct route_data_t * rd) {
	int i,j;
	for (i=0; i<rd->carrier_num; i++) {
		for (j=0; j<rd->carriers[i]->domain_num; j++) {
			if (rd->carriers[i]->domains[j] && rd->carriers[i]->domains[j]->tree) {
				LM_INFO("fixing tree %.*s\n", rd->carriers[i]->domains[j]->name->len, rd->carriers[i]->domains[j]->name->s);
				if (rule_fixup_recursor(rd->carriers[i]->domains[j]->tree) < 0) {
					return -1;
				}
			} else {
				LM_NOTICE("empty tree at [%i][%i]\n", i, j);
			}
		}
	}
	return 0;
}
