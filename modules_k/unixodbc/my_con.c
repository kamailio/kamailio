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
 *  2005-12-01  initial commit (chgen) */

#include "my_con.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "utils.h"
#include <time.h>

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

	if (!id)
	{
		LOG(L_ERR, "new_connection: Invalid parameter value\n");
		return 0;
	}

	ptr = (struct my_con*)pkg_malloc(sizeof(struct my_con));
	if (!ptr)
	{
		LOG(L_ERR, "new_connection: No memory left\n");
		return 0;
	}

	memset(ptr, 0, sizeof(struct my_con));
	ptr->ref = 1;

/*NEW CODE*/
	SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &(ptr->env));
	SQLSetEnvAttr(ptr->env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);
	SQLAllocHandle(SQL_HANDLE_DBC, ptr->env, &(ptr->dbc));
	char stringDNS[200];
	sprintf( stringDNS, "%s%s%s", "DSN=", id->database, ";");
	ret = SQLDriverConnect(ptr->dbc, (void *)1, (SQLCHAR*)stringDNS, SQL_NTS,
		outstr, sizeof(outstr), &outstrlen,
		SQL_DRIVER_COMPLETE);
	if (SQL_SUCCEEDED(ret))
	{
		printf("Connected\n");
		printf("Returned connection string was:\n\t%s\n", outstr);
		if (ret == SQL_SUCCESS_WITH_INFO)
		{
			printf("Driver reported the following diagnostics\n");
			extract_error("SQLDriverConnect", ptr->dbc, SQL_HANDLE_DBC);
			goto err;
		}
	}
	else
	{
		fprintf(stderr, "Failed to connect\n");
		extract_error("SQLDriverConnect", ptr->dbc, SQL_HANDLE_DBC);
		goto err;
	}
/*END NEW CODE*/
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
SQLSMALLINT type)
{
	SQLINTEGER   i = 0;
	SQLINTEGER   native;
	SQLCHAR  state[ 7 ];
	SQLCHAR  text[256];
	SQLSMALLINT  len;
	SQLRETURN	ret;

	fprintf(stderr,
		"\n"
		"The driver reported the following diagnostics whilst running "
		"%s\n\n",
		fn);

	do
	{
		ret = SQLGetDiagRec(type, handle, ++i, state, &native, text,
			sizeof(text), &len );
		if (SQL_SUCCEEDED(ret))
			printf("%s:%ld:%ld:%s\n", state, (long)i, (long)native, text);
	}
	while( ret == SQL_SUCCESS );
}
