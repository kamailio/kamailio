/*
 * $Id$
 *
 * DBText module core functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * DBText module interface
 *  
 * 2003-01-30 created by Daniel
 * 
 */


#ifndef _DBTEXT_H_
#define _DBTEXT_H_

#include "../../lib/srdb2/db_con.h"
#include "../../lib/srdb2/db_res.h"
#include "../../db/db_key.h"
#include "../../db/db_op.h"
#include "../../db/db_val.h"


/*
 * Initialize database connection
 */
db_con_t* dbt_init(const char* _sqlurl);


/*
 * Close a database connection
 */
void dbt_close(db_con_t* _h);


/*
 * Free all memory allocated by get_result
 */
int dbt_free_query(db_con_t* _h, db_res_t* _r);


/*
 * Do a query
 */
int dbt_query(db_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v, 
			db_key_t* _c, int _n, int _nc, db_key_t _o, db_res_t** _r);


/*
 * Raw SQL query
 */
int dbt_raw_query(db_con_t* _h, char* _s, db_res_t** _r);


/*
 * Insert a row into table
 */
int dbt_insert(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n);


/*
 * Delete a row from table
 */
int dbt_delete(db_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n);


/*
 * Update a row in table
 */
int dbt_update(db_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v,
	      db_key_t* _uk, db_val_t* _uv, int _n, int _un);

#endif

