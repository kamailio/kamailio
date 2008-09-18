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
 * \file route_rule.c
 * \brief Contains the functions to manage routing rule data.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../ut.h"

#include "carrierroute.h"
#include "route_rule.h"

static int rule_fixup_recursor(struct route_tree_item * rt);

static int fixup_rule_backup(struct route_flags * rf, struct route_rule * rr);

/**
 * Adds a route rule to rf. prefix, rewrite_hostpart, rewrite_local_prefix,
 * rewrite_local_suffix, and comment must not contain NULL pointers.
 *
 * @param rf the current route_flags struct
 * @param prefix the whole scan prefix
 * @param max_targets the number of targets
 * @param prob the weight of the rule
 * @param rewrite_hostpart the rewrite_host of the rule
 * @param strip the strip value of the rule
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
		const str * comment) {
	struct route_rule * shm_rr, * prev = NULL, * tmp = NULL;
	struct route_rule_p_list * t_rl;
	int * t_bu;

	if (max_targets) {
		rf->max_targets = max_targets;
	} else {
		rf->max_targets++;
	}

	if ((shm_rr = shm_malloc(sizeof(struct route_rule))) == NULL) {
		LM_ERR("out of shared memory\n");
		return -1;
	}
	memset(shm_rr, 0, sizeof(struct route_rule));

	if (shm_str_dup(&shm_rr->host, rewrite_hostpart) != 0) {
		goto mem_error;
	}

	if (shm_str_dup(&shm_rr->prefix, prefix) != 0) {
		goto mem_error;
 	}

	shm_rr->strip = strip;

	if (shm_str_dup(&shm_rr->local_prefix, rewrite_local_prefix) != 0) {
		goto mem_error;
	}

	if (shm_str_dup(&shm_rr->local_suffix, rewrite_local_suffix) != 0) {
		goto mem_error;
	}

	if (shm_str_dup(&shm_rr->comment, comment) != 0) {
		goto mem_error;
	}

	shm_rr->status = status;
	shm_rr->hash_index = hash_index;
	shm_rr->orig_prob = prob;
	if (shm_rr->status || backup != -1) {
		shm_rr->prob = prob;
	}	else {
	    shm_rr->prob = 0;
	}
	if (backup >= 0) {
		if ((shm_rr->backup = shm_malloc(sizeof(struct route_rule_p_list))) == NULL) {
			goto mem_error;
		}
		memset(shm_rr->backup, 0, sizeof(struct route_rule_p_list));
		shm_rr->backup->hash_index = backup;
	}
	shm_rr->backed_up = NULL;
	t_bu = backed_up;
	if(!backed_up){
		LM_INFO("no backed up rules\n");
	}
	while (t_bu && *t_bu != -1) {
		if ((t_rl = shm_malloc(sizeof(struct route_rule_p_list))) == NULL) {
			goto mem_error;
		}
		memset(t_rl, 0, sizeof(struct route_rule_p_list));
		t_rl->hash_index = *t_bu;
		t_rl->next = shm_rr->backed_up;
		shm_rr->backed_up = t_rl;
		t_bu++;
	}

	/* rules with a probability of zero are always at the beginning of the list */
	tmp = rf->rule_list;
	while(tmp && tmp->prob == 0){
		prev = tmp;
		tmp = tmp->next;
	}
	/* rules with prob > 0 are sorted by hash_index */
	while(tmp && (tmp->hash_index < shm_rr->hash_index)){
		prev = tmp;
		tmp = tmp->next;
	}
	if(prev){
		shm_rr->next = prev->next;
		prev->next = shm_rr;
	} else {
		shm_rr->next = rf->rule_list;
		rf->rule_list = shm_rr;
	}

	return 0;

mem_error:
	LM_ERR("out of shared memory\n");
	destroy_route_rule(shm_rr);
	return -1;
}


/**
 * Compares the priority of two failure route rules.
 *
 * @param rr1 first failure rule
 * @param rr2 second failure rule
 *
 * @return 0 if rr1 and rr2 have the same priority, -1 if rr1 has higher priority than rr2, 1 if rr1 has lower priority than rr2.
 *
 * @see add_failure_route_to_tree()
 */
int rule_prio_cmp(struct failure_route_rule *rr1, struct failure_route_rule *rr2) {
	int n1, n2, i;
	
	/* host has highest priority */
	if ((rr1->host.len == 0) && (rr2->host.len > 0)) {
		/* host1 is wildcard -> rr1 has lower priority */
		return 1;
	}
	else if ((rr1->host.len > 0) && (rr2->host.len == 0)) {
		/* host2 is wildcard -> rr1 has higher priority */
		return -1;
	}
	else {
		/* reply_code has second highest priority */
		n1=0;
		n2=0;
		for (i=0; i < rr1->reply_code.len; i++) {
			if (rr1->reply_code.s[i]=='.') n1++;
		}
		for (i=0; i < rr2->reply_code.len; i++) {
			if (rr2->reply_code.s[i]=='.') n2++;
		}
		if (n1 < n2) {
			/* reply_code1 has fewer wildcards -> rr1 has higher priority */
			return -1;
		}
		else if (n1 > n2) {
			/* reply_code1 has more wildcards -> rr1 has lower priority */
			return 1;
		}
		else {
			/* flags have lowest priority */
			if (rr1->mask > rr2->mask) {
				return -1;
			}
			else if (rr1->mask < rr2->mask) {
				return 1;
			}
		}
	}
	
	return 0;
}


/**
 * Adds a failure route rule to rt. prefix, host, reply_code, and comment
 * must not contain NULL pointers.
 *
 * @param failure_tree the current route tree node
 * @param prefix the whole scan prefix
 * @param host the hostname last tried
 * @param reply_code the reply code 
 * @param flags user defined flags
 * @param mask mask for user defined flags
 * @param next_domain continue routing with this domain
 * @param comment a comment for the route rule
 *
 * @return 0 on success, -1 on failure
 *
 * @see add_failure_route_to_tree()
 */
int add_failure_route_rule(struct failure_route_tree_item * failure_tree, const str * prefix,
		const str * host, const str * reply_code, flag_t flags, flag_t mask,
		const int next_domain, const str * comment) {
	struct failure_route_rule *shm_rr, *rr, *prev;
	
	if ((shm_rr = shm_malloc(sizeof(struct failure_route_rule))) == NULL) {
		LM_ERR("out of shared memory\n");
		return -1;
	}
	memset(shm_rr, 0, sizeof(struct failure_route_rule));
	
	if (shm_str_dup(&shm_rr->host, host) != 0) {
		goto mem_error;
	}
	
	if (shm_str_dup(&shm_rr->reply_code, reply_code) != 0) {
		goto mem_error;
	}
	
	shm_rr->flags = flags;
	shm_rr->mask = mask;
	shm_rr->next_domain = next_domain;
	
	if (shm_str_dup(&shm_rr->comment, comment) != 0) {
		goto mem_error;
	}
	
	/* before inserting into list, check priorities! */
	rr=failure_tree->rule_list;
	prev=NULL;
	while ((rr != NULL) && (rule_prio_cmp(shm_rr, rr) > 0)) {
		prev=rr;
		rr=rr->next;
	}
	if(prev){
		shm_rr->next = prev->next;
		prev->next = shm_rr;
	} else {
		shm_rr->next = failure_tree->rule_list;
		failure_tree->rule_list = shm_rr;
	}
	
	return 0;

mem_error:
	LM_ERR("out of shared memory\n");
	destroy_failure_route_rule(shm_rr);
	return -1;
}



/**
 * Fixes the route rules by creating an array for accessing
 * route rules by hash index directly
 *
 * @param rd route data to be fixed
 *
 * @return 0 on success, -1 on failure
 */
int rule_fixup(struct rewrite_data * rd) {
	int i,j;
	for (i=0; i<rd->tree_num; i++) {
		for (j=0; j<rd->carriers[i]->tree_num; j++) {
			if (rd->carriers[i]->trees[j] && rd->carriers[i]->trees[j]->tree) {
				LM_INFO("fixing tree %.*s\n", rd->carriers[i]->trees[j]->name.len, rd->carriers[i]->trees[j]->name.s);
				if (rule_fixup_recursor(rd->carriers[i]->trees[j]->tree) < 0) {
					return -1;
				}
			} else {
				LM_NOTICE("empty tree at [%i][%i]\n", i, j);
			}
		}
	}
	return 0;
}


/**
 * Does the work for rule_fixup recursively.
 * First, it tries to set a pointer the rules with an existing hash index
 * at the marching array index. Afterward, remaining rules are populated
 * with incrementing hash indices.
 *
 * @param rt the route tree node to be fixed up
 *
 * @return 0 on success, -1 on failure
 */
static int rule_fixup_recursor(struct route_tree_item * rt) {
	struct route_rule * rr;
	struct route_flags * rf;
	int i, p_dice, ret = 0;

	for (rf=rt->flag_list; rf!=NULL; rf=rf->next) {
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
		if (rt->nodes[i]) {
			ret += rule_fixup_recursor(rt->nodes[i]);
		}
	}

	return ret;
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


struct route_rule * find_rule_by_hash(struct route_flags * rf, int hash){
	struct route_rule * rr;
	rr = rf->rule_list;
	while(rr){
		if(rr->hash_index == hash){
			return rr;
		}
		rr = rr->next;
	}
	return NULL;
}


struct route_rule * find_rule_by_host(struct route_flags * rf, str * host){
	struct route_rule * rr;
	rr = rf->rule_list;
	while(rr){
		if(str_strcmp(&(rr->host), host) == 0){
			return rr;
		}
		rr = rr->next;
	}
	return NULL;
}

int add_backup_route(struct route_rule * rule, struct route_rule * backup){
	struct route_rule_p_list * tmp = NULL;
	if(!backup->status){
		LM_ERR("desired backup route is inactive\n");
		return -1;
	}
	if((tmp = shm_malloc(sizeof(struct route_rule_p_list))) == NULL) {
		LM_ERR("out of shared memory\n");
		return -1;
	}
	memset(tmp, 0, sizeof(struct route_rule_p_list));
	tmp->hash_index = rule->hash_index;
	tmp->rr = rule;
	tmp->next = backup->backed_up;
	backup->backed_up =  tmp;

	tmp = NULL;
	if((tmp = shm_malloc(sizeof(struct route_rule_p_list))) == NULL) {
		LM_ERR("out of shared memory\n");
		return -1;
	}
	memset(tmp, 0, sizeof(struct route_rule_p_list));
	tmp->hash_index = backup->hash_index;
	tmp->rr = backup;
	rule->backup = tmp;

	if(rule->backed_up){
		tmp = rule->backed_up;
		while(tmp->next) {
			tmp = tmp->next;
		}
		tmp->next = backup->backed_up;
		backup->backed_up = rule->backed_up;
		rule->backed_up = NULL;
	}
	tmp = rule->backup->rr->backed_up;
	while(tmp) {
		tmp->rr->backup->hash_index = rule->backup->hash_index;
		tmp->rr->backup->rr = rule->backup->rr;
		tmp = tmp->next;
	}
	return 0;
}


int remove_backed_up(struct route_rule * rule){
	struct route_rule_p_list * rl, * prev = NULL;
	if(rule->backup) {
		if(rule->backup->rr) {
			rl = rule->backup->rr->backed_up;
			while(rl) {
				if(rl->hash_index == rule->hash_index) {
					if(prev) {
						prev->next = rl->next;
					} else {
						rule->backup->rr->backed_up = rl->next;
					}
					shm_free(rl);
					shm_free(rule->backup);
					rule->backup = NULL;
					return 0;
				}
				prev = rl;
				rl = rl->next;
			}
		}
		return -1;
	}
	return 0;
}


struct route_rule * find_auto_backup(struct route_flags * rf, struct route_rule * rule){
	struct route_rule * rr;
	rr = rf->rule_list;
	while(rr){
		if(!rr->backed_up && (rr->hash_index != rule->hash_index) && rr->status){
			return rr;
		}
		rr = rr->next;
	}
	return NULL;
}

/**
 * Destroys route rule rr by freeing all its memory.
 *
 * @param rr route rule to be destroyed
 */
void destroy_route_rule(struct route_rule * rr) {
	struct route_rule_p_list * t_rl;
	if (rr->host.s) {
		shm_free(rr->host.s);
	}
	if (rr->local_prefix.s) {
		shm_free(rr->local_prefix.s);
	}
	if (rr->local_suffix.s) {
		shm_free(rr->local_suffix.s);
	}
	if (rr->comment.s) {
		shm_free(rr->comment.s);
	}
	if (rr->prefix.s) {
		shm_free(rr->prefix.s);
	}
	if(rr->backup){
		shm_free(rr->backup);
	}
	while(rr->backed_up){
		t_rl = rr->backed_up->next;
		shm_free(rr->backed_up);
		rr->backed_up = t_rl;
	}
	shm_free(rr);
	return;
}


/**
 * Destroys failure route rule rr by freeing all its memory.
 *
 * @param rr route rule to be destroyed
 */
void destroy_failure_route_rule(struct failure_route_rule * rr) {
	if (rr->host.s) {
		shm_free(rr->host.s);
	}
	if (rr->comment.s) {
		shm_free(rr->comment.s);
	}
	if (rr->prefix.s) {
		shm_free(rr->prefix.s);
	}
	if (rr->reply_code.s) {
		shm_free(rr->reply_code.s);
	}
	shm_free(rr);
	return;
}
