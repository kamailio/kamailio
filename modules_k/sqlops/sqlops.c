/**
 * $Id$
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*! \file
 * \ingroup sqlops
 * \brief SIP-router SQL-operations :: Module interface
 *
 * - Module: \ref sqlops
 */

/*! \defgroup sqlops SIP-Router :: SQL Operations
 *  \note Kamailio module - part of modules_k

 * The module adds support for raw SQL queries in the configuration file.

 */





#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../sr_module.h"
#include "../../dprint.h"

#include "../../pvar.h"
#include "sql_api.h"
#include "sql_var.h"


MODULE_VERSION

static int bind_sqlops(sqlops_api_t* api);

/** module functions */
static int sql_query(struct sip_msg*, char*, char*, char*);
#ifdef WITH_XAVP
static int sql_xquery(struct sip_msg *msg, char *dbl, char *query, char *res);
#endif
static int sql_rfree(struct sip_msg*, char*, char*);
static int child_init(int rank);
static void destroy(void);

static int fixup_sql_query(void** param, int param_no);
#ifdef WITH_XAVP
static int fixup_sql_xquery(void** param, int param_no);
#endif
static int fixup_sql_rfree(void** param, int param_no);

static int sql_con_param(modparam_t type, void* val);
static int sql_res_param(modparam_t type, void* val);

static pv_export_t mod_pvs[] = {
	{ {"dbr", sizeof("dbr")-1}, PVT_OTHER, pv_get_dbr, 0,
		pv_parse_dbr_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static cmd_export_t cmds[]={
	{"sql_query",  (cmd_function)sql_query, 3, fixup_sql_query, 0, 
		REQUEST_ROUTE | FAILURE_ROUTE |
		ONREPLY_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE},
#ifdef WITH_XAVP
	{"sql_xquery",  (cmd_function)sql_xquery, 3, fixup_sql_xquery, 0, 
		REQUEST_ROUTE | FAILURE_ROUTE |
		ONREPLY_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE},
#endif
	{"sql_result_free",  (cmd_function)sql_rfree,  1, fixup_sql_rfree, 0, 
		REQUEST_ROUTE | FAILURE_ROUTE |
		ONREPLY_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE},
	{"bind_sqlops", (cmd_function)bind_sqlops, 0, 0, 0, 0},
	{0,0,0,0,0,0}
};

static param_export_t params[]={
	{"sqlcon",  STR_PARAM|USE_FUNC_PARAM, (void*)sql_con_param},
	{"sqlres",  STR_PARAM|USE_FUNC_PARAM, (void*)sql_res_param},
	{0,0,0}
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
	0,          /* module initialization function */
	0,
	(destroy_function) destroy,
	child_init  /* per-child init function */
};

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

#ifdef WITH_XAVP
/**
 *
 */
static int sql_xquery(struct sip_msg *msg, char *dbl, char *query, char *res)
{
	return sql_do_xquery(msg, (sql_con_t*)dbl, query, (pv_elem_t*)res);
}
#endif

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

	return 0;
}
