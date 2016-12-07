/*
 * $Id$
 *
 * Copyright (C) 2007,2008 TRUNK MOBILE
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

#ifndef ORA_CON_H
#define ORA_CON_H

#include <oci.h>
#include "../../lib/srdb1/db_pool.h"
#include "../../lib/srdb1/db_id.h"
#include "../../lib/srdb1/db_val.h"

/* Temporary -- callback data for submit_query/store_result */
struct query_data {
    OCIStmt** _rs;
    const db_val_t* _v;
    int _n;
    const db_val_t* _w;
    int _nw;
};
typedef struct query_data query_data_t;


struct ora_con {
	struct pool_con hdr;	/* Standard fields */

	OCIError *errhp;        /* Error */
	OCISvcCtx *svchp;	/* Server Context */
	OCIEnv *envhp;          /* Environment */
	OCISession *authp;	/* Authorized Session */
	OCIServer *srvhp;	/* Server */

	int connected;		/* Authorized session started */
	int bindpos;		/* Last Bind handle position */
	
	query_data_t* pqdata;	/* Temporary: cb data for submit_query/store_result */

	int  uri_len;
	char uri[];
};
typedef struct ora_con ora_con_t;


/*
 * Some convenience wrappers
 */
#define CON_ORA(db_con)		((ora_con_t*)db_con->tail)


/*
 * Create a new connection structure,
 * open the Oracle connection and set reference count to 1
 */
ora_con_t* db_oracle_new_connection(const struct db_id* id);


/*
 * Close the connection and release memory
 */
void db_oracle_free_connection(ora_con_t* con);


/*
 * Disconnect after network error
 */
void db_oracle_disconnect(ora_con_t* con);


/*
 * Reconnect to server (after error)
 */
sword db_oracle_reconnect(ora_con_t* con);


/*
 * Decode oracle error
 */
const char* db_oracle_error(ora_con_t* con, sword status);

#endif /* ORA_CON_H */
