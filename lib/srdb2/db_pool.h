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

#ifndef _DB_POOL_H
#define _DB_POOL_H  1

/** \ingroup DB_API 
 * @{ 
 */

#include "db_drv.h"
#include "db_uri.h"
#include "../../list.h"
#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*
 * This is a stub that contains all attributes
 * that pool members must have, it is not really
 * used, real connection structures are created
 * by database backends. All such structures (
 * created by the backends) must have these
 * attributes.
 */
typedef struct db_pool_entry {
	db_drv_t drv_gen;  /* Generic part of the driver specific data */
	SLIST_ENTRY(db_pool_entry) next;
	db_uri_t* uri;     /* Pointer to the URI representing the connection */
	unsigned int ref;  /* Reference count */
} db_pool_entry_t;


int db_pool_entry_init(struct db_pool_entry *entry, void* free_func, db_uri_t* uri);
void db_pool_entry_free(struct db_pool_entry* entry);	


/*
 * Search the pool for a connection with
 * the identifier equal to id, NULL is returned
 * when no connection is found
 */
struct db_pool_entry* db_pool_get(db_uri_t* uri);


/*
 * Insert a new connection into the pool
 */
void db_pool_put(struct db_pool_entry* entry);


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
int db_pool_remove(struct db_pool_entry* entry);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* _DB_POOL_H */
