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
 * \file cr_carrier.c
 * \brief Contains the functions to manage carrier data.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include <stdlib.h>
#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "cr_carrier.h"
#include "cr_domain.h"
#include "cr_map.h"


/**
 * Create a new carrier_data struct in shared memory and set it up.
 *
 * @param carrier_id id of carrier
 * @param carrier_name pointer to the name of the carrier
 * @param domains number of domains for that carrier
 *
 * @return a pointer to the newly allocated carrier data or NULL on
 * error, in which case it LOGs an error message.
 */
struct carrier_data_t * create_carrier_data(int carrier_id, str *carrier_name, int domains) {
	struct carrier_data_t * tmp;
	if ((tmp = shm_malloc(sizeof(struct carrier_data_t))) == NULL) {
		SHM_MEM_ERROR;
		return NULL;
	}
	memset(tmp, 0, sizeof(struct carrier_data_t));
	tmp->id = carrier_id;
	tmp->name = carrier_name;
	tmp->domain_num = domains;
	if(domains > 0){
		if ((tmp->domains = shm_malloc(sizeof(struct domain_data_t *) * domains)) == NULL) {
			SHM_MEM_ERROR;
			shm_free(tmp);
			return NULL;
		}
		memset(tmp->domains, 0, sizeof(struct domain_data_t *) * domains);
	}
	return tmp;
}


/**
 * Destroys the given carrier and frees the used memory.
 *
 * @param carrier_data the structure to be destroyed.
 */
void destroy_carrier_data(struct carrier_data_t *carrier_data) {
	int i;
	if (carrier_data) {
		if (carrier_data->domains != NULL) {
			for (i=0; i<carrier_data->domain_num; i++) {
				destroy_domain_data(carrier_data->domains[i]);
			}
			shm_free(carrier_data->domains);
		}
		shm_free(carrier_data);
	}
}


/**
 * Adds a domain_data struct to the given carrier data structure at the given index.
 * Other etries are moved one position up to make space for the new one.
 *
 * @param carrier_data the carrier data struct where domain_data should be inserted
 * @param domain_data the domain data struct to be inserted
 * @param index the index where to insert the domain_data structure in the domain array
 *
 * @return 0 on success, -1 on failure
 */
int add_domain_data(struct carrier_data_t * carrier_data, struct domain_data_t * domain_data, int index) {
	LM_INFO("adding domain %d '%.*s' to carrier %d '%.*s'", domain_data->id, domain_data->name->len, domain_data->name->s, carrier_data->id, carrier_data->name->len, carrier_data->name->s);
 	LM_DBG("domain position %d (domain_num=%d, first_empty_domain=%d)", index, (int) carrier_data->domain_num, (int) carrier_data->first_empty_domain);

	if ((index < 0) || (index > carrier_data->first_empty_domain)) {
		LM_ERR("got invalid index during binary search\n");
		return -1;
	}
		
	if (carrier_data->first_empty_domain >= carrier_data->domain_num) {
		LM_ERR("cannot add new domain '%.*s' into carrier '%.*s' - array already full\n", domain_data->name->len, domain_data->name->s, carrier_data->name->len, carrier_data->name->s);
		return -1;
	}

	if (index < carrier_data->first_empty_domain) {
		/* move other entries one position up */
		memmove(&carrier_data->domains[index+1], &carrier_data->domains[index], sizeof(struct domain_data_t *)*(carrier_data->first_empty_domain-index));
	}
	carrier_data->domains[index] = domain_data;
	carrier_data->first_empty_domain++;

	return 0;
}


/**
 * Returns the domain data for the given id by doing a binary search.
 * @note The domain array must be sorted!
 *
 * @param carrier_data carrier data to be searched
 * @param domain_id the id of desired domain
 *
 * @return a pointer to the desired domain data, NULL if not found.
 */
struct domain_data_t * get_domain_data(struct carrier_data_t * carrier_data, int domain_id) {
	struct domain_data_t **ret;
	struct domain_data_t key;
	struct domain_data_t *pkey = &key;

	if (!carrier_data) {
		LM_ERR("NULL pointer in parameter\n");
		return NULL;
	}
	key.id = domain_id;
	ret = bsearch(&pkey, carrier_data->domains, carrier_data->domain_num, sizeof(carrier_data->domains[0]), compare_domain_data);
	if (ret) return *ret;
	return NULL;
}


/**
 * Compares the IDs of two carrier data structures.
 * A NULL pointer is always greater than any ID.
 *
 * @return -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
 */
int compare_carrier_data(const void *v1, const void *v2) {
  struct carrier_data_t *c1 = *(struct carrier_data_t * const *)v1;
	struct carrier_data_t *c2 = *(struct carrier_data_t * const *)v2;
	if (c1 == NULL) {
		if (c2 == NULL) return 0;
		else return 1;
	}
	else {
		if (c2 == NULL) return -1;
		else {
			if (c1->id < c2->id) return -1;
			else if (c1->id > c2->id) return 1;
			else return 0;
		}
	}
}
