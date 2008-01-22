/* 
 * $Id$
 *
 * UNIXODBC module result related functions
 *
 * Copyright (C) 2005-2006 Marco Lorrai
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
#include "../../db/db_res.h"
#include "my_con.h"
#include "res.h"
#include "list.h"
#include <stdlib.h>
#include <string.h>

/*
 * Get and convert columns from a result
 */
static inline int get_columns(const db_con_t* _h, db_res_t* _r)
{
	int i;
	SQLSMALLINT n;										  //columns number

	if ((!_h) || (!_r))
	{
		LM_ERR("invalid parameter\n");
		return -1;
	}

	SQLNumResultCols(CON_RESULT(_h), &n);
	if (!n)
	{
		LM_ERR("no columns\n");
		return -2;
	}

	RES_NAMES(_r) = (db_key_t*)pkg_malloc(sizeof(db_key_t) * n);
	if (!RES_NAMES(_r))
	{
		LM_ERR("no memory left\n");
		return -3;
	}

	RES_TYPES(_r) = (db_type_t*)pkg_malloc(sizeof(db_type_t) * n);
	if (!RES_TYPES(_r))
	{
		LM_ERR("no memory left\n");
		pkg_free(RES_NAMES(_r));
		return -4;
	}

	RES_COL_N(_r) = n;
	for(i = 0; i < n; i++)
	{
		RES_NAMES(_r)[i] = (str*)pkg_malloc(sizeof(str));
		if (! RES_NAMES(_r)[i]) {
			LM_ERR("no private memory left\n");
			pkg_free(RES_NAMES(_r));
			pkg_free(RES_TYPES(_r));
			// FIXME we should also free all previous allocated RES_NAMES[i]
			return -5;
		}

		char ColumnName[80];
		SQLRETURN ret;
		SQLSMALLINT NameLength, DataType, DecimalDigits, Nullable;
		SQLULEN ColumnSize;

		ret=SQLDescribeCol(CON_RESULT(_h), i + 1, (SQLCHAR *)ColumnName, 80,
			&NameLength, &DataType, &ColumnSize, &DecimalDigits, &Nullable);
		if(!SQL_SUCCEEDED(ret))
		{
			LM_ERR("SQLDescribeCol failed: %d\n", ret);
			db_unixodbc_extract_error("SQLExecDirect", CON_RESULT(_h), SQL_HANDLE_STMT,
				NULL);
			// FIXME should we fail here completly?
		}
		RES_NAMES(_r)[i]->s = ColumnName;
		RES_NAMES(_r)[i]->len = NameLength;

		switch(DataType)
		{
			case SQL_SMALLINT:
			case SQL_INTEGER:
			case SQL_TINYINT:
			case SQL_BIGINT:
			case SQL_DECIMAL:
			case SQL_NUMERIC:
				RES_TYPES(_r)[i] = DB_INT;
				break;

			case SQL_REAL:
			case SQL_FLOAT:
			case SQL_DOUBLE:
				RES_TYPES(_r)[i] = DB_DOUBLE;
				break;

			case SQL_TYPE_TIMESTAMP:
			case SQL_DATE:
			case SQL_TIME:
			case SQL_TIMESTAMP:
			case SQL_TYPE_DATE:
			case SQL_TYPE_TIME:
				RES_TYPES(_r)[i] = DB_DATETIME;
				break;

			case SQL_BINARY:
			case SQL_VARBINARY:
			case SQL_LONGVARBINARY:
			case SQL_BIT:
				RES_TYPES(_r)[i] = DB_BLOB;
				break;

			default:
				RES_TYPES(_r)[i] = DB_STRING;
				break;
		}
	}
	return 0;
}

/*
 * Convert rows from UNIXODBC to db API representation
 */
static inline int convert_rows(const db_con_t* _h, db_res_t* _r)
{
	int row_n = 0, i = 0, ret = 0;
	SQLSMALLINT columns;
	list* rows = NULL;
	list* rowstart = NULL;
	strn* temp_row = NULL;

	if((!_h) || (!_r))
	{
		LM_ERR("invalid parameter\n");
		return -1;
	}

	SQLNumResultCols(CON_RESULT(_h), (SQLSMALLINT *)&columns);
	temp_row = (strn*)pkg_malloc( columns*sizeof(strn) );
	if(!temp_row)
	{
		LM_ERR("no memory left\n");
		return -1;
	}

	while(SQL_SUCCEEDED(ret = SQLFetch(CON_RESULT(_h))))
	{
		for(i=0; i < columns; i++)
		{
			SQLLEN indicator;
			ret = SQLGetData(CON_RESULT(_h), i+1, SQL_C_CHAR,
				temp_row[i].s, STRN_LEN, &indicator);
			if (SQL_SUCCEEDED(ret)) {
				if (indicator == SQL_NULL_DATA)
					strcpy(temp_row[i].s, "NULL");
			}
			else
			{
				LM_ERR("SQLGetData failed\n");
			}
		}

		if (insert(&rowstart, &rows, columns, temp_row) < 0) {
			LM_ERR("insert failed\n");
			pkg_free(temp_row);
			temp_row= NULL;
			return -5;
		}

		row_n++;
	}
	/* free temporary row data */
	pkg_free(temp_row);
	CON_ROW(_h) = NULL;

	RES_ROW_N(_r) = row_n;
	if (!row_n)
	{
		RES_ROWS(_r) = 0;
		return 0;
	}
	RES_ROWS(_r) = (struct db_row*)pkg_malloc(sizeof(db_row_t) * row_n);
	if (!RES_ROWS(_r))
	{
		LM_ERR("no more memory left\n");
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
			RES_ROW_N(_r) = row_n;
			db_free_rows(_r);
			return -3;
		}
		if (convert_row(_h, _r, &(RES_ROWS(_r)[i]), rows->lengths) < 0)
		{
			LM_ERR("converting row failed #%d\n", i);
			RES_ROW_N(_r) = i;
			db_free_rows(_r);
			return -4;
		}
		i++;
		rows = rows->next;
	}
	destroy(rowstart);
	return 0;
}


/*
 * Fill the structure with data from database
 */
int convert_result(const db_con_t* _h, db_res_t* _r)
{
	if ((!_h) || (!_r))
	{
		LM_ERR("invalid parameter\n");
		return -1;
	}

	if (get_columns(_h, _r) < 0)
	{
		LM_ERR("getting column names failed\n");
		return -2;
	}

	if (convert_rows(_h, _r) < 0)
	{
		LM_ERR("converting rows failed\n");
		db_free_columns(_r);
		return -3;
	}
	return 0;
}

/*
 * Release memory used by a result structure
 */
int free_result(db_res_t* _r)
{
	if (!_r)
	{
		LM_ERR("invalid parameter\n");
		return -1;
	}

	db_free_columns(_r);
	db_free_rows(_r);
	pkg_free(_r);
	return 0;
}
