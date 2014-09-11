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
 * \file cr_rule.c
 * \brief Contains the functions to manage routing rule data.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include "../../ut.h"
#include "cr_rule.h"


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
		SHM_MEM_ERROR;
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
	SHM_MEM_ERROR;
	destroy_route_rule(shm_rr);
	return -1;
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
 * Try to find a matching route_flags struct in rt and return it, add it if not found.
 *
 * @param rf_head pointer to the head of the route flags list, might be changed during insert.
 * @param flags user defined flags
 * @param mask mask for user defined flags
 *
 * @return pointer to the route_flags struct on success, NULL on failure.
 *
 */
struct route_flags * add_route_flags(struct route_flags **rf_head, const flag_t flags, const flag_t mask)
{
	struct route_flags *shm_rf;
	struct route_flags *prev_rf, *tmp_rf;
	prev_rf = tmp_rf = NULL;

	if (rf_head) {
		/* search for matching route_flags struct */
		for (tmp_rf=*rf_head; tmp_rf!=NULL; tmp_rf=tmp_rf->next) {
			if ((tmp_rf->flags == flags) && (tmp_rf->mask == mask)) return tmp_rf;
		}
		
		/* not found, insert one */
		for (tmp_rf=*rf_head; tmp_rf!=NULL; tmp_rf=tmp_rf->next) {
			if (tmp_rf->mask < mask) break;
			prev_rf=tmp_rf;
		}
	}

	if ((shm_rf = shm_malloc(sizeof(struct route_flags))) == NULL) {
		SHM_MEM_ERROR;
		return NULL;
	}
	memset(shm_rf, 0, sizeof(struct route_flags));

	shm_rf->flags=flags;
	shm_rf->mask=mask;
	shm_rf->next=tmp_rf;
	
	if (prev_rf) {
		prev_rf->next = shm_rf;
	}
	else {
		if (rf_head) *rf_head=shm_rf;
	}

	return shm_rf;
}


/**
 * Destroys route_flags in shared memory by freing all its memory.
 *
 * @param rf route_flags struct to be destroyed
 */
void destroy_route_flags(struct route_flags *rf) {
	struct route_rule *rs, *rs_tmp;

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
 * Compares the priority of two failure route rules.
 *
 * @param frr1 first failure rule
 * @param frr2 second failure rule
 *
 * @return 0 if frr1 and frr2 have the same priority, -1 if frr1 has higher priority than frr2, 1 if frr1 has lower priority than frr2.
 *
 * @see add_failure_route_to_tree()
 */
static int failure_rule_prio_cmp(struct failure_route_rule *frr1, struct failure_route_rule *frr2) {
	int n1, n2, i;
	
	/* host has highest priority */
	if ((frr1->host.len == 0) && (frr2->host.len > 0)) {
		/* host1 is wildcard -> frr1 has lower priority */
		return 1;
	}
	else if ((frr1->host.len > 0) && (frr2->host.len == 0)) {
		/* host2 is wildcard -> frr1 has higher priority */
		return -1;
	}
	else {
		/* reply_code has second highest priority */
		n1=0;
		n2=0;
		for (i=0; i < frr1->reply_code.len; i++) {
			if (frr1->reply_code.s[i]=='.') n1++;
		}
		for (i=0; i < frr2->reply_code.len; i++) {
			if (frr2->reply_code.s[i]=='.') n2++;
		}
		if (n1 < n2) {
			/* reply_code1 has fewer wildcards -> frr1 has higher priority */
			return -1;
		}
		else if (n1 > n2) {
			/* reply_code1 has more wildcards -> frr1 has lower priority */
			return 1;
		}
		else {
			/* flags have lowest priority */
			if (frr1->mask > frr2->mask) {
				return -1;
			}
			else if (frr1->mask < frr2->mask) {
				return 1;
			}
		}
	}
	
	return 0;
}


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
		flag_t flags, flag_t mask, const int next_domain, const str * comment) {
	struct failure_route_rule *shm_frr, *frr, *prev;
	frr = prev = NULL;
	
	if ((shm_frr = shm_malloc(sizeof(struct failure_route_rule))) == NULL) {
		SHM_MEM_ERROR;
		return NULL;
	}
	memset(shm_frr, 0, sizeof(struct failure_route_rule));
	
	if (shm_str_dup(&shm_frr->host, host) != 0) {
		goto mem_error;
	}
	
	if (shm_str_dup(&shm_frr->reply_code, reply_code) != 0) {
		goto mem_error;
	}
	
	shm_frr->flags = flags;
	shm_frr->mask = mask;
	shm_frr->next_domain = next_domain;
	
	if (shm_str_dup(&shm_frr->comment, comment) != 0) {
		goto mem_error;
	}
	
	/* before inserting into list, check priorities! */
	if (frr_head) {
		frr=*frr_head;
		prev=NULL;
		while ((frr != NULL) && (failure_rule_prio_cmp(shm_frr, frr) > 0)) {
			prev=frr;
			frr=frr->next;
		}
	}

	shm_frr->next = frr;

	if(prev){
		prev->next = shm_frr;
	}
	else {
		if (frr_head) *frr_head=shm_frr;
	}

	return shm_frr;
	
mem_error:
	SHM_MEM_ERROR;
	destroy_failure_route_rule(shm_frr);
	return NULL;
}


/**
 * Destroys failure route rule frr by freeing all its memory.
 *
 * @param frr route rule to be destroyed
 */
void destroy_failure_route_rule(struct failure_route_rule * frr) {
	if (frr->host.s) {
		shm_free(frr->host.s);
	}
	if (frr->comment.s) {
		shm_free(frr->comment.s);
	}
	if (frr->prefix.s) {
		shm_free(frr->prefix.s);
	}
	if (frr->reply_code.s) {
		shm_free(frr->reply_code.s);
	}
	shm_free(frr);
	return;
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


int add_backup_rule(struct route_rule * rule, struct route_rule * backup){
	struct route_rule_p_list * tmp = NULL;
	if(!backup->status){
		LM_ERR("desired backup route is inactive\n");
		return -1;
	}
	if((tmp = shm_malloc(sizeof(struct route_rule_p_list))) == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(tmp, 0, sizeof(struct route_rule_p_list));
	tmp->hash_index = rule->hash_index;
	tmp->rr = rule;
	tmp->next = backup->backed_up;
	backup->backed_up =  tmp;

	tmp = NULL;
	if((tmp = shm_malloc(sizeof(struct route_rule_p_list))) == NULL) {
		SHM_MEM_ERROR;
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
