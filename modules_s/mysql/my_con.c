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

#include <string.h>
#include <time.h>
#include "my_con.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "utils.h"


/*
 * Create a new connection structure,
 * open the MySQL connection and set reference count to 1
 */
struct my_con* new_connection(struct my_id* id)
{
	struct my_con* ptr;

	if (!id) {
		LOG(L_ERR, "new_connection(): Invalid parameter value\n");
		return 0;
	}

	ptr = (struct my_con*)pkg_malloc(sizeof(struct my_con));
	if (!ptr) {
		LOG(L_ERR, "new_connection(): No memory left\n");
		return 0;
	}

	memset(ptr, 0, sizeof(struct my_con));
	ptr->ref = 1;
	
	ptr->con = (MYSQL*)pkg_malloc(sizeof(MYSQL));
	if (!ptr->con) {
		LOG(L_ERR, "new_connection(): No enough memory\n");
		goto err;
	}

	mysql_init(ptr->con);

	if (!mysql_real_connect(ptr->con, id->host.s, id->username.s, id->password.s, id->database.s, id->port, 0, 0)) {
		LOG(L_ERR, "new_connection(): %s\n", mysql_error(ptr->con));
		mysql_close(ptr->con);
		goto err;
	}

	ptr->timestamp = time(0);

	ptr->id = id;
	return ptr;

 err:
	if (ptr && ptr->con) pkg_free(ptr->con);
	if (ptr) pkg_free(ptr);
	return 0;
}


/*
 * Close the connection and release memory
 */
void free_connection(struct my_con* con)
{
	if (!con) return;
	if (con->res) mysql_free_result(con->res);
	if (con->id) free_my_id(con->id);
	if (con->con) {
		mysql_close(con->con);
		pkg_free(con->con);
	}
	pkg_free(con);
}
