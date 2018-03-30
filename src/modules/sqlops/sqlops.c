/**
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
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
 */

/*! \file
 * \ingroup sqlops
 * \brief Kamailio SQL-operations :: Module interface
 *
 * - Module: \ref sqlops
 */

/*! \defgroup sqlops Kamailio :: SQL Operations
 *
 * The module adds support for raw SQL queries in the configuration file.
 *
 */





#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/pvar.h"
#include "../../core/kemi.h"

#include "sql_api.h"
#include "sql_var.h"
#include "sql_trans.h"


MODULE_VERSION

static int bind_sqlops(sqlops_api_t* api);

/** module functions */
static int sql_query(struct sip_msg*, char*, char*, char*);
static int sql_query2(struct sip_msg*, char*, char*);
static int sql_query_async(struct sip_msg*, char*, char*);
#ifdef WITH_XAVP
static int sql_xquery(struct sip_msg *msg, char *dbl, char *query, char *res);
#endif
static int sql_pvquery(struct sip_msg *msg, char *dbl, char *query, char *res);
static int sql_rfree(struct sip_msg*, char*, char*);
static int mod_init(void);
static int child_init(int rank);
static void destroy(void);

static int fixup_sql_query(void** param, int param_no);
#ifdef WITH_XAVP
static int fixup_sql_xquery(void** param, int param_no);
#endif
static int fixup_sql_pvquery(void** param, int param_no);
static int fixup_sql_rfree(void** param, int param_no);

static int sql_con_param(modparam_t type, void* val);
static int sql_res_param(modparam_t type, void* val);

extern int sqlops_tr_buf_size;

static pv_export_t mod_pvs[] = {
	{ {"dbr", sizeof("dbr")-1}, PVT_OTHER, pv_get_dbr, 0,
		pv_parse_dbr_name, 0, 0, 0 },
	{ {"sqlrows", sizeof("sqlrows")-1}, PVT_OTHER, pv_get_sqlrows, 0,
		pv_parse_con_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static cmd_export_t cmds[]={
	{"sql_query",  (cmd_function)sql_query, 3, fixup_sql_query, 0,
		ANY_ROUTE},
	{"sql_query",  (cmd_function)sql_query2, 2, fixup_sql_query, 0,
		ANY_ROUTE},
	{"sql_query_async",  (cmd_function)sql_query_async, 2, fixup_sql_query, 0,
		ANY_ROUTE},
#ifdef WITH_XAVP
	{"sql_xquery",  (cmd_function)sql_xquery, 3, fixup_sql_xquery, 0,
		ANY_ROUTE},
#endif
	{"sql_pvquery",  (cmd_function)sql_pvquery, 3, fixup_sql_pvquery, 0,
		ANY_ROUTE},
	{"sql_result_free",  (cmd_function)sql_rfree,  1, fixup_sql_rfree, 0,
		ANY_ROUTE},
	{"bind_sqlops", (cmd_function)bind_sqlops, 0, 0, 0, 0},
	{0,0,0,0,0,0}
};

static param_export_t params[]={
	{"sqlcon",  PARAM_STRING|USE_FUNC_PARAM, (void*)sql_con_param},
	{"sqlres",  PARAM_STRING|USE_FUNC_PARAM, (void*)sql_res_param},
	{"tr_buf_size",     PARAM_INT,   &sqlops_tr_buf_size},
	{0,0,0}
};

static tr_export_t mod_trans[] = {
	{ {"sql", sizeof("sql")-1}, tr_parse_sql },
	{ { 0, 0 }, 0 }
};


/** module exports */
struct module_exports exports= {
	"sqlops",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,          /* exported statistics */
	0  ,        /* exported MI functions */
	mod_pvs,    /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	(destroy_function) destroy,
	child_init  /* per-child init function */
};

static int mod_init(void)
{
	if(sqlops_tr_buffer_init()<0) {
		return -1;
	}
	return 0;
}

static int child_init(int rank)
{
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0;
	return sql_connect();
}

/**
 * destroy function
 */
static void destroy(void)
{
	sql_destroy();
}

/**
 * parse sqlcon module parameter
 */
int sql_con_param(modparam_t type, void *val)
{
	if(val==NULL)
		goto error;

	return sql_parse_param((char*)val);
error:
	return -1;

}

/**
 * parse sqlres module parameter
 */
int sql_res_param(modparam_t type, void *val)
{
	sql_result_t *res = NULL;
	str s;

	if(val==NULL)
	{
		LM_ERR("invalid parameter\n");
		goto error;
	}

	s.s = (char*)val;
	s.len = strlen(s.s);

	res = sql_get_result(&s);
	if(res==NULL)
	{
		LM_ERR("invalid result [%s]\n", s.s);
		goto error;
	}
	return 0;
error:
	return -1;
}

/**
 *
 */
static int sql_query(struct sip_msg *msg, char *dbl, char *query, char *res)
{
	str sq;
	if(pv_printf_s(msg, (pv_elem_t*)query, &sq)!=0)
	{
		LM_ERR("cannot print the sql query\n");
		return -1;
	}
	return sql_do_query((sql_con_t*)dbl, &sq, (sql_result_t*)res);
}

static int sql_query2(struct sip_msg *msg, char *dbl, char *query)
{
	return sql_query(msg, dbl, query, NULL);
}

static int sql_query_async(struct sip_msg *msg, char *dbl, char *query)
{
	str sq;
	if(pv_printf_s(msg, (pv_elem_t*)query, &sq)!=0)
	{
		LM_ERR("cannot print the sql query\n");
		return -1;
	}
	return sql_do_query_async((sql_con_t*)dbl, &sq);
}


#ifdef WITH_XAVP
/**
 *
 */
static int sql_xquery(struct sip_msg *msg, char *dbl, char *query, char *res)
{
	return sql_do_xquery(msg, (sql_con_t*)dbl, (pv_elem_t*)query, (pv_elem_t*)res);
}
#endif

/**
 *
 */
static int sql_pvquery(struct sip_msg *msg, char *dbl, char *query, char *res)
{
	return sql_do_pvquery(msg, (sql_con_t*)dbl, (pv_elem_t*)query, (pvname_list_t*)res);
}

/**
 *
 */
static int sql_rfree(struct sip_msg *msg, char *res, char *s2)
{
	sql_reset_result((sql_result_t*)res);
	return 1;
}

/**
 *
 */
static int fixup_sql_query(void** param, int param_no)
{
	sql_con_t *con = NULL;
	pv_elem_t *query = NULL;
	sql_result_t *res = NULL;
	str s;

	s.s = (char*)(*param);
	s.len = strlen(s.s);

	if (param_no==1) {
		con = sql_get_connection(&s);
		if(con==NULL)
		{
			LM_ERR("invalid connection [%s]\n", s.s);
			return E_UNSPEC;
		}
		*param = (void*)con;
	} else if (param_no==2) {
		if(pv_parse_format(&s, &query)<0)
		{
			LM_ERR("invalid query string [%s]\n", s.s);
			return E_UNSPEC;
		}
		*param = (void*)query;
	} else if (param_no==3) {
		res = sql_get_result(&s);
		if(res==NULL)
		{
			LM_ERR("invalid result [%s]\n", s.s);
			return E_UNSPEC;
		}
		*param = (void*)res;
	}
	return 0;
}

#ifdef WITH_XAVP
/**
 *
 */
static int fixup_sql_xquery(void** param, int param_no)
{
	sql_con_t *con = NULL;
	pv_elem_t *pv = NULL;
	str s;

	s.s = (char*)(*param);
	s.len = strlen(s.s);

	if (param_no==1) {
		con = sql_get_connection(&s);
		if(con==NULL)
		{
			LM_ERR("invalid connection [%s]\n", s.s);
			return E_UNSPEC;
		}
		*param = (void*)con;
	} else if (param_no==2) {
		if(pv_parse_format(&s, &pv)<0)
		{
			LM_ERR("invalid query string [%s]\n", s.s);
			return E_UNSPEC;
		}
		*param = (void*)pv;
	} else if (param_no==3) {
		if(pv_parse_format(&s, &pv)<0)
		{
			LM_ERR("invalid result [%s]\n", s.s);
			return E_UNSPEC;
		}
		*param = (void*)pv;
	}
	return 0;
}
#endif

/**
 *
 */
static int fixup_sql_pvquery(void** param, int param_no)
{
	sql_con_t *con = NULL;
	pv_elem_t *pv = NULL;
	pvname_list_t *res = NULL;
	pvname_list_t *pvl = NULL;
	str s;
	int i;

	if(*param == NULL)
	{
		LM_ERR("missing parameter %d\n", param_no);
		return E_UNSPEC;
	}
	s.s = (char*)(*param);
	s.len = strlen(s.s);

	if (param_no==1) {
		con = sql_get_connection(&s);
		if(con==NULL)
		{
			LM_ERR("invalid connection [%s]\n", s.s);
			return E_UNSPEC;
		}
		*param = (void*)con;
	} else if (param_no==2) {
		if(pv_parse_format(&s, &pv)<0)
		{
			LM_ERR("invalid query string [%s]\n", s.s);
			return E_UNSPEC;
		}
		*param = (void*)pv;
	} else if (param_no==3) {
		/* parse result variables into list of pv_spec_t's */
		res = parse_pvname_list(&s, 0);
		if(res==NULL)
		{
			LM_ERR("invalid result parameter [%s]\n", s.s);
			return E_UNSPEC;
		}
		/* check if all result variables are writable */
		pvl = res;
		i = 1;
		while (pvl) {
			if (pvl->sname.setf == NULL)
			{
				LM_ERR("result variable [%d] is read-only\n", i);
				free_pvname_list(res);
				return E_UNSPEC;
			}
			i++;
			pvl = pvl->next;
		}
		*param = (void*)res;
		return 0;
	}
	return 0;
}

/**
 *
 */
static int fixup_sql_rfree(void** param, int param_no)
{
	sql_result_t *res = NULL;
	str s;

	s.s = (char*)(*param);
	s.len = strlen(s.s);

	if (param_no==1) {
		res = sql_get_result(&s);
		if(res==NULL)
		{
			LM_ERR("invalid result [%s]\n", s.s);
			return E_UNSPEC;
		}
		*param = (void*)res;
	}
	return 0;
}

/**
 * @brief bind functions to SQLOPS API structure
 */
static int bind_sqlops(sqlops_api_t* api)
{
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}
	api->query = sqlops_do_query;
	api->value = sqlops_get_value;
	api->is_null = sqlops_is_null;
	api->column  = sqlops_get_column;
	api->reset   = sqlops_reset_result;
	api->nrows   = sqlops_num_rows;
	api->ncols   = sqlops_num_columns;
	api->xquery  = sqlops_do_xquery;

	return 0;
}

static int ki_sqlops_query(sip_msg_t *msg, str *scon, str *squery, str *sres)
{
	return sqlops_do_query(scon, squery, sres);
}

static int ki_sqlops_reset_result(sip_msg_t *msg, str *sres)
{
	sqlops_reset_result(sres);
	return 1;
}

static int ki_sqlops_num_rows(sip_msg_t *msg, str *sres)
{
	return sqlops_num_rows(sres);
}

static int ki_sqlops_num_columns(sip_msg_t *msg, str *sres)
{
	return sqlops_num_columns(sres);
}

static int ki_sqlops_is_null(sip_msg_t *msg, str *sres, int i, int j)
{
	return sqlops_is_null(sres, i, j);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_sqlops_exports[] = {
	{ str_init("sqlops"), str_init("sql_query"),
		SR_KEMIP_INT, ki_sqlops_query,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sqlops"), str_init("sql_result_free"),
		SR_KEMIP_INT, ki_sqlops_reset_result,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sqlops"), str_init("sql_num_rows"),
		SR_KEMIP_INT, ki_sqlops_num_rows,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sqlops"), str_init("sql_num_columns"),
		SR_KEMIP_INT, ki_sqlops_num_columns,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sqlops"), str_init("sql_is_null"),
		SR_KEMIP_INT, ki_sqlops_is_null,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_INT,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sqlops"), str_init("sql_xquery"),
		SR_KEMIP_INT, sqlops_do_xquery,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_sqlops_exports);
	return register_trans_mod(path, mod_trans);
}
