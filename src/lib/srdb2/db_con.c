/* 
 * Copyright (C) 2001-2003 FhG FOKUS
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

#include "db_con.h"

#include "../../mem/mem.h"
#include "../../dprint.h"

#include <string.h>
#include <stdlib.h>


/*
 * Default implementation of the connect function is noop,
 * db drivers can override the function pointer in db_con
 * structures
 */
static int db_con_connect(db_con_t* con)
{
	return 0;
}


/*
 * Default implementation of the disconnect function is noop,
 * db drivers can override the function pointer in db_con
 * structures
 */
static void db_con_disconnect(db_con_t* con)
{
}


/*
 * Create a new generic db_con structure representing a
 * database connection and call the driver specific function
 * in the driver that is associated with the structure based
 * on the scheme of uri parameter
 */
db_con_t* db_con(db_ctx_t* ctx, db_uri_t* uri)
{
    db_con_t* newp;

    newp = (db_con_t*)pkg_malloc(sizeof(db_con_t));
    if (newp == NULL) {
		ERR("db_con: No memory left\n");
		goto error;
    }

    memset(newp, '\0', sizeof(db_con_t));
	if (db_gen_init(&newp->gen) < 0) goto error;

    newp->uri = uri;
	newp->ctx = ctx;
	newp->connect = db_con_connect;
	newp->disconnect = db_con_disconnect;

	/* Call db_ctx function if the driver has it */
	if (db_drv_call(&uri->scheme, "db_con", newp, ctx->con_n) < 0) {
		goto error;
	}

	return newp;

 error:
	if (newp) {
		db_gen_free(&newp->gen);
		pkg_free(newp);
	}
	return NULL;
}


/*
 * Releaase all memory used by the structure
 */
void db_con_free(db_con_t* con)
{
    if (con == NULL) return;
	db_gen_free(&con->gen);
	if (con->uri) db_uri_free(con->uri);
    pkg_free(con);
}

/** @} */
