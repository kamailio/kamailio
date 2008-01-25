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
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
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
#include "db_res.h"
#include "db_val.h"
#include "pg_con.h"
#include "pg_type.h"


/**
 * Fill the result structure with data from the query
 */
int db_postgres_convert_result(const db_con_t* _con, db_res_t* _res)
{
	if (!_con || !_res)  {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (db_postgres_get_columns(_con, _res) < 0) {
		LM_ERR("failed to get column names\n");
		return -2;
	}

	if (db_postgres_convert_rows(_con, _res, 0, PQntuples(CON_RESULT(_con))) < 0) {
		LM_ERR("failed to convert rows\n");
		db_postgres_free_columns(_res);
		return -3;
	}

        return 0;
}

/**
 * Get and convert columns from a result set
 */
int db_postgres_get_columns(const db_con_t* _con, db_res_t* _res)
{
	int cols, col, len;

	if (!_con || !_res)  {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	/* Get the number of rows (tuples) in the query result. */
	RES_NUM_ROWS(_res) = PQntuples(CON_RESULT(_con));


	/* Get the number of columns (fields) in each row of the query result. */
	cols = PQnfields(CON_RESULT(_con));

	if (!cols) {
		LM_DBG("No columns returned from the query\n");
		return -2;
	} else {
		LM_DBG("%d column(s) returned from the query\n", cols);
	}

	/* Allocate storage to hold a pointer to each column name */
        RES_NAMES(_res) = (db_key_t*)pkg_malloc(sizeof(db_key_t) * cols);
	LM_DBG("%p=pkg_malloc(%lu) RES_NAMES\n", RES_NAMES(_res), 
			(unsigned long)(sizeof(db_key_t) * cols));

	if (!RES_NAMES(_res)) {
		LM_ERR("failed to allocate %lu bytes in pkg memory for column"
				"names\n", (unsigned long)(sizeof(db_key_t) * cols));
		return -3;
	}

	/* Allocate storage to hold the type of each column */
	RES_TYPES(_res) = (db_type_t*)pkg_malloc(sizeof(db_type_t) * cols);
	LM_DBG("%p=pkg_malloc(%lu) RES_TYPES\n", RES_TYPES(_res), 
			(unsigned long)(sizeof(db_type_t) * cols));

	if (!RES_TYPES(_res)) {
		LM_ERR("failed to allocate %lu bytes in pkg memory for column"
				"types\n", (unsigned long)(sizeof(db_type_t) * cols));
		/* Free previously allocated storage that was to hold column names */
		LM_DBG("%p=pkg_free() RES_NAMES\n", RES_NAMES(_res));
		pkg_free(RES_NAMES(_res));
		return -4;
	}

	/* Save number of columns in the result structure */
	RES_COL_N(_res) = cols;

	/* For each column both the name and the OID number of the data type are saved. */
	for(col = 0; col < cols; col++) {
		int ft;

		RES_NAMES(_res)[col] = (str*)pkg_malloc(sizeof(str));
		if (! RES_NAMES(_res)[col]) {
			LM_ERR("no private memory left\n");
			pkg_free(RES_NAMES(_res));
			pkg_free(RES_TYPES(_res));
			// FIXME we should also free all previous allocated RES_NAMES[col]
			return -5;
		}
		/* The pointer that is here returned is part of the PGResult structure. */
		len = strlen(PQfname(CON_RESULT(_con),col));
		RES_NAMES(_res)[col]->s = pkg_malloc(len);
		LM_DBG("%p=pkg_malloc(%d) RES_NAMES[%d]->s\n", RES_NAMES(_res)[col]->s, len, col);

		if (! RES_NAMES(_res)[col]->s) {
			LM_ERR("failed to allocate %d bytes to hold column name\n", len);
			// FIXME we should also free all previous allocated memory here
			return -1;
		}
		/* copy the name, because it will freed when the PGResult is freed */
		memset((char *)RES_NAMES(_res)[col]->s, 0, len);
		strncpy((char *)RES_NAMES(_res)[col]->s, PQfname(CON_RESULT(_con),col), len);
		RES_NAMES(_res)[col]->len = len;

		LM_DBG("RES_NAMES(%p)[%d]=[%.*s]\n", RES_NAMES(_res)[col], col,
				RES_NAMES(_res)[col]->len, RES_NAMES(_res)[col]->s);

		/* get the datatype of the column */
		switch(ft = PQftype(CON_RESULT(_con),col))
		{
			case INT2OID:
			case INT4OID:
			case INT8OID:
				RES_TYPES(_res)[col] = DB_INT;
			break;

			case FLOAT4OID:
			case FLOAT8OID:
			case NUMERICOID:
				RES_TYPES(_res)[col] = DB_DOUBLE;
			break;

			case DATEOID:
			case TIMESTAMPOID:
			case TIMESTAMPTZOID:
				RES_TYPES(_res)[col] = DB_DATETIME;
			break;

			case BOOLOID:
			case CHAROID:
			case VARCHAROID:
			case BPCHAROID:
			case TEXTOID:
				RES_TYPES(_res)[col] = DB_STRING;
			break;

			case BYTEAOID:
				RES_TYPES(_res)[col] = DB_BLOB;
			break;

			case BITOID:
			case VARBITOID:
				RES_TYPES(_res)[col] = DB_BITMAP;
			break;

			default:
				LM_WARN("unhandled data type column (%.*s) OID (%d), "
						"defaulting to STRING\n", RES_NAMES(_res)[col]->len,
						RES_NAMES(_res)[col]->s, ft);
				RES_TYPES(_res)[col] = DB_STRING;
			break;
		}
	}
	return 0;
}

/**
 * Convert rows from PostgreSQL to db API representation
 */
int db_postgres_convert_rows(const db_con_t* _con, db_res_t* _res, int row_start,
		int row_count)
{
	int row, cols, col;
	char **row_buf, *s;
	int len, fetch_count;

	if (!_con || !_res)  {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (row_count == 0) {
		LM_ERR("no rows requested from the query\n");
		return 0;
	}

	if (!RES_NUM_ROWS(_res)) {
		LM_DBG("no rows returned from the query\n");
		return 0;
	}

	if (row_start < 0)  {
		LM_ERR("starting row (%d) cannot be less "
			"then zero, setting it to zero\n", row_start);
		row_start = 0;
	}

	if ((row_start + row_count) > RES_NUM_ROWS(_res))  {
		LM_ERR("starting row + row count cannot be > "
			"total rows. Setting row count to read remainder of result set\n");
		row_count = RES_NUM_ROWS(_res) - row_start;
	}

	/* Save the number of rows in the current fetch */
	RES_ROW_N(_res) = row_count;

	/* Save the number of columns in the result query */
	cols = RES_COL_N(_res);

	/*
	 * Allocate an array of pointers one per column. It that will be used to hold
	 * the address of the string representation of each column.
	 */
	len = sizeof(char *) * cols;
	row_buf = (char **)pkg_malloc(len);
	LM_DBG("%p=pkg_malloc(%d) row_buf %d pointers\n", row_buf, len, cols);
	if (!row_buf) {
		LM_ERR("failed to allocate %d bytes in pkg memory for row buffer\n",
				len);
		return -1;
	}
	memset(row_buf, 0, len);

	/* Allocate a row structure for each row in the current fetch. */
	len = sizeof(db_row_t) * row_count;
	RES_ROWS(_res) = (db_row_t*)pkg_malloc(len);
	LM_DBG("%p=pkg_malloc(%d) RES_ROWS %d rows\n", 
			RES_ROWS(_res), len, row_count);
	if (!RES_ROWS(_res)) {
		LM_ERR("failed to allocate %d bytes in pkg memory, %d rows "
			"for row structure\n", len, row_count);
		return -1;
	}
	memset(RES_ROWS(_res), 0, len);

	fetch_count = 0;
	for(row = row_start; row < (row_start + row_count); row++) {
		for(col = 0; col < cols; col++) {
				/*
				 * The row data pointer returned by PQgetvalue points to storage
				 * that is part of the PGresult structure. One should not modify
				 * the data it points to, and one must explicitly copy the data
				 * into other storage if it is to be used past the lifetime of
				 * the PGresult structure itself.
				 */
				s = PQgetvalue(CON_RESULT(_con), row, col);
				LM_DBG("PQgetvalue(%p,%d,%d)=[%s]\n", _con, row, col, s);
				len = strlen(s);
				row_buf[col] = pkg_malloc(len+1);
				if (!row_buf[col]) {
					LM_ERR("failed to allocate %d bytes in pkg memory "
						"for row_buf[%d]\n", len+1, col);
					return -1;
				}
				memset(row_buf[col], 0, len+1);
				LM_DBG("%p=pkg_malloc(%d) row_buf[%d]\n", row_buf[col], len, col);

				strncpy(row_buf[col], s, len);
				LM_DBG("[%d][%d] Column[%.*s]=[%s]\n",
					row, col, RES_NAMES(_res)[col]->len, RES_NAMES(_res)[col]->s, row_buf[col]);
		}

		/*
		** ASSERT: row_buf contains an entire row in strings
		*/
		if(db_postgres_convert_row(_con,_res,&(RES_ROWS(_res)[fetch_count]),row_buf)<0){
			LM_ERR("failed to convert row #%d\n",  row);
			RES_ROW_N(_res) = row - row_start;
			for (col=0; col<cols; col++) {
				LM_DBG("%p=pkg_free() row_buf[%d]\n",(char *)row_buf[col], col);
				pkg_free((char *)row_buf[col]);	
			}
			LM_DBG("%p=pkg_free() row_buf\n", row_buf);
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
		for (col=0; col<cols; col++) {
			switch (RES_TYPES(_res)[col]) {
				case DB_STRING:
				case DB_STR:
					break;
				default:
					LM_DBG("[%d][%d] Col[%.*s] Type[%d] "
						"Freeing row_buf[%p]\n", row, col,
						RES_NAMES(_res)[col]->len, RES_NAMES(_res)[col]->s,
						RES_TYPES(_res)[col], row_buf[col]);
					LM_DBG("%p=pkg_free() row_buf[%d]\n",
						(char *)row_buf[col], col);
					pkg_free((char *)row_buf[col]);
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
	fetch_count++;
	}

	LM_DBG("%p=pkg_free() row_buf\n", row_buf);
	pkg_free(row_buf);
	row_buf = NULL;
	return 0;
}

/**
 * Convert a row from the result query into db API representation
 */
int db_postgres_convert_row(const db_con_t* _con, db_res_t* _res, db_row_t* _row,
		char **row_buf)
{
	int col, len;

	if (!_con || !_res || !_row)  {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	/*
	 * Allocate storage to hold the data type value converted from a string
	 * because PostgreSQL returns (most) data as strings
	 */
	len = sizeof(db_val_t) * RES_COL_N(_res);
	ROW_VALUES(_row) = (db_val_t*)pkg_malloc(len);
	LM_DBG("%p=pkg_malloc(%d) ROW_VALUES for %d columns\n",
		ROW_VALUES(_row), len, RES_COL_N(_res));

	if (!ROW_VALUES(_row)) {
		LM_ERR("No memory left\n");
		return -1;
	}
	memset(ROW_VALUES(_row), 0, len);

	/* Save the number of columns in the ROW structure */
	ROW_N(_row) = RES_COL_N(_res);

	/* For each column in the row */
	for(col = 0; col < ROW_N(_row); col++) {
		LM_DBG("col[%d]\n", col);
		/* Convert the string representation into the value representation */
		if (db_postgres_str2val(RES_TYPES(_res)[col], &(ROW_VALUES(_row)[col]),
		row_buf[col], strlen(row_buf[col])) < 0) {
			LM_ERR("failed to convert value\n");
			LM_DBG("%p=pkg_free() _row\n", _row);
			db_postgres_free_row(_row);
			return -3;
		}
	}
	return 0;
}


/**
 * Release memory used by rows
 */
int db_postgres_free_rows(db_res_t* _res)
{
	int row;

	if (!_res)  {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	LM_DBG("freeing %d rows\n", RES_ROW_N(_res));

	for(row = 0; row < RES_ROW_N(_res); row++) {
		LM_DBG("row[%d]=%p\n", row, &(RES_ROWS(_res)[row]));
		db_postgres_free_row(&(RES_ROWS(_res)[row]));
	}
	RES_ROW_N(_res) = 0;

	if (RES_ROWS(_res)) {
		LM_DBG("%p=pkg_free() RES_ROWS\n", RES_ROWS(_res));
		pkg_free(RES_ROWS(_res));
		RES_ROWS(_res) = NULL;
	}

	return 0;
}

/**
 * Release memory used by row
 * This function loops over each column and calls pkg_free on
 * STRINGs, this is different 
 * from the mysql and unixodbc behaviour which skips this step. 
 * So we need an own implementation of this function here. 
 * This needs some investigation..
 */
int db_postgres_free_row(db_row_t* _row)
{
	int	col;
	db_val_t* _val;

	if (!_row) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	/*
	 * Loop thru each columm, then check to determine if the storage pointed to
	 * by db_val_t structure must be freed. This is required for all data types
	 * which use a pointer to a buffer like DB_STRING, DB_STR and DB_BLOB.
	 * If this is not done, a memory leak will happen.
	 */
	for (col = 0; col < ROW_N(_row); col++) {
		_val = &(ROW_VALUES(_row)[col]);
		switch (VAL_TYPE(_val)) {
			case DB_STRING:
				LM_DBG("%p=pkg_free() VAL_STRING[%d]\n",
					(char *)VAL_STRING(_val), col);
				pkg_free((char *)(VAL_STRING(_val)));
				VAL_STRING(_val) = (char *)NULL;
				break;
			case DB_STR:
				LM_DBG("%p=pkg_free() VAL_STR[%d]\n",
					(char *)(VAL_STR(_val).s), col);
				pkg_free((char *)(VAL_STR(_val).s));
				VAL_STR(_val).s = (char *)NULL;
				break;
			case DB_BLOB:
				LM_DBG("%p=pkg_free() VAL_BLOB[%d]\n",
					(char *)(VAL_BLOB(_val).s), col);
				PQfreemem(VAL_BLOB(_val).s);
				VAL_BLOB(_val).s = (char *)NULL;
				break;
			default:
				break;
		}
	}

	/* Free db_val_t structure. */
	if (ROW_VALUES(_row)) {
		LM_DBG("%p=pkg_free() ROW_VALUES\n", ROW_VALUES(_row));
		pkg_free(ROW_VALUES(_row));
		ROW_VALUES(_row) = NULL;
	}
	return 0;
}

/**
 * Release memory used by columns
 * The same applies as in pg_free_colum
 */
int db_postgres_free_columns(db_res_t* _res)
{
	int col;

	if (!_res) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	/* Free memory previously allocated to save column names */
	for(col = 0; col < RES_COL_N(_res); col++) {
		LM_DBG("Freeing RES_NAMES(%p)[%d] -> free(%p) '%.*s'\n", 
				_res, col, RES_NAMES(_res)[col], RES_NAMES(_res)[col]->len,
				RES_NAMES(_res)[col]->s);
		LM_DBG("%p=pkg_free() RES_NAMES[%d]->s\n", RES_NAMES(_res)[col]->s, col);
		pkg_free((char *)RES_NAMES(_res)[col]->s);
		LM_DBG("%p=pkg_free() RES_NAMES[%d]\n", RES_NAMES(_res)[col], col);
		pkg_free((str *)RES_NAMES(_res)[col]);
		RES_NAMES(_res)[col] = (str *)NULL;
	}
 
	if (RES_NAMES(_res)) {
		LM_DBG("%p=pkg_free() RES_NAMES\n", RES_NAMES(_res));
		pkg_free(RES_NAMES(_res));
		RES_NAMES(_res) = NULL;
	}
	if (RES_TYPES(_res)) {
		LM_DBG("%p=pkg_free() RES_TYPES\n",	RES_TYPES(_res));
		pkg_free(RES_TYPES(_res));
		RES_TYPES(_res) = NULL;
	}

	return 0;
}

/**
 * Release memory used by the result structure
 */
int db_postgres_free_result(db_res_t* _res)
{
	if (!_res) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	db_postgres_free_columns(_res);
	db_postgres_free_rows(_res);

	LM_DBG("%p=pkg_free() _res\n", _res);
	pkg_free(_res);
	_res = NULL;
	return 0;
}

