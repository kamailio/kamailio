/*
 * $Id$
 *
 * POSTGRES module, portions of this code were templated using
 * the mysql module, thus it's similarity.
 *
 * Copyright (C) 2003 August.Net Services, LLC
 * Copyright (C) 2006 Norman Brandinger
 * Copyright (C) 2008 1&1 Internet AG
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * ---
 *
 * History
 * -------
 * 2003-04-06 initial code written (Greg Fausak/Andy Fullford)
 *
 * 2006-07-26 added BPCHAROID as a valid type for DB_STRING conversions
 *            this removes the "unknown type 1042" log messages (norm)
 *
 * 2006-10-27 Added fetch support (norm)
 *            Removed dependency on aug_* memory routines (norm)
 *            Added connection pooling support (norm)
 *            Standardized API routines to pg_* names (norm)
 *
 */

#include <stdlib.h>
#include <string.h>
#include "../../db/db_id.h"
#include "../../db/db_res.h"
#include "../../db/db_con.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "res.h"
#include "val.h"
#include "pg_con.h"
#include "pg_type.h"



/**
 * Fill the result structure with data from the query
 */
int db_postgres_convert_result(const db_con_t* _h, db_res_t* _r)
{
	if (!_h || !_r)  {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (db_postgres_get_columns(_h, _r) < 0) {
		LM_ERR("failed to get column names\n");
		return -2;
	}

	if (db_postgres_convert_rows(_h, _r) < 0) {
		LM_ERR("failed to convert rows\n");
		db_free_columns(_r);
		return -3;
	}
	return 0;
}

/**
 * Get and convert columns from a result set
 */
int db_postgres_get_columns(const db_con_t* _h, db_res_t* _r)
{
	int col, datatype;

	if (!_h || !_r)  {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	/* Get the number of rows (tuples) in the query result. */
	RES_ROW_N(_r) = PQntuples(CON_RESULT(_h));

	/* Get the number of columns (fields) in each row of the query result. */
	RES_COL_N(_r) = PQnfields(CON_RESULT(_h));

	if (!RES_COL_N(_r)) {
		LM_DBG("no columns returned from the query\n");
		return -2;
	} else {
		LM_DBG("%d columns returned from the query\n", RES_COL_N(_r));
	}

	if (db_allocate_columns(_r, RES_COL_N(_r)) != 0) {
		LM_ERR("could not allocate columns\n");
		return -3;
	}

	/* For each column both the name and the OID number of the data type are saved. */
	for(col = 0; col < RES_COL_N(_r); col++) {

		RES_NAMES(_r)[col] = (str*)pkg_malloc(sizeof(str));
		if (! RES_NAMES(_r)[col]) {
			LM_ERR("no private memory left\n");
			db_free_columns(_r);
			return -4;
		}
		LM_DBG("allocate %d bytes for RES_NAMES[%d] at %p\n", (unsigned int) sizeof(str), col,
				RES_NAMES(_r)[col]);

		/* The pointer that is here returned is part of the result structure. */
		RES_NAMES(_r)[col]->s = PQfname(CON_RESULT(_h), col);
		RES_NAMES(_r)[col]->len = strlen(PQfname(CON_RESULT(_h), col));

		LM_DBG("RES_NAMES(%p)[%d]=[%.*s]\n", RES_NAMES(_r)[col], col,
				RES_NAMES(_r)[col]->len, RES_NAMES(_r)[col]->s);

		/* get the datatype of the column */
		switch(datatype = PQftype(CON_RESULT(_h),col))
		{
			case INT2OID:
			case INT4OID:
			case INT8OID:
				LM_DBG("use DB_INT result type\n");
				RES_TYPES(_r)[col] = DB_INT;
			break;

			case FLOAT4OID:
			case FLOAT8OID:
			case NUMERICOID:
				LM_DBG("use DB_DOUBLE result type\n");
				RES_TYPES(_r)[col] = DB_DOUBLE;
			break;

			case DATEOID:
			case TIMESTAMPOID:
			case TIMESTAMPTZOID:
				LM_DBG("use DB_DATETIME result type\n");
				RES_TYPES(_r)[col] = DB_DATETIME;
			break;

			case BOOLOID:
			case CHAROID:
			case VARCHAROID:
			case BPCHAROID:
				LM_DBG("use DB_STRING result type\n");
				RES_TYPES(_r)[col] = DB_STRING;
			break;

			case TEXTOID:
			case BYTEAOID:
				LM_DBG("use DB_BLOB result type\n");
				RES_TYPES(_r)[col] = DB_BLOB;
			break;

			case BITOID:
			case VARBITOID:
				LM_DBG("use DB_BITMAP result type\n");
				RES_TYPES(_r)[col] = DB_BITMAP;
			break;
				
			default:
				LM_WARN("unhandled data type column (%.*s) type id (%d), "
						"use DB_STRING as default\n", RES_NAMES(_r)[col]->len,
						RES_NAMES(_r)[col]->s, datatype);
				RES_TYPES(_r)[col] = DB_STRING;
			break;
		}
	}
	return 0;
}

/**
 * Convert rows from PostgreSQL to db API representation
 */
int db_postgres_convert_rows(const db_con_t* _h, db_res_t* _r)
{
	char **row_buf, *s;
	int row, col, len;

	if (!_h || !_r)  {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	if (!RES_ROW_N(_r)) {
		LM_DBG("no rows returned from the query\n");
		RES_ROWS(_r) = 0;
		return 0;
	}
	/*Allocate an array of pointers per column to holds the string representation */
	len = sizeof(char *) * RES_COL_N(_r);
	row_buf = (char**)pkg_malloc(len);
	if (!row_buf) {
		LM_ERR("no private memory left\n");
		return -1;
	}
	LM_DBG("allocate for %d columns %d bytes in row buffer at %p\n", RES_COL_N(_r), len, row_buf);
	memset(row_buf, 0, len);

	/* Allocate a row structure for each row in the current fetch. */
	len = sizeof(db_row_t) * RES_ROW_N(_r);
	RES_ROWS(_r) = (db_row_t*)pkg_malloc(len);
	LM_DBG("allocate %d bytes for %d rows at %p\n", len, RES_ROW_N(_r), RES_ROWS(_r));

	if (!RES_ROWS(_r)) {
		LM_ERR("no private memory left\n");
		return -1;
	}
	memset(RES_ROWS(_r), 0, len);

	for(row = RES_LAST_ROW(_r); row < (RES_LAST_ROW(_r) + RES_ROW_N(_r)); row++) {
		for(col = 0; col < RES_COL_N(_r); col++) {
				/*
				 * The row data pointer returned by PQgetvalue points to storage
				 * that is part of the PGresult structure. One should not modify
				 * the data it points to, and one must explicitly copy the data
				 * into other storage if it is to be used past the lifetime of
				 * the PGresult structure itself.
				 */
				s = PQgetvalue(CON_RESULT(_h), row, col);
				LM_DBG("PQgetvalue(%p,%d,%d)=[%s]\n", _h, row, col, s);
				len = strlen(s);
				row_buf[col] = pkg_malloc(len+1);
				if (!row_buf[col]) {
					LM_ERR("no private memory left\n");
					return -1;
				}
				memset(row_buf[col], 0, len+1);
				LM_DBG("allocated %d bytes for row_buf[%d] at %p\n", len, col, row_buf[col]);

				strncpy(row_buf[col], s, len);
				LM_DBG("[%d][%d] Column[%.*s]=[%s]\n",
					row, col, RES_NAMES(_r)[col]->len, RES_NAMES(_r)[col]->s, row_buf[col]);
		}

		/* ASSERT: row_buf contains an entire row in strings */
		if(db_postgres_convert_row(_h, _r, &(RES_ROWS(_r)[row - RES_LAST_ROW(_r)]), row_buf)<0){
			LM_ERR("failed to convert row #%d\n",  row);
			RES_ROW_N(_r) = row - RES_LAST_ROW(_r);
			for (col = 0; col < RES_COL_N(_r); col++) {
				LM_DBG("freeing row_buf[%d] at %p\n", col, row_buf[col]);
				pkg_free(row_buf[col]);
			}
			LM_DBG("freeing row buffer at %p\n", row_buf);
			pkg_free(row_buf);
			return -4;
		}
		/*
		 * pkg_free() must be done for the above allocations now that the row
		 * has been converted. During pg_convert_row (and subsequent pg_str2val)
		 * processing, data types that don't need to be converted (namely STRINGS
		 * and STR) have their addresses saved. These data types should not have
		 * their pkg_malloc() allocations freed here because they are still
		 * needed.  However, some data types (ex: INT, DOUBLE) should have their
		 * pkg_malloc() allocations freed because during the conversion process,
		 * their converted values are saved in the union portion of the db_val_t
		 * structure. BLOB will be copied during PQunescape in str2val, thus it
		 * has to be freed here AND in pg_free_row().
		 *
		 * Warning: when the converted row is no longer needed, the data types
		 * whose addresses were saved in the db_val_t structure must be freed
		 * or a memory leak will happen. This processing should happen in the
		 * pg_free_row() subroutine. The caller of this routine should ensure
		 * that pg_free_rows(), pg_free_row() or pg_free_result() is eventually
		 * called.
		 */
		for (col = 0; col < RES_COL_N(_r); col++) {
			switch (RES_TYPES(_r)[col]) {
				case DB_STRING:
				case DB_STR:
					break;
				default:
					LM_DBG("freeing row_buf[%d] at %p\n", col, row_buf[col]);
					pkg_free(row_buf[col]);
			}
			/*
			 * The following housekeeping may not be technically required, but it
			 * is a good practice to NULL pointer fields that are no longer valid.
			 * Note that DB_STRING fields have not been pkg_free(). NULLing DB_STRING
			 * fields would normally not be good to do because a memory leak would
			 * occur.  However, the pg_convert_row() routine  has saved the DB_STRING
			 * pointer in the db_val_t structure.  The db_val_t structure will 
			 * eventually be used to pkg_free() the DB_STRING storage.
			 */
			row_buf[col] = (char *)NULL;
		}
	}

	LM_DBG("freeing row buffer at %p\n", row_buf);
	pkg_free(row_buf);
	row_buf = NULL;
	return 0;
}


/**
 * Convert a row from the result query into db API representation
 */
int db_postgres_convert_row(const db_con_t* _h, db_res_t* _r, db_row_t* _row,
		char **row_buf)
{
	int col, len;

	if (!_h || !_r || !_row)  {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	/*
	 * Allocate storage to hold the data type value converted from a string
	 * because PostgreSQL returns (most) data as strings
	 */
	len = sizeof(db_val_t) * RES_COL_N(_r);
	ROW_VALUES(_row) = (db_val_t*)pkg_malloc(len);

	if (!ROW_VALUES(_row)) {
		LM_ERR("no private memory left\n");
		return -1;
	}
	LM_DBG("allocate %d bytes for row values at %p\n", len, ROW_VALUES(_row));
	ROW_N(_row) = RES_COL_N(_r);
	memset(ROW_VALUES(_row), 0, len);

	/* Save the number of columns in the ROW structure */
	ROW_N(_row) = RES_COL_N(_r);

	/* For each column in the row */
	for(col = 0; col < ROW_N(_row); col++) {
		/* Convert the string representation into the value representation */
		if (db_postgres_str2val(RES_TYPES(_r)[col], &(ROW_VALUES(_row)[col]),
		row_buf[col], strlen(row_buf[col])) < 0) {
			LM_ERR("failed to convert value\n");
			LM_DBG("free row at %pn", _row);
			db_free_row(_row);
			return -3;
		}
	}
	return 0;
}
