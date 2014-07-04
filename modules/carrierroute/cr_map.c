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
 * \file cr_map.c
 * \brief Contains the functions to map domain and carrier names to ids.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include <stdlib.h>
#include "cr_map.h"
#include "../../mem/shm_mem.h"
#include "../../ut.h"



/**
 * Searches for the ID of a name
 *
 * @param map the mapping list to search in
 * @param size the size of the list
 * @param name the name, we are looking for
 *
 * @return values: on succcess the id for this name, -1 on failure
 */
int map_name2id(struct name_map_t * map, int size, const str * name) {
	int i;

	if ((!name) || (name->len <= 0)) {
		return -1;
	}

	for (i=0; i<size; i++) {
		if (str_strcmp(&map[i].name, name) == 0) return map[i].id;
	}
	return -1;
}


/**
 * Searches for the name of an ID
 *
 * @param map the mapping list to search in
 * @param size the size of the list
 * @param id the id, we are looking for
 *
 * @return values: on succcess the name for this id, NULL on failure
 */
str * map_id2name(struct name_map_t * map, int size, int id) {
	struct name_map_t key;
	struct name_map_t * tmp;

	key.id = id;
	tmp = bsearch(&key, map, size, sizeof(struct name_map_t), compare_name_map);
	if (tmp == NULL) return NULL;
	return &tmp->name;
}


/**
 * Compares the IDs of two name_map_t structures.
 *
 * @return -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
 */
int compare_name_map(const void *v1, const void *v2) {
	if (((struct name_map_t *)v1)->id < ((struct name_map_t *)v2)->id) return -1;
	else if (((struct name_map_t *)v1)->id > ((struct name_map_t *)v2)->id) return 1;
	else return 0;
}
