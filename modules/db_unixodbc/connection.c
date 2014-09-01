/*
 * $Id$
 *
 * UNIXODBC module
 *
 * Copyright (C) 2005-2006 Marco Lorrai
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
 *
 * History:
 * --------
 *  2005-12-01  initial commit (chgen)
 *  2006-01-10  UID (username) and PWD (password) attributes added to
 *              connection string (bogdan)
 *  2006-05-05  extract_error passes back last error state on return (sgupta)
 */

#include "connection.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include <time.h>

#define DSN_ATTR  "DSN="
#define DSN_ATTR_LEN  (sizeof(DSN_ATTR)-1)
#define UID_ATTR  "UID="
#define UID_ATTR_LEN  (sizeof(UID_ATTR)-1)
#define PWD_ATTR  "PWD="
#define PWD_ATTR_LEN  (sizeof(PWD_ATTR)-1)


char *db_unixodbc_build_conn_str(const struct db_id* id, char *buf)
{
	int len, ld, lu, lp;
	char *p;

	if (!buf) return 0;

	ld = id->database?strlen(id->database):0;
	lu = id->username?strlen(id->username):0;
	lp = id->password?strlen(id->password):0;

	len = (ld?(DSN_ATTR_LEN + ld + 1):0)
		+ (lu?(UID_ATTR_LEN + lu + 1):0)
		+ PWD_ATTR_LEN + lp + 1;

	if ( len>=MAX_CONN_STR_LEN ){
		LM_ERR("connection string too long! Increase MAX_CONN_STR_LEN"
				" and recompile\n");
		return 0;
	}

	p = buf;
	if (ld) {
		memcpy( p , DSN_ATTR, DSN_ATTR_LEN);
		p += DSN_ATTR_LEN;
		memcpy( p, id->database, ld);
		p += ld;
	}
	if (lu) {
		*(p++) = ';';
		memcpy( p , UID_ATTR, UID_ATTR_LEN);
		p += UID_ATTR_LEN;
		memcpy( p, id->username, lu);
		p += lu;
	}
	if (lp) {
		*(p++) = ';';
		memcpy( p , PWD_ATTR, PWD_ATTR_LEN);
		p += PWD_ATTR_LEN;
		memcpy( p, id->password, lp);
		p += lp;
	}
	*(p++) = ';';
	*p = 0 ; /* make it null terminated */

	return buf;
}


/*
 * Create a new connection structure,
 * open the UNIXODBC connection and set reference count to 1
 */
struct my_con* db_unixodbc_new_connection(struct db_id* id)
{
	SQLCHAR outstr[1024];
	SQLSMALLINT outstrlen;
	int ret;
	struct my_con* ptr;
	char conn_str[MAX_CONN_STR_LEN];

	if (!id)
	{
		LM_ERR("invalid parameter value\n");
		return 0;
	}

	ptr = (struct my_con*)pkg_malloc(sizeof(struct my_con));
	if (!ptr)
	{
		LM_ERR("no more memory left\n");
		return 0;
	}

	memset(ptr, 0, sizeof(struct my_con));
	ptr->ref = 1;
	// allocate environment handle
	ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &(ptr->env));
	if ((ret != SQL_SUCCESS) && (ret != SQL_SUCCESS_WITH_INFO))
	{
		LM_ERR("could not alloc a SQL handle\n");
		if (ptr) pkg_free(ptr);
		return 0;
	}
	// set the environment
	ret = SQLSetEnvAttr(ptr->env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);
	if ((ret != SQL_SUCCESS) && (ret != SQL_SUCCESS_WITH_INFO))
	{
		LM_ERR("could not set the environment\n");
		goto err1;
	}
	// allocate connection handle
	ret = SQLAllocHandle(SQL_HANDLE_DBC, ptr->env, &(ptr->dbc));
	if ((ret != SQL_SUCCESS) && (ret != SQL_SUCCESS_WITH_INFO))
	{
		LM_ERR("could not alloc a connection handle %d\n", ret);
		goto err1;
	}

	if (!db_unixodbc_build_conn_str(id, conn_str)) {
		LM_ERR("failed to build connection string\n");
		goto err2;
	}

	LM_DBG("opening connection: unixodbc://xxxx:xxxx@%s/%s\n", ZSW(id->host),
		ZSW(id->database));

	ret = SQLDriverConnect(ptr->dbc, NULL, (SQLCHAR*)conn_str, SQL_NTS,
		outstr, sizeof(outstr), &outstrlen,
		SQL_DRIVER_COMPLETE);
	if (SQL_SUCCEEDED(ret))
	{
		LM_DBG("connection succeeded with reply <%s>\n", outstr);
		if (ret == SQL_SUCCESS_WITH_INFO)
		{
			LM_DBG("driver reported the following diagnostics\n");
			db_unixodbc_extract_error("SQLDriverConnect", ptr->dbc, SQL_HANDLE_DBC, NULL);
		}
	}
	else
	{
		LM_ERR("failed to connect\n");
		db_unixodbc_extract_error("SQLDriverConnect", ptr->dbc, SQL_HANDLE_DBC, NULL);
		goto err2;
	}

	ptr->stmt_handle = NULL;

	ptr->timestamp = time(0);
	ptr->id = id;
	return ptr;

err1:
	SQLFreeHandle(SQL_HANDLE_ENV, &(ptr->env));
	if (ptr) pkg_free(ptr);
	return 0;

err2:
	SQLFreeHandle(SQL_HANDLE_ENV, &(ptr->env));
	SQLFreeHandle(SQL_HANDLE_DBC, &(ptr->dbc));
	if (ptr) pkg_free(ptr);
	return 0;
}

/*
 * Close the connection and release memory
 */
void db_unixodbc_free_connection(struct my_con* con)
{
	if (!con) return;
	SQLFreeHandle(SQL_HANDLE_ENV, con->env);
	SQLDisconnect(con->dbc);
	SQLFreeHandle(SQL_HANDLE_DBC, con->dbc);
	pkg_free(con);
}


void db_unixodbc_extract_error(const char *fn, const SQLHANDLE handle, const SQLSMALLINT type, char* stret)
{
	SQLINTEGER   i = 0;
	SQLINTEGER   native;
	SQLCHAR  state[ 7 ];
	SQLCHAR  text[256];
	SQLSMALLINT  len;
	SQLRETURN	ret;

	do
	{
		ret = SQLGetDiagRec(type, handle, ++i, state, &native, text,
			sizeof(text), &len );
		if (SQL_SUCCEEDED(ret)) {
			LM_ERR("unixodbc:%s=%s:%ld:%ld:%s\n", fn, state, (long)i,
					(long)native, text);
			if(stret) strcpy( stret, (char*)state );
		}
	}
	while( ret == SQL_SUCCESS );
}

/*
 * Allocate a new row of cells, without any data
 */
strn * db_unixodbc_new_cellrow(size_t ncols)
{
	strn * temp_row;

	temp_row = (strn *)pkg_malloc(ncols * sizeof(strn));
	if (temp_row) memset(temp_row, 0, ncols * sizeof(strn));
	return temp_row;
}

/*
 * Free row of cells and all associated memory
 */
void db_unixodbc_free_cellrow(size_t ncols, strn * row)
{
	size_t i;

	for (i = 0; i < ncols; i++) {
		if (row[i].s != NULL) pkg_free(row[i].s);
	}
	pkg_free(row);
}

/*
 * Load ODBC cell data into a single cell
 */
int db_unixodbc_load_cell(const db1_con_t* _h, int colindex, strn * cell, const db_type_t _t)
{
	SQLRETURN ret = 0;
	unsigned int truesize = 0;
	unsigned char hasnull = (_t != DB1_BLOB) ? 1 : 0;

	do {
		SQLLEN indicator;
		int chunklen;
		char * s;	/* Pointer to available area for next chunk */
		char * ns;

		if (cell->buflen > 0) {
			ns = (char *)pkg_realloc(cell->s, cell->buflen + STRN_LEN);
			if (ns == NULL) {
				LM_ERR("no memory left\n");
				return 0;
			}
			cell->s = ns;

			/* Overwrite the previous null terminator */
			s = cell->s + cell->buflen - hasnull;
			chunklen = STRN_LEN + hasnull;
		} else {
			ns = (char *)pkg_malloc(STRN_LEN);
			if (ns == NULL) {
				LM_ERR("no memory left\n");
				return 0;
			}
			cell->s = ns;
			s = cell->s;
			chunklen = STRN_LEN;
		}
		cell->buflen += STRN_LEN;

		ret = SQLGetData(CON_RESULT(_h), colindex, hasnull ? SQL_C_CHAR : SQL_C_BINARY,
					s, chunklen, &indicator);
		LM_DBG("SQLGetData returned ret=%d indicator=%d\n", (int)ret, (int)indicator);
		if (ret == SQL_SUCCESS) {
			if (indicator == SQL_NULL_DATA) {
			    /* TODO: set buffer pointer to NULL instead of string "NULL" */
			    strcpy(cell->s, "NULL");
			    truesize = 4 + (1 - hasnull);
			} else {
			    /* Get length of data that was available before last SQLGetData call */
			    if (truesize == 0) truesize = indicator;
			}
		} else if (ret == SQL_SUCCESS_WITH_INFO) {
			SQLINTEGER   i = 0;
			SQLINTEGER   native;
			SQLCHAR  state[ 7 ];
			SQLCHAR  text[256];
			SQLSMALLINT  len;
			SQLRETURN	ret2;

			/* Check whether field data was truncated */
			do
			{
				ret2 = SQLGetDiagRec(SQL_HANDLE_STMT, CON_RESULT(_h), ++i, state, &native, text,
					sizeof(text), &len );
				if (SQL_SUCCEEDED(ret2)) {
					if (!strcmp("00000", (const char*)state)) break;
					if (strcmp("01004", (const char*)state) != 0) {
						/* Not a string truncation */
						LM_ERR("SQLGetData failed unixodbc:  =%s:%ld:%ld:%s\n", state, (long)i,
							(long)native, text);
						return 0;
					}

					/* Get length of data that was available before last SQLGetData call */
					if (truesize == 0) truesize = indicator;
				} else if (ret2 == SQL_NO_DATA) {
				    /* Reached end of diagnostic records */
					break;
				} else {
					/* Failed to get diagnostics */
					LM_ERR("SQLGetData failed, failed to get diagnostics (ret2=%d i=%d)\n",
						ret2, i);
					return 0;
				}
			}
			while( ret2 == SQL_SUCCESS );
		} else {
			LM_ERR("SQLGetData failed\n");
		}
	} while (ret == SQL_SUCCESS_WITH_INFO);

	LM_DBG("Total allocated for this cell (before resize): %d bytes\n", cell->buflen);

	if (cell->buflen > truesize) {
		cell->s[truesize] = '\0'; /* guarantee strlen() will terminate */
	}
	cell->buflen = truesize + hasnull;

	LM_DBG("Total allocated for this cell (after resize): %d bytes\n", cell->buflen);
	LM_DBG("strlen() reports for this cell: %d bytes\n", (int)strlen(cell->s));

	return 1;
}
