/*
 * $Id$
 *
 * UNIXODBC module core functions
 *
 * Copyright (C) 2005-2006 Marco Lorrai
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
 *
 * History:
 * --------
 *  2005-12-01  initial commit (chgen)
 */


#ifndef DBASE_H
#define DBASE_H

#include "../../lib/srdb1/db_con.h"
#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db_key.h"
#include "../../lib/srdb1/db_op.h"
#include "../../lib/srdb1/db_val.h"

/*
 * Initialize database connection
 */
db1_con_t* db_unixodbc_init(const str* _sqlurl);

/*
 * Close a database connection
 */
void db_unixodbc_close(db1_con_t* _h);

/*
 * Free all memory allocated by get_result
 */
int db_unixodbc_free_result(db1_con_t* _h, db1_res_t* _r);

/*
 * Do a query
 */
int db_unixodbc_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op, const db_val_t* _v,
const db_key_t* _c, const int _n, const int _nc, const db_key_t _o, db1_res_t** _r);

/*
 * Fetch rows from a result
 */
int db_unixodbc_fetch_result(const db1_con_t* _h, db1_res_t** _r, const int nrows);

/*
 * Raw SQL query
 */
int db_unixodbc_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r);

/*! \brief
 *  * Raw SQL query via async framework
 *   */
int db_unixodbc_raw_query_async(const db1_con_t* _h, const str* _s);

/*
 * Insert a row into table
 */
int db_unixodbc_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v, const int _n);

/*! \brief
 *  * Insert a row into table via async framework
 *   */
int db_unixodbc_insert_async(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v, const int _n);
/*
 * Delete a row from table
 */
int db_unixodbc_delete(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o, const db_val_t* _v,
const int _n);

/*
 * Update a row in table
 */
int db_unixodbc_update(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o, const db_val_t* _v,
const db_key_t* _uk, const db_val_t* _uv, const int _n, const int _un);

/*
 * Just like insert, but replace the row if it exists
 */
int db_unixodbc_replace(const db1_con_t* handle, const db_key_t* keys, const db_val_t* vals,
		const int n, const int _un, const int _m);

/*
 * Just like insert, but update the row if it exists or insert it if not. This function is used when
 * the odbc replace query is not supported.
 */
int db_unixodbc_update_or_insert(const db1_con_t* handle, const db_key_t* keys, const db_val_t* vals,
		const int n, const int _un, const int _m);

/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_unixodbc_use_table(db1_con_t* _h, const str* _t);

#endif                                                      /* DBASE_H */
