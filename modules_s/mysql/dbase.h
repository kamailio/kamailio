/*
 * $Id$
 *
 * MySQL module core functions
 */

#ifndef DBASE_H
#define DBASE_H

#include "../../db/db_con.h"
#include "../../db/db_res.h"
#include "../../db/db_key.h"
#include "../../db/db_val.h"


/*
 * Initialize database connection
 */
db_con_t* db_init(const char* _sqlurl);


/*
 * Close a database connection
 */
void db_close(db_con_t* _h);


/*
 * Return result of previous query
 */
int get_result(db_con_t* _h, db_res_t** _r);


/*
 * Free all memory allocated by get_result
 */
int db_free_query(db_con_t* _h, db_res_t* _r);


/*
 * Do a query
 */
int db_query(db_con_t* _h, db_key_t* _k, db_val_t* _v, db_key_t* _c, int _n, int _nc,
	     db_key_t _o, db_res_t** _r);


/*
 * Insert a row into table
 */
int db_insert(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n);


/*
 * Delete a row from table
 */
int db_delete(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n);


/*
 * Update a row in table
 */
int db_update(db_con_t* _h, db_key_t* _k, db_val_t* _v,
	      db_key_t* _uk, db_val_t* _uv, int _n, int _un);


#endif /* DBASE_H */
