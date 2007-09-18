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

#include "../../mem/shm_mem.h"
#include "../../dprint.h"

#include "carrierroute.h"
#include "route_rule.h"

static int rule_fixup_recursor(struct route_tree_item * rt);

static int fixup_rule_backup(struct route_tree_item * rt, struct route_rule * rr);

/**
 * Adds a route rule to rt
 *
 * @param rt the current route tree node
 * @param full_prefix the whole scan prefix
 * @param max_locdb the number of locdbs
 * @param prob the weight of the rule
 * @param rewrite_hostpart the rewrite_host of the rule
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
int add_route_rule(struct route_tree_item * route_tree, const char * prefix,
                   int max_locdb, double prob, const char * rewrite_hostpart, int strip,
                   const char * rewrite_local_prefix, const char * rewrite_local_suffix,
                   int status, int hash_index, int backup, int * backed_up,
                   const char * comment) {
	struct route_rule * shm_rr;
	struct route_rule_p_list * t_rl;
	int * t_bu;

	if (max_locdb) {
		route_tree->max_locdb = max_locdb;
	} else {
		route_tree->max_locdb++;
	}

	if ((shm_rr = shm_malloc(sizeof(struct route_rule))) == NULL) {
		LM_ERR("out of shared memory\n");
		return -1;
	}
	memset(shm_rr, 0, sizeof(struct route_rule));

	if (rewrite_hostpart) {
		if ((shm_rr->host.s = shm_malloc(strlen(rewrite_hostpart) + 1)) == NULL) {
			LM_ERR("out of shared memory\n");
			destroy_route_rule(shm_rr);
			return -1;
		}
		strcpy(shm_rr->host.s, rewrite_hostpart);
		shm_rr->host.len = strlen(rewrite_hostpart);
	}

	if (prefix) {
		if ((shm_rr->prefix.s = shm_malloc(strlen(prefix) + 1)) == NULL) {
			LM_ERR("out of shared memory\n");
			destroy_route_rule(shm_rr);
			return -1;
		}
		strcpy(shm_rr->prefix.s, prefix);
		shm_rr->prefix.len = strlen(prefix);
	}

	shm_rr->strip = strip;

	if (rewrite_local_prefix) {
		if ((shm_rr->local_prefix.s = shm_malloc(strlen(rewrite_local_prefix) + 1)) == NULL) {
			LM_ERR("out of shared memory\n");
			destroy_route_rule(shm_rr);
			return -1;
		}
		strcpy(shm_rr->local_prefix.s, rewrite_local_prefix);
		shm_rr->local_prefix.len = strlen(rewrite_local_prefix);
	}

	if (rewrite_local_suffix) {
		if ((shm_rr->local_suffix.s = shm_malloc(strlen(rewrite_local_suffix) + 1)) == NULL) {
			LM_ERR("out of shared memory\n");
			destroy_route_rule(shm_rr);
			return -1;
		}
		strcpy(shm_rr->local_suffix.s, rewrite_local_suffix);
		shm_rr->local_suffix.len = strlen(rewrite_local_suffix);
	}

	if (comment) {
		if ((shm_rr->comment.s = shm_malloc(strlen(comment) + 1)) == NULL) {
			LM_ERR("out of shared memory\n");
			destroy_route_rule(shm_rr);
			return -1;
		}
		strcpy(shm_rr->comment.s, comment);
		shm_rr->comment.len = strlen(comment);
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
			LM_ERR("out of shared memory\n");
			destroy_route_rule(shm_rr);
			return -1;
		}
		memset(shm_rr->backup, 0, sizeof(struct route_rule_p_list));
		shm_rr->backup->hash_index = backup;
	}
	shm_rr->backed_up = NULL;
	t_bu = backed_up;
	if(!backed_up){
		LM_ERR("no backed up rules\n");
	}
	while (t_bu && *t_bu != -1) {
		if ((t_rl = shm_malloc(sizeof(struct route_rule_p_list))) == NULL) {
			LM_ERR("out of shared memory\n");
			destroy_route_rule(shm_rr);
			return -1;
		}
		memset(t_rl, 0, sizeof(struct route_rule_p_list));
		t_rl->hash_index = *t_bu;
		t_rl->next = shm_rr->backed_up;
		shm_rr->backed_up = t_rl;
		t_bu++;
	}

	shm_rr->next = route_tree->rule_list;
	route_tree->rule_list = shm_rr;

	return 0;
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
				LM_NOTICE("fixing tree %.*s\n", rd->carriers[i]->trees[j]->name.len, rd->carriers[i]->trees[j]->name.s);
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
	int i;
	int ret = 0;
	int p_dice = 0;
	if (rt->rule_list) {
		rr = rt->rule_list;
		rt->rule_num = 0;
		while (rr) {
			rt->rule_num++;
			rt->dice_max += rr->prob * DICE_MAX;
			rr = rr->next;
		}
		rr = rt->rule_list;
		while (rr) {
			rr->dice_to = (rr->prob * DICE_MAX) + p_dice;
			p_dice = rr->dice_to;
			rr = rr->next;
		}

		if (rt->rule_num != rt->max_locdb) {
			LM_ERR("number of rules(%i) differs from max_locdb(%i), maybe your config is wrong?\n", rt->rule_num, rt->max_locdb);
			return -1;
		}
		if(rt->rules){
			shm_free(rt->rules);
            rt->rules = NULL;
		}
		if ((rt->rules = shm_malloc(sizeof(struct route_rule *) * rt->rule_num)) == NULL) {
			LM_ERR("out of shared memory\n");
			return -1;
		}
		memset(rt->rules, 0, sizeof(struct route_rule *) * rt->rule_num);
		for (rr = rt->rule_list; rr; rr = rr->next) {
			if (rr->hash_index) {
				if (rr->hash_index > rt->rule_num) {
					LM_ERR("too large hash index %i, max is %i\n", rr->hash_index, rt->rule_num);
					shm_free(rt->rules);
					return -1;
				}
				if (rt->rules[rr->hash_index - 1]) {
					LM_ERR("duplicate hash index %i\n", rr->hash_index);
					shm_free(rt->rules);
					return -1;
				}
				rt->rules[rr->hash_index - 1] = rr;
				LM_ERR("rule with host %.*s hash has hashindex %i.\n", rr->host.len, rr->host.s, rr->hash_index);
			}
		}

		rr = rt->rule_list;
		i=0;
		while (rr && i < rt->rule_num) {
			if (!rr->hash_index) {
				if (rt->rules[i]) {
					i++;
				} else {
					rt->rules[i] = rr;
					rr->hash_index = i + 1;
					LM_ERR("hashless rule with host %.*s hash hash_index %i\n", rr->host.len, rr->host.s, i+1);
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
		for(i=0; i<rt->rule_num; i++){
			ret += fixup_rule_backup(rt, rt->rules[i]);
		}
	}
	for (i=0; i<10; i++) {
		if (rt->nodes[i]) {
			ret += rule_fixup_recursor(rt->nodes[i]);
		}
	}
	return ret;
}

static int fixup_rule_backup(struct route_tree_item * rt, struct route_rule * rr){
	struct route_rule_p_list * rl;
	if(!rr->status && rr->backup){
		if((rr->backup->rr = find_rule_by_hash(rt, rr->backup->hash_index)) == NULL){
			LM_ERR("didn't find backup route\n");
			return -1;
		}
	}
	rl = rr->backed_up;
	while(rl){
		if((rl->rr = find_rule_by_hash(rt, rl->hash_index)) == NULL){
			LM_ERR("didn't find backed up route\n");
			return -1;
		}
		rl = rl->next;
	}
	return 0;
}


struct route_rule * find_rule_by_hash(struct route_tree_item * rt, int hash){
	struct route_rule * rr;
	rr = rt->rule_list;
	while(rr){
		if(rr->hash_index == hash){
			return rr;
		}
		rr = rr->next;
	}
	return NULL;
}


struct route_rule * find_rule_by_host(struct route_tree_item * rt, str * host){
	struct route_rule * rr;
	rr = rt->rule_list;
	while(rr){
		if(strcmp(rr->host.s, host->s) == 0){
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
		while(tmp->next){
			tmp = tmp->next;
		}
		tmp->next = backup->backed_up;
		backup->backed_up = rule->backed_up;
		rule->backed_up = NULL;
	}
	tmp = rule->backup->rr->backed_up;
	while(tmp){
		tmp->rr->backup->hash_index = rule->backup->hash_index;
		tmp->rr->backup->rr = rule->backup->rr;
		tmp = tmp->next;
	}
	return 0;
}


int remove_backed_up(struct route_rule * rule){
	struct route_rule_p_list * rl, * prev = NULL;
	if(rule->backup){
		if(rule->backup->rr){
			rl = rule->backup->rr->backed_up;
			while(rl){
				if(rl->hash_index == rule->hash_index){
					if(prev){
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


struct route_rule * find_auto_backup(struct route_tree_item * rt, struct route_rule * rule){
	struct route_rule * rr;
	rr = rt->rule_list;
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
