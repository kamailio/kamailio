#ifndef __DB_VAL_H__
#define __DB_VAL_H__

#include <time.h>


/*
 * Accepted column types
 */
typedef enum {
	DB_INT,
        DB_FLOAT,
	DB_STRING,
	DB_DATETIME
} db_type_t;


/*
 * Column value structure
 */
typedef struct {
	db_type_t type;                  /* Type of the value */
	union {
		int          int_val;    /* integer value */
		float        float_val;  /* float value */
		time_t       time_val;   /* unix time value */
		const char*  string_val; /* NULL terminated string */
	} val;                           /* union of all possible types */
} db_val_t;


#define SET_TYPE(val, t) do {                \
                              val->type = t; \
                            } while(0)

/*
 * Convert string to given type
 */
int str2val(db_type_t _t, db_val_t* _v, const char* _s);

/*
 * Convert given type to string
 */
int val2str(db_val_t* _v, char* _s, int* _len);

#endif
