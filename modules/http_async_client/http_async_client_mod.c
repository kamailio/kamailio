/**
 * Copyright 2016 (C) Federico Cabiddu <federico.cabiddu@gmail.com>
 * Copyright 2016 (C) Giacomo Vacca <giacomo.vacca@gmail.com>
 * Copyright 2016 (C) Orange - Camille Oudot <camille.oudot@orange.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*! \file
 * \brief  Kamailio http_async_client :: The module interface file
 * \ingroup http_async_client
 */

/*! \defgroup http_async_client Kamailio :: Async module interface to Curl/HTTP
 *
 * http://curl.haxx.se
 * A generic library for many protocols
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../globals.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../pt.h"
#include "../../pvar.h"
#include "../../mem/shm_mem.h"
#include "../../mod_fix.h"
#include "../../pvar.h"
#include "../../cfg/cfg_struct.h"
#include "../../lib/kcore/faked_msg.h"

#include "../../modules/tm/tm_load.h"
#include "../../modules/pv/pv_api.h"

#include "async_http.h"

MODULE_VERSION

extern int  num_workers;

int http_timeout = 500; /* query timeout in ms */
int hash_size = 2048;
int tls_version = 0; // Use default SSL version in HTTPS requests (see curl/curl.h)
int tls_verify_host = 1; // By default verify host in HTTPS requests
int tls_verify_peer = 1; // By default verify peer in HTTPS requests
int curl_verbose = 0;
str tls_client_cert = STR_STATIC_INIT(""); // client SSL certificate path, defaults to NULL
str tls_client_key = STR_STATIC_INIT(""); // client SSL certificate key path, defaults to NULL
str tls_ca_path = STR_STATIC_INIT(""); // certificate authority dir path, defaults to NULL
static char *memory_manager = "shm";
extern int curl_memory_manager;

static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);

static int w_http_async_get(sip_msg_t* msg, char* query, char* rt);
static int w_http_async_post(sip_msg_t* msg, char* query, char* post, char* rt);
static int w_tls_verify_host(sip_msg_t* msg, char* vh, char*);
static int w_tls_verify_peer(sip_msg_t* msg, char* vp, char*);
static int w_http_async_suspend_transaction(sip_msg_t* msg, char* vp, char*);
static int w_http_set_timeout(sip_msg_t* msg, char* tout, char*);
static int w_http_append_header(sip_msg_t* msg, char* hdr, char*);
static int w_http_set_method(sip_msg_t* msg, char* method, char*);
static int w_http_set_tls_client_cert(sip_msg_t* msg, char* sc, char*);
static int w_http_set_tls_client_key(sip_msg_t* msg, char* sk, char*);
static int w_http_set_tls_ca_path(sip_msg_t* msg, char* cp, char*);
static int set_query_param(str* param, str input);
static int fixup_http_async_get(void** param, int param_no);
static int fixup_http_async_post(void** param, int param_no);

/* pv api binding */
static int ah_get_reason(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int ah_get_hdr(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int w_pv_parse_hdr_name(pv_spec_p sp, str *in);
static int ah_get_status(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int ah_get_msg_body(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int ah_get_body_size(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int ah_get_msg_buf(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int ah_get_msg_len(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int ah_parse_req_name(pv_spec_p sp, str *in);
static int ah_set_req(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val);


static str pv_str_1 = {"1", 1};
static str pv_str_0 = {"0", 1};

static int ah_get_ok(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int ah_get_err(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);

/* tm */
struct tm_binds tmb;
/* pv */
pv_api_t pv_api;

stat_var *requests;
stat_var *replies;
stat_var *errors;
stat_var *timeouts;

enum http_req_name_t {
	E_HRN_ALL = 0,
	E_HRN_HDR, E_HRN_METHOD, E_HRN_TIMEOUT,
	E_HRN_TLS_CA_PATH, E_HRN_TLS_CLIENT_KEY,
	E_HRN_TLS_CLIENT_CERT, E_HRN_SUSPEND
};

static cmd_export_t cmds[]={
	{"http_async_query",  (cmd_function)w_http_async_get, 2, fixup_http_async_get,
		0, ANY_ROUTE},
	{"http_async_query", (cmd_function)w_http_async_post, 3, fixup_http_async_post,
		0, ANY_ROUTE},
	{"tls_verify_host", (cmd_function)w_tls_verify_host, 1, fixup_igp_all,
		0, ANY_ROUTE},
	{"tls_verify_peer", (cmd_function)w_tls_verify_peer, 1, fixup_igp_all,
		0, ANY_ROUTE},
	{"http_async_suspend", (cmd_function)w_http_async_suspend_transaction, 1, fixup_igp_all,
		0, ANY_ROUTE},
	{"http_set_timeout", (cmd_function)w_http_set_timeout, 1, fixup_igp_all,
		0, ANY_ROUTE},
	{"http_append_header", (cmd_function)w_http_append_header, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"http_set_method", (cmd_function)w_http_set_method, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"http_set_tls_client_cert", (cmd_function)w_http_set_tls_client_cert, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"http_set_tls_client_key", (cmd_function)w_http_set_tls_client_key, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"http_set_tls_ca_path", (cmd_function)w_http_set_tls_ca_path, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
{"workers",					INT_PARAM,   &num_workers},
	{"connection_timeout",	INT_PARAM,   &http_timeout},
	{"hash_size",			INT_PARAM,   &hash_size},
	{"tls_version",			INT_PARAM,   &tls_version},
	{"tls_verify_host",		INT_PARAM,   &tls_verify_host},
	{"tls_verify_peer",		INT_PARAM,   &tls_verify_peer},
	{"curl_verbose",		INT_PARAM,   &curl_verbose},
	{"tls_client_cert",		PARAM_STR,   &tls_client_cert},
	{"tls_client_key",		PARAM_STR,   &tls_client_key},
	{"tls_ca_path",			PARAM_STR,   &tls_ca_path},
	{"memory_manager",	PARAM_STRING,&memory_manager},
	{0, 0, 0}
};

/*! \brief We expose internal variables via the statistic framework below.*/
stat_export_t mod_stats[] = {
        {"requests",    STAT_NO_RESET, &requests        },
        {"replies", 	STAT_NO_RESET, &replies 	},
        {"errors",      STAT_NO_RESET, &errors       	},
        {"timeouts",    STAT_NO_RESET, &timeouts	},
        {0, 0, 0}
};

static pv_export_t pvs[] = {
	{STR_STATIC_INIT("http_hdr"),
		PVT_HDR, ah_get_hdr, 0,
		w_pv_parse_hdr_name, pv_parse_index, 0, 0},
	{STR_STATIC_INIT("http_rr"),
		PVT_OTHER, ah_get_reason, 0,
		0, 0, 0, 0},
	{STR_STATIC_INIT("http_rs"),
		PVT_OTHER, ah_get_status, 0,
		0, 0, 0, 0},
	{STR_STATIC_INIT("http_rb"),
		PVT_MSG_BODY, ah_get_msg_body, 0,
		0, 0, 0, 0},
	{STR_STATIC_INIT("http_bs"),
		PVT_OTHER, ah_get_body_size, 0,
		0, 0, 0, 0},
	{STR_STATIC_INIT("http_mb"),
		PVT_OTHER, ah_get_msg_buf, 0,
		0, 0, 0, 0},
	{STR_STATIC_INIT("http_ml"),
		PVT_OTHER, ah_get_msg_len, 0,
		0, 0, 0, 0},
	{STR_STATIC_INIT("http_ok"),
		PVT_OTHER, ah_get_ok, 0,
		0, 0, 0, 0},
	{STR_STATIC_INIT("http_err"),
		PVT_OTHER, ah_get_err, 0,
		0, 0, 0, 0},
	{STR_STATIC_INIT("http_req"),
		PVT_OTHER, pv_get_null, ah_set_req,
		ah_parse_req_name, 0, 0, 0},
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

struct module_exports exports = {
	"http_async_client",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	mod_stats,   	/* exported statistics */
	0,              /* exported MI functions */
	pvs,            /* exported pseudo-variables */
	0,              /* extra processes */
	mod_init,       /* module initialization function */
	0,              /* response function */
	mod_destroy,    /* destroy function */
	child_init      /* per child init function */
};



/**
 * init module function
 */
static int mod_init(void)
{
	unsigned int n;
	pv_register_api_t pvra;
	LM_INFO("Initializing Http Async module\n");

#ifdef STATISTICS
	/* register statistics */
	if (register_module_stats( exports.name, mod_stats)!=0 ) {
		LM_ERR("failed to register core statistics\n");
		return -1;
	}
#endif
	/* sanitize hash_size */
	if (hash_size < 1){
		LM_WARN("hash_size is smaller "
				"than 1  -> rounding from %d to 1\n",
				hash_size);
				hash_size = 1;
	}
	/* check that the hash table size is a power of 2 */
	for( n=0 ; n<(8*sizeof(n)) ; n++) {
		if (hash_size==(1<<n))
			break;
		if (n && hash_size<(1<<n)) {
			LM_WARN("hash_size is not a power "
				"of 2 as it should be -> rounding from %d to %d (n=%d)\n",
				hash_size, 1<<(n-1), n);
			hash_size = 1<<(n-1);
			break;
		}
	}
	/* check 'workers' param */
	if (num_workers < 1) {
		LM_ERR("the 'workers' parameter must be >= 1\n");
		return -1;
	}

	tls_verify_host = tls_verify_host?1:0;
	tls_verify_peer = tls_verify_peer?1:0;

	/* init http parameters list */
	init_query_params(&ah_params);

	if (strncmp("shm", memory_manager, 3) == 0) {
		curl_memory_manager = 0;
	} else if (strncmp("sys", memory_manager, 3) == 0) {
		curl_memory_manager = 1;
	} else {
		LM_ERR("invalid memory_manager parameter: '%s'\n", memory_manager);
		return -1;
	}

	/* init faked sip msg */
	if(faked_msg_init()<0) {
		LM_ERR("failed to init faked sip msg\n");
		return -1;
	}

	if(load_tm_api( &tmb ) < 0) {
		LM_INFO("cannot load the TM-functions - async relay disabled\n");
		memset(&tmb, 0, sizeof(tm_api_t));
	}

	pvra = (pv_register_api_t)find_export("pv_register_api", NO_SCRIPT, 0);
	if (!pvra) {
		LM_ERR("Cannot import pv functions (pv module must be loaded before this module)\n");
		return -1;
	}
	pvra(&pv_api);

	/* allocate workers array */
	workers = shm_malloc(num_workers * sizeof(*workers));
	if(workers == NULL) {
		LM_ERR("error in shm_malloc\n");
		return -1;
	}
	memset(workers, 0, num_workers * sizeof(*workers));

	register_procs(num_workers);

	/* add child to update local config framework structures */
	cfg_register_child(num_workers);

	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	int pid;
	int i;

	LM_DBG("child initializing async http\n");

	if(num_workers<=0)
		return 0;

	if (rank==PROC_INIT) {
		for(i=0; i<num_workers; i++) {
			LM_DBG("initializing worker notification socket: %d\n", i);
			if(async_http_init_sockets(&workers[i])<0) {
				LM_ERR("failed to initialize tasks sockets\n");
				return -1;
			}
		}

		return 0;
	}

	if(rank>0) {
		for(i=0; i<num_workers; i++) {
			close(workers[i].notication_socket[0]);
		}
		return 0;
	}
	if (rank!=PROC_MAIN)
		return 0;

	for(i=0; i<num_workers; i++) {
		if(async_http_init_worker(i+1, &workers[i])<0) {
			LM_ERR("failed to initialize worker process: %d\n", i);
			return -1;
		}
		pid=fork_process(PROC_RPC, "Http Worker", 1);
		if (pid<0)
			return -1; /* error */
		if(pid==0) {
			/* child */
			/* enforce http_reply_parse=yes */
			http_reply_parse = 1;
			/* initialize the config framework */
			if (cfg_child_init())
				return -1;
			/* init msg structure for http reply parsing */
			ah_reply = pkg_malloc(sizeof(struct sip_msg));
			if(!ah_reply) {
				LM_ERR("failed to allocate pkg memory\n");
				return -1;
			}
			memset(ah_reply, 0, sizeof(struct sip_msg));
			/* main function for workers */
			async_http_run_worker(&workers[i]);
		}
	}

	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
}

/**
 *
 */
static int w_http_async_get(sip_msg_t *msg, char *query, char* rt)
{
	str sdata;
	cfg_action_t *act;
	str rn;
	int ri;

	if(msg==NULL)
		return -1;

	if(fixup_get_svalue(msg, (gparam_t*)query, &sdata)!=0) {
		LM_ERR("unable to get data\n");
		return -1;
	}
	if(sdata.s==NULL || sdata.len == 0) {
		LM_ERR("invalid data parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)rt, &rn)!=0)
	{
		LM_ERR("no route block name\n");
		return -1;
	}

	ri = route_get(&main_rt, rn.s);
	if(ri<0)
	{
		LM_ERR("unable to find route block [%.*s]\n", rn.len, rn.s);
		return -1;
	}
	act = main_rt.rlist[ri];
	if(act==NULL)
	{
		LM_ERR("empty action lists in route block [%.*s]\n", rn.len, rn.s);
		return -1;
	}

	return async_send_query(msg, &sdata, NULL, act);

}

/**
 *
 */
static int w_http_async_post(sip_msg_t *msg, char *query, char* post, char* rt)
{
	str sdata;
	str post_data;
	cfg_action_t *act;
	str rn;
	int ri;

	if(msg==NULL)
		return -1;

	if(fixup_get_svalue(msg, (gparam_t*)query, &sdata)!=0) {
		LM_ERR("unable to get data\n");
		return -1;
	}

	if(sdata.s==NULL || sdata.len == 0) {
		LM_ERR("invalid data parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)post, &post_data)!=0) {
		LM_ERR("unable to get post data\n");
		return -1;
	}

	if(post_data.s==NULL || post_data.len == 0) {
		LM_ERR("invalid post data parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)rt, &rn)!=0)
	{
		LM_ERR("no route block name\n");
		return -1;
	}

	ri = route_get(&main_rt, rn.s);
	if(ri<0)
	{
		LM_ERR("unable to find route block [%.*s]\n", rn.len, rn.s);
		return -1;
	}
	act = main_rt.rlist[ri];
	if(act==NULL)
	{
		LM_ERR("empty action lists in route block [%.*s]\n", rn.len, rn.s);
		return -1;
	}

	if(async_send_query(msg, &sdata, &post_data, act)<0)
		return -1;

	/* force exit in config */
	return 0;
}

#define _IVALUE_ERROR(NAME) LM_ERR("invalid parameter '" #NAME "' (must be a number)\n")
#define _IVALUE(NAME)\
int i_##NAME ;\
if(fixup_get_ivalue(msg, (gparam_t*)NAME, &( i_##NAME))!=0)\
{ \
	_IVALUE_ERROR(NAME);\
	return -1;\
}

static int w_tls_verify_host(sip_msg_t* msg, char* vh, char*foo)
{
	_IVALUE (vh);
	ah_params.tls_verify_host = i_vh?1:0;
	return 1;
}

static int w_tls_verify_peer(sip_msg_t* msg, char* vp, char*foo)
{
	_IVALUE (vp);
	ah_params.tls_verify_peer = i_vp?1:0;
	return 1;
}

static int w_http_async_suspend_transaction(sip_msg_t* msg, char* vp, char*foo)
{
	_IVALUE (vp);
	ah_params.suspend_transaction = i_vp?1:0;
	return 1;
}

static int w_http_set_timeout(sip_msg_t* msg, char* tout, char*foo)
{
	_IVALUE (tout);
	if (i_tout < 0) {
		LM_ERR("timeout must be >= 0 (got %d)\n", i_tout);
		return -1;
	}
	ah_params.timeout = i_tout;
	return 1;
}

static int w_http_append_header(sip_msg_t* msg, char* hdr, char*foo)
{
	str shdr;

	if(fixup_get_svalue(msg, (gparam_t*)hdr, &shdr)!=0) {
		LM_ERR("unable to get header value\n");
		return -1;
	}

	header_list_add(&ah_params.headers, &shdr);

	return 1;
}

static int w_http_set_method(sip_msg_t* msg, char* meth, char*foo)
{
	str smeth;

	if(fixup_get_svalue(msg, (gparam_t*)meth, &smeth)!=0) {
		LM_ERR("unable to get method value\n");
		return -1;
	}

	query_params_set_method(&ah_params, &smeth);

	return 1;
}

static int w_http_set_tls_client_cert(sip_msg_t* msg, char* sc, char*foo)
{
	str _tls_client_cert;

	if(fixup_get_svalue(msg, (gparam_t*)sc, &_tls_client_cert)!=0) {
		LM_ERR("unable to get method value\n");
		return -1;
	}

	return set_query_param(&ah_params.tls_client_cert, _tls_client_cert);
}

static int w_http_set_tls_client_key(sip_msg_t* msg, char* sk, char*foo)
{
	str _tls_client_key;

	if(fixup_get_svalue(msg, (gparam_t*)sk, &_tls_client_key)!=0) {
		LM_ERR("unable to get method value\n");
		return -1;
	}

	return set_query_param(&ah_params.tls_client_key, _tls_client_key);
}

static int w_http_set_tls_ca_path(sip_msg_t* msg, char* cp, char*foo)
{
	str _tls_ca_path;

	if(fixup_get_svalue(msg, (gparam_t*)cp, &_tls_ca_path)!=0) {
		LM_ERR("unable to get method value\n");
		return -1;
	}

	return set_query_param(&ah_params.tls_ca_path, _tls_ca_path);
}

/*
 * Helper to copy input string parameter into a query parameter
 */
static int set_query_param(str* param, str input)
{
	if (param->s) {
		shm_free(param->s);
		param->s = NULL;
		param->len = 0;
	}

	if (input.s && input.len > 0) {
		if (shm_str_dup(param, &input) < 0) {
			LM_ERR("Error allocating parameter\n");
			return -1;
		}
	}

	return 1;
}

/**
 *
 */
static int fixup_http_async_get(void** param, int param_no)
{
	if (param_no == 1) {
		return fixup_spve_null(param, 1);
	}
	if (param_no == 2) {
		return fixup_var_str_12(param, param_no);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/**
 *
 */
static int fixup_http_async_post(void** param, int param_no)
{
	if (param_no == 1 || param_no == 2) {
		return fixup_spve_null(param, 1);
	}
	if (param_no == 3) {
		return fixup_var_str_12(param, param_no);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/* module PVs */

#define AH_WRAP_GET_PV(AH_F, PV_F) static int AH_F(struct sip_msg *msg, pv_param_t *param, pv_value_t *res) \
	{ \
		if (ah_reply) { \
			if (ah_error.s) { \
				LM_WARN("an async variable was read after http error, use $http_ok to check the request's status\n"); \
				return pv_get_null(msg, param, res); \
			} else { \
				return pv_api.PV_F(ah_reply, param, res); \
			} \
		} else { \
			LM_ERR("the async variables can only be read from an async http worker\n"); \
			return pv_get_null(msg, param, res); \
		} \
	}

AH_WRAP_GET_PV(ah_get_reason,      get_reason)
AH_WRAP_GET_PV(ah_get_hdr,         get_hdr)
AH_WRAP_GET_PV(ah_get_status,      get_status)
AH_WRAP_GET_PV(ah_get_msg_body,    get_msg_body)
AH_WRAP_GET_PV(ah_get_body_size,   get_body_size)
AH_WRAP_GET_PV(ah_get_msg_buf,     get_msg_buf)
AH_WRAP_GET_PV(ah_get_msg_len,     get_msg_len)

static int w_pv_parse_hdr_name(pv_spec_p sp, str *in) {
	return pv_api.parse_hdr_name(sp, in);
}

static int ah_get_ok(struct sip_msg *msg, pv_param_t *param, pv_value_t *res) {
	if (ah_reply) {
		if (ah_error.s) {
			return pv_get_intstrval(msg, param, res, 0, &pv_str_0);
		} else {
			return pv_get_intstrval(msg, param, res, 1, &pv_str_1);
		}
	} else {
		LM_ERR("the async variables can only be read from an async http worker\n");
		return pv_get_null(msg, param, res);
	}
}

static int ah_get_err(struct sip_msg *msg, pv_param_t *param, pv_value_t *res) {
	if (ah_reply) {
		if (ah_error.s) {
			return pv_get_strval(msg, param, res, &ah_error);
		} else {
			return pv_get_null(msg, param, res);
		}
	} else {
		LM_ERR("the async variables can only be read from an async http worker\n");
		return pv_get_null(msg, param, res);
	}
}

static int ah_parse_req_name(pv_spec_p sp, str *in) {
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 3:
			if(strncmp(in->s, "all", 3)==0)
				sp->pvp.pvn.u.isname.name.n = E_HRN_ALL;
			else if(strncmp(in->s, "hdr", 3)==0)
				sp->pvp.pvn.u.isname.name.n = E_HRN_HDR;
			else goto error;
			break;
		case 6:
			if(strncmp(in->s, "method", 6)==0)
				sp->pvp.pvn.u.isname.name.n = E_HRN_METHOD;
			else goto error;
			break;
		case 7:
			if(strncmp(in->s, "timeout", 7)==0)
				sp->pvp.pvn.u.isname.name.n = E_HRN_TIMEOUT;
			else if(strncmp(in->s, "suspend", 7)==0)
				sp->pvp.pvn.u.isname.name.n = E_HRN_SUSPEND;
			else goto error;
			break;
		case 11:
			if(strncmp(in->s, "tls_ca_path", 11)==0)
				sp->pvp.pvn.u.isname.name.n = E_HRN_TLS_CA_PATH;
			else goto error;
			break;
		case 14:
			if(strncmp(in->s, "tls_client_key", 14)==0)
				sp->pvp.pvn.u.isname.name.n = E_HRN_TLS_CLIENT_KEY;
			else goto error;
			break;
		case 15:
			if(strncmp(in->s, "tls_client_cert", 15)==0)
				sp->pvp.pvn.u.isname.name.n = E_HRN_TLS_CLIENT_CERT;
			else goto error;
			break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown http_req name %.*s\n", in->len, in->s);
	return -1;
}

static int ah_set_req(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	pv_value_t *tval;

	if(param==NULL || tmb.t_request==NULL)
		return -1;

	tval = val;
	if((tval!=NULL) && (tval->flags&PV_VAL_NULL)) {
		tval = NULL;
	}

	switch((enum http_req_name_t) param->pvn.u.isname.name.n) {
	case E_HRN_ALL:
		if (tval == NULL)
			set_query_params(&ah_params);
		break;
	case E_HRN_HDR:
		if (tval) {
			if (!(tval->flags & PV_VAL_STR)) {
				LM_ERR("invalid value type for $http_req(hdr)\n");
				return -1;
			}
			header_list_add(&ah_params.headers, &tval->rs);
		}
		break;
	case E_HRN_METHOD:
		if (tval) {
			if (!(tval->flags & PV_VAL_STR)) {
				LM_ERR("invalid value type for $http_req(method)\n");
				return -1;
			}
			query_params_set_method(&ah_params, &tval->rs);
		} else {
			ah_params.method = AH_METH_DEFAULT;
		}
		break;
	case E_HRN_TIMEOUT:
		if (tval) {
			if (!(tval->flags & PV_VAL_INT)) {
				LM_ERR("invalid value type for $http_req(timeout)\n");
				return -1;
			}
			ah_params.timeout = tval->ri;
		} else {
			ah_params.timeout = http_timeout;
		}
		break;
	case E_HRN_TLS_CA_PATH:
		if (tval) {
			if (!(tval->flags & PV_VAL_STR)) {
				LM_ERR("invalid value type for $http_req(tls_ca_path)\n");
				return -1;
			}
			set_query_param(&ah_params.tls_ca_path, tval->rs);
		} else {
			set_query_param(&ah_params.tls_ca_path, tls_ca_path);
		}
		break;
	case E_HRN_TLS_CLIENT_KEY:
		if (tval) {
			if (!(tval->flags & PV_VAL_STR)) {
				LM_ERR("invalid value type for $http_req(tls_client_key)\n");
				return -1;
			}
			set_query_param(&ah_params.tls_client_key, tval->rs);
		} else {
			set_query_param(&ah_params.tls_client_key, tls_client_key);
		}
		break;
	case E_HRN_TLS_CLIENT_CERT:
		if (tval) {
			if (!(tval->flags & PV_VAL_STR)) {
				LM_ERR("invalid value type for $http_req(tls_client_cert)\n");
				return -1;
			}
			set_query_param(&ah_params.tls_client_cert, tval->rs);
		} else {
			set_query_param(&ah_params.tls_client_cert, tls_client_cert);
		}
		break;
	case E_HRN_SUSPEND:
		if (tval) {
			if (!(tval->flags & PV_VAL_INT)) {
				LM_ERR("invalid value type for $http_req(suspend)\n");
				return -1;
			}
			ah_params.suspend_transaction = tval->ri?1:0;
		} else {
			ah_params.suspend_transaction = 1;
		}
		break;
	}

	return 1;
}
