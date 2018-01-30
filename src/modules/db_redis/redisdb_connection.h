/**
 * Copyright (C) 2017 plivo (plivo.com)
 * Author : Surendra Tiwari (surendratiwari3@gmail.com)
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


#ifndef _REDISDB_CONNECTION_H_
#define _REDISDB_CONNECTION_H_

#include <hiredis/hiredis.h>
#include "../../lib/srdb1/db_pool.h"
#include "../../lib/srdb1/db_id.h"

typedef struct km_redis_con {
	struct db_id* id;        /*!< Connection identifier */
	unsigned int ref;        /*!< Reference count */
	struct pool_con* next;   /*!< Next connection in the pool */
	redisContext *con;
    	redisReply *reply;
	int nrcols;  /*!< Nunmber of columns */
} km_redis_con_t;

#define REDIS_CON(db_con)  ((km_redis_con_t*)((db_con)->tail))

km_redis_con_t* db_redis_new_connection(const struct db_id* id);
void db_redis_free_connection(struct pool_con* con);
#endif
