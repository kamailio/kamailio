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
 */

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "row.h"
#include "my_con.h"
#include "res.h"
#include "list.h"
#include <stdlib.h>
#include <string.h>

/*
 * Get and convert columns from a result
 */
static inline int get_columns(db_con_t* _h, db_res_t* _r)
{
	int i;
	SQLSMALLINT n;										  //columns number

	if ((!_h) || (!_r))
	{
		LOG(L_ERR, "get_columns: Invalid parameter\n");
		return -1;
	}

	SQLNumResultCols(CON_RESULT(_h), &n);
	if (!n)
	{
		LOG(L_ERR, "get_columns: No columns\n");
		return -2;
	}

	RES_NAMES(_r) = (db_key_t*)pkg_malloc(sizeof(db_key_t) * n);
	if (!RES_NAMES(_r))
	{
		LOG(L_ERR, "get_columns: No memory left\n");
		return -3;
	}

	RES_TYPES(_r) = (db_type_t*)pkg_malloc(sizeof(db_type_t) * n);
	if (!RES_TYPES(_r))
	{
		LOG(L_ERR, "get_columns: No memory left\n");
		pkg_free(RES_NAMES(_r));
		return -4;
	}

	RES_COL_N(_r) = n;
	for(i = 0; i < n; i++)
	{
		char ColumnName[80];
		SQLRETURN ret;
		SQLSMALLINT NameLength, DataType, DecimalDigits, Nullable;
		SQLUINTEGER ColumnSize;

		ret=SQLDescribeCol(CON_RESULT(_h), i + 1, (SQLCHAR *)ColumnName, 80, &NameLength,
			&DataType, &ColumnSize, &DecimalDigits, &Nullable);
		if(!SQL_SUCCEEDED(ret))
		{
			LOG(L_ERR, "SQLDescribeCol fallita: %d\n", ret);
			extract_error("SQLExecDirect", CON_RESULT(_h), SQL_HANDLE_STMT);
		}
		RES_NAMES(_r)[i]=ColumnName;
		switch(DataType)
		{
			case SQL_SMALLINT:
			case SQL_INTEGER:
			case SQL_TINYINT:
			case SQL_BIGINT:
				RES_TYPES(_r)[i] = DB_INT;
				break;

			case SQL_REAL:
			case SQL_FLOAT:
			case SQL_DOUBLE:
				RES_TYPES(_r)[i] = DB_DOUBLE;
				break;

			case SQL_TYPE_TIMESTAMP:
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
 * Release memory used by rows
 */
static inline int free_rows(db_res_t* _r)
{
	int i;

	if (!_r)
	{
		LOG(L_ERR, "free_rows: Invalid parameter value\n");
		return -1;
	}

	for(i = 0; i < RES_ROW_N(_r); i++)
	{
		free_row(&(RES_ROWS(_r)[i]));
	}
	if (RES_ROWS(_r)) pkg_free(RES_ROWS(_r));
	return 0;
}

/*
 * Convert rows from UNIXODBC to db API representation
 */
static inline int convert_rows(db_con_t* _h, db_res_t* _r)
{
	int row, i, ret;
	SQLSMALLINT columns;
	list rows = NULL;
	list l;

	if((!_h) || (!_r))
	{
		LOG(L_ERR, "convert_rows: Invalid parameter\n");
		return -1;
	}

	row = 0;
	SQLNumResultCols(CON_RESULT(_h), (SQLSMALLINT *)&columns);
	CON_ROW(_h) = (strn*)pkg_malloc( columns*sizeof(strn) );
	if(!CON_ROW(_h))
	{
		LOG(L_ERR, "convert_rows: No memory left\n");
		return -1;
	}

	while(SQL_SUCCEEDED(ret = SQLFetch(CON_RESULT(_h))))
	{
		for(i=1; i <= columns; i++)
		{
			SQLINTEGER indicator;
			ret = SQLGetData(CON_RESULT(_h), i, SQL_C_CHAR,
				(CON_ROW(_h)[i-1]).s, STRN_LEN, &indicator);
			if (SQL_SUCCEEDED(ret))
			{
				if (indicator == SQL_NULL_DATA) strcpy((CON_ROW(_h)[i-1]).s, "NULL");
			}
			else
			{
				LOG(L_ERR, "Error in SQLGetData\n");
			}
		}
		if(!row)
		{
			create(&rows, columns, CON_ROW(_h));
		}
		else
		{
			insert(rows, columns, CON_ROW(_h));
		}
		row++;
	}

	RES_ROW_N(_r) = row;
	if (!row)
	{
		RES_ROWS(_r) = 0;
		return 0;
	}
	RES_ROWS(_r) = (struct db_row*)pkg_malloc(sizeof(db_row_t) * row);
	if (!RES_ROWS(_r))
	{
		LOG(L_ERR, "convert_rows: No memory left\n");
		return -2;
	}
	i = 0;
	l = rows;
	while(l!=NULL)
	{
		CON_ROW(_h) = view(l);
		//for(j=0; j<columns; j++)
		//{
			//if(!strcmp(CON_ROW(_h)[j].s, "NULL"))
			//	strcpy(CON_ROW(_h)[j].s, "");
		//}
		if (!CON_ROW(_h))
		{
			LOG(L_ERR, "convert_rows: string null\n");
			RES_ROW_N(_r) = row;
			free_rows(_r);
			return -3;
		}
		if (convert_row(_h, _r, &(RES_ROWS(_r)[i])) < 0)
		{
			LOG(L_ERR, "convert_rows: Error while converting row #%d\n", i);
			RES_ROW_N(_r) = i;
			free_rows(_r);
			return -4;
		}
		i++;
		l=l->next;
	}
	destroy(rows);
	return 0;
}

/*
 * Release memory used by columns
 */
static inline int free_columns(db_res_t* _r)
{
	if (!_r)
	{
		LOG(L_ERR, "free_columns: Invalid parameter\n");
		return -1;
	}

	if (RES_NAMES(_r)) pkg_free(RES_NAMES(_r));
	if (RES_TYPES(_r)) pkg_free(RES_TYPES(_r));
	return 0;
}

/*
 * Create a new result structure and initialize it
 */
db_res_t* new_result(void)
{
	db_res_t* r;
	r = (db_res_t*)pkg_malloc(sizeof(db_res_t));
	if (!r)
	{
		LOG(L_ERR, "new_result: No memory left\n");
		return 0;
	}
	RES_NAMES(r) = 0;
	RES_TYPES(r) = 0;
	RES_COL_N(r) = 0;
	RES_ROWS(r) = 0;
	RES_ROW_N(r) = 0;
	return r;
}

/*
 * Fill the structure with data from database
 */
int convert_result(db_con_t* _h, db_res_t* _r)
{
	if ((!_h) || (!_r))
	{
		LOG(L_ERR, "convert_result: Invalid parameter\n");
		return -1;
	}

	if (get_columns(_h, _r) < 0)
	{
		LOG(L_ERR, "convert_result: Error while getting column names\n");
		return -2;
	}

	if (convert_rows(_h, _r) < 0)
	{
		LOG(L_ERR, "convert_result: Error while converting rows\n");
		free_columns(_r);
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
		LOG(L_ERR, "free_result: Invalid parameter\n");
		return -1;
	}

	free_columns(_r);
	free_rows(_r);
	pkg_free(_r);
	return 0;
}
