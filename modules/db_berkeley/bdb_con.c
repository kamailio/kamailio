/*
 * BDB Database Driver for SER
 *
 * Copyright (C) 2008 iptelorg GmbH
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Kamailio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/** \addtogroup bdb
 * @{
 */

/*! \file
 * Functions related to connections to BDB.
 *
 * \ingroup database
 */

#include <stdlib.h>
#include <string.h>

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"

#include "bdb_con.h"
#include "bdb_uri.h"
#include "bdb_lib.h"

/** Free all memory allocated for a bdb_con structure.
 * This function function frees all memory that is in use by
 * a bdb_con structure.
 * @param con A generic db_con connection structure.
 * @param payload BDB specific payload to be freed.
 */
static void bdb_con_free(db_con_t* con, bdb_con_t *payload)
{
	if (!payload)
		return;

	/* Delete the structure only if there are no more references
	 * to it in the connection pool
	 */
	if (db_pool_remove((db_pool_entry_t*)payload) == 0) return;

	db_pool_entry_free(&payload->gen);

	/* destroy and free BDB env */
	pkg_free(payload);
}


int bdb_con(db_con_t* con)
{
	bdb_con_t* bcon;
	bdb_uri_t* buri;

	buri = DB_GET_PAYLOAD(con->uri);

	/* First try to lookup the connection in the connection pool and
	 * re-use it if a match is found
	 */
	bcon = (bdb_con_t*)db_pool_get(con->uri);
	if (bcon) {
		DBG("bdb: Connection to %s found in connection pool\n",
			buri->uri);
		goto found;
	}

	bcon = (bdb_con_t*)pkg_malloc(sizeof(bdb_con_t));
	if (!bcon) {
		ERR("bdb: No memory left\n");
		goto error;
	}
	memset(bcon, '\0', sizeof(bdb_con_t));
	if (db_pool_entry_init(&bcon->gen, bdb_con_free, con->uri) < 0) goto error;

	DBG("bdb: Preparing new connection to %s\n", buri->uri);
	if(bdb_is_database(buri->path.s)!=0)
	{	
		ERR("bdb: database [%.*s] does not exists!\n",
				buri->path.len, buri->path.s);
		goto error;
	}

	/* Put the newly created BDB connection into the pool */
	db_pool_put((struct db_pool_entry*)bcon);
	DBG("bdb: Connection stored in connection pool\n");

found:
	/* Attach driver payload to the db_con structure and set connect and
	 * disconnect functions
	 */
	DB_SET_PAYLOAD(con, bcon);
	con->connect = bdb_con_connect;
	con->disconnect = bdb_con_disconnect;
	return 0;

error:
	if (bcon) {
		db_pool_entry_free(&bcon->gen);
		pkg_free(bcon);
	}
	return -1;
}



int bdb_con_connect(db_con_t* con)
{
	bdb_con_t *bcon;
	bdb_uri_t *buri;

	bcon = DB_GET_PAYLOAD(con);
	buri = DB_GET_PAYLOAD(con->uri);

	/* Do not reconnect already connected connections */
	if (bcon->flags & BDB_CONNECTED) return 0;

	DBG("bdb: Connecting to %s\n", buri->uri);

	/* create BDB environment */
	bcon->dbp = bdblib_get_db(&buri->path);
	if(bcon->dbp == NULL)
	{
		ERR("bdb: error binding to DB %s\n", buri->uri);
		return -1;
	}

	DBG("bdb: Successfully bound to %s\n", buri->uri);
	bcon->flags |= BDB_CONNECTED;
	return 0;

}


void bdb_con_disconnect(db_con_t* con)
{
	bdb_con_t *bcon;
	bdb_uri_t *buri;

	bcon = DB_GET_PAYLOAD(con);
	buri = DB_GET_PAYLOAD(con->uri);

	if ((bcon->flags & BDB_CONNECTED) == 0) return;

	DBG("bdb: Unbinding from %s\n", buri->uri);
	if(bcon->dbp==NULL)
	{
		bcon->flags &= ~BDB_CONNECTED;
		return;
	}

	bdblib_close(bcon->dbp, &buri->path);
	bcon->dbp = 0;

	bcon->flags &= ~BDB_CONNECTED;
}


/** @} */
