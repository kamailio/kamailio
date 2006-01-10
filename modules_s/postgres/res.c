/* 
 * $Id$ 
 *
 * Postgres module result related functions
 *
 * Portions Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2003 August.Net Services, LLC
 * Portions Copyright (C) 2005 iptelorg GmbH
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

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "pg_type.h"
#include "pg_con.h"
#include "db_mod.h"
#include "res.h"
#include <netinet/in.h>
#include <string.h>


/*
 * Get and convert columns from a result
 */
static inline int get_columns(db_res_t* res)
{
	PGresult* pgres;
	int n, i, type;

	if (!res) {
	        ERR("Invalid parameter\n");
		goto err;
	}

	pgres = (PGresult*)res->data;
	if (!pgres) {
		ERR("No postgres result found\n");
		goto err;
	}

	n = PQnfields(pgres);
	if (!n) {
		ERR("No columns\n");
		goto err;
	}

	if (n == 0) return 0;

        res->col.names = (db_key_t*)pkg_malloc(sizeof(db_key_t) * n);
	if (!res->col.names) {
		ERR("No memory left\n");
		goto err;
	}

	res->col.types = (db_type_t*)pkg_malloc(sizeof(db_type_t) * n);
	if (!res->col.types) {
		ERR("No memory left\n");
		goto err;
	}

	res->col.n = n;

	for(i = 0; i < n; i++) {
		if (PQfformat(pgres, i) == 0) {
			ERR("Text format of columns not supported\n");
			goto err;
		}

		res->col.names[i] = PQfname(pgres, i);
		type = PQftype(pgres, i);
		switch(type) {
		case BOOLOID:    /* boolean, 'true'/'false' */
		case INT2OID:    /* -32 thousand to 32 thousand, 2-byte storage */
		case INT4OID:    /* -2 billion to 2 billion integer, 4-byte storage */
		case INT8OID:    /* ~18 digit integer, 8-byte storage */
			res->col.types[i] = DB_INT;
			break;

		case FLOAT4OID:  /* single-precision floating point number, 4-byte storage */
			res->col.types[i] = DB_FLOAT;
			break;

		case FLOAT8OID:  /* double-precision floating point number, 8-byte storage */
			res->col.types[i] = DB_DOUBLE;
			break;

		case TIMESTAMPOID:   /* date and time */
		case TIMESTAMPTZOID: /* date and time with time zone */
			res->col.types[i] = DB_DATETIME;
			break;

		case CHAROID:    /* single character */
		case TEXTOID:    /* variable-length string, no limit specified */
		case BPCHAROID:  /* char(length), blank-padded string, fixed storage length */
		case VARCHAROID: /* varchar(length), non-blank-padded string, variable storage length */
			res->col.types[i] = DB_STRING;
			break;

		case BYTEAOID: /* variable-length string, binary values escaped" */
			res->col.types[i] = DB_BLOB;
			break;

		case BITOID:    /* fixed-length bit string */
		case VARBITOID: /* variable-length bit string */
			res->col.types[i] = DB_BITMAP;
			break;

		default:
			ERR("Unsupported column type with oid %d\n", type);
			goto err;
		}
	}
	return 0;

 err:
	if (res->col.types) pkg_free(res->col.types);
	if (res->col.names) pkg_free(res->col.names);
	res->col.types = 0;
	res->col.names = 0;
	return -1;
}


/*
 * Release memory used by columns
 */
static inline int free_columns(db_res_t* res)
{
	if (!res) {
		ERR("Invalid parameter\n");
		return -1;
	}

	if (res->col.names) pkg_free(res->col.names);
	if (res->col.types) pkg_free(res->col.types);
	return 0;
}


/*
 * Release memory used by rows
 */
static inline void free_rows(db_res_t* res)
{
	int r;

	if (!res->rows) return;

	for(r = 0; r < res->n; r++) {
		pkg_free(res->rows[r].values);
	}
	
	pkg_free(res->rows);
}


static inline int convert_cell(db_con_t* con, db_res_t* res, int row, int col)
{
	static str dummy_str = STR_STATIC_INIT("");
	PGresult* pgres;
	db_val_t* val;
	int type, pglen;
	const char* pgval;
	union {
		int i4;
		long long i8;
		float f4;
		double f8;
		char c[8];
	} tmp;

	val = &res->rows[row].values[col];
	pgres = (PGresult*)res->data;

	val->type = res->col.types[col];

	if (PQgetisnull(pgres, row, col)) {
		val->nul = 1;
		switch(res->col.types[col]) {
		case DB_INT:      val->val.int_val = 0;              break;
		case DB_FLOAT:    val->val.float_val = 0;            break;
		case DB_DOUBLE:   val->val.double_val = 0;           break;
		case DB_STRING:   val->val.string_val = dummy_str.s; break;
		case DB_STR:      val->val.str_val = dummy_str;      break;
		case DB_DATETIME: val->val.time_val = 0;             break;
		case DB_BLOB:     val->val.blob_val = dummy_str;     break;
		case DB_BITMAP:   val->val.bitmap_val = 0;           break;
		}
		return 0;
	}

	val->nul = 0;
	type = PQftype(pgres, col);
	pgval = PQgetvalue(pgres, row, col);
	pglen = PQgetlength(pgres, row, col);
	
	     /* Postgres delivers binary parameters in network byte order,
	      * thus we have to convert them to host byte order. All data 
	      * returned by PQgetvalue is zero terminated. Memory allocator
	      * in libpq aligns data in memory properly so reading multibyte
	      * values from memory at once is safe.
	      */
	switch(type) {
	case BOOLOID: 
		val->val.int_val = *pgval; 
		break;

	case INT2OID: 
		val->val.int_val = ntohs(*(unsigned short*)pgval);
		break;

	case FLOAT4OID:
		     /* FLOAT4 will be stored in (8-byte) double */
		     /* FIXME: More efficient implementation could be done here
		      * provided that we know that the numbers are stored in IEEE 754
		      */
		tmp.i4 = ntohl(*(unsigned int*)pgval);
		val->val.double_val = tmp.f4;
		break;

	case INT4OID: 
		val->val.int_val = ntohl(*(unsigned int*)pgval);
		break;
		
	case INT8OID: 
		val->val.int_val = ntohl(*(unsigned int*)(pgval + 4));
		break; 

	case FLOAT8OID:
		tmp.i8 = (((unsigned long long)ntohl(*(unsigned int*)pgval)) << 32) + 
			(unsigned int)ntohl(*(unsigned int*)(pgval + 4));
		val->val.double_val = tmp.f8;
		break;

	case TIMESTAMPOID:
	case TIMESTAMPTZOID:
		tmp.i8 = (((unsigned long long)ntohl(*(unsigned int*)pgval)) << 32) + 
			(unsigned int)ntohl(*(unsigned int*)(pgval + 4));
		if (CON_FLAGS(con) & PG_INT8_TIMESTAMP) {
			     /* int8 format */
			val->val.time_val = tmp.i8 / 1000000 + PG_EPOCH_TIME;
		} else {
			     /* double format */
			val->val.time_val = PG_EPOCH_TIME + (long long)tmp.f8;
		}
		break;
		
	case CHAROID:    /* single character */
	case TEXTOID:    /* variable-length string, no limit specified */
	case BPCHAROID:  /* char(length), blank-padded string, fixed storage length */
	case VARCHAROID: /* varchar(length), non-blank-padded string, variable storage length */
		val->val.str_val.s = (char*)pgval;
		val->val.str_val.len = pglen;
		break;
		
	case BYTEAOID: /* variable-length string, binary values escaped" */
		val->val.blob_val.s = (char*)pgval;
		val->val.blob_val.len = pglen;
		break;
		
	case BITOID:    /* fixed-length bit string */
	case VARBITOID: /* variable-length bit string */
		if (ntohl(*(unsigned int*)pgval) != 32) {
			ERR("Only 32-bit long bitfieds supported\n");
			return -1;
		}
		val->val.bitmap_val = ntohl(*(unsigned int*)(pgval + 4));
		break;
		
	default:
		ERR("Unsupported column type with oid %d\n", type);
		return -1;
		
	}

	return 0;
}
 
 
/*
 * Convert rows from postgres to db API representation
 */
static inline int convert_rows(db_con_t* con, db_res_t* res)
{
	db_row_t* row;
	int r, c;

	if (!res) {
		ERR("Invalid parameter\n");
		return -1;
	}

	res->n = PQntuples((PGresult*)res->data); /* Number of rows */

	     /* Assert: number of columns is > 0, otherwise get_columns would fail */
	if (!res->n) {
		res->rows = 0;
		return 0;
	}

	res->rows = (struct db_row*)pkg_malloc(sizeof(db_row_t) * res->n);
	if (!res->rows) {
		ERR("No memory left\n");
		goto err;
	}

	for(r = 0; r < res->n; r++) {
		row = &res->rows[r];
		row->values = (db_val_t*)pkg_malloc(sizeof(db_val_t) * res->col.n);
		if (!row->values) {
			ERR("No memory left to allocate row\n");
			res->n = r; /* This is to make sure that the cleanup function release only rows
				     * that has been really allocated
				     */
			goto err;
		}
		row->n = res->col.n;

		for(c = 0; c < row->n; c++) {
			if (convert_cell(con, res, r, c) < 0) {
				row->n = c;
				res->n = r;
				goto err;
			}
		}
	}

	return 0;

 err:
	     /*	free_rows(res); Do not free here, pg_free_result will take care of it */
	return -1;
}


/*
 * Create a new result structure and initialize it
 */
db_res_t* pg_new_result(PGresult* pgres)
{
	db_res_t* r;
	r = (db_res_t*)pkg_malloc(sizeof(db_res_t));
	if (!r) {
		ERR("No memory left\n");
		return 0;
	}

	memset(r, 0, sizeof(db_res_t));
	r->data = pgres;
	return r;
}


/*
 * Fill the structure with data from database
 */
int pg_convert_result(db_res_t* res, db_con_t* con)
{
	if (!res) {
		ERR("Invalid parameter\n");
		return -1;
	}

	if (get_columns(res) < 0) {
		ERR("Error while getting column names\n");
		return -2;
	}

	if (convert_rows(con, res) < 0) {
		ERR("Error while converting rows\n");
		     /* Do not free columns here, pg_free_result will do it */
		return -3;
	}
	return 0;
}


/*
 * Release memory used by a result structure
 */
int pg_free_result(db_res_t* res)
{
	if (!res) {
		ERR("Invalid parameter\n");
		return -1;
	}

	free_columns(res);
	free_rows(res);
	if (res->data) PQclear((PGresult*)res->data);
	pkg_free(res);
	return 0;
}
