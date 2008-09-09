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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include "../../db/db_query.h"
#include "val.h"
#include "con.h"
#include "res.h"
#include "db_unixodbc.h"
#include "dbase.h"


/*
 * Reconnect if connection is broken
 */
static int reconnect(const db_con_t* _h)
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
static int db_unixodbc_submit_query(const db_con_t* _h, const str* _s)
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
db_con_t* db_unixodbc_init(const str* _url)
{
	return db_do_init(_url, (void*)db_unixodbc_new_connection);
}

/*
 * Shut down database module
 * No function should be called after this
 */
void db_unixodbc_close(db_con_t* _h)
{
	return db_do_close(_h, db_unixodbc_free_connection);
}

/*
 * Retrieve result set
 */
static int db_unixodbc_store_result(const db_con_t* _h, db_res_t** _r)
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
		pkg_free(*_r);
		*_r = 0;
		return -4;
	}
	return 0;
}

/*
 * Release a result set from memory
 */
int db_unixodbc_free_result(db_con_t* _h, db_res_t* _r)
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
int db_unixodbc_query(const db_con_t* _h, const db_key_t* _k, const db_op_t* _op,
const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
const db_key_t _o, db_res_t** _r)
{
	return db_do_query(_h, _k, _op, _v, _c, _n, _nc, _o, _r,
	db_unixodbc_val2str,  db_unixodbc_submit_query, db_unixodbc_store_result);
}

/*
 * Execute a raw SQL query
 */
int db_unixodbc_raw_query(const db_con_t* _h, const str* _s, db_res_t** _r)
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
int db_unixodbc_insert(const db_con_t* _h, const db_key_t* _k, const db_val_t* _v, const int _n)
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
int db_unixodbc_delete(const db_con_t* _h, const db_key_t* _k, const db_op_t* _o,
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
int db_unixodbc_update(const db_con_t* _h, const db_key_t* _k, const db_op_t* _o,
const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv, const int _n, const int _un)
{
	return db_do_update(_h, _k, _o, _v, _uk, _uv, _n, _un, db_unixodbc_val2str,
	db_unixodbc_submit_query);
}

/*
 * Just like insert, but replace the row if it exists
 */
int db_unixodbc_replace(const db_con_t* _h, const db_key_t* _k, const db_val_t* _v, const int _n)
{
	return db_do_replace(_h, _k, _v, _n, db_unixodbc_val2str,
	db_unixodbc_submit_query);
}

/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_unixodbc_use_table(db_con_t* _h, const str* _t)
{
	return db_use_table(_h, _t);
}
