/* 
 * $Id$
 *
 * Flatstore module connection pool
 *
 * Copyright (C) 2004 FhG Fokus
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
#include "flat_pool.h"
#include "flat_id.h"


/* The head of the pool */
static struct flat_con* pool = 0;

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
struct flat_con* flat_get_connection(char* dir, char* table)
{
	struct flat_id* id;
	struct flat_con* ptr;
	int pid;

	if (!dir || !table) {
		LOG(L_ERR, "flat_get_connection: Invalid parameter value\n");
		return 0;
	}

	pid = getpid();
	if (pool && (pool_pid != pid)) {
		LOG(L_ERR, "flat_get_connection: Inherited open database connections, this is not a good idea\n");
		return 0;
	}

	pool_pid = pid;

	id = new_flat_id(dir, table);
	if (!id) return 0;

	ptr = pool;
	while (ptr) {
		if (cmp_flat_id(id, ptr->id)) {
			DBG("flat_get_connection: Connection found in the pool\n");
			ptr->ref++;
			free_flat_id(id);
			return ptr;
		}
		ptr = ptr->next;
	}

	DBG("flat_get_connection: Connection not found in the pool\n");
	ptr = flat_new_connection(id);
	if (!ptr) {
		free_flat_id(id);
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
void flat_release_connection(struct flat_con* con)
{
	struct flat_con* ptr;

	if (!con) return;

	if (con->ref > 1) {
		     /* There are still other users, just
		      * decrease the reference count and return
		      */
		DBG("flat_release_connection: Connection still kept in the pool\n");
		con->ref--;
		return;
	}

	DBG("flat_release_connection: Removing connection from the pool\n");

	if (pool == con) {
		pool = pool->next;
	} else {
		ptr = pool;
		while(ptr) {
			if (ptr->next == con) break;
			ptr = ptr->next;
		}
		if (!ptr) {
			LOG(L_ERR, "flat_release_connection: Weird, connection not found in the pool\n");
		} else {
			     /* Remove the connection from the pool */
			ptr->next = con->next;
		}
	}

	flat_free_connection(con);
}


/*
 * Close and reopen all opened connections
 */
int flat_rotate_logs(void)
{
	struct flat_con* ptr;

	ptr = pool;
	while(ptr) {
		if (flat_reopen_connection(ptr)) {
			return -1;
		}
		ptr = ptr->next;
	}

	return 0;
}
