/*
 * $Id$
 *
 * CASSANDRA module interface
 *
 * Copyright (C) 2012 1&1 Internet AG
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
 *
 * History:
 * --------
 * 2012-01  first version (Anca Vamanu)
 */

#ifndef _CASSA_DBASE_H
#define _CASSA_DBASE_H


#ifdef __cplusplus
extern "C" {
#endif


#include "../../lib/srdb1/db_pool.h"
#include "../../lib/srdb1/db_row.h"
#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db_op.h"

extern unsigned int cassa_auto_reconnect;
extern unsigned int cassa_retries;
extern unsigned int cassa_conn_timeout;
extern unsigned int cassa_send_timeout;
extern unsigned int cassa_recv_timeout;

/*
 * Open connection
 */
void* db_cassa_new_connection(struct db_id* id);

/*
 * Free connection
 */
void db_cassa_free_connection(struct pool_con* con);


/*
 * Do a query
 */
int db_cassa_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
		const db_val_t* _v, const db_key_t* _c, int _n, int _nc,
		const db_key_t _o, db1_res_t** _r);

/*
 * Insert a row into table
 */
int db_cassa_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
		int _n);


/*
 * Replace a row into table - same as insert for cassandra
 */
int db_cassa_replace(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
		int _n, const int _un, const int _m);

/*
 * Delete a row from table
 */
int db_cassa_delete(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, int _n);


/*
 * Update a row in table
 */
int db_cassa_update(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv,
		int _n, int _un);

int db_cassa_free_result(db1_con_t* _h, db1_res_t* _r);

int db_cassa_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r);

#ifdef __cplusplus
}
#endif


#endif
