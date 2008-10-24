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
 * \file cr_carrier.c
 * \brief Contains the functions to manage carrier data.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "cr_carrier.h"
#include "carrierroute.h"
#include "cr_rule.h"
#include "cr_config.h"
#include "cr_db.h"
#include "cr_map.h"
#include "cr_domain.h"


/**
 * Adds the given route information to the routing domain identified by
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
		const str * domain, const str * scan_prefix, flag_t flags, flag_t mask, int max_targets,
		double prob, const str * rewrite_hostpart, int strip,
		const str * rewrite_local_prefix, const str * rewrite_local_suffix,
		int status, int hash_index, int backup, int * backed_up, const str * comment) {
	struct carrier_data_t * carrier_data = NULL;
	struct domain_data_t * domain_data = NULL;
	LM_INFO("adding prefix %.*s, prob %f\n", scan_prefix->len, scan_prefix->s, prob);

	if ((carrier_data = get_carrier_data(rd, carrier_id)) == NULL) {
		LM_ERR("could not retrieve carrier data\n");
		return -1;
	}

	if ((domain_data = get_domain_data_by_name(carrier_data,domain)) == NULL) {
		LM_ERR("could not retrieve domain data\n");
		return -1;
	}
	LM_INFO("found route, now adding\n");
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
 * @param domain the routing domain of the new route
 * @param scan_prefix the number prefix
 * @param host the hostname last tried
 * @param reply_code the reply code 
 * @param flags user defined flags
 * @param mask for user defined flags
 * @param next_domain continue routing with this domain
 * @param comment a comment for the failure route rule
 *
 * @return 0 on success, -1 on error in which case it LOGs a message.
 */
int add_failure_route(struct route_data_t * rd, int carrier_id, const str * domain,
		const str * scan_prefix, const str * host, const str * reply_code,
		flag_t flags, flag_t mask, const str * next_domain, const str * comment) {
	int next_domain_id;
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
	
	if ((domain_data = get_domain_data_by_name(carrier_data, domain)) == NULL) {
		LM_ERR("could not retrieve domain data\n");
		return -1;
	}

	if ((next_domain_id = add_domain(next_domain)) < 0) {
		LM_ERR("add_domain failed\n");
		return -1;
	}
	
	LM_INFO("found failure route, now adding\n");
	return add_failure_route_to_tree(domain_data->failure_tree, scan_prefix, scan_prefix, host, reply_code,
			flags, mask, next_domain_id, comment);
}


/**
 * adds a carrier_data struct for given carrier
 *
 * @param rd route data to be searched
 * @param carrier the name of desired carrier
 * @param carrier_id the id of the carrier
 * @param domains number of domains for that carrier
 *
 * @return a pointer to the root node of the desired routing tree,
 * NULL on failure
 */
struct carrier_data_t * add_carrier_data(struct route_data_t * rd, const str * carrier, int carrier_id, int domains) {
	int i, index;
	if (!rd) {
		LM_ERR("NULL pointer in parameter\n");
		return NULL;
	}
	LM_INFO("add carrier %.*s\n", carrier->len, carrier->s);
	for (i=0; i<rd->carrier_num; i++) {
		if (rd->carriers[i]) {
			if (rd->carriers[i]->id == carrier_id) {
				LM_INFO("found carrier %i: %.*s\n", rd->carriers[i]->id, rd->carriers[i]->name.len, rd->carriers[i]->name.s);
				return rd->carriers[i];
			}
		}
	}
	LM_INFO("carrier %.*s not found, add it\n", carrier->len, carrier->s);
	if ((index = add_carrier(carrier, carrier_id)) < 0) {
		LM_ERR("could not add carrier\n");
		return NULL;
	}
	if (index > rd->carrier_num) {
		LM_ERR("weird: to large tree index\n");
		return NULL;
	}
	if ((rd->carriers[index] = create_carrier_data(carrier, carrier_id, index, domains)) == NULL) {
		return NULL;
	}
	rd->carriers[index]->index = index;
	LM_INFO("created carrier data: %.*s, with id %i and %ld domains\n", 
		rd->carriers[index]->name.len, rd->carriers[index]->name.s, rd->carriers[index]->id, 
		(long)rd->carriers[index]->domain_num);
	return rd->carriers[index];
}


/**
 * Create a new carrier_data struct in shared memory and set it up.
 *
 * @param carrier_name the name of the carrier
 * @param carrier_id id of carrier
 * @param index the index for that carrier
 * @param domains number of domains for that carrier
 *
 * @return a pointer to the newly allocated carrier data or NULL on
 * error, in which case it LOGs an error message.
 */
struct carrier_data_t * create_carrier_data(const str *carrier_name, int carrier_id, int index, int domains) {
	struct carrier_data_t * tmp;
	if ((tmp = shm_malloc(sizeof(struct carrier_data_t))) == NULL) {
		LM_ERR("out of shared memory\n");
		return NULL;
	}
	memset(tmp, 0, sizeof(struct carrier_data_t));
	if (shm_str_dup(&tmp->name, carrier_name)!=0) {
		LM_ERR("cannot duplicate string\n");
		shm_free(tmp);
		return NULL;
	}
	tmp->id = carrier_id;
	tmp->index = index;
	tmp->domain_num = domains;
	if(domains > 0){
		if ((tmp->domains = shm_malloc(sizeof(struct domain_data_t *) * domains)) == NULL) {
			LM_ERR("out of shared memory\n");
			shm_free(tmp->name.s);
			shm_free(tmp);
			return NULL;
		}
		memset(tmp->domains, 0, sizeof(struct domain_data_t *) * domains);
	}
	return tmp;
}


/**
 * returns the routing tree for the given domain, if domain's tree
 * doesnt exist, it will be created. If the trees are completely
 * filled and a not existing domain shall be added, an error is
 * returned
 *
 * @param rd route data to be searched
 * @param carrier_id the id of the desired carrier
 *
 * @return a pointer to the root node of the desired routing tree,
 * NULL on failure
 */
struct carrier_data_t *get_carrier_data(struct route_data_t * rd, int carrier_id) {
	int i;
	if (!rd) {
		LM_ERR("NULL pointer in parameter\n");
		return NULL;
	}
	for (i=0; i<rd->carrier_num; i++) {
		if (rd->carriers[i]->id == carrier_id) {
			return rd->carriers[i];
		}
	}
	return NULL;
}


static int add_domain_data(struct carrier_data_t * carrier_data, struct domain_data_t * domain_data) {
	int i;
	LM_INFO("tree %.*s has %ld trees\n",
			carrier_data->name.len, carrier_data->name.s, (long)carrier_data->domain_num);
	for (i=0; i<carrier_data->domain_num; i++) {
		LM_DBG("tree %p", carrier_data->domains[i]);
		if (carrier_data->domains[i] == 0) {
			carrier_data->domains[i] = domain_data;
			return 0;
		}
	}
	return -1;
}


/**
 * Returns the domain data for the given name. If it doesnt exist,
 * it will be created. If the domain list is completely
 * filled and a not existing domain shall be added, an error is
 * returned
 *
 * @param carrier_data carrier data to be searched
 * @param domain the name of desired domain
 *
 * @return a pointer to the desired domain data, NULL on failure.
 */
struct domain_data_t *get_domain_data_by_name(struct carrier_data_t * carrier_data, const str * domain) {
	int i, id;
	struct domain_data_t * domain_data = NULL;
	if (!carrier_data) {
		LM_ERR("NULL pointer in parameter\n");
		return NULL;
	}
	for (i=0; i<carrier_data->domain_num; i++) {
		if (carrier_data->domains[i] && carrier_data->domains[i]->name.s) {
			if (str_strcmp(&carrier_data->domains[i]->name, domain) == 0) {
				LM_INFO("found domain %.*s\n", carrier_data->domains[i]->name.len, carrier_data->domains[i]->name.s);
				return carrier_data->domains[i];
			}
		}
	}
	LM_INFO("domain %.*s not found, add it\n", domain->len, domain->s);
	if ((id = add_domain(domain)) < 0) {
		LM_ERR("could not add domain\n");
		return NULL;
	}
	if ((domain_data = create_domain_data(domain, id)) == NULL) {
		return NULL;
	}
	if (add_domain_data(carrier_data, domain_data) < 0) {
		LM_ERR("couldn't add domain data\n");
		destroy_domain_data(domain_data);
		return NULL;
	}
	LM_INFO("created domain data: %.*s, with id %i\n", domain_data->name.len, domain_data->name.s, domain_data->id);
	return domain_data;
}


/**
 * Returns the domain data for the given id.
 *
 * @param carrier_data carrier data to be searched
 * @param domain the name of desired domain
 *
 * @return a pointer to the desired domain data, NULL if not found.
 */
struct domain_data_t * get_domain_data_by_id(struct carrier_data_t * carrier_data, int id) {
	int i;
	LM_DBG("searching in carrier %.*s, id %d\n", carrier_data->name.len, carrier_data->name.s, carrier_data->id);
	for (i=0; i<carrier_data->domain_num; i++) {
		if (carrier_data->domains[i]) {
			LM_DBG("tree %.*s, domain %.*s : %i\n", carrier_data->name.len, carrier_data->name.s, carrier_data->domains[i]->name.len, carrier_data->domains[i]->name.s, carrier_data->domains[i]->id);
			if (carrier_data->domains[i]->id == id) {
				return carrier_data->domains[i];
			}
		}
	}
	return NULL;
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
				LM_ERR("out of shared memory\n");
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
						LM_INFO("hashless rule with host %.*s hash hash_index %i\n", rr->host.len, rr->host.s, i+1);
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

	for (i=0; i<10; i++) {
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
				LM_INFO("fixing tree %.*s\n", rd->carriers[i]->domains[j]->name.len, rd->carriers[i]->domains[j]->name.s);
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
