/*
 * $Id$
 *
 * UNIXODBC module result related functions
 *
 * Copyright (C) 2005-2006 Marco Lorrai
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
 *
 *
 * History:
 * --------
 *  2005-12-01  initial commit (chgen)
 *  2006-04-04  fixed memory leak in convert_rows (sgupta)
 *  2006-05-05  removed static allocation of 1k per column data (sgupta)
 */

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "row.h"
#include "../../lib/srdb1/db_res.h"
#include "connection.h"
#include "res.h"
#include "list.h"
#include <stdlib.h>
#include <string.h>

/*
 * Get and convert columns from a result
 */
int db_unixodbc_get_columns(const db1_con_t* _h, db1_res_t* _r)
{
	int col;
	SQLSMALLINT cols; /* because gcc don't like RES_COL_N */

	if ((!_h) || (!_r)) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	/* Save number of columns in the result structure */
	SQLNumResultCols(CON_RESULT(_h), &cols);
	RES_COL_N(_r) = cols;
	if (!RES_COL_N(_r)) {
		LM_ERR("no columns returned from the query\n");
		return -2;
	} else {
		LM_DBG("%d columns returned from the query\n", RES_COL_N(_r));
	}

	if (db_allocate_columns(_r, RES_COL_N(_r)) != 0) {
		LM_ERR("could not allocate columns\n");
		return -3;
	}

	for(col = 0; col < RES_COL_N(_r); col++)
	{
		RES_NAMES(_r)[col] = (str*)pkg_malloc(sizeof(str));
		if (! RES_NAMES(_r)[col]) {
			LM_ERR("no private memory left\n");
			db_free_columns(_r);
			return -4;
		}
		LM_DBG("allocate %lu bytes for RES_NAMES[%d] at %p\n",
			(unsigned long)sizeof(str),col,	RES_NAMES(_r)[col]);

		char columnname[80];
		SQLRETURN ret;
		SQLSMALLINT namelength, datatype, decimaldigits, nullable;
		SQLULEN columnsize;

		ret = SQLDescribeCol(CON_RESULT(_h), col + 1, (SQLCHAR *)columnname, 80,
			&namelength, &datatype, &columnsize, &decimaldigits, &nullable);
		if(!SQL_SUCCEEDED(ret)) {
			LM_ERR("SQLDescribeCol failed: %d\n", ret);
			db_unixodbc_extract_error("SQLExecDirect", CON_RESULT(_h), SQL_HANDLE_STMT,
				NULL);
			// FIXME should we fail here completly?
		}
		/* The pointer that is here returned is part of the result structure. */
		RES_NAMES(_r)[col]->s = columnname;
		RES_NAMES(_r)[col]->len = namelength;

		LM_DBG("RES_NAMES(%p)[%d]=[%.*s]\n", RES_NAMES(_r)[col], col,
				RES_NAMES(_r)[col]->len, RES_NAMES(_r)[col]->s);

		switch(datatype)
		{
			case SQL_SMALLINT:
			case SQL_INTEGER:
			case SQL_TINYINT:
			case SQL_DECIMAL:
			case SQL_NUMERIC:
				LM_DBG("use DB1_INT result type\n");
				RES_TYPES(_r)[col] = DB1_INT;
				break;

			case SQL_BIGINT:
				LM_DBG("use DB1_BIGINT result type\n");
				RES_TYPES(_r)[col] = DB1_BIGINT;
				break;

			case SQL_REAL:
			case SQL_FLOAT:
			case SQL_DOUBLE:
				LM_DBG("use DB1_DOUBLE result type\n");
				RES_TYPES(_r)[col] = DB1_DOUBLE;
				break;

			case SQL_TYPE_TIMESTAMP:
			case SQL_DATE:
			case SQL_TIME:
			case SQL_TIMESTAMP:
			case SQL_TYPE_DATE:
			case SQL_TYPE_TIME:
				LM_DBG("use DB1_DATETIME result type\n");
				RES_TYPES(_r)[col] = DB1_DATETIME;
				break;

			case SQL_CHAR:
			case SQL_VARCHAR:
			case SQL_WCHAR:
			case SQL_WVARCHAR:
				LM_DBG("use DB1_STRING result type\n");
				RES_TYPES(_r)[col] = DB1_STRING;
				break;

			case SQL_BINARY:
			case SQL_VARBINARY:
			case SQL_LONGVARBINARY:
			case SQL_BIT:
			case SQL_LONGVARCHAR:
			case SQL_WLONGVARCHAR:
				LM_DBG("use DB1_BLOB result type\n");
				RES_TYPES(_r)[col] = DB1_BLOB;
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

/*
 * Convert rows from UNIXODBC to db API representation
 */
static inline int db_unixodbc_convert_rows(const db1_con_t* _h, db1_res_t* _r)
{
	int i = 0, ret = 0;
	SQLSMALLINT columns;
	list* rows = NULL;
	list* rowstart = NULL;
	strn* temp_row = NULL;

	if((!_h) || (!_r)) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	SQLNumResultCols(CON_RESULT(_h), (SQLSMALLINT *)&columns);

	while(SQL_SUCCEEDED(ret = SQLFetch(CON_RESULT(_h))))
	{
		temp_row = db_unixodbc_new_cellrow(columns);
		if (!temp_row) {
			LM_ERR("no private memory left\n");
			return -1;
		}

		for(i=0; i < columns; i++)
		{
			if (!db_unixodbc_load_cell(_h, i+1, temp_row + i, RES_TYPES(_r)[i])) {
			    db_unixodbc_free_cellrow(columns, temp_row);
			    return -5;
			}
		}

		if (db_unixodbc_list_insert(&rowstart, &rows, columns, temp_row) < 0) {
			LM_ERR("insert failed\n");
			db_unixodbc_free_cellrow(columns, temp_row);
			return -5;
		}
		RES_ROW_N(_r)++;

		/* free temporary row data */
		db_unixodbc_free_cellrow(columns, temp_row);
		temp_row = NULL;
	}
	CON_ROW(_h) = NULL;

	if (!RES_ROW_N(_r)) {
		RES_ROWS(_r) = 0;
		return 0;
	}

	if (db_allocate_rows(_r) != 0) {
		LM_ERR("could not allocate rows");
		db_unixodbc_list_destroy(rowstart);
		return -2;
	}

	i = 0;
	rows = rowstart;
	while(rows)
	{
		CON_ROW(_h) = rows->data;
		if (!CON_ROW(_h))
		{
			LM_ERR("string null\n");
			RES_ROW_N(_r) = i;
			db_free_rows(_r);
			db_unixodbc_list_destroy(rowstart);
			return -3;
		}
		if (db_unixodbc_convert_row(_h, _r, &(RES_ROWS(_r)[i]), rows->lengths) < 0) {
			LM_ERR("converting row failed #%d\n", i);
			RES_ROW_N(_r) = i;
			db_free_rows(_r);
			db_unixodbc_list_destroy(rowstart);
			return -4;
		}
		i++;
		rows = rows->next;
	}
	db_unixodbc_list_destroy(rowstart);
	return 0;
}


/*
 * Fill the structure with data from database
 */
int db_unixodbc_convert_result(const db1_con_t* _h, db1_res_t* _r)
{
	if (!_h || !_r) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	if (db_unixodbc_get_columns(_h, _r) < 0) {
		LM_ERR("getting column names failed\n");
		return -2;
	}

	if (db_unixodbc_convert_rows(_h, _r) < 0) {
		LM_ERR("converting rows failed\n");
		db_free_columns(_r);
		return -3;
	}
	return 0;
}
