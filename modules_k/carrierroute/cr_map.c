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
 * \file cr_map.c
 * \brief Contains the functions to map domain and carrier names to ids.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include "cr_map.h"
#include "../../mem/shm_mem.h"
#include "../../ut.h"


/**
 * used to map routing domain names to numbers for
 * faster access.
 */
struct domain_map_t {
	str name; /*!< name of the routing domain */
	int index; /*!< domain index */
	struct domain_map_t * next; /*!< pointer to the next element */
};

/**
 * used to map carrier names to numbers for
 * faster access.
 */
struct carrier_map_t {
	str name; /*!< name of the carrier */
	int id; /*!< id of the carrier */
	int index; /*!< number of carrier array index for rewrite_data.trees */
	struct carrier_map_t * next; /*!< pointer to the next element */
};


/**
 * holds the map between routing domain names and numbers
 */
static struct domain_map_t ** domain_map = NULL;


/**
 * holds the map between carrier names and numbers
 */
static struct carrier_map_t ** carrier_map = NULL;


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
	struct domain_map_t * tmp, * prev = NULL;
	int index = 0;
	if (!domain_map) {
		if ((domain_map = shm_malloc(sizeof(struct domain_map_t *))) == NULL) {
			LM_ERR("out of shared memory\n");
			return -1;
		}
		memset(domain_map, 0, sizeof(struct domain_map_t *));
	}

	tmp = *domain_map;

	while (tmp) {
		if (str_strcmp(&tmp->name, domain) == 0) {
			return tmp->index;
		}
		index = tmp->index + 1;
		prev = tmp;
		tmp = tmp->next;
	}
	if ((tmp = shm_malloc(sizeof(struct domain_map_t))) == NULL) {
		LM_ERR("out of shared memory\n");
		return -1;
	}
	memset(tmp, 0, sizeof(struct domain_map_t));
	if (shm_str_dup(&tmp->name, domain) != 0) {
		LM_ERR("cannot duplicate string\n");
		shm_free(tmp);
		return -1;
	}
	tmp->index = index;
	if (!prev) {
		*domain_map = tmp;
	} else {
		prev->next = tmp;
	}
	LM_INFO("domain %.*s has index %i\n", domain->len, domain->s, index);
	return index;
}


/**
 * Destroy the domain map by freeing its memory.
 */
void destroy_domain_map(void) {
	struct domain_map_t * tmp;
	if (domain_map) {
		tmp = *domain_map;
		while (*domain_map) {
			tmp = *domain_map;
			*domain_map = tmp->next;
			shm_free(tmp);
		}
		shm_free(domain_map);
		domain_map = NULL;
	}
}


/**
 * Tries to add a carrier name to the carrier map. If the given carrier
 * doesn't exist, it is added. Otherwise, nothing happens.
 *
 * @param carrier_name the carrier name to be added
 * @param carrier_id the corresponding id
 *
 * @return values: on succcess the numerical index of the given carrier,
 * -1 on failure
 */
int add_carrier(const str * tree, int carrier_id) {
	struct carrier_map_t * tmp, * prev = NULL;
	int index = 0;
	if (!carrier_map) {
		if ((carrier_map = shm_malloc(sizeof(struct carrier_map_t *))) == NULL) {
			LM_ERR("out of shared memory\n");
			return -1;
		}
		*carrier_map = NULL;
	}
	tmp = *carrier_map;

	while (tmp) {
		if (carrier_id == tmp->id) {
			return tmp->index;
		}
		index = tmp->index + 1;
		prev = tmp;
		tmp = tmp->next;
	}
	if ((tmp = shm_malloc(sizeof(struct carrier_map_t))) == NULL) {
		LM_ERR("out of shared memory\n");
		return -1;
	}
	memset(tmp, 0, sizeof(struct carrier_map_t));
	if (shm_str_dup(&tmp->name, tree)!=0) {
		LM_ERR("cannot duplicate string\n");
		shm_free(tmp);
		return -1;
	}
	tmp->index = index;
	tmp->id = carrier_id;
	if (!prev) {
		*carrier_map = tmp;
	} else {
		prev->next = tmp;
	}
	LM_INFO("tree %.*s has internal id %i\n", tree->len, tree->s, index);
	return index;
}


/**
 * Searches for the ID for a Carrier-Name
 *
 * @param carrier_name the carrier name, we are looking for
 *
 * @return values: on succcess the id for this carrier name,
 * -1 on failure
 */
int find_carrier(str carrier_name) {
	struct carrier_map_t * tmp;
	if (!carrier_map) {
		return -1;
	}
	if (carrier_name.len <= 0) {
		return -1;
	}
	tmp = *carrier_map;

	while (tmp) {
		if (str_strcmp(&carrier_name, &tmp->name) == 0) {
			return tmp->id;
		}
		tmp = tmp->next;
	}
	return -1;
}


/**
 * Destroy the carrier map by freeing its memory.
 */
void destroy_carrier_map(void) {
	struct carrier_map_t * tmp;
	if (carrier_map) {
		tmp = *carrier_map;
		while (*carrier_map) {
			tmp = *carrier_map;
			*carrier_map = tmp->next;
			shm_free(tmp);
		}
		shm_free(carrier_map);
		carrier_map = NULL;
	}
}
