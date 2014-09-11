/*
 * $Id$
 *
 * DB CLuster core functions
 *
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
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

/*! \file
 *  \brief DB_CLUSTER :: Core
 *  \ingroup db_cluster
 *  Module: \ref db_cluster
 */



#ifndef _DBCL_API_H_
#define _DBCL_API_H_


#include "../../lib/srdb1/db_con.h"
#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db_key.h"
#include "../../lib/srdb1/db_op.h"
#include "../../lib/srdb1/db_val.h"
#include "../../str.h"

/*! \brief
 * Initialize database connection
 */
db1_con_t* db_cluster_init(const str* _sqlurl);


/*! \brief
 * Close a database connection
 */
void db_cluster_close(db1_con_t* _h);


/*! \brief
 * Free all memory allocated by get_result
 */
int db_cluster_free_result(db1_con_t* _h, db1_res_t* _r);


/*! \brief
 * Do a query
 */
int db_cluster_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
	     const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
	     const db_key_t _o, db1_res_t** _r);


/*! \brief
 * fetch rows from a result
 */
int db_cluster_fetch_result(const db1_con_t* _h, db1_res_t** _r, const int nrows);


/*! \brief
 * Raw SQL query
 */
int db_cluster_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r);


/*! \brief
 * Insert a row into table
 */
int db_cluster_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v, const int _n);


/*! \brief
 * Delete a row from table
 */
int db_cluster_delete(const db1_con_t* _h, const db_key_t* _k, const 
	db_op_t* _o, const db_val_t* _v, const int _n);


/*! \brief
 * Update a row in table
 */
int db_cluster_update(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
	const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv, const int _n,
	const int _un);


/*! \brief
 * Just like insert, but replace the row if it exists
 */
int db_cluster_replace(const db1_con_t* handle, const db_key_t* keys,
		const db_val_t* vals, const int n, const int _un, const int _m);

/*! \brief
 * Returns the last inserted ID
 */
int db_cluster_last_inserted_id(const db1_con_t* _h);


/*! \brief
 * Returns number of affected rows for last query
 */
int db_cluster_affected_rows(const db1_con_t* _h);


/*! \brief
 * Insert a row into table, update on duplicate key
 */
int db_cluster_insert_update(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
	const int _n);


/*! \brief
 * Insert a row into table
 */
int db_cluster_insert_delayed(const db1_con_t* _h, const db_key_t* _k,
		const db_val_t* _v, const int _n);


/*! \brief
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_cluster_use_table(db1_con_t* _h, const str* _t);


#endif /* KM_DBASE_H */
