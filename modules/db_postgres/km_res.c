/*
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History
 * -------
 * 2003-04-06 initial code written (Greg Fausak/Andy Fullford)
 *
 * 2006-07-26 added BPCHAROID as a valid type for DB1_STRING conversions
 *            this removes the "unknown type 1042" log messages (norm)
 *
 * 2006-10-27 Added fetch support (norm)
 *            Removed dependency on aug_* memory routines (norm)
 *            Added connection pooling support (norm)
 *            Standardized API routines to pg_* names (norm)
 */

/*! \file
 *  \brief DB_POSTGRES :: Core
 *  \ingroup db_postgres
 *  Module: \ref db_postgres
 */

#include <stdlib.h>
#include <string.h>
#include "../../lib/srdb1/db_id.h"
#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db_con.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "km_res.h"
#include "km_val.h"
#include "km_pg_con.h"
#include "km_pg_type.h"


/*!
 * \brief Fill the result structure with data from the query
 * \param _h database connection
 * \param _r result set
 * \return 0 on success, negative on error
 */
int db_postgres_convert_result(const db1_con_t* _h, db1_res_t* _r)
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


/*!
 * \brief Get and convert columns from a result set
 * \param _h database connection
 * \param _r result set
 * \return 0 on success, negative on error
 */
int db_postgres_get_columns(const db1_con_t* _h, db1_res_t* _r)
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
				LM_DBG("use DB1_INT result type\n");
				RES_TYPES(_r)[col] = DB1_INT;
			break;

			case INT8OID:
				LM_DBG("use DB1_BIGINT result type\n");
				RES_TYPES(_r)[col] = DB1_BIGINT;

			case FLOAT4OID:
			case FLOAT8OID:
			case NUMERICOID:
				LM_DBG("use DB1_DOUBLE result type\n");
				RES_TYPES(_r)[col] = DB1_DOUBLE;
			break;

			case DATEOID:
			case TIMESTAMPOID:
			case TIMESTAMPTZOID:
				LM_DBG("use DB1_DATETIME result type\n");
				RES_TYPES(_r)[col] = DB1_DATETIME;
			break;

			case BOOLOID:
			case CHAROID:
			case VARCHAROID:
			case BPCHAROID:
				LM_DBG("use DB1_STRING result type\n");
				RES_TYPES(_r)[col] = DB1_STRING;
			break;

			case TEXTOID:
			case BYTEAOID:
				LM_DBG("use DB1_BLOB result type\n");
				RES_TYPES(_r)[col] = DB1_BLOB;
			break;

			case BITOID:
			case VARBITOID:
				LM_DBG("use DB1_BITMAP result type\n");
				RES_TYPES(_r)[col] = DB1_BITMAP;
			break;
				
			default:
				LM_WARN("unhandled data type column (%.*s) type id (%d), "
						"use DB1_STRING as default\n", RES_NAMES(_r)[col]->len,
						RES_NAMES(_r)[col]->s, datatype);
				RES_TYPES(_r)[col] = DB1_STRING;
			break;
		}
	}
	return 0;
}


/*!
 * \brief Convert rows from PostgreSQL to db API representation
 * \param _h database connection
 * \param _r result set
 * \return 0 on success, negative on error
 */
int db_postgres_convert_rows(const db1_con_t* _h, db1_res_t* _r)
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

	if (db_allocate_rows(_r) < 0) {
		LM_ERR("could not allocate rows\n");
		LM_DBG("freeing row buffer at %p\n", row_buf);
		pkg_free(row_buf);
		return -2;
	}

	for(row = RES_LAST_ROW(_r); row < (RES_LAST_ROW(_r) + RES_ROW_N(_r)); row++) {
		/* reset row buf content */
		memset(row_buf, 0, len);
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
			/*
			 * A empty string can be a NULL value, or just an empty string.
			 * This differs from the mysql behaviour, that further processing
			 * steps expect. So we need to simulate this here unfortunally.
			 */
			if (PQgetisnull(CON_RESULT(_h), row, col) == 0) {
				row_buf[col] = s;
				LM_DBG("[%d][%d] Column[%.*s]=[%s]\n",
					row, col, RES_NAMES(_r)[col]->len, RES_NAMES(_r)[col]->s, row_buf[col]);
			}
		}

		/* ASSERT: row_buf contains an entire row in strings */
		if(db_postgres_convert_row(_h, _r, &(RES_ROWS(_r)[row - RES_LAST_ROW(_r)]), row_buf)<0){
			LM_ERR("failed to convert row #%d\n",  row);
			RES_ROW_N(_r) = row - RES_LAST_ROW(_r);
			LM_DBG("freeing row buffer at %p\n", row_buf);
			pkg_free(row_buf);
			db_free_rows(_r);
			return -4;
		}
	}

	LM_DBG("freeing row buffer at %p\n", row_buf);
	pkg_free(row_buf);
	row_buf = NULL;
	return 0;
}


/*!
 * \brief Convert a row from the result query into db API representation
 * \param _h database connection
 * \param _r result set
 * \param _row row
 * \param row_buf row buffer
 * \return 0 on success, negative on error
 */
int db_postgres_convert_row(const db1_con_t* _h, db1_res_t* _r, db_row_t* _row,
		char **row_buf)
{
	int col, col_len;

	if (!_h || !_r || !_row)  {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (db_allocate_row(_r, _row) != 0) {
		LM_ERR("could not allocate row\n");
		return -2;
	}

	/* For each column in the row */
	for(col = 0; col < ROW_N(_row); col++) {
		/* because it can contain NULL */
		if (!row_buf[col]) {
			col_len = 0;
		} else {
			col_len = strlen(row_buf[col]);
		}
		/* Convert the string representation into the value representation */
		if (db_postgres_str2val(RES_TYPES(_r)[col], &(ROW_VALUES(_row)[col]),
		row_buf[col], col_len) < 0) {
			LM_ERR("failed to convert value\n");
			LM_DBG("free row at %p\n", _row);
			db_free_row(_row);
			return -3;
		}
	}
	return 0;
}
