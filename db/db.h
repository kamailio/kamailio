/*
 * $Id$
 */

#ifndef DB_H
#define DB_H

#include "db_key.h"
#include "db_val.h"
#include "db_con.h"
#include "db_row.h"
#include "db_res.h"


/*
 * Specify table name that will be used for
 * subsequent operations
 */
typedef int (*db_use_table_f)(db_con_t* _h, const char* _t);


/*
 * Initialize database connection and
 * obtain the connection handle
 */
typedef db_con_t* (*db_init_f) (const char* _sqlurl);


/*
 * Close a database connection and free
 * all memory used
 */
typedef void (*db_close_f) (db_con_t* _h); 


/*
 * Query table for specified rows
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys that must match
 * _c: column names to return
 * _n: nmber of key=values pairs to compare
 * _nc: number of columns to return
 * _o: order by the specified column
 * _r: Result will be stored in this variable
 *     NULL if there is no result
 */
typedef int (*db_query_f) (db_con_t* _h, db_key_t* _k, 
			   db_val_t* _v, db_key_t* _c, 
			   int _n, int _nc,
			   db_key_t _o, db_res_t** _r);


/*
 * Free a result allocated by db_query
 * _h: structure representing database connection
 * _r: db_res structure
 */
typedef int (*db_free_query_f) (db_con_t* _h, db_res_t* _r);


/*
 * Insert a row into specified table
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys
 * _n: number of key=value pairs
 */
typedef int (*db_insert_f) (db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n);


/*
 * Delete a row from the specified table
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys that must match
 * _n: number of key=value pairs
 */
typedef int (*db_delete_f) (db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n);


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
typedef int (*db_update_f) (db_con_t* _h, db_key_t* _k, db_val_t* _v,
			    db_key_t* _uk, db_val_t* _uv, int _n, int _un);



typedef struct db_func{
	db_use_table_f  use_table;   /* Specify table name */
	db_init_f       init;        /* Initialize dabase connection */
	db_close_f      close;       /* Close database connection */
	db_query_f      query;       /* query a table */
	db_free_query_f free_query;  /* Free a query result */
	db_insert_f     insert;      /* Insert into table */
	db_delete_f     delete;      /* Delete from table */ 
	db_update_f     update;      /* Update table */
} db_func_t;


/*
 * Bind database module functions
 * returns TRUE if everything went OK
 * FALSE otherwise
 */

extern db_func_t dbf;


#define db_use_table  (dbf.use_table)
#define db_init       (dbf.init)
#define db_close      (dbf.close)
#define db_query      (dbf.query)
#define db_free_query (dbf.free_query)
#define db_insert     (dbf.insert)
#define db_delete     (dbf.delete)
#define db_update     (dbf.update)


int bind_dbmod(void);
 

#endif /* DB_H */
