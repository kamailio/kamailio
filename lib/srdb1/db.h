/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2007-2008 1&1 Internet AG
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file db/db.h
 * \brief Generic Database Interface
 *
 * This is a generic database interface for modules that need to utilize a
 * database. The interface should be used by all modules that access database.
 * The interface will be independent of the underlying database server.
 * Notes:
 * If possible, use the predefined macros if you need to access any structure
 * attributes.
 * For additional description, see the comments in the sources of mysql module.
 *
 * If you want to see more complicated examples of how the API could be used,
 * take a look at the sources of the usrloc or auth modules.
 */

#ifndef DB_H
#define DB_H

#include "db_key.h"
#include "db_op.h"
#include "db_val.h"
#include "db_con.h"
#include "db_row.h"
#include "db_res.h"
#include "db_cap.h"
#include "db_con.h"


/**
 * \brief Specify table name that will be used for subsequent operations.
 * 
 * The function db_use_table takes a table name and stores it db_con_t structure.
 * All subsequent operations (insert, delete, update, query) are performed on
 * that table.
 * \param _h database connection handle
 * \param _t table name
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_use_table_f)(db_con_t* _h, const str * _t);

/**
 * \brief Initialize database connection and obtain the connection handle.
 *
 * This function initialize the database API and open a new database
 * connection. This function must be called after bind_dbmod but before any
 * other database API function is called.
 * 
 * The function takes one parameter, the parameter must contain the database
 * connection URL. The URL is of the form 
 * mysql://username:password@host:port/database where:
 * 
 * username: Username to use when logging into database (optional).
 * password: password if it was set (optional)
 * host:     Hosname or IP address of the host where database server lives (mandatory)
 * port:     Port number of the server if the port differs from default value (optional)
 * database: If the database server supports multiple databases, you must specify the
 * name of the database (optional).
 * \see bind_dbmod
 * \param _sqlurl database connection URL
 * \return returns a pointer to the db_con_t representing the connection if it was
 * successful, otherwise 0 is returned
 */
typedef db_con_t* (*db_init_f) (const str* _sqlurl);

/**
 * \brief Close a database connection and free all memory used.
 *
 * The function closes previously open connection and frees all previously 
 * allocated memory. The function db_close must be the very last function called.
 * \param _h db_con_t structure representing the database connection
 */
typedef void (*db_close_f) (db_con_t* _h); 


/**
 * \brief Query table for specified rows.
 *
 * This function implements the SELECT SQL directive.
 * If _k and _v parameters are NULL and _n is zero, you will get the whole table.
 *
 * if _c is NULL and _nc is zero, you will get all table columns in the result.
 * _r will point to a dynamically allocated structure, it is neccessary to call
 * db_free_result function once you are finished with the result.
 *
 * If _op is 0, equal (=) will be used for all key-value pairs comparisons.
 *
 * Strings in the result are not duplicated, they will be discarded if you call
 * db_free_result, make a copy yourself if you need to keep it after db_free_result.
 *
 * You must call db_free_result before you can call db_query again!
 * \see db_free_result
 *
 * \param _h database connection handle
 * \param _k array of column names that will be compared and their values must match
 * \param _op array of operators to be used with key-value pairs
 * \param _v array of values, columns specified in _k parameter must match these values
 * \param _c array of column names that you are interested in
 * \param _n number of key-value pairs to match in _k and _v parameters
 * \param _nc number of columns in _c parameter
 * \param _o order by statement for query
 * \param _r address of variable where pointer to the result will be stored
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_query_f) (const db_con_t* _h, const db_key_t* _k, const db_op_t* _op,
				const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
				const db_key_t _o, db_res_t** _r);

/**
 * \brief Fetch a number of rows from a result.
 * 
 * The function fetches a number of rows from a database result. If the number
 * of wanted rows is zero, the function returns anything with a result of zero.
 * \param _h structure representing database connection
 * \param _r structure for the result
 * \param _n the number of rows that should be fetched
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_fetch_result_f) (const db_con_t* _h, db_res_t** _r, const int _n);


/**
 * \brief Raw SQL query.
 *
 * This function can be used to do database specific queries. Please
 * use this function only if needed, as this creates portability issues
 * for the different databases. Also keep in mind that you need to
 * escape all external data sources that you use. You could use the
 * escape_common and unescape_common functions in the core for this task.
 * \see escape_common
 * \see unescape_common
 * \param _h structure representing database connection
 * \param _s the SQL query
 * \param _r structure for the result
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_raw_query_f) (const db_con_t* _h, const str* _s, db_res_t** _r);


/**
 * \brief Free a result allocated by db_query.
 *
 * This function frees all memory allocated previously in db_query. Its
 * neccessary to call this function on a db_res_t structure if you don't need the
 * structure anymore. You must call this function before you call db_query again!
 * \param _h database connection handle
 * \param _r pointer to db_res_t structure to destroy
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_free_result_f) (db_con_t* _h, db_res_t* _r);


/**
 * \brief Insert a row into the specified table.
 * 
 * This function implements INSERT SQL directive, you can insert one or more
 * rows in a table using this function.
 * \param _h database connection handle
 * \param _k array of keys (column names) 
 * \param _v array of values for keys specified in _k parameter
 * \param _n number of keys-value pairs int _k and _v parameters
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_insert_f) (const db_con_t* _h, const db_key_t* _k,
				const db_val_t* _v, const int _n);


/**
 * \brief Delete a row from the specified table.
 *
 * This function implements DELETE SQL directive, it is possible to delete one or
 * more rows from a table.
 * If _k is NULL and _v is NULL and _n is zero, all rows are deleted, the
 * resulting table will be empty.
 * If _o is NULL, the equal operator "=" will be used for the comparison.
 * 
 * \param _h database connection handle
 * \param _k array of keys (column names) that will be matched
 * \param _o array of operators to be used with key-value pairs
 * \param _v array of values that the row must match to be deleted
 * \param _n number of keys-value parameters in _k and _v parameters
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_delete_f) (const db_con_t* _h, const db_key_t* _k, const db_op_t* _o,
				const db_val_t* _v, const int _n);


/**
 * \brief Update some rows in the specified table.
 *
 * The function implements UPDATE SQL directive. It is possible to modify one
 * or more rows in a table using this function.
 * \param _h database connection handle
 * \param _k array of keys (column names) that will be matched
 * \param _o array of operators to be used with key-value pairs
 * \param _v array of values that the row must match to be modified
 * \param _uk array of keys (column names) that will be modified
 * \param _uv new values for keys specified in _k parameter
 * \param _n number of key-value pairs in _k and _v parameters
 * \param _un number of key-value pairs in _uk and _uv parameters
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_update_f) (const db_con_t* _h, const db_key_t* _k, const db_op_t* _o,
				const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv,
				const int _n, const int _un);


/**
 * \brief Insert a row and replace if one already exists.
 *
 * The function implements the REPLACE SQL directive. It is possible to insert
 * a row and replace if one already exists. The old row will be deleted before
 * the insertion of the new data.
 * \param _h structure representing database connection
 * \param _k key names
 * \param _v values of the keys
 * \param _n number of key=value pairs
 * \return returns 0 if everything is OK, otherwise returns value < 0
*/
typedef int (*db_replace_f) (const db_con_t* handle, const db_key_t* keys,
				const db_val_t* vals, const int n);


/**
 * \brief Retrieve the last inserted ID in a table.
 *
 * The function returns the value generated for an AUTO_INCREMENT column by the
 * previous INSERT or UPDATE  statement. Use this function after you have 
 * performed an INSERT statement into a table that contains an AUTO_INCREMENT
 * field.
 * \param _h structure representing database connection
 * \return returns the ID as integer or returns 0 if the previous statement
   does not use an AUTO_INCREMENT value.
 */
typedef int (*db_last_inserted_id_f) (const db_con_t* _h);


/**
 * \brief Insert a row into specified table, update on duplicate key.
 * 
 * The function implements the INSERT ON DUPLICATE KEY UPDATE SQL directive.
 * It is possible to insert a row and update if one already exists.
 * The old row will not deleted before the insertion of the new data.
 * \param _h structure representing database connection
 * \param _k key names
 * \param _v values of the keys
 * \param _n number of key=value pairs
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_insert_update_f) (const db_con_t* _h, const db_key_t* _k,
				const db_val_t* _v, const int _n);


/**
 * \brief Database module callbacks
 * 
 * This structure holds function pointer to all database functions. Before this
 * structure can be used it must be initialized with bind_dbmod.
 * \see bind_dbmod
 */
typedef struct db_func {
	unsigned int      cap;           /* Capability vector of the database transport */
	db_use_table_f    use_table;     /* Specify table name */
	db_init_f         init;          /* Initialize database connection */
	db_close_f        close;         /* Close database connection */
	db_query_f        query;         /* query a table */
	db_fetch_result_f fetch_result;  /* fetch result */
	db_raw_query_f    raw_query;     /* Raw query - SQL */
	db_free_result_f  free_result;   /* Free a query result */
	db_insert_f       insert;        /* Insert into table */
	db_delete_f       delete;        /* Delete from table */ 
	db_update_f       update;        /* Update table */
	db_replace_f      replace;       /* Replace row in a table */
	db_last_inserted_id_f  last_inserted_id;  /* Retrieve the last inserted ID
	                                            in a table */
	db_insert_update_f insert_update; /* Insert into table, update on duplicate key */ 
} db_func_t;


/**
 * \brief Bind database module functions
 *
 * This function is special, it's only purpose is to call find_export function in
 * the core and find the addresses of all other database related functions. The 
 * db_func_t callback given as parameter is updated with the found addresses.
 *
 * This function must be called before any other database API call!
 *
 * The database URL is of the form "mysql://username:password@host:port/database" or
 * "mysql" (database module name).
 * In the case of a database connection URL, this function looks only at the first
 * token (the database protocol). In the example above that would be "mysql":
 * \see db_func_t
 * \param mod database connection URL or a database module name
 * \param dbf database module callbacks
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
int db_bind_mod(const str* mod, db_func_t* dbf);


/**
 * \brief Helper for db_init function.
 *
 * This helper method do the actual work for the database specific db_init
 * functions.
 * \param url database connection URL
 * \param (*new_connection)() Pointer to the db specific connection creation method
 * \return returns a pointer to the db_con_t representing the connection if it was
   successful, otherwise 0 is returned.
 */
db_con_t* db_do_init(const str* url, void* (*new_connection)());


/**
 * \brief Helper for db_close function.
 *
 * This helper method does some work for the closing of a database 
 * connection. No function should be called after this
 * \param _h database connection handle
 * \param (*free_connection) Pointer to the db specifc free_connection method
 */
void db_do_close(db_con_t* _h, void (*free_connection)());


/**
 * \brief Get the version of a table.
 *
 * Returns the version number of a given table from the version table.
 * \param dbf database module callbacks
 * \param con database connection handle
 * \param table checked table
 * \return the version number if present, 0 if no version data available, < 0 on error
 */
int db_table_version(const db_func_t* dbf, db_con_t* con, const str* table);


/**
 * \brief Stores the name of a table.
 *
 * Stores the name of the table that will be used by subsequent database
 * functions calls in a db_con_t structure.
 * \param _h database connection handle
 * \param _t stored name
 * \return 0 if everything is ok, otherwise returns value < 0
 */
int db_use_table(db_con_t* _h, const str* _t);


#endif /* DB_H */
