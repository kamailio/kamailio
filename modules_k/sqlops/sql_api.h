/**
 * $Id$
 *
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
		       
#ifndef _SQL_API_H_
#define _SQL_API_H_

#include "../../lib/srdb1/db.h"
#include "../../pvar.h"

typedef struct _sql_col
{
	str name;
    unsigned int colid;
} sql_col_t;

typedef struct _sql_val
{
	int flags;
	int_str value;
} sql_val_t;

typedef struct _sql_result
{
	unsigned int resid;
	str name;
	int nrows;
	int ncols;
	sql_col_t *cols;
	sql_val_t **vals;
	struct _sql_result *next;
} sql_result_t;

typedef struct _sql_con
{
	str name;
	unsigned int conid;
	str db_url;
	db1_con_t  *dbh;
	db_func_t dbf;
	struct _sql_con *next;
} sql_con_t;

int sql_parse_param(char *val);
void sql_destroy(void);
int sql_connect(void);
int sql_do_query(struct sip_msg *msg, sql_con_t *con, pv_elem_t *query,
		sql_result_t *res);
sql_con_t* sql_get_connection(str *name);
sql_result_t* sql_get_result(str *name);

void sql_reset_result(sql_result_t *res);

#endif
