/* 
 * $Id$
 *
 * UNIXODBC module
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
 *  2006-01-10  UID (username) and PWD (password) attributes added to 
 *              connection string (bogdan)
 *  2006-05-05  extract_error passes back last error state on return (sgupta)
 */

#include "my_con.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "utils.h"
#include <time.h>

#define DSN_ATTR  "DSN="
#define DSN_ATTR_LEN  (sizeof(DSN_ATTR)-1)
#define UID_ATTR  "UID="
#define UID_ATTR_LEN  (sizeof(UID_ATTR)-1)
#define PWD_ATTR  "PWD="
#define PWD_ATTR_LEN  (sizeof(PWD_ATTR)-1)


char *build_conn_str(struct db_id* id, char *buf)
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
		LOG(L_ERR,"ERROR:unixodbc:build_conn_str: connection string too long!"
			"Increase MAX_CONN_STR_LEN and recompile\n");
		return 0;
	}

	p = buf;
	if (ld) {
		memcpy( p , DSN_ATTR, DSN_ATTR_LEN);
		p += DSN_ATTR_LEN;
		memcpy( p, id->database, ld);
		p += ld;
		*(p++) = ';';
	}
	if (lu) {
		memcpy( p , UID_ATTR, UID_ATTR_LEN);
		p += UID_ATTR_LEN;
		memcpy( p, id->username, lu);
		p += lu;
		*(p++) = ';';
	}
	memcpy( p , PWD_ATTR, PWD_ATTR_LEN);
	p += PWD_ATTR_LEN;
	if (lp) {
		memcpy( p, id->password, lp);
		p += lp;
	}
	*(p++) = ';';
	*p = 0 ; /* make it null terminated */

	DBG("DEBUG:unixodbc:build_conn_str: connection string is <%s>\n",buf);
	return buf;
}


/*
 * Create a new connection structure,
 * open the UNIXODBC connection and set reference count to 1
 */
struct my_con* new_connection(struct db_id* id)
{
	SQLCHAR outstr[1024];
	SQLSMALLINT outstrlen;
	int ret;
	struct my_con* ptr;
	char conn_str[MAX_CONN_STR_LEN];

	if (!id)
	{
		LOG(L_ERR,"ERROR:unixodbc:new_connection: Invalid parameter value\n");
		return 0;
	}

	ptr = (struct my_con*)pkg_malloc(sizeof(struct my_con));
	if (!ptr)
	{
		LOG(L_ERR,"ERROR:unixodbc:new_connection: No memory left\n");
		return 0;
	}

	memset(ptr, 0, sizeof(struct my_con));
	ptr->ref = 1;

	SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &(ptr->env));
	SQLSetEnvAttr(ptr->env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);
	SQLAllocHandle(SQL_HANDLE_DBC, ptr->env, &(ptr->dbc));

	if (!build_conn_str(id, conn_str)) {
		LOG(L_ERR, "ERROR:unixodbc:new_connection: failed to build "
			"connection string\n");
		return 0;
	}
	ret = SQLDriverConnect(ptr->dbc, (void *)1, (SQLCHAR*)conn_str, SQL_NTS,
		outstr, sizeof(outstr), &outstrlen,
		SQL_DRIVER_COMPLETE);
	if (SQL_SUCCEEDED(ret))
	{
		DBG("DEBUG:unixodbc:new_connection: connection succeeded with reply"
			" <%s>\n", outstr);
		if (ret == SQL_SUCCESS_WITH_INFO)
		{
			DBG("DEBUG:unixodbc:new_connection: driver reported the "
				"following diagnostics\n");
			extract_error("SQLDriverConnect", ptr->dbc, SQL_HANDLE_DBC, NULL);
			goto err;
		}
	}
	else
	{
		LOG(L_ERR, "ERROR:unixodbc:new_connection: failed to connect\n");
		extract_error("SQLDriverConnect", ptr->dbc, SQL_HANDLE_DBC, NULL);
		goto err;
	}

	ptr->stmt_handle = NULL;

	ptr->timestamp = time(0);
	ptr->id = id;
	return ptr;

	err:
	if (ptr) pkg_free(ptr);
	return 0;
}

/*
 * Close the connection and release memory
 */
void free_connection(struct my_con* con)
{
	if (!con) return;
	SQLFreeHandle(SQL_HANDLE_ENV, con->env);
	SQLDisconnect(con->dbc);
	SQLFreeHandle(SQL_HANDLE_DBC, con->dbc);
	pkg_free(con);
}


void extract_error(
char *fn,
SQLHANDLE handle,
SQLSMALLINT type,
char* stret)
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
			LOG(L_ERR,"unixodbc:%s=%s:%ld:%ld:%s\n", fn, state, (long)i, 
					(long)native, text);
			if(stret) strcpy( stret, (char*)state );
		}
	}
	while( ret == SQL_SUCCESS );
}
