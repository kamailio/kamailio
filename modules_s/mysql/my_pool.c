/* 
 * $Id$
 *
 *
 * Copyright (C) 2001-2004 iptel.org
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

#include <unistd.h>
#include "../../dprint.h"
#include "my_pool.h"
#include "my_id.h"


/* The head of the pool */
static struct my_con* pool = 0;

/*
 * Pid of the process that added the last
 * connection to the pool. This is used to
 * check for inherited database connections.
 */
static int pool_pid;


/*
 * Get a connection from the pool, reuse existing
 * if possible, otherwise create a new one
 */
struct my_con* get_connection(const char* url)
{
	struct my_id* id;
	struct my_con* ptr;
	int pid;

	if (!url) {
		LOG(L_ERR, "get_connection(): Invalid parameter value\n");
		return 0;
	}

	pid = getpid();
	if (pool && (pool_pid != pid)) {
		LOG(L_ERR, "get_connection(): Inherited open database connections, this is not a good idea\n");
		return 0;
	}

	pool_pid = pid;

	id = new_my_id(url);
	if (!id) return 0;

	ptr = pool;
	while (ptr) {
		if (cmp_my_id(id, ptr->id)) {
			DBG("get_connection(): Connection found in the pool\n");
			ptr->ref++;
			free_my_id(id);
			return ptr;
		}
		ptr = ptr->next;
	}

	DBG("get_connection(): Connection not found in the pool\n");
	ptr = new_connection(id);
	if (!ptr) {
		free_my_id(id);
		return 0;
	}

	ptr->next = pool;
	pool = ptr;
	return ptr;
}


/*
 * Release a connection, the connection will be left
 * in the pool if ref count != 0, otherwise it
 * will be delete completely
 */
void release_connection(struct my_con* con)
{
	struct my_con* ptr;

	if (!con) return;

	if (con->ref > 1) {
		     /* There are still other users, just
		      * decrease the reference count and return
		      */
		DBG("release_connection(): Connection still kept in the pool\n");
		con->ref--;
		return;
	}

	DBG("release_connection(): Removing connection from the pool\n");

	if (pool == con) {
		pool = pool->next;
	} else {
		ptr = pool;
		while(ptr) {
			if (ptr->next == con) break;
			ptr = ptr->next;
		}
		if (!ptr) {
			LOG(L_ERR, "release_connection(): Weird, connection not found in the pool\n");
		} else {
			     /* Remove the connection from the pool */
			ptr->next = con->next;
		}
	}

	free_connection(con);
}
