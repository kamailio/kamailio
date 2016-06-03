/*
 * http_client API test Module
 * Copyright (C) 2015-2016 Edvina AB, Olle E. Johansson
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
 */

/*! \file
 * \brief  Kamailio http_client :: The test module
 * 	   Not compiled by default
 * \ingroup http_client
 */

#include "../../modules/http_client/curl_api.h"

#include "../../mod_fix.h"
#include "../../sr_module.h"
#include "../../ut.h"
#include "../../resolve.h"
#include "../../locking.h"
#include "../../config.h"
#include "../../lvalue.h"

MODULE_VERSION

/* Module parameter variables */
str		default_http_conn = STR_NULL;		/*!< Default connection to test */

/* Module management function prototypes */
static int mod_init(void);
static int child_init(int);
static void destroy(void);

/* Fixup functions to be defined later */
static int fixup_testcurl_connect(void** param, int param_no);
static int fixup_free_testcurl_connect(void** param, int param_no);
static int fixup_testcurl_connect_post(void** param, int param_no);
static int fixup_free_testcurl_connect_post(void** param, int param_no);
static int w_testcurl_connect(struct sip_msg* _m, char* _con, char * _url, char* _result);
static httpc_api_t httpapi;

/* Exported functions */
static cmd_export_t cmds[] = {
	/* Test_http_connect(connection, <URL>, <result pvar>)  - HTTP GET */
	{"test_http_connect", (cmd_function)w_testcurl_connect, 3, fixup_testcurl_connect,
	 	fixup_free_testcurl_connect,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
};


/* Exported parameters */
static param_export_t params[] = {
	{"test_connection", PARAM_INT, &default_connection_timeout},
	{0, 0, 0}
};

/* Module interface */
struct module_exports exports = {
	"httpapitest",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,      /* Exported functions */
	params,    /* Exported parameters */
	0,         /* exported statistics */
	0,   	/* exported MI functions */
	0,         /* exported pseudo-variables */
	0,         /* extra processes */
	mod_init,  /* module initialization function */
	0,         /* response function*/
	destroy,   /* destroy function */
	child_init /* per-child init function */
};


/* Module initialization function */
static int mod_init(void)
{
	
	LM_DBG("init httpapitest module\n");

	if (httpc_load_api(&httpapi) != 0) {
		LM_ERR("Can not bind to http_client API \n");
		return -1;
	}

	LM_DBG("**** init httpapitest module done.\n");
	return 0;
}


/* Child initialization function */
static int child_init(int rank)
{	
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN) {
		return 0; /* do nothing for the main process */
	}

	return 0;
}


static void destroy(void)
{
	;
}



/* Fixup functions */


/*
 * Fix test_curl_connect params: 
 * 1. connection(string/pvar) 
 * 2. url (string that may contain pvars) and
 * 3. result (writable pvar).
 */
static int fixup_testcurl_connect(void** param, int param_no)
{

	/* 1. Connection */
	if (param_no == 1) {
		/* We want char * strings */
		return 0;
	}
	/* 2. URL and data may contain pvar */
	if (param_no == 2) {
		return fixup_spve_null(param, 1);
	}
	/* 3. PVAR for result */
	if (param_no == 3) {
		if (fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if (((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writeble\n");
			return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Free testcurl_connect params.
 */
static int fixup_free_testcurl_connect(void** param, int param_no)
{
	if (param_no == 1) {
		/* Char strings don't need freeing */
		return 0;
	}
	if (param_no == 2) {
		return fixup_free_spve_null(param, 1);
	}

	if (param_no == 3) {
		return fixup_free_pvar_null(param, 1);
	}
	
	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Wrapper for Curl_connect (GET)
 */
static int w_testcurl_connect(struct sip_msg* _m, char* _con, char * _url, char* _result) {

	str con = {NULL,0};
	str url = {NULL,0};
	str result = {NULL,0};
	pv_spec_t *dst;
	pv_value_t val;
	int ret = 0;

	if (_con == NULL || _url == NULL || _result == NULL) {
		LM_ERR("Invalid parameter\n");
		return -1;
	}
	con.s = _con;
	con.len = strlen(con.s);

	if (get_str_fparam(&url, _m, (gparam_p)_url) != 0) {
		LM_ERR("_url has no value\n");
		return -1;
	}

	LM_DBG("**** Curl Connection %s URL %s Result var %s\n", _con, _url, _result);

	
	/* API    http_connect(msg, connection, url, result, content_type, post) */
	ret = httpapi.http_connect(_m, &con, &url, &result, NULL, NULL);

	val.rs = result;
	val.flags = PV_VAL_STR;
	dst = (pv_spec_t *)_result;
	dst->setf(_m, &dst->pvp, (int)EQ_T, &val);

	if (result.s != NULL)
		pkg_free(result.s);

	return ret;
}
