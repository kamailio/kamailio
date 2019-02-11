/**
 * Copyright (C) 2014 Daniel-Constantin Mierla (asipto.com)
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
 *
 */


#ifndef _MONGODB_CONNECTION_H_
#define _MONGODB_CONNECTION_H_

#include <mongoc.h>
#include <bson.h>

#include "../../lib/srdb1/db_pool.h"
#include "../../lib/srdb1/db_id.h"

typedef struct km_mongodb_con {
	struct db_id* id;        /*!< Connection identifier */
	unsigned int ref;        /*!< Reference count */
	struct pool_con* next;   /*!< Next connection in the pool */

	mongoc_client_t *con;              /*!< Connection representation */
	mongoc_collection_t *collection;   /*!< Collection link */
	mongoc_cursor_t *cursor;           /*!< Cursor link */

	bson_t *colsdoc; /*!< Names of columns */
	int nrcols;  /*!< Nunmber of columns */
} km_mongodb_con_t;

#define MONGODB_CON(db_con)  ((km_mongodb_con_t*)((db_con)->tail))

km_mongodb_con_t* db_mongodb_new_connection(const struct db_id* id);
void db_mongodb_free_connection(struct pool_con* con);
#endif
