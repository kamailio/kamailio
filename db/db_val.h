/* 
 * $Id$ 
 */

#ifndef DB_VAL_H
#define DB_VAL_H

#include <time.h>


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

#define VAL_TYPE(dv)   ((dv)->type)
#define VAL_NULL(dv)   ((dv)->nul)
#define VAL_INT(dv)    ((dv)->val.int_val)
#define VAL_DOUBLE(dv) ((dv)->val.double_val)
#define VAL_TIME(dv)   ((dv)->val.time_val)
#define VAL_STRING(dv) ((dv)->val.string_val)


/*
 * Convert string to given type
 */
int str2val(db_type_t _t, db_val_t* _v, const char* _s);


/*
 * Convert given type to string
 */
int val2str(db_val_t* _v, char* _s, int* _len);


#endif
