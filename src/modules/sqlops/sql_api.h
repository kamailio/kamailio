/**
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*! \file
 * \ingroup sqlops
 * \brief Kamailio SQL-operations :: API
 *
 * - Module: \ref sqlops
 */

		       
#ifndef _SQL_API_H_
#define _SQL_API_H_

#include "../../sr_module.h"
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

int sql_do_query(sql_con_t *con, str *query, sql_result_t *res);
int sql_do_query_async(sql_con_t *con, str *query);
#ifdef WITH_XAVP
int sql_do_xquery(sip_msg_t *msg, sql_con_t *con, pv_elem_t *query,
		pv_elem_t *res);
#endif
int sql_do_pvquery(sip_msg_t *msg, sql_con_t *con, pv_elem_t *query,
		pvname_list_t *res);
int pv_get_sqlrows(sip_msg_t *msg,  pv_param_t *param,
		pv_value_t *res);
int pv_parse_con_name(pv_spec_p sp, str *in);

sql_con_t* sql_get_connection(str *name);
sql_result_t* sql_get_result(str *name);

void sql_reset_result(sql_result_t *res);

typedef int (*sqlops_do_query_f)(str *scon, str *squery, str *sres);
int sqlops_do_query(str *scon, str *squery, str *sres);

typedef int (*sqlops_get_value_f)(str *sres, int i, int j, sql_val_t **val);
int sqlops_get_value(str *sres, int i, int j, sql_val_t **val);

typedef int (*sqlops_is_null_f)(str *sres, int i, int j);
int sqlops_is_null(str *res, int i, int j);

typedef int (*sqlops_get_column_f)(str *sres, int i, str *col);
int sqlops_get_column(str *sres, int i, str *name);

typedef int (*sqlops_num_columns_f)(str *sres);
int sqlops_num_columns(str *sres);

typedef int (*sqlops_num_rows_f)(str *sres);
int sqlops_num_rows(str *sres);

typedef void (*sqlops_reset_result_f)(str *sres);
void sqlops_reset_result(str *sres);

typedef int (*sqlops_do_xquery_f)(sip_msg_t *msg, str *scon, str *squery, str *sxavp);
int sqlops_do_xquery(sip_msg_t *msg, str *scon, str *squery, str *sxavp);

typedef struct sqlops_api {
	sqlops_do_query_f query;
	sqlops_get_value_f value;
	sqlops_is_null_f is_null;
	sqlops_get_column_f column;
	sqlops_reset_result_f reset;
	sqlops_num_rows_f nrows;
	sqlops_num_columns_f ncols;
	sqlops_do_xquery_f xquery;
} sqlops_api_t;

typedef int (*bind_sqlops_f)(sqlops_api_t* api);

/**
 * @brief Load the SQLOps API
 */
static inline int sqlops_load_api(sqlops_api_t *sqb)
{
	bind_sqlops_f bindsqlops;

	bindsqlops = (bind_sqlops_f)find_export("bind_sqlops", 0, 0);
	if ( bindsqlops == 0) {
		LM_ERR("cannot find bind_sqlops\n");
		return -1;
	}
	if (bindsqlops(sqb)==-1)
	{
		LM_ERR("cannot bind sqlops api\n");
		return -1;
	}
	return 0;
}


#endif
