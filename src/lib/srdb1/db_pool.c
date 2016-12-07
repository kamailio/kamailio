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
 * \file lib/srdb1/db_pool.c
 * \brief Functions for managing a pool of database connections.
 * \ingroup db1
 */

#include "../../dprint.h"
#include "db_pool.h"


/* The head of the pool */
static struct pool_con* db_pool = 0;


/*
 * Search the pool for a connection with
 * the identifier equal to id, NULL is returned
 * when no connection is found
 */
struct pool_con* pool_get(const struct db_id* id)
{
	struct pool_con* ptr;

	if (!id) {
		LM_ERR("invalid parameter value\n");
		return 0;
	}

	ptr = db_pool;
	while (ptr) {
		if (cmp_db_id(id, ptr->id)) {
			ptr->ref++;
			return ptr;
		}
		ptr = ptr->next;
	}

	return 0;
}


/*
 * Insert a new connection into the pool
 */
void pool_insert(struct pool_con* con)
{
	if (!con) return;

	con->next = db_pool;
	db_pool = con;
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
int pool_remove(struct pool_con* con)
{
	struct pool_con* ptr;

	if (!con) return -2;

	if (con->ref > 1) {
		     /* There are still other users, just
		      * decrease the reference count and return
		      */
		LM_DBG("connection still kept in the pool\n");
		con->ref--;
		return 0;
	}

	LM_DBG("removing connection from the pool\n");

	if (db_pool == con) {
		db_pool = db_pool->next;
	} else {
		ptr = db_pool;
		while(ptr) {
			if (ptr->next == con) break;
			ptr = ptr->next;
		}
		if (!ptr) {
			LM_ERR("weird, connection not found in the pool\n");
			return -1;
		} else {
			     /* Remove the connection from the pool */
			ptr->next = con->next;
		}
	}

	return 1;
}
