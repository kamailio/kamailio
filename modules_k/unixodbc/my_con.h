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
 */


#ifndef MY_CON_H
#define MY_CON_H

#include "../../db/db_pool.h"
#include "../../db/db_id.h"

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <sql.h>
#include <sqlext.h>

#define STRN_LEN

typedef struct strn
{
	char s[STRN_LEN];
} strn;

struct my_con
{
	struct db_id* id;				  /* Connection identifier */
	unsigned int ref;				  /* Reference count */
	struct pool_con* next;			  /* Next connection in the pool */
	SQLHENV env;
	SQLHSTMT stmt_handle;			  /* Actual result */
	SQLHDBC dbc;					  /* Connection representation */
	strn *row;						  /* Actual row in the result */
	time_t timestamp;				  /* Timestamp of last query */
};

/*
 * Some convenience wrappers
 */
#define CON_RESULT(db_con)	 (((struct my_con*)((db_con)->tail))->stmt_handle)
#define CON_CONNECTION(db_con) (((struct my_con*)((db_con)->tail))->dbc)
#define CON_ROW(db_con)		(((struct my_con*)((db_con)->tail))->row)
#define CON_TIMESTAMP(db_con)  (((struct my_con*)((db_con)->tail))->timestamp)

/*
 * Create a new connection structure,
 * open the UNIXODBC connection and set reference count to 1
 */
struct my_con* new_connection(struct db_id* id);

/*
 * Close the connection and release memory
 */
void free_connection(struct my_con* con);

void extract_error(char *fn, SQLHANDLE handle, SQLSMALLINT type);
#endif													  /* MY_CON_H */
