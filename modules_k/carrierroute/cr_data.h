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
 * \file cr_data.h
 * \brief Contains the functions to manage routing data.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#ifndef CR_DATA_H
#define CR_DATA_H

#include <sys/types.h>
#include "../../locking.h"


/**
 * contains all routing data.
 */
struct route_data_t {
	struct carrier_data_t ** carriers; /*!< array of carriers */
	size_t carrier_num; /*!< number of carriers */
	int default_carrier_index;
	int proc_cnt; /*!< a ref counter for the shm data */
	gen_lock_t lock; /*!< lock for ref counter updates */
};

/**
 * initialises the routing data, initialises the global data pointer
 *
 * @return 0 on success, -1 on failure
 */
int init_route_data(void);


/**
 * Frees the routing data
 */
void destroy_route_data(void);


/**
 * Clears the complete routing data.
 *
 * @param data route data to be cleared
 */
void clear_route_data(struct route_data_t *data);


/**
 * Loads the routing data into the routing trees and sets the
 * global_data pointer to the new data. The old_data is removed
 * when it is not locked anymore.
 *
 * @return 0 on success, -1 on failure
 */
int reload_route_data(void);


/**
 * Increases lock counter and returns a pointer to the
 * current routing data
 *
 * @return pointer to the global routing data on success,
 * NULL on failure
*/
struct route_data_t * get_data(void);


/**
 * decrements the lock counter of the routing data
 *
 * @param data data to be released
 */
void release_data(struct route_data_t *data);

#endif
