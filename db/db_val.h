/* 
 * $Id$ 
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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


#ifndef DB_VAL_H
#define DB_VAL_H

#include <time.h>
#include "../str.h"


/*
 * Accepted column types
 */
typedef enum {
	DB_INT,
        DB_DOUBLE,
	DB_STRING,
	DB_STR,
	DB_DATETIME,
	DB_BLOB
} db_type_t;


/*
 * Column value structure
 */
typedef struct {
	db_type_t type;                /* Type of the value */
	int nul;                       /* Means that the column in database
									  has no value */
	union {
		int          int_val;    /* integer value */
		double       double_val; /* double value */
		time_t       time_val;   /* unix time value */
		const char*  string_val; /* NULL terminated string */
		str          str_val;    /* str string value */
		str          blob_val;   /* Blob data */
	} val;                       /* union of all possible types */
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
#define VAL_STR(dv)    ((dv)->val.str_val)
#define VAL_BLOB(dv)   ((dv)->val.blob_val)


/*
 * Convert string to given type
 */
int str2val(db_type_t _t, db_val_t* _v, const char* _s, int _l);


/*
 * Convert given type to string
 */
int val2str(db_val_t* _v, char* _s, int* _len);


#endif /* DB_VAL_H */
