/*
 * Copyright (C) 2007-2008 1&1 Internet AG
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

/**
 * \file lib/srdb1/db_query.h
 * \brief Query helper for database drivers
 * \ingroup db1
 *
 * This helper methods for database queries are used from the database
 * SQL driver to do the actual work. Each function uses some functions from
 * the actual driver with function pointers to the concrete, specific
 * implementation.
*/

#ifndef DB1_QUERY_H
#define DB1_QUERY_H

#include "db_key.h"
#include "db_op.h"
#include "db_val.h"
#include "db_con.h"
#include "db_row.h"
#include "db_res.h"
#include "db_cap.h"


/**
 * \brief Helper function for db queries
 *
 * This method evaluates the actual arguments for the database query and
 * setups the string that is used for the query in the db module.
 * Then its submit the query and stores the result if necessary. It uses for
 * its work the implementation in the concrete database module.
 *
 * \param _h structure representing database connection
 * \param _k key names, if not present the whole table will be returned
 * \param _op operators
 * \param _v values of the keys that must match
 * \param _c column names that should be returned
 * \param _n number of key/value pairs that are compared, if zero then no comparison is done
 * \param _nc number of colums that should be returned
 * \param _o order by the specificied column, optional
 * \param _r the result that is returned, set to NULL if you want to use fetch_result later
 * \param (*val2str) function pointer to the db specific val conversion function
 * \param (*submit_query) function pointer to the db specific query submit function
 * \param (*store_result) function pointer to the db specific store result function
 * \return zero on success, negative on errors
 */
int db_do_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
	const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
	const db_key_t _o, db1_res_t** _r, int (*val2str) (const db1_con_t*,
	const db_val_t*, char*, int*), int (*submit_query)(const db1_con_t* _h,
	const str* _c), int (*store_result)(const db1_con_t* _h, db1_res_t** _r));

/**
 * \brief Helper function for db queries with update lock
 *
 * This method evaluates the actual arguments for the database query and
 * setups the string that is used for the query in the db module.
 * Then its submit the query and stores the result if necessary. It uses for
 * its work the implementation in the concrete database module.
 *
 * \param _h structure representing database connection
 * \param _k key names, if not present the whole table will be returned
 * \param _op operators
 * \param _v values of the keys that must match
 * \param _c column names that should be returned
 * \param _n number of key/value pairs that are compared, if zero then no comparison is done
 * \param _nc number of colums that should be returned
 * \param _o order by the specificied column, optional
 * \param _r the result that is returned, set to NULL if you want to use fetch_result later
 * \param (*val2str) function pointer to the db specific val conversion function
 * \param (*submit_query) function pointer to the db specific query submit function
 * \param (*store_result) function pointer to the db specific store result function
 * \return zero on success, negative on errors
 */
int db_do_query_lock(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
	const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
	const db_key_t _o, db1_res_t** _r, int (*val2str) (const db1_con_t*,
	const db_val_t*, char*, int*), int (*submit_query)(const db1_con_t* _h,
	const str* _c), int (*store_result)(const db1_con_t* _h, db1_res_t** _r));

/**
 * \brief Helper function for raw db queries
 *
 * This method evaluates the actual arguments for the database raw query
 * and setups the string that is used for the query in the db module.
 * Then its submit the query and stores the result if necessary.
 * It uses for its work the implementation in the concrete database module.
 *
 * \param _h structure representing database connection
 * \param _s char holding the raw query
 * \param _r the result that is returned
 * \param (*submit_query) function pointer to the db specific query submit function
 * \param (*store_result) function pointer to the db specific store result function
 * \return zero on success, negative on errors
 */
int db_do_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r,
	int (*submit_query)(const db1_con_t* _h, const str* _c),
	int (*store_result)(const db1_con_t* _h, db1_res_t** _r));


/**
 * \brief Helper function for db insert operations
 *
 * This method evaluates the actual arguments for the database operation
 * and setups the string that is used for the insert operation in the db
 * module. Then its submit the query for the operation. It uses for its work
 * the implementation in the concrete database module.
 *
 * \param _h structure representing database connection
 * \param _k key names
 * \param _v values of the keys
 * \param _n number of key/value pairs 
 * \param (*val2str) function pointer to the db specific val conversion function
 * \param (*submit_query) function pointer to the db specific query submit function
 * \return zero on success, negative on errors
 */
int db_do_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
	const int _n, int (*val2str) (const db1_con_t*, const db_val_t*, char*, int*),
	int (*submit_query)(const db1_con_t* _h, const str* _c));


/**
 * \brief Helper function for db delete operations
 *
 * This method evaluates the actual arguments for the database operation
 * and setups the string that is used for the delete operation in the db
 * module. Then its submit the query for the operation. It uses for its work
 * the implementation in the concrete database module.
 *
 * \param _h structure representing database connection
 * \param _k key names
 * \param _o operators
 * \param _v values of the keys that must match
 * \param _n number of key/value pairs that are compared, if zero then the whole table is deleted
 * \param (*val2str) function pointer to the db specific val conversion function
 * \param (*submit_query) function pointer to the db specific query submit function
 * \return zero on success, negative on errors
 */
int db_do_delete(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
	const db_val_t* _v, const int _n, int (*val2str) (const db1_con_t*,
	const db_val_t*, char*, int*), int (*submit_query)(const db1_con_t* _h,
	const str* _c));


/**
 * \brief Helper function for db update operations
 *
 * This method evaluates the actual arguments for the database operation
 * and setups the string that is used for the update operation in the db
 * module. Then its submit the query for the operation. It uses for its work
 * the implementation in the concrete database module.
 *
 * \param _h structure representing database connection
 * \param _k key names, if not present the whole table will be returned
 * \param _o operators
 * \param _v values of the keys that must match
 * \param _uk: updated columns
 * \param _uv: updated values of the columns
 * \param _n number of key/value pairs that are compared, if zero then no comparison is done
 * \param _un: number of columns that should be updated
 * \param (*val2str) function pointer to the db specific val conversion function
 * \param (*submit_query) function pointer to the db specific query submit function
 * \return zero on success, negative on errors
 */
int db_do_update(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
	const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv, const int _n,
	const int _un, int (*val2str) (const db1_con_t*, const db_val_t*, char*, int*),
	int (*submit_query)(const db1_con_t* _h, const str* _c));


/**
 * \brief Helper function for db delete operations
 *
 * This helper method evaluates the actual arguments for the database operation
 * and setups the string that is used for the replace operation in the db
 * module. Then its submit the query for the operation. It uses for its work the
 * implementation in the concrete database module.
 *
 * \param _h structure representing database connection
 * \param _k key names, if not present the whole table will be returned
 * \param _v values of the keys that must match
 * \param _n number of key/value pairs that are compared, if zero then no comparison is done
 * \param (*val2str) function pointer to the db specific val conversion function
 * \param (*submit_query) function pointer to the db specific query submit function
 * \return zero on success, negative on errors
 */
int db_do_replace(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
	const int _n, int (*val2str) (const db1_con_t*, const db_val_t*, char*,
	int*), int (*submit_query)(const db1_con_t* _h, const str* _c));


/**
 * \brief Helper function for db insert delayed operations
 *
 * This method evaluates the actual arguments for the database operation
 * and setups the string that is used for the insert delayed operation in the db
 * module. Then its submit the query for the operation. It uses for its work
 * the implementation in the concrete database module.
 *
 * \param _h structure representing database connection
 * \param _k key names
 * \param _v values of the keys
 * \param _n number of key/value pairs
 * \param (*val2str) function pointer to the db specific val conversion function
 * \param (*submit_query) function pointer to the db specific query submit function
 * \return zero on success, negative on errors
 */
int db_do_insert_delayed(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
	const int _n, int (*val2str) (const db1_con_t*, const db_val_t*, char*, int*),
	int (*submit_query)(const db1_con_t* _h, const str* _c));


/**
 * \brief Initialisation function - should be called from db.c at start-up
 *
 * This initialises the db_query module, and should be called before any functions in db_query are called.
 *
 * \return zero on success, negative on errors
 */
int db_query_init(void);
    
#endif
