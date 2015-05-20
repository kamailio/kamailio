/*
 * DBText module core functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */


#ifndef _DBTEXT_H_
#define _DBTEXT_H_

#include "../../lib/srdb1/db_con.h"
#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db_key.h"
#include "../../lib/srdb1/db_op.h"
#include "../../lib/srdb1/db_val.h"


/*
 * Initialize database connection
 */
db1_con_t* dbt_init(const str* _sqlurl);


/*
 * Close a database connection
 */
void dbt_close(db1_con_t* _h);


/*
 * Free all memory allocated by get_result
 */
int dbt_free_result(db1_con_t* _h, db1_res_t* _r);


/*
 * Do a query
 */
int dbt_query(db1_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v, 
			db_key_t* _c, int _n, int _nc, db_key_t _o, db1_res_t** _r);


/*
 * Raw SQL query
 */
int dbt_raw_query(db1_con_t* _h,  str* _s, db1_res_t** _r);


/*
 * Insert a row into table
 */
int dbt_insert(db1_con_t* _h, db_key_t* _k, db_val_t* _v, int _n);


/*
 * Delete a row from table
 */
int dbt_delete(db1_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n);


/*
 * Update a row in table
 */
int dbt_update(db1_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v,
	      db_key_t* _uk, db_val_t* _uv, int _n, int _un);

/*
 * Affected rows
 */
int dbt_affected_rows(db1_con_t* _h);

#endif

