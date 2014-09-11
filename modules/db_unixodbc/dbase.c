/*
 * $Id$
 *
 * UNIXODBC module core functions
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
 *  2006-04-03  fixed invalid handle to extract error (sgupta)
 *  2006-04-04  removed deprecated ODBC functions, closed cursors on error
 *              (sgupta)
 *  2006-05-05  Fixed reconnect code to actually work on connection loss
 *              (sgupta)
 */


#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../lib/srdb1/db_query.h"
#include "val.h"
#include "connection.h"
#include "row.h"
#include "res.h"
#include "list.h"
#include "db_unixodbc.h"
#include "dbase.h"


/*
 * Reconnect if connection is broken
 */
static int reconnect(const db1_con_t* _h)
{
	int ret = 0;
	SQLCHAR outstr[1024];
	SQLSMALLINT outstrlen;
	char conn_str[MAX_CONN_STR_LEN];

	LM_ERR("Attempting DB reconnect\n");

	/* Disconnect */
	SQLDisconnect (CON_CONNECTION(_h));

	/* Reconnect */
	if (!db_unixodbc_build_conn_str(CON_ID(_h), conn_str)) {
		LM_ERR("failed to build connection string\n");
		return ret;
	}

	ret = SQLDriverConnect(CON_CONNECTION(_h), (void *)1,
			(SQLCHAR*)conn_str, SQL_NTS, outstr, sizeof(outstr),
			&outstrlen, SQL_DRIVER_COMPLETE);
	if (!SQL_SUCCEEDED(ret)) {
		LM_ERR("failed to connect\n");
		db_unixodbc_extract_error("SQLDriverConnect", CON_CONNECTION(_h),
			SQL_HANDLE_DBC, NULL);
		return ret;
	}

	ret = SQLAllocHandle(SQL_HANDLE_STMT, CON_CONNECTION(_h),
			&CON_RESULT(_h));
	if (!SQL_SUCCEEDED(ret)) {
		LM_ERR("Statement allocation error %d\n", (int)(long)CON_CONNECTION(_h));
		db_unixodbc_extract_error("SQLAllocStmt", CON_CONNECTION(_h), SQL_HANDLE_DBC,NULL);
		return ret;
	}

	return ret;
}

/*
 * Send an SQL query to the server
 */
static int db_unixodbc_submit_query(const db1_con_t* _h, const str* _s)
{
	int ret = 0;
	SQLCHAR sqlstate[7];

	if (!_h || !_s || !_s->s) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	/* first do some cleanup if required */
	if(CON_RESULT(_h))
	{
		SQLCloseCursor(CON_RESULT(_h));
		SQLFreeHandle(SQL_HANDLE_STMT, CON_RESULT(_h));
	}

	ret = SQLAllocHandle(SQL_HANDLE_STMT, CON_CONNECTION(_h), &CON_RESULT(_h));
	if (!SQL_SUCCEEDED(ret))
	{
		LM_ERR("statement allocation error %d\n",
				(int)(long)CON_CONNECTION(_h));
		db_unixodbc_extract_error("SQLAllocStmt", CON_CONNECTION(_h), SQL_HANDLE_DBC,
			(char*)sqlstate);

		/* Connection broken */
		if( !strncmp((char*)sqlstate,"08003",5) ||
		!strncmp((char*)sqlstate,"08S01",5) ) {
			ret = reconnect(_h);
			if( !SQL_SUCCEEDED(ret) ) return ret;
		} else {
			return ret;
		}
	}

	ret=SQLExecDirect(CON_RESULT(_h),  (SQLCHAR*)_s->s, _s->len);
	if (!SQL_SUCCEEDED(ret))
	{
		SQLCHAR sqlstate[7];
		LM_ERR("rv=%d. Query= %.*s\n", ret, _s->len, _s->s);
		db_unixodbc_extract_error("SQLExecDirect", CON_RESULT(_h), SQL_HANDLE_STMT,
			(char*)sqlstate);

		/* Connection broken */
		if( !strncmp((char*)sqlstate,"08003",5) ||
		    !strncmp((char*)sqlstate,"08S01",5)
		    )
		{
			ret = reconnect(_h);
			if( SQL_SUCCEEDED(ret) ) {
				/* Try again */
				ret=SQLExecDirect(CON_RESULT(_h),  (SQLCHAR*)_s->s, _s->len);
				if (!SQL_SUCCEEDED(ret)) {
					LM_ERR("rv=%d. Query= %.*s\n", ret, _s->len, _s->s);
					db_unixodbc_extract_error("SQLExecDirect", CON_RESULT(_h),
						SQL_HANDLE_STMT, (char*)sqlstate);
					/* Close the cursor */
					SQLCloseCursor(CON_RESULT(_h));
					SQLFreeHandle(SQL_HANDLE_STMT, CON_RESULT(_h));
				}
			}

		}
		else {
			/* Close the cursor */
			SQLCloseCursor(CON_RESULT(_h));
			SQLFreeHandle(SQL_HANDLE_STMT, CON_RESULT(_h));
		}
	}

	return ret;
}



/*
 * Initialize database module
 * No function should be called before this
 */
db1_con_t* db_unixodbc_init(const str* _url)
{
	return db_do_init(_url, (void*)db_unixodbc_new_connection);
}

/*
 * Shut down database module
 * No function should be called after this
 */
void db_unixodbc_close(db1_con_t* _h)
{
	return db_do_close(_h, db_unixodbc_free_connection);
}

/*
 * Retrieve result set
 */
static int db_unixodbc_store_result(const db1_con_t* _h, db1_res_t** _r)
{
	if ((!_h) || (!_r))
	{
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	*_r = db_new_result();

	if (*_r == 0)
	{
		LM_ERR("no memory left\n");
		return -2;
	}

	if (db_unixodbc_convert_result(_h, *_r) < 0)
	{
		LM_ERR("failed to convert result\n");
		LM_DBG("freeing result set at %p\n", _r);
		pkg_free(*_r);
		*_r = 0;
		return -4;
	}
	return 0;
}

/*
 * Release a result set from memory
 */
int db_unixodbc_free_result(db1_con_t* _h, db1_res_t* _r)
{
	if ((!_h) || (!_r))
	{
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (db_free_result(_r) < 0)
	{
		LM_ERR("failed to free result structure\n");
		return -1;
	}
	SQLFreeHandle(SQL_HANDLE_STMT, CON_RESULT(_h));
	CON_RESULT(_h) = 0;
	return 0;
}

/*
 * Query table for specified rows
 * _h: structure representing database connection
 * _k: key names
 * _op: operators
 * _v: values of the keys that must match
 * _c: column names to return
 * _n: number of key=values pairs to compare
 * _nc: number of columns to return
 * _o: order by the specified column
 */
int db_unixodbc_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
		const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
		const db_key_t _o, db1_res_t** _r)
{
	return db_do_query(_h, _k, _op, _v, _c, _n, _nc, _o, _r,
			db_unixodbc_val2str,  db_unixodbc_submit_query, db_unixodbc_store_result);
}

/*!
 * \brief Gets a partial result set, fetch rows from a result
 *
 * Gets a partial result set, fetch a number of rows from a databae result.
 * This function initialize the given result structure on the first run, and
 * fetches the nrows number of rows. On subsequenting runs, it uses the
 * existing result and fetches more rows, until it reaches the end of the
 * result set. Because of this the result needs to be null in the first
 * invocation of the function. If the number of wanted rows is zero, the
 * function returns anything with a result of zero.
 * \param _h structure representing the database connection
 * \param _r pointer to a structure representing the result
 * \param nrows number of fetched rows
 * \return return zero on success, negative value on failure
 */
int db_unixodbc_fetch_result(const db1_con_t* _h, db1_res_t** _r, const int nrows)
{
	int row_n = 0, i = 0, ret = 0, len;
	SQLSMALLINT columns;
	list* rows = NULL;
	list* rowstart = NULL;
	strn* temp_row = NULL;

	if ((!_h) || (!_r) || nrows < 0)
	{
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	/* exit if the fetch count is zero */
	if (nrows == 0) {
		if (*_r)
			db_free_result(*_r);
		*_r = 0;
		return 0;
	}

	/* On the first fetch for a query, allocate structures and get columns */
	if(*_r == NULL) {
		/* Allocate a new result structure */
		*_r = db_new_result();
		LM_DBG("just allocated a new db result structure");

		if (*_r == NULL) {
			LM_ERR("no memory left\n");
			return -2;
		}

		/* Get columns names and count */
		if (db_unixodbc_get_columns(_h, *_r) < 0) {
			LM_ERR("getting column names failed\n");
			db_free_columns(*_r);
			return -2;
		}

	/* On subsequent fetch attempts, reuse already allocated structures */
	} else {
		LM_DBG("db result structure already exist, reusing\n");
		/* free old rows */
		if(RES_ROWS(*_r) != NULL)
			db_free_rows(*_r);
		RES_ROWS(*_r) = 0;
		RES_ROW_N(*_r) = 0;
	}

	SQLNumResultCols(CON_RESULT(_h), (SQLSMALLINT *)&columns);

	/* Now fetch nrows at most */
	len = sizeof(db_row_t) * nrows;
	RES_ROWS(*_r) = (struct db_row*)pkg_malloc(len);
	if (!RES_ROWS(*_r)) {
		LM_ERR("no memory left\n");
		return -5;
	}
	LM_DBG("allocated %d bytes for RES_ROWS at %p\n", len, RES_ROWS(*_r));

	LM_DBG("Now fetching %i rows at most\n", nrows);
	while(SQL_SUCCEEDED(ret = SQLFetch(CON_RESULT(_h)))) {
		/* Allocate a temporary row */
		temp_row = db_unixodbc_new_cellrow(columns);
		if (!temp_row) {
			LM_ERR("no private memory left\n");
			pkg_free(RES_ROWS(*_r));
			pkg_free(*_r);
			*_r = 0;
			return -1;
		}

		LM_DBG("fetching %d columns for row %d...\n",columns, row_n);
		for(i=0; i < columns; i++) {
			LM_DBG("fetching column %d\n",i);
			if (!db_unixodbc_load_cell(_h, i+1, temp_row + i, RES_TYPES(*_r)[i])) {
			    pkg_free(RES_ROWS(*_r));
			    db_unixodbc_free_cellrow(columns, temp_row);
			    pkg_free(*_r);
			    *_r = 0;
			    return -5;
			}
		}

		LM_DBG("got temp_row at %p\n", temp_row);

		if (db_unixodbc_list_insert(&rowstart, &rows, columns, temp_row) < 0) {
			LM_ERR("SQL result row insert failed\n");
			pkg_free(RES_ROWS(*_r));
			db_unixodbc_free_cellrow(columns, temp_row);
			pkg_free(*_r);
			*_r = 0;
			return -5;
		}

		/* Free temporary row data */
		LM_DBG("freeing temp_row at %p\n", temp_row);
		db_unixodbc_free_cellrow(columns, temp_row);
		temp_row = NULL;

		row_n++;
		if (row_n == nrows) {
			break;
		}
	}

	CON_ROW(_h) = NULL;

	RES_ROW_N(*_r) = row_n;
	if (!row_n) {
		LM_DBG("no more rows to process for db fetch");
		pkg_free(RES_ROWS(*_r));
		RES_ROWS(*_r) = 0;
		return 0;
	}

	/* Convert rows to internal format */
	memset(RES_ROWS(*_r), 0, len);
	i = 0;
	rows = rowstart;
	while(rows)
	{
		LM_DBG("converting row #%d\n", i);
		CON_ROW(_h) = rows->data;
		if (!CON_ROW(_h))
		{
			LM_ERR("string null\n");
			RES_ROW_N(*_r) = row_n;
			db_free_rows(*_r);
			return -3;
		}
		if (db_unixodbc_convert_row(_h, *_r, &(RES_ROWS(*_r)[i]), rows->lengths) < 0) {
			LM_ERR("converting fetched row #%d failed\n", i);
			RES_ROW_N(*_r) = i;
			db_free_rows(*_r);
			return -4;
		}
		i++;
		rows = rows->next;
	}
	db_unixodbc_list_destroy(rowstart);

	/* update the total number of rows processed */
	RES_LAST_ROW(*_r) += row_n;
	LM_DBG("fetch from db processed %d rows so far\n", RES_LAST_ROW(*_r));

	return 0;
}


/*
 * Execute a raw SQL query
 */
int db_unixodbc_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r)
{
	return db_do_raw_query(_h, _s, _r, db_unixodbc_submit_query,
			db_unixodbc_store_result);
}

/*
 * Insert a row into specified table
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys
 * _n: number of key=value pairs
 */
int db_unixodbc_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v, const int _n)
{
	return db_do_insert(_h, _k, _v, _n, db_unixodbc_val2str,
			db_unixodbc_submit_query);
}

/*
 * Delete a row from the specified table
 * _h: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _n: number of key=value pairs
 */
int db_unixodbc_delete(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, const int _n)
{
	return db_do_delete(_h, _k, _o, _v, _n, db_unixodbc_val2str,
			db_unixodbc_submit_query);
}

/*
 * Update some rows in the specified table
 * _h: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _uk: updated columns
 * _uv: updated values of the columns
 * _n: number of key=value pairs
 * _un: number of columns to update
 */
int db_unixodbc_update(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv, const int _n, const int _un)
{
	return db_do_update(_h, _k, _o, _v, _uk, _uv, _n, _un, db_unixodbc_val2str,
			db_unixodbc_submit_query);
}

/*
 * Just like insert, but replace the row if it exists
 */
int db_unixodbc_replace(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v, const int _n,
		const int _un, const int _m)
{
	return db_do_replace(_h, _k, _v, _n, db_unixodbc_val2str,
			db_unixodbc_submit_query);
}

/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_unixodbc_use_table(db1_con_t* _h, const str* _t)
{
	return db_use_table(_h, _t);
}
