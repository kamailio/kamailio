/* 
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006-2007 iptelorg GmbH
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

#ifndef _DB_FLD_H
#define _DB_FLD_H  1

/** \ingroup DB_API 
 * @{ 
 */

#include "db_gen.h"
#include "../../str.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


enum db_fld_type {
	DB_NONE = 0,   /* Bumper */
	DB_INT,        /* 32-bit integer */
	DB_FLOAT,      /* 32-bit fixed-precision number */
	DB_DOUBLE,     /* double data type */
	DB_CSTR,       /* Zero-terminated string */
	DB_STR,        /* str structure */
	DB_DATETIME,   /* Date and time in number of seconds since 1-Jan-1970 */
	DB_BLOB,       /* Generic binary object*/
	DB_BITMAP      /* Bitmap of flags */
};

extern char* db_fld_str[];

enum db_fld_op {
	DB_EQ = 0, /* The value of the field must be equal */
	DB_NE,     /* The value of the filed must be not equal */
	DB_LT,     /* The value of the field must be less than */
	DB_GT,     /* The value of the field must be greater than */
	DB_LEQ,    /* The value of the field must be less than or equal */
	DB_GEQ     /* The value of the field must be greater than or equal */
};

enum db_flags {
	DB_NULL = (1 << 0),  /**< The field is NULL, i.e. no value was provided */
	DB_NO_TZ = (1 << 1), /**< Inhibit time-zone shifts for timestamp fields */
};

/* union of all possible types */
typedef union db_fld_val {
	int          int4;   /* integer value */
	float        flt;    /* float value */
	double       dbl;    /* double value */
	time_t       time;   /* unix time value */
	char*        cstr;   /* NULL terminated string */
	str          lstr;   /* String with known length */
	str          blob;   /* Blob data */
	unsigned int bitmap; /* Bitmap data type, 32 flags, should be enough */ 
	long long    int8;   /* 8-byte integer */
} db_fld_val_t;

typedef struct db_fld {
	db_gen_t gen;  /* Generic part of the structure */
	char* name;
	enum db_fld_type type;
	unsigned int flags;
	db_fld_val_t v;
	enum db_fld_op op;
} db_fld_t;

#define DB_FLD_LAST(fld) ((fld).name == NULL)
#define DB_FLD_EMPTY(fld) ((fld) == NULL || (fld)[0].name == NULL)

struct db_fld* db_fld(size_t n);
void db_fld_free(struct db_fld* fld);

int db_fld_init(struct db_fld* fld);
void db_fld_close(struct db_fld* fld);

db_fld_t* db_fld_copy(db_fld_t* fld);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* _DB_FLD_H */
