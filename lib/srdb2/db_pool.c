/* 
 * Copyright (C) 2001-2005 iptel.org
 * Copyright (C) 2006-2007 iptelorg GmbH
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

/** \ingroup DB_API 
 * @{ 
 */

#include "db_pool.h"
#include "../../dprint.h"
#include <unistd.h>
#include <string.h>


SLIST_HEAD(db_pool_head, db_pool_entry);

/* The global connection pool */
struct db_pool_head db_pool = SLIST_HEAD_INITIALIZER(db_pool);


int db_pool_entry_init(struct db_pool_entry *entry, void* free_func, db_uri_t* uri)
{
	if (db_drv_init(&entry->drv_gen, free_func) < 0) return -1;
	SLIST_NEXT(entry, next) = NULL;
	entry->uri = uri;
	entry->ref = 1;
	return 0;
}


void db_pool_entry_free(struct db_pool_entry* entry)
{
	db_drv_free(&entry->drv_gen);
	entry->uri = NULL;
	entry->ref = 0;
}


/*
 * Search the pool for a connection with
 * the URI equal to uri, NULL is returned
 * when no connection is found
 */
struct db_pool_entry* db_pool_get(db_uri_t* uri)
{
	db_pool_entry_t* ptr;

	SLIST_FOREACH(ptr, &db_pool, next) {
		if (db_uri_cmp(ptr->uri, uri)) {
			ptr->ref++;
			return ptr;
		}
	}
	return 0;
}


/*
 * Insert a new connection into the pool
 */
void db_pool_put(db_pool_entry_t* entry)
{
	SLIST_INSERT_HEAD(&db_pool, entry, next);
}


/*
 * Release connection from the pool, the function
 * would return 1 when if the connection is not
 * referenced anymore and thus can be closed and
 * deleted by the backend. The function returns
 * 0 if the connection should still be kept open
 * because some other module is still using it.
 * The function returns -1 if the connection is
 * not in the pool.
 */
int db_pool_remove(db_pool_entry_t* entry)
{
	if (!entry) return -2;

	if (entry->ref > 1) {
		     /* There are still other users, just
		      * decrease the reference count and return
		      */
		DBG("db_pool_remove: Connection still kept in the pool\n");
		entry->ref--;
		return 0;
	}

	DBG("db_pool_remove: Removing connection from the pool\n");

	SLIST_REMOVE(&db_pool, entry, db_pool_entry, next);
	return 1;
}

/** @} */
