/* 
 * Copyright (C) 2001-2005 iptel.org
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * \file lib/srdb1/db_pool.h
 * \brief Functions for managing a pool of database connections.
 * \ingroup db1
 */

#ifndef _DB1_POOL_H
#define _DB1_POOL_H

#include "db_id.h"
#include "db_con.h"


/**
 * This is a stub that contains all attributes
 * that pool members must have, it is not really
 * used, real connection structures are created
 * by database backends. All such structures (
 * created by the backends) must have these
 * attributes.
 */
struct pool_con {
	struct db_id* id;        /**< Connection identifier */
	unsigned int ref;        /**< Reference count */
	struct pool_con* next;   /**< Next element in the pool */
};


/**
 * Search the pool for a connection with the identifier equal to
 * the id.
 * \param id searched id
 * \return the connection if it could be found, NULL otherwise
 */
struct pool_con* pool_get(const struct db_id* id);


/**
 * Insert a new connection into the pool.
 * \param con the inserted connection 
 */
void pool_insert(struct pool_con* con);


/**
 * Release a connection from the pool, the function
 * would return 1 when if the connection is not
 * referenced anymore and thus can be closed and
 * deleted by the backend. The function returns
 * 0 if the connection should still be kept open
 * because some other module is still using it.
 * The function returns -1 if the connection is
 * not in the pool.
 * \param con connection that should be removed
 * \return 1 if the connection can be freed, 0 if it can't be freed, -1 if not found
 */
int pool_remove(struct pool_con* con);


#endif /* _DB1_POOL_H */
