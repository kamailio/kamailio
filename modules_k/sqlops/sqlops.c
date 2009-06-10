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

/** module functions */
static int sql_query(struct sip_msg*, char*, char*, char*);
static int sql_rfree(struct sip_msg*, char*, char*);
static int child_init(int rank);
void destroy(void);

static int fixup_sql_query(void** param, int param_no);
static int fixup_sql_rfree(void** param, int param_no);

static int sql_param(modparam_t type, void* val);

static pv_export_t mod_pvs[] = {
	{ {"dbr", sizeof("dbr")-1}, PVT_OTHER, pv_get_dbr, 0,
		pv_parse_dbr_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static cmd_export_t cmds[]={
	{"sql_query",  (cmd_function)sql_query, 3, fixup_sql_query, 0, 
		REQUEST_ROUTE | FAILURE_ROUTE |
		ONREPLY_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE},
	{"sql_result_free",  (cmd_function)sql_rfree,  1, fixup_sql_rfree, 0, 
		REQUEST_ROUTE | FAILURE_ROUTE |
		ONREPLY_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE},
	{0,0,0,0,0,0}
};

static param_export_t params[]={
	{"sqlcon",  STR_PARAM|USE_FUNC_PARAM, (void*)sql_param},
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
void destroy(void)
{
	sql_destroy();
}

int sql_param(modparam_t type, void *val)
{
	if(val==NULL)
		goto error;

	return sql_parse_param((char*)val);
error:
	return -1;

}

static int sql_query(struct sip_msg *msg, char *dbl, char *query, char *res)
{
	return sql_do_query(msg, (sql_con_t*)dbl, (pv_elem_t*)query,
			(sql_result_t*)res);
}

static int sql_rfree(struct sip_msg *msg, char *res, char *s2)
{
	sql_reset_result((sql_result_t*)res);
	return 1;
}

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

