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
 * @file carrier_tree.c
 *
* @author: Jonas Appel <jonas dot appel at schlund dot de>
 *
 * @date Mo May 21 2007
 *
 * Copyright: 2007 1 & 1 Internet AG
 *
 * @brief contains the functions to manage carrier
 * tree data
 *
 */

#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "carrier_tree.h"
#include "carrierroute.h"
#include "route_tree.h"
#include "route_rule.h"
#include "load_data.h"

/**
 * points to the data loading function
 */
route_data_load_func_t load_data;

/**
 * Pointer to the routing data.
 */
struct rewrite_data ** global_data = NULL;

/**
 * holds the map between routing tree names and numbers
 */
struct tree_map ** script_trees = NULL;

static int carrier_tree_fixup(struct rewrite_data * rd);

/**
 * initialises the routing data, i.e. it binds the data loader
 * initialises the global data pointer
 *
 * @param source data source, can be db or file
 *
 * @return 0 on success, -1 on failure
 */
int init_route_data(const char * source) {
	if (global_data == NULL) {
		global_data = (struct rewrite_data **)
		              shm_malloc(sizeof(struct rewrite_data *));
		if (global_data == NULL) {
			LM_ERR("Out of shared memory before even "
			    "doing anything.\n");
			return -1;
		}
	}
	*global_data = NULL;
	return bind_data_loader(source, &load_data);
}

/**
 * Loads the routing data into the routing tree and sets the
 * global_data pointer to the new data. The old_data is removed
 * when it is not locked anymore.
 *
 * @return 0 on success, -1 on failure
 */
int prepare_route_tree(void) {
	struct rewrite_data * old_data;
	struct rewrite_data * new_data = NULL;
	int i;

	if ((new_data = shm_malloc(sizeof(struct rewrite_data))) == NULL) {
		LM_ERR("out of shared memory\n");
		return -1;
	}
	memset(new_data, 0, sizeof(struct rewrite_data));

	if (!load_data || load_data(new_data) < 0) {
		LM_ERR("could not load routing data\n");
		return -1;
	}
	if (new_data == NULL) {
		return -1;
	}

	if (rule_fixup(new_data) < 0) {
		LM_ERR("could not fixup rules\n");
		return -1;
	}

	if (carrier_tree_fixup(new_data) < 0){
		LM_ERR("could not fixup trees\n");
		return -1;
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
		destroy_rewrite_data(old_data);
	}
	return 0;
}

/**
 * Increases lock counter and returns a pointer to the
 * current routing data
 *
 * @return pointer to the global routing data on success,
 * NULL on failure
*/
struct rewrite_data * get_data(void) {
	struct rewrite_data *ret;
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
void release_data(struct rewrite_data *data) {
	lock_get(&data->lock);
	--data->proc_cnt;
	lock_release(&data->lock);
}

int find_tree(str tree){
	struct tree_map * tmp;
	if (!script_trees){
		return -1;
	}
	if (tree.len <= 0) {
		return -1;
	}
	tmp = *script_trees;

	while (tmp) {
		if (str_strcmp(&tree, &tmp->name) == 0) {
			/*
			we return the number of the tree instead of the id, because the
			user could choose the id randomly, this would lead to a not
			optimal datastructure, e.g. for id=10000. So we're working 
			internaly with this id instead of the id from the database.
			*/
			return tmp->no;
		}
		tmp = tmp->next;
	}
	return -1;
}

/**
 * Adds the given route information to the route tree identified by
 * domain. scan_prefix identifies the number for which the information
 * is and the rewrite_* parameters define what to do in case of a match.
 * prob gives the probability with which this rule applies if there are
 * more than one for a given prefix.
 *
 * @param rd the route data to which the route shall be added
 * @param carrier_id the carrier id of the route to be added
 * @param domain the routing domain of the new route
 * @param scan_prefix the number prefix
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
int add_route(struct rewrite_data * rd, int carrier_id,
              const char * domain, const char * scan_prefix, int max_targets,
              double prob, const char * rewrite_hostpart, int strip,
              const char * rewrite_local_prefix, const char * rewrite_local_suffix,
              int status, int hash_index, int backup, int * backed_up, const char * comment) {
	struct carrier_tree * ct = NULL;
	struct route_tree_item * rt = NULL;
	LM_NOTICE("adding prefix %s, prob %f\n", scan_prefix, prob);

	if ((ct = get_carrier_tree(carrier_id, rd)) == NULL) {
		LM_ERR("could not retrieve carrier tree\n");
		return -1;
	}

	if ((rt = get_route_tree(domain, ct)) == NULL) {
		LM_ERR("could not retrieve route tree\n");
		return -1;
	}
	LM_INFO("found route, now adding\n");
	return add_route_to_tree(rt, scan_prefix, scan_prefix, max_targets, prob, rewrite_hostpart,
	                         strip, rewrite_local_prefix, rewrite_local_suffix, status,
	                         hash_index, backup, backed_up, comment);
}

/**
 * adds a carrier tree for the given carrier
 *
 * @param tree the carrier name of desired routing tree
 * @param carrier_id the id of the carrier
 * @param rd route data to be searched
 *
 * @return a pointer to the root node of the desired routing tree,
 * NULL on failure
 */
struct carrier_tree * add_carrier_tree(const char * carrier, int carrier_id, struct rewrite_data * rd, int trees) {
	int i, id;
	if (!rd) {
		LM_ERR("NULL-pointer in parameter\n");
		return NULL;
	}
	LM_INFO("add carrier %s\n", carrier);
	for (i=0; i<rd->tree_num; i++) {
		if (rd->carriers[i]) {
			if (rd->carriers[i]->id == carrier_id) {
				LM_INFO("found carrier %i: %s\n", rd->carriers[i]->id, rd->carriers[i]->name.s);
				return rd->carriers[i];
			}
		}
	}
	LM_INFO("carrier %s not found, add it\n", carrier);
	if ((id = add_tree(carrier, carrier_id)) < 0) {
		LM_ERR("could not add tree\n");
		return NULL;
	}
	if (id > rd->tree_num) {
		LM_ERR("weird: to large tree id\n");
		return NULL;
	}
	if ((rd->carriers[id] = create_carrier_tree(carrier, carrier_id, id, trees)) == NULL) {
		return NULL;
	}
	rd->carriers[id]->index = id;
	LM_INFO("created carrier tree: %s, with id %i and %i trees\n", rd->carriers[id]->name.s, rd->carriers[id]->id, rd->carriers[id]->tree_num);
	return rd->carriers[id];
}

/**
 * Create a new carrier tree in shared memory and set it up.
 *
 * @param tree the name of the carrier tree
 * @param id the domain id of the carrier tree
 *
 * @return a pointer to the newly allocated route tree or NULL on
 * error, in which case it LOGs an error message.
 */
struct carrier_tree * create_carrier_tree(const char * tree, int carrier_id, int id, int trees) {
	struct carrier_tree * tmp;
	if ((tmp = shm_malloc(sizeof(struct carrier_tree))) == NULL) {
		LM_ERR("out of shared memory\n");
		return NULL;
	}
	memset(tmp, 0, sizeof(struct carrier_tree));
	if ((tmp->name.s = shm_malloc(strlen(tree) + 1)) == NULL) {
		LM_ERR("out of shared memory\n");
		shm_free(tmp);
		return NULL;
	}
	memset(tmp->name.s, 0, strlen(tree) + 1);
	strcpy(tmp->name.s, tree);
	tmp->name.len = strlen(tree);
	tmp->id = carrier_id;
	tmp->index = id;
	tmp->tree_num = trees;
	if(trees > 0){
		if ((tmp->trees = shm_malloc(sizeof(struct route_tree *) * trees)) == NULL) {
			LM_ERR("out of shared memory\n");
			shm_free(tmp->name.s);
			shm_free(tmp);
			return NULL;
		}
		memset(tmp->trees, 0, sizeof(struct route_tree *) * trees);
	}
	return tmp;
}


/**
 * returns the routing tree for the given domain, if domain's tree
 * doesnt exist, it will be created. If the trees are completely
 * filled and a not existing domain shall be added, an error is
 * returned
 *
 * @param domain the id of the desired carrier tree
 * @param rd route data to be searched
 *
 * @return a pointer to the root node of the desired routing tree,
 * NULL on failure
 */
struct carrier_tree * get_carrier_tree(int carrier_id, struct rewrite_data * rd) {
	int i;
	if (!rd) {
		LM_ERR("NULL-pointer in parameter\n");
		return NULL;
	}
	for (i=0; i<rd->tree_num; i++) {
		if (rd->carriers[i]->id == carrier_id) {
			return rd->carriers[i];
		}
	}
	return NULL;
}

/**
 * Tries to add a tree to the tree map. If the given tree doesn't
 * exist, it is added. Otherwise, nothing happens.
 *
 * @param tree the tree to be added
 *
 * @return values: on succcess the numerical index of the given tree,
 * -1 on failure
 */
int add_tree(const char * tree, int carrier_id) {
	struct tree_map * tmp, * prev = NULL;
	int id = 0;
	if (!script_trees) {
		if ((script_trees = shm_malloc(sizeof(struct tree_map *))) == NULL) {
			LM_ERR("out of shared memory\n");
			return -1;
		}
		*script_trees = NULL;
	}
	tmp = *script_trees;

	while (tmp) {
		if (carrier_id == tmp->id) {
			return tmp->no;
		}
		id = tmp->no + 1;
		prev = tmp;
		tmp = tmp->next;
	}
	if ((tmp = shm_malloc(sizeof(struct tree_map))) == NULL) {
		LM_ERR("out of shared memory\n");
		return -1;
	}
	memset(tmp, 0, sizeof(struct tree_map));
	if ((tmp->name.s = shm_malloc(strlen(tree) + 1)) == NULL) {
		LM_ERR("out of shared memory\n");
		shm_free(tmp);
		return -1;
	}
	strcpy(tmp->name.s, tree);
	tmp->name.len = strlen(tmp->name.s);
	tmp->no = id;
	tmp->id = carrier_id;
	if (!prev) {
		*script_trees = tmp;
	} else {
		prev->next = tmp;
	}
	LM_INFO("tree %s has internal id %i\n", tree, id);
	return id;
}

void destroy_route_data(void){
	struct rewrite_data * rd = get_data();
	struct tree_map * tmp3, * tmp4;
	destroy_rewrite_data(rd);
	destroy_route_map();
	if(script_trees){
		tmp3 = *script_trees;
		while(tmp3){
			tmp4 = tmp3;
			tmp3 = tmp3->next;
			shm_free(tmp4);
		}
		shm_free(script_trees);
		script_trees = NULL;
	}
	if(global_data){
		*global_data = NULL;
	}
	global_data = NULL;
}

/**
 * Destroys a carrier tree
 *
 * @param tree route data to be destroyed
 */
void destroy_carrier_tree(struct carrier_tree * tree) {
	int i;

	if (tree == NULL) {
		return;
	}
	if (tree->trees != NULL) {
		for (i = 0; i < tree->tree_num; ++i) {
			if (tree->trees[i] != NULL) {
				destroy_route_tree(tree->trees[i]);
			}
		}
		shm_free(tree->trees);
	}
	if(tree->name.s){
		shm_free(tree->name.s);
	}
	shm_free(tree);
	return;
}

/**
 * Destroys the complete routing tree data.
 *
 * @param data route data to be destroyed
 */
void destroy_rewrite_data(struct rewrite_data *data) {
	int i;

	if (data == NULL) {
		return;
	}
	if (data->carriers != NULL) {
		for (i = 0; i < data->tree_num; ++i) {
			if (data->carriers[i] != NULL) {
				destroy_carrier_tree(data->carriers[i]);
			}
		}
		shm_free(data->carriers);
	}
	shm_free(data);
	return;
}

static int carrier_tree_fixup(struct rewrite_data * rd){
	int i;
	rd->default_carrier_index = -1;
	for(i=0; i<rd->tree_num; i++){
		if(rd->carriers[i]){
			if(strcmp(rd->carriers[i]->name.s, default_tree) == 0){
				rd->default_carrier_index = i;
			}
		}
	}
	if(rd->default_carrier_index < 0){
		LM_ERR("default_carrier not found\n");
	}
	return 0;
}
