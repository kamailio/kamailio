/* 
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
 */

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "mongodb_connection.h"

/*! \brief
 * Create a new connection structure,
 * open the mongodb connection and set reference count to 1
 */
km_mongodb_con_t* db_mongodb_new_connection(const struct db_id* id)
{
	km_mongodb_con_t *ptr;

	if (!id) {
		LM_ERR("invalid parameter value\n");
		return 0;
	}

	ptr = (km_mongodb_con_t*)pkg_malloc(sizeof(km_mongodb_con_t));
	if (!ptr) {
		LM_ERR("no private memory left\n");
		return 0;
	}

	memset(ptr, 0, sizeof(km_mongodb_con_t));
	ptr->ref = 1;

	mongoc_init();
	ptr->con = mongoc_client_new (id->url.s);
	if (!ptr->con) {
		LM_ERR("cannot open connection: %.*s\n", id->url.len, id->url.s);
		goto err;
	}

	LM_DBG("connection open to: %.*s\n", id->url.len, id->url.s);

	ptr->id = (struct db_id*)id;
	return ptr;

 err:
	if (ptr) pkg_free(ptr);
	return 0;
}


/*! \brief
 * Close the connection and release memory
 */
void db_mongodb_free_connection(struct pool_con* con)
{
	km_mongodb_con_t * _c;
	
	if (!con) return;

	_c = (km_mongodb_con_t*) con;

	if (_c->id) free_db_id(_c->id);
	if (_c->con) {
		mongoc_client_destroy(_c->con);
	}
	pkg_free(_c);
}
