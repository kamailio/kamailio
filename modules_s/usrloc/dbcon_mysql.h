/* 
 * $Id$ 
 */

#ifndef __DBCON_MYSQL_H__
#define __DBCON_MYSQL_H__

#include "location.h"
#include "contact.h"
#include "db_con.h"
#include "db_val.h"
#include "db_row.h"
#include "db_key.h"
#include "db_res.h"


/*
 * Initialize database connection and
 * obtain the connection handle
 */
db_con_t* db_init(const char* _sqlurl);


/*
 * Close a database connection and free
 * all memory used
 */
void db_close (db_con_t* _h); 


/*
 * Query table for specified rows
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys that must match
 * _c: column names to return
 * _n: nmber of key=values pairs to compare
 * _nc: number of columns to return
 * _o: order by the specified column
 */
db_res_t* query_table (db_con_t* _h, db_key_t* _k, 
		       db_val_t* _v, db_key_t* _c, int* _n, int _nc);


/*
 * Insert a row into specified table
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys
 * _n: number of key=value pairs
 */
int insert_row (db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n);


/*
 * Delete a row from the specified table
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys that must match
 * _n: number of key=value pairs
 */
int delete_row (db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n);


/*
 * Update some rows in the specified table
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys that must match
 * _uk: updated columns
 * _uv: updated values of the columns
 * _n: number of key=value pairs
 * _un: number of columns to update
 */
int update_table (db_con_t* _h, db_key_t* _k, db_val_t* _v,
		  db_key_t* _uk, db_val_t* _uv, int _n, int _un);


#endif
