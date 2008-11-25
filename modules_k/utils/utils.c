/*
 * utils Module
 *
 * Copyright (C) 2008 Juha Heinanen
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
 *
 * History:
 * -------
 * 2008-11-24: Introduced utils module and its first function: http_query.
 *
 */


#include <curl/curl.h>

#include "../../mod_fix.h"
#include "../../sr_module.h"
#include "../../ut.h"

#include "functions.h"


MODULE_VERSION


/* Module management function prototypes */
static int mod_init(void);
static void destroy(void);


/* Module parameter variables */

int http_query_timeout = 4;
str http_server = str_init("http://localhost");


/* Internal variables */


/* Fixup functions to be defined later */
static int fixup_http_query(void** param, int param_no);
static int fixup_free_http_query(void** param, int param_no);


/* Exported functions */
static cmd_export_t cmds[] = {
    {"http_query", (cmd_function)http_query, 3, fixup_http_query,
     fixup_free_http_query,
     REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
    {0, 0, 0, 0, 0, 0}
};


/* Exported parameters */
static param_export_t params[] = {
    {"http_query_timeout", INT_PARAM, &http_query_timeout},
    {"http_server", STR_PARAM, &http_server.s},
    {0, 0, 0}
};


/* Module interface */
struct module_exports exports = {
    "utils",
    DEFAULT_DLFLAGS, /* dlopen flags */
    cmds,      /* Exported functions */
    params,    /* Exported parameters */
    0,         /* exported statistics */
    0,         /* exported MI functions */
    0,         /* exported pseudo-variables */
    0,         /* extra processes */
    mod_init,  /* module initialization function */
    0,         /* response function*/
    destroy,   /* destroy function */
    0          /* per-child init function */
};


/* Module initialization function */
static int mod_init(void)
{
    /* Update length of module variables */
    http_server.len = strlen(http_server.s);

    /* Initialize curl */
    if (curl_global_init(CURL_GLOBAL_ALL)) {
	LM_ERR("curl_global_init failed\n");
	return -1;
    }

    return 0;
}


static void destroy(void)
{
    /* Cleanup curl */
    curl_global_cleanup();
}


/* Fixup functions */

/*
 * Fix http_query params: server page (string that may contain pvars),
 * params (string that may contain pvars), and result (writable pvar).
 */
static int fixup_http_query(void** param, int param_no)
{
    LM_INFO("entering fixup_http_query\n");

    if ((param_no == 1) || (param_no == 2)) {
	LM_INFO("leaving fixup_http_query\n");
	return fixup_spve_null(param, 1);
    }

    if (param_no == 3) {
	if (fixup_pvar(param) != 0) {
	    LM_ERR("failed to fixup result pvar\n");
	    return -1;
	}
	if (((pv_spec_t *)(*param))->setf == NULL) {
	    LM_ERR("result pvar is not writeble\n");
	    return -1;
	}
	LM_INFO("leaving fixup_http_query\n");
	return 0;
    }

    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}

/*
 * Free http_query params.
 */
static int fixup_free_http_query(void** param, int param_no)
{
    if ((param_no == 1) || (param_no == 2)) {
	LM_WARN("free function has not been defined for spve\n");
	return 0;
    }

    if (param_no == 3) {
	return fixup_free_pvar_null(param, 1);
    }
    
    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}
