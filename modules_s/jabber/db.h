/*
 * $Id$
 */

#ifndef __DB_H__
#define __DB_H__

#include <time.h>

/*
 * Generic database interface
 *
 * $id$
 */


/* =================== db_key =============== */

/*
 * Type of a database key (column)
 */
typedef const char* db_key_t;


/* =================== db_val =============== */

/*
 * Accepted column types
 */
typedef enum {
	DB_INT,
	DB_DOUBLE,
	DB_STRING,
	DB_DATETIME
} db_type_t;


/*
 * Column value structure
 */
typedef struct {
	db_type_t type;                  /* Type of the value */
	int nul;                         /* Means that the column in database has no value */
	union {
		int          int_val;    /* integer value */
		double       double_val; /* double value */
		time_t       time_val;   /* unix time value */
		const char*  string_val; /* NULL terminated string */
	} val;                           /* union of all possible types */
} db_val_t;


/*
 * Useful macros for accessing attributes of db_val structure
 */

/* Get value type */
#define VAL_TYPE(dv)   ((dv)->type)

/* Get null flag (means that value in dabase is null) */
#define VAL_NULL(dv)   ((dv)->nul)

/* Get integer value */
#define VAL_INT(dv)    ((dv)->val.int_val)

/* Get double value */
#define VAL_DOUBLE(dv) ((dv)->val.double_val)

/* Get time_t value */
#define VAL_TIME(dv)   ((dv)->val.time_val)

/* Get char* value */
#define VAL_STRING(dv) ((dv)->val.string_val)


/* ==================== db_con ======================= */

/*
 * This structure represents a database connection
 * and pointer to this structure is used as a connection
 * handle
 */
typedef struct {
	char* table;     /* Default table to use */
	void* con;       /* Mysql Connection */
	void* res;       /* Result of previous operation */
	void* row;       /* Actual row in the result */
	int   connected; /* TRUE if connection is established */
} db_con_t;


/* ===================== db_row ====================== */

/*
 * Structure holding result of query_table function (ie. table row)
 */
typedef struct db_row {
	db_val_t* values;  /* Columns in the row */
	int n;             /* Number of columns in the row */
} db_row_t;

/* Useful macros for manipulating db_row structure attributes */

/* Get row members */
#define ROW_VALUES(rw) ((rw)->values)

/* Get number of member in the row */
#define ROW_N(rw)      ((rw)->n)


/* ===================== db_res ====================== */

typedef struct db_res {
	struct {
		db_key_t* names;   /* Column names */
		db_type_t* types;  /* Column types */
		int n;             /* Number of columns */
	} col;
	struct db_row* rows;       /* Rows */
	int n;                     /* Number of rows */
} db_res_t;

/* Useful macros for manipulating db_res attributes */

/* Column names */
#define RES_NAMES(re) ((re)->col.names)

/* Column types */
#define RES_TYPES(re) ((re)->col.types)

/* Number of columns */
#define RES_COL_N(re) ((re)->col.n)

/* Rows */
#define RES_ROWS(re)  ((re)->rows)

/* Number of rows */
#define RES_ROW_N(re) ((re)->n)


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
int bind_dbmod(void);


extern db_func_t dbf;


#define db_use_table  (dbf.use_table)
#define db_init       (dbf.init)
#define db_close      (dbf.close)
#define db_query      (dbf.query)
#define db_free_query (dbf.free_query)
#define db_insert     (dbf.insert)
#define db_delete     (dbf.delete)
#define db_update     (dbf.update)
 
#endif
