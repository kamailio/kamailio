/*
 * $Id$
 *
 * Copyright (C) 2003 August.Net Services, LLC
 * Copyright (C) 2008 1&1 Internet AG
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
 * History
 * -------
 * 2003-04-06 initial code written (Greg Fausak/Andy Fullford)
 */

/*! \file
 *  \brief DB_POSTGRES :: Core
 *  \ingroup db_postgres
 *  Module: \ref db_postgres
 */

#ifndef KM_DBASE_H
#define KM_DBASE_H

#include "../../lib/srdb1/db_con.h"
#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db_key.h"
#include "../../lib/srdb1/db_op.h"
#include "../../lib/srdb1/db_val.h"


/*
 * Initialize database connection
 */
db1_con_t* db_postgres_init(const str* _url);

/*
 * Initialize database connection - no pooling
 */
db1_con_t* db_postgres_init2(const str* _url, db_pooling_t pooling);

/*
 * Close a database connection
 */
void db_postgres_close(db1_con_t* _h);

/*
 * Return result of previous query
 */
int db_postgres_store_result(const db1_con_t* _h, db1_res_t** _r);


/*
 * Free all memory allocated by get_result
 */
int db_postgres_free_result(db1_con_t* _h, db1_res_t* _r);


/*
 * Do a query
 */
int db_postgres_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
		const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
		const db_key_t _o, db1_res_t** _r);


/*
 * Do a query and lock rows for update
 */
int db_postgres_query_lock(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
		const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
		const db_key_t _o, db1_res_t** _r);


/*
 * Raw SQL query
 */
int db_postgres_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r);


/*
 * Insert a row into table
 */
int db_postgres_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
		const int _n);


/*
 * Delete a row from table
 */
int db_postgres_delete(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, const int _n);


/*
 * Update a row in table
 */
int db_postgres_update(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv, const int _n,
		const int _un);

/*
 * fetch rows from a result
 */
int db_postgres_fetch_result(const db1_con_t* _h, db1_res_t** _r, const int nrows);

/*
 * number of rows affected by the last DB query/statement
 */
int db_postgres_affected_rows(const db1_con_t* _h);

/*
 * SQL BEGIN
 */
int db_postgres_start_transaction(db1_con_t* _h, db_locking_t _l);

/*
 * SQL COMMIT
 */
int db_postgres_end_transaction(db1_con_t* _h);

/*
 * SQL ROLLBACK
 */
int db_postgres_abort_transaction(db1_con_t* _h);

/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_postgres_use_table(db1_con_t* _h, const str* _t);

/*
 * Replace a row in table (via update/insert)
 */
int db_postgres_replace(const db1_con_t* _h, const db_key_t* _k,
		const db_val_t* _v, const int _n, const int _un, const int _m);

#endif /* KM_DBASE_H */
