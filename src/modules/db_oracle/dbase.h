/*
 * $Id$
 *
 * Oracle module core functions
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
db1_con_t* db_oracle_init(const str* _sqlurl);


/*
 * Close a database connection
 */
void db_oracle_close(db1_con_t* _h);


/*
 * Free all memory allocated by get_result
 */
int db_oracle_free_result(db1_con_t* _h, db1_res_t* _r);


/*
 * Do a query
 */
int db_oracle_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
		const db_val_t* _v, const db_key_t* _c, int _n, int _nc,
		const db_key_t _o, db1_res_t** _r);


/*
 * Raw SQL query
 */
int db_oracle_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r);


/*
 * Insert a row into table
 */
int db_oracle_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
		int _n);


/*
 * Delete a row from table
 */
int db_oracle_delete(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, int _n);


/*
 * Update a row in table
 */
int db_oracle_update(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv,
		int _n, int _un);


/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_oracle_use_table(db1_con_t* _h, const str* _t);


/*
 * Make error message. Always return negative value
 */
int sql_buf_small(void);

#endif /* DBASE_H */
