/*
 * $Id$
 *
 * Postgres module core functions
 *
 * Portions Copyright (C) 2001-2005 FhG FOKUS
 * Copyright (C) 2003 August.Net Services, LLC
 * Portions Copyright (C) 2005 iptelorg GmbH
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef _DBASE_H
#define _DBASE_H


#include "../../db/db_con.h"
#include "../../db/db_res.h"
#include "../../db/db_key.h"
#include "../../db/db_op.h"
#include "../../db/db_val.h"


/*
 * Initialize database connection
 */
db_con_t* pg_init(const char* uri);


/*
 * Close a database connection
 */
void pg_close(db_con_t* con);


/*
 * Free all memory allocated by get_result
 */
int pg_db_free_result(db_con_t* con, db_res_t* res);


/*
 * Do a query
 */
int pg_query(db_con_t* con, db_key_t* keys, db_op_t* ops, db_val_t* vals, db_key_t* cols, int n, int nc,
	     db_key_t order, db_res_t** res);


/*
 * Raw SQL query
 */
int pg_raw_query(db_con_t* con, char* query, db_res_t** res);


/*
 * Insert a row into table
 */
int pg_insert(db_con_t* con, db_key_t* keys, db_val_t* vals, int n);


/*
 * Delete a row from table
 */
int pg_delete(db_con_t* con, db_key_t* keys, db_op_t* ops, db_val_t* vals, int n);


/*
 * Update a row in table
 */
int pg_update(db_con_t* con, db_key_t* keys, db_op_t* ops, db_val_t* vals,
	      db_key_t* ucols, db_val_t* uvals, int n, int un);



/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int pg_use_table(db_con_t* con, const char* table);


#endif /* _DBASE_H */
