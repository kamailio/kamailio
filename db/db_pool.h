/* 
 * $Id$
 *
 * Copyright (C) 2001-2005 iptel.org
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _DB_POOL_H
#define _DB_POOL_H

#include "db_id.h"
#include "db_con.h"


/*
 * This is a stub that contains all attributes
 * that pool members must have, it is not really
 * used, real connection structures are created
 * by database backends. All such structures (
 * created by the backends) must have these
 * attributes.
 */
struct pool_con {
	struct db_id* id;        /* Connection identifier */
	unsigned int ref;        /* Reference count */
	struct pool_con* next;   /* Next element in the pool */
};


/*
 * Search the pool for a connection with
 * the identifier equal to id, NULL is returned
 * when no connection is found
 */
struct pool_con* pool_get(struct db_id* id);


/*
 * Insert a new connection into the pool
 */
void pool_insert(struct pool_con* con);


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
int pool_remove(struct pool_con* con);


#endif /* _POOL_H */
