/* 
 * Copyright (C) 2001-2003 FhG Fokus
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
 */

/**
 * \file lib/srdb1/db_val.h
 * \brief Data structures that represents values in the database.
 *
 * This file defines data structures that represents values in the database.
 * Several datatypes are recognized and converted by the database API.
 * Available types: DB1_INT, DB1_DOUBLE, DB1_STRING, DB1_STR, DB1_DATETIME, DB1_BLOB and DB1_BITMAP
 * It also provides some macros for convenient access to this values,
 * and a function to convert a str to a value.
 * \ingroup db1
 */


#ifndef DB1_VAL_H
#define DB1_VAL_H

#include "db_con.h"
#include <time.h>
#include "../../str.h"


/**
 * Each cell in a database table can be of a different type. To distinguish
 * among these types, the db_type_t enumeration is used. Every value of the
 * enumeration represents one datatype that is recognized by the database
 * API.
 */
typedef enum {
	DB1_INT,        /**< represents an 32 bit integer number      */
	DB1_BIGINT,     /**< represents an 64 bit integer number      */
	DB1_DOUBLE,     /**< represents a floating point number       */
	DB1_STRING,     /**< represents a zero terminated const char* */
	DB1_STR,        /**< represents a string of 'str' type        */
	DB1_DATETIME,   /**< represents date and time                 */
	DB1_BLOB,       /**< represents a large binary object         */
	DB1_BITMAP,     /**< an one-dimensional array of 32 flags     */
	DB1_UNKNOWN     /**< represents an unknown type               */
} db_type_t;


/**
 * This structure represents a value in the database. Several datatypes are
 * recognized and converted by the database API. These datatypes are automaticaly
 * recognized, converted from internal database representation and stored in the
 * variable of corresponding type.
 *
 * Module that want to use this values needs to copy them to another memory
 * location, because after the call to free_result there are not more available.
 *
 * If the structure holds a pointer to a string value that needs to be freed
 * because the module allocated new memory for it then the free flag must
 * be set to a non-zero value. A free flag of zero means that the string
 * data must be freed internally by the database driver.
 */
typedef struct {
	db_type_t type; /**< Type of the value                              */
	int nul;		/**< Means that the column in database has no value */
	int free;		/**< Means that the value should be freed */
	/** Column value structure that holds the actual data in a union.  */
	union {
		int           int_val;    /**< integer value              */
		long long     ll_val;     /**< long long value            */
		double        double_val; /**< double value               */
		time_t        time_val;   /**< unix time_t value          */
		const char*   string_val; /**< zero terminated string     */
		str           str_val;    /**< str type string value      */
		str           blob_val;   /**< binary object data         */
		unsigned int  bitmap_val; /**< Bitmap data type           */
	} val;
} db_val_t;


/**
 * Useful macros for accessing attributes of db_val structure.
 * All macros expect a reference to a db_val_t variable as parameter.
 */

/**
 * Use this macro if you need to set/get the type of the value.
 */
#define VAL_TYPE(dv)   ((dv)->type)


/**
 * Use this macro if you need to set/get the null flag. A non-zero flag means that
 * the corresponding cell in the database contains no data (a NULL value in MySQL
 * terminology).
 */
#define VAL_NULL(dv)   ((dv)->nul)


/**
 * Use this macro if you need to set/ get the free flag. A non-zero flag means that
 * the corresponding cell in the database contains data that must be freed from the
 * DB API.
 */
#define VAL_FREE(dv)   ((dv)->free)


/**
 * Use this macro if you need to access the integer value in the db_val_t structure.
 */
#define VAL_INT(dv)    ((dv)->val.int_val)


/**
 * Use this macro if you need to access the integer value in the db_val_t structure
 * casted to unsigned int.
 */
#define VAL_UINT(dv)    ((unsigned int)(dv)->val.int_val)


/**
 * Use this macro if you need to access the long long value in the db_val_t structure.
 */
#define VAL_BIGINT(dv)    ((dv)->val.ll_val)


/**
 * Use this macro if you need to access the double value in the db_val_t structure.
 */
#define VAL_DOUBLE(dv) ((dv)->val.double_val)


/**
 * Use this macro if you need to access the time_t value in the db_val_t structure.
 */
#define VAL_TIME(dv)   ((dv)->val.time_val)


/**
 * Use this macro if you need to access the string value in the db_val_t structure.
 */
#define VAL_STRING(dv) ((dv)->val.string_val)


/**
 * Use this macro if you need to access the str structure in the db_val_t structure.
 */
#define VAL_STR(dv)    ((dv)->val.str_val)


/**
 * Use this macro if you need to access the blob value in the db_val_t structure.
 */
#define VAL_BLOB(dv)   ((dv)->val.blob_val)


/**
 * Use this macro if you need to access the bitmap value in the db_val_t structure.
 */
#define VAL_BITMAP(dv) ((dv)->val.bitmap_val)


/*!
 * \brief Convert a str to a db value
 *
 * Convert a str to a db value, does not copy strings if _cpy is zero
 * \param _t destination value type
 * \param _v destination value
 * \param _s source string
 * \param _l string length
 * \param _cpy when set to zero does not copy strings, otherwise copy strings
 * \return 0 on success, negative on error
 */
int db_str2val(const db_type_t _t, db_val_t* _v, const char* _s, const int _l,
		const unsigned int _cpy);


/*!
 * \brief Convert a numerical value to a string
 *
 * Convert a numerical value to a string, used when converting result from a query.
 * Implement common functionality needed from the databases, does parameter checking.
 * \param _c database connection
 * \param _v source value
 * \param _s target string
 * \param _len target string length
 * \return 0 on success, negative on error, 1 if value must be converted by other means
 */
int db_val2str(const db1_con_t* _c, const db_val_t* _v, char* _s, int* _len);

#endif /* DB1_VAL_H */
