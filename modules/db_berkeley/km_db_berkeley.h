/*
 * db_berkeley module, portions of this code were templated using
 * the dbtext and postgres modules.

 * Copyright (C) 2007 Cisco Systems
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

/*! \file
 * Berkeley DB : 
 *
 * \ingroup database
 */

#ifndef _KM_BDB_H_
#define _KM_BDB_H_

#include "../../lib/srdb1/db_con.h"
#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db_key.h"
#include "../../lib/srdb1/db_op.h"
#include "../../lib/srdb1/db_val.h"
#include "../../lib/srdb1/db.h"

/* reloads the berkeley db */
int bdb_reload(char* _n);

void bdb_check_reload(db1_con_t* _con);
int  bdb_use_table(db1_con_t* _h, const str* _t);

/*
 * Initialize database connection
 */
db1_con_t* bdb_init(const str* _sqlurl);


/*
 * Close a database connection
 */
void bdb_close(db1_con_t* _h);


/*
 * Free all memory allocated by get_result
 */
int bdb_free_query(db1_con_t* _h, db1_res_t* _r);


/*
 * Do a query
 */
int km_bdb_query(db1_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v, 
				 db_key_t* _c, int _n, int _nc, db_key_t _o, db1_res_t** _r);


/*
 * Raw SQL query
 */
int bdb_raw_query(db1_con_t* _h, char* _s, db1_res_t** _r);


/*
 * Insert a row into table
 */
int bdb_insert(db1_con_t* _h, db_key_t* _k, db_val_t* _v, int _n);


/*
 * Delete a row from table
 */
int bdb_delete(db1_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n);
int _bdb_delete_cursor(db1_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v, int _n);

/*
 * Update a row in table
 */
int bdb_update(db1_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v,
	      db_key_t* _uk, db_val_t* _uv, int _n, int _un);

int bdb_bind_api(db_func_t *dbb);

int km_mod_init(void);
void km_destroy(void);

#endif

