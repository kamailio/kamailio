/*
 * lost module
 *
 * Copyright (C) 2019 Wolfgang Kampichler
 * DEC112, FREQUENTIS AG
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

/*!
 * \file
 * \brief Kamailio lost ::
 * \ingroup lost
 * Module: \ref lost
 */

#include "../../modules/http_client/curl_api.h"

#include "../../core/mod_fix.h"
#include "../../core/sr_module.h"
#include "../../core/ut.h"
#include "../../core/locking.h"

#include "../../core/pvar.h"
#include "../../core/mem/mem.h"
#include "../../core/dprint.h"

#include "../../core/script_cb.h"

#include "functions.h"

MODULE_VERSION

/* Module parameter variables */
httpc_api_t httpapi;

/* Module management function prototypes */
static int mod_init(void);
static int child_init(int);
static void destroy(void);

/* Fixup functions to be defined later */
static int fixup_lost_held_query(void **param, int param_no);
static int fixup_free_lost_held_query(void **param, int param_no);
static int fixup_lost_held_query_id(void **param, int param_no);
static int fixup_free_lost_held_query_id(void **param, int param_no);

static int fixup_lost_query(void **param, int param_no);
static int fixup_free_lost_query(void **param, int param_no);
static int fixup_lost_query_all(void **param, int param_no);
static int fixup_free_lost_query_all(void **param, int param_no);

/* Wrappers for http_query to be defined later */
static int w_lost_held_query(
		struct sip_msg *_m, char *_con, char *_pidf, char *_url, char *_err);
static int w_lost_held_query_id(struct sip_msg *_m, char *_con, char *_id,
		char *_pidf, char *_url, char *_err);
static int w_lost_query(
		struct sip_msg *_m, char *_con, char *_uri, char *_name, char *_err);
static int w_lost_query_all(struct sip_msg *_m, char *_con, char *_pidf,
		char *_urn, char *_uri, char *_name, char *_err);

/* Exported functions */
static cmd_export_t cmds[] = {
		{"lost_held_query", (cmd_function)w_lost_held_query, 4,
				fixup_lost_held_query, fixup_free_lost_held_query,
				REQUEST_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE},
		{"lost_held_query", (cmd_function)w_lost_held_query_id, 5,
				fixup_lost_held_query_id, fixup_free_lost_held_query_id,
				REQUEST_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE},
		{"lost_query", (cmd_function)w_lost_query, 4, fixup_lost_query,
				fixup_free_lost_query,
				REQUEST_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE},
		{"lost_query", (cmd_function)w_lost_query_all, 6, fixup_lost_query_all,
				fixup_free_lost_query_all,
				REQUEST_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE},
		{0, 0, 0, 0, 0, 0}};


/* Module interface */
struct module_exports exports = {
		"lost",			 /* module name*/
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			 /* exported functions */
		0,				 /* exported parameters */
		0,				 /* RPC method exports */
		0,				 /* exported pseudo-variables */
		0,				 /* response handling function */
		mod_init,		 /* module initialization function */
		child_init,		 /* per-child init function */
		destroy			 /* module destroy function */
};

/* Module initialization function */
static int mod_init(void)
{
	LM_DBG("init lost module\n");

	if(httpc_load_api(&httpapi) != 0) {
		LM_ERR("Can not bind to http_client API \n");
		return -1;
	}

	LM_DBG("**** init lost module done.\n");

	return 0;
}

/* Child initialization function */
static int child_init(int rank)
{
	return 0;
}

static void destroy(void)
{
	;
	/* do nothing */
}

/*
 * Fix 4 lost_held_query params: con (string/pvar)
 * and pidf, url, err (writable pvar).
 */
static int fixup_lost_held_query(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_spve_null(param, 1);
	}
	if((param_no == 2) || (param_no == 3) || (param_no == 4)) {
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writable\n");
			return -1;
		}
		return 0;
	}
	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Free lost_held_query params.
 */
static int fixup_free_lost_held_query(void **param, int param_no)
{
	if(param_no == 1) {
		/* char strings don't need freeing */
		return 0;
	}
	if((param_no == 2) || (param_no == 3) || (param_no == 4)) {
		return fixup_free_pvar_null(param, 1);
	}
	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Fix 5 lost_held_query_id params: con (string/pvar) id (string that may contain
 * pvars) and pidf, url, err (writable pvar).
 */
static int fixup_lost_held_query_id(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_spve_null(param, 1);
	}
	if(param_no == 2) {
		return fixup_spve_null(param, 1);
	}
	if((param_no == 3) || (param_no == 4) || (param_no == 5)) {
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writable\n");
			return -1;
		}
		return 0;
	}
	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Free lost_held_query_id params.
 */
static int fixup_free_lost_held_query_id(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_free_spve_null(param, 1);
	}
	if(param_no == 2) {
		return fixup_free_spve_null(param, 1);
	}
	if((param_no == 3) || (param_no == 4) || (param_no == 5)) {
		return fixup_free_pvar_null(param, 1);
	}
	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Fix 4 lost_query params: con (string/pvar)
 * and uri, name, err (writable pvar).
 */
static int fixup_lost_query(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_spve_null(param, 1);
	}
	if((param_no == 2) || (param_no == 3) || (param_no == 4)) {
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writable\n");
			return -1;
		}
		return 0;
	}
	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Free lost_held_query_id params.
 */
static int fixup_free_lost_query(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_free_spve_null(param, 1);
	}
	if((param_no == 2) || (param_no == 3) || (param_no == 4)) {
		return fixup_free_pvar_null(param, 1);
	}
	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Fix 6 lost_query params: con (string/pvar) pidf, urn (string that may contain
 * pvars) and uri, name, err (writable pvar).
 */
static int fixup_lost_query_all(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_spve_null(param, 1);
	}
	if((param_no == 2) || (param_no == 3)) {
		return fixup_spve_null(param, 1);
	}
	if((param_no == 4) || (param_no == 5) || (param_no == 6)) {
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writable\n");
			return -1;
		}
		return 0;
	}
	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Free lost_held_query_id params.
 */
static int fixup_free_lost_query_all(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_free_spve_null(param, 1);
	}
	if((param_no == 2) || (param_no == 3)) {
		return fixup_free_spve_null(param, 1);
	}
	if((param_no == 4) || (param_no == 5) || (param_no == 6)) {
		return fixup_free_pvar_null(param, 1);
	}
	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Wrapper for lost_held_query w/o id
 */
static int w_lost_held_query(
		struct sip_msg *_m, char *_con, char *_pidf, char *_url, char *_err)
{
	return lost_function_held(_m, _con, _pidf, _url, _err, NULL);
}

/*
 * Wrapper for lost_held_query with id
 */
static int w_lost_held_query_id(struct sip_msg *_m, char *_con, char *_id,
		char *_pidf, char *_url, char *_err)
{
	return lost_function_held(_m, _con, _pidf, _url, _err, _id);
}

/*
 * Wrapper for lost_query w/o pudf, urn
 */
static int w_lost_query(
		struct sip_msg *_m, char *_con, char *_uri, char *_name, char *_err)
{
	return lost_function(_m, _con, _uri, _name, _err, NULL, NULL);
}

/*
 * Wrapper for lost_query with pidf, urn
 */
static int w_lost_query_all(struct sip_msg *_m, char *_con, char *_pidf,
		char *_urn, char *_uri, char *_name, char *_err)
{
	return lost_function(_m, _con, _uri, _name, _err, _pidf, _urn);
}
