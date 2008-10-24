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
 * \file cr_data.c
 * \brief Contains the functions to manage routing data.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include "../../mem/shm_mem.h"
#include "cr_data.h"
#include "carrierroute.h"
#include "cr_config.h"
#include "cr_db.h"
#include "cr_carrier.h"
#include "cr_domain.h"


/**
 * Pointer to the routing data.
 */
struct route_data_t ** global_data = NULL;


/**
 * Destroys a carrier
 *
 * @param tree route data to be destroyed
 */
static void destroy_carrier_data(struct carrier_data_t * carrier_data) {
	int i;

	if (carrier_data == NULL) {
		return;
	}
	if (carrier_data->domains != NULL) {
		for (i = 0; i < carrier_data->domain_num; ++i) {
			if (carrier_data->domains[i] != NULL) {
				destroy_domain_data(carrier_data->domains[i]);
			}
		}
		shm_free(carrier_data->domains);
	}
	if(carrier_data->name.s){
		shm_free(carrier_data->name.s);
	}
	shm_free(carrier_data);
	return;
}


static int carrier_data_fixup(struct route_data_t * rd){
	int i;
	str tmp;
	tmp = default_tree;
	rd->default_carrier_index = -1;
	for(i=0; i<rd->carrier_num; i++){
		if(rd->carriers[i]){
			if(str_strcmp(&(rd->carriers[i]->name), &tmp) == 0){
				rd->default_carrier_index = i;
			}
		}
	}
	if(rd->default_carrier_index < 0){
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
			LM_ERR("Out of shared memory before even doing anything.\n");
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
	shm_free(data);
	return;
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
		LM_ERR("out of shared memory\n");
		return -1;
	}
	memset(new_data, 0, sizeof(struct route_data_t));

	switch (mode) {
	case CARRIERROUTE_MODE_DB:
		if (load_route_data_db(new_data) < 0) {
			LM_ERR("could not load routing data\n");
			return -1;
		}
		break;
	case CARRIERROUTE_MODE_FILE:
		if (load_config(new_data) < 0) {
			LM_ERR("could not load routing data\n");
			return -1;
		}
		break;
	default:
		LM_ERR("invalid mode");
		return -1;
	}
	if (new_data == NULL) {
		LM_ERR("loading routing data failed (NULL pointer)");
		return -1;
	}

	if (rule_fixup(new_data) < 0) {
		LM_ERR("could not fixup rules\n");
		return -1;
	}

	if (carrier_data_fixup(new_data) < 0){
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
		clear_route_data(old_data);
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
