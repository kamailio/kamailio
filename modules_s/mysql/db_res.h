/* 
 * $Id$ 
 */

#ifndef __DB_RES_H__
#define __DB_RES_H__

#include "db_row.h"
#include "db_key.h"
#include "db_val.h"
#include "db_con.h"


struct db_row;

typedef struct db_res {
	struct {
		db_key_t* names;   /* Column names */
		db_type_t* types;  /* Column types */
		int n;             /* Number of columns */
	} col;
	struct db_row* rows;       /* Rows */
	int n;                     /* Number of rows */
} db_res_t;

#define RES_NAMES(re) ((re)->col.names)
#define RES_TYPES(re) ((re)->col.types)
#define RES_COL_N(re) ((re)->col.n)
#define RES_ROWS(re)  ((re)->rows)
#define RES_ROW_N(re) ((re)->n)

/*
 * Create a new result structure 
 */
db_res_t* new_result(void);


/*
 * Fill the structure with data from database
 */
int convert_result(db_con_t* _h, db_res_t* _r);


/*
 * Free all memory allocated by the structure
 */
int free_result(db_res_t* _r);


#endif
