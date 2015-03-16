/* 
 * Copyright (C) 2001-2004 iptel.org
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

#include "my_con.h"

#include "mysql_mod.h"
#include "my_uri.h"

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"

#include <string.h>
#include <time.h>


/*
 * Close the connection and release memory
 */
static void my_con_free(db_con_t* con, struct my_con* payload)
{
	if (!payload) return;
	
	/* Delete the structure only if there are no more references
	 * to it in the connection pool
	 */
	if (db_pool_remove((db_pool_entry_t*)payload) == 0) return;
	
	db_pool_entry_free(&payload->gen);
	if (payload->con) pkg_free(payload->con);
	pkg_free(payload);
}


int my_con_connect(db_con_t* con)
{
	struct my_con* mcon;
	struct my_uri* muri;
	
	mcon = DB_GET_PAYLOAD(con);
	muri = DB_GET_PAYLOAD(con->uri);
	
	/* Do not reconnect already connected connections */
	if (mcon->flags & MY_CONNECTED) return 0;

	DBG("mysql: Connecting to %.*s:%.*s\n",
		con->uri->scheme.len, ZSW(con->uri->scheme.s),
		con->uri->body.len, ZSW(con->uri->body.s));

	if (my_connect_to) {
		if (mysql_options(mcon->con, MYSQL_OPT_CONNECT_TIMEOUT, 
						  (char*)&my_connect_to))
			WARN("mysql: failed to set MYSQL_OPT_CONNECT_TIMEOUT\n");
	}

#if MYSQL_VERSION_ID >= 40101 
	if ((my_client_ver >= 50025) || 
		((my_client_ver >= 40122) && 
		 (my_client_ver < 50000))) {
		if (my_send_to) {
			if (mysql_options(mcon->con, MYSQL_OPT_WRITE_TIMEOUT , 
							  (char*)&my_send_to))
				WARN("mysql: failed to set MYSQL_OPT_WRITE_TIMEOUT\n");
		}
		if (my_recv_to){
			if (mysql_options(mcon->con, MYSQL_OPT_READ_TIMEOUT , 
							  (char*)&my_recv_to))
				WARN("mysql: failed to set MYSQL_OPT_READ_TIMEOUT\n");
		}
	}
#endif
	
	if (!mysql_real_connect(mcon->con, muri->host, muri->username, 
							muri->password, muri->database, muri->port, 0, 0)) {
		LOG(L_ERR, "mysql: %s\n", mysql_error(mcon->con));
		return -1;
	}
	
	DBG("mysql: Connection type is %s\n", mysql_get_host_info(mcon->con));
	DBG("mysql: Protocol version is %d\n", mysql_get_proto_info(mcon->con));
	DBG("mysql: Server version is %s\n", mysql_get_server_info(mcon->con));

	mcon->flags |= MY_CONNECTED;
	return 0;
}


void my_con_disconnect(db_con_t* con)
{
	struct my_con* mcon;

	mcon = DB_GET_PAYLOAD(con);

	if ((mcon->flags & MY_CONNECTED) == 0) return;

	DBG("mysql: Disconnecting from %.*s:%.*s\n",
		con->uri->scheme.len, ZSW(con->uri->scheme.s),
		con->uri->body.len, ZSW(con->uri->body.s));

	mysql_close(mcon->con);
	mcon->flags &= ~MY_CONNECTED;

	/* Increase the variable that keeps track of number of connection
	 * resets on this connection. The mysql module uses the variable to
	 * determine when a pre-compiled command needs to be uploaded to the
	 * server again. If the number in the my_con structure is larger than
	 * the number kept in my_cmd then it means that we have to upload the
	 * command to the server again because the connection was reset.
	 */
	mcon->resets++;
}


int my_con(db_con_t* con)
{
	struct my_con* ptr;

	/* First try to lookup the connection in the connection pool and
	 * re-use it if a match is found
	 */
	ptr = (struct my_con*)db_pool_get(con->uri);
	if (ptr) {
		DBG("mysql: Connection to %.*s:%.*s found in connection pool\n",
			con->uri->scheme.len, ZSW(con->uri->scheme.s),
			con->uri->body.len, ZSW(con->uri->body.s));
		goto found;
	}

	ptr = (struct my_con*)pkg_malloc(sizeof(struct my_con));
	if (!ptr) {
		LOG(L_ERR, "mysql: No memory left\n");
		goto error;
	}
	memset(ptr, '\0', sizeof(struct my_con));
	if (db_pool_entry_init(&ptr->gen, my_con_free, con->uri) < 0) goto error;

	ptr->con = (MYSQL*)pkg_malloc(sizeof(MYSQL));
	if (!ptr->con) {
		LOG(L_ERR, "mysql: No enough memory\n");
		goto error;
	}
	mysql_init(ptr->con);

	DBG("mysql: Creating new connection to: %.*s:%.*s\n",
		con->uri->scheme.len, ZSW(con->uri->scheme.s),
		con->uri->body.len, ZSW(con->uri->body.s));

	/* Put the newly created mysql connection into the pool */
	db_pool_put((struct db_pool_entry*)ptr);
	DBG("mysql: Connection stored in connection pool\n");

 found:
	/* Attach driver payload to the db_con structure and set connect and
	 * disconnect functions
	 */
	DB_SET_PAYLOAD(con, ptr);
	con->connect = my_con_connect;
	con->disconnect = my_con_disconnect;
	return 0;

 error:
	if (ptr) {
		db_pool_entry_free(&ptr->gen);
		if (ptr->con) pkg_free(ptr->con);
		pkg_free(ptr);
	}
	return 0;
}
