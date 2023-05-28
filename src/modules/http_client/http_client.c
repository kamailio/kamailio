/*
 * http_client Module
 * Copyright (C) 2015-2016 Edvina AB, Olle E. Johansson
 *
 * Based on part of the utils module and part
 * of the json-rpc-c module
 *
 * Copyright (C) 2008 Juha Heinanen
 * Copyright (C) 2009 1&1 Internet AG
 * Copyright (C) 2013 Carsten Bock, ng-voice GmbH
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 * \brief  Kamailio http_client :: The module interface file
 * \ingroup http_client
 */

/*! \defgroup http_client Kamailio :: Module interface to Curl
 *
 * http://curl.haxx.se
 * A generic library for many protocols
 *
 *  http_connect(connection, url, $avp)
 *  http_connect(connection, url, content-type, data, $avp)
 *
 * 	$var(res) = http_connect("anders", "/postlåda", "application/json", "{ ok, {200, ok}}", "$avp(gurka)");
 * 	$var(res) = http_connect("anders", "/postlåda", "application/json", "$var(tomat)", "$avp(gurka)");
 *
 */


#include <curl/curl.h>

#include "../../core/mod_fix.h"
#include "../../core/sr_module.h"
#include "../../core/ut.h"
#include "../../core/resolve.h"
#include "../../core/locking.h"
#include "../../core/script_cb.h"
#include "../../core/mem/shm_mem.h"
#include "../../lib/srdb1/db.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/config.h"
#include "../../core/lvalue.h"
#include "../../core/pt.h" /* Process table */
#include "../../core/kemi.h"

#include "functions.h"
#include "curlcon.h"
#include "curlrpc.h"
#include "curl_api.h"

MODULE_VERSION

#define CURL_USER_AGENT NAME " (" VERSION " (" ARCH "/" OS_QUOTED "))"
#define CURL_USER_AGENT_LEN (sizeof(CURL_USER_AGENT) - 1)

/* Module parameter variables */
unsigned int default_connection_timeout = 4;
char *default_tls_cacert =
		NULL; /*!< File name: Default CA cert to use for curl TLS connection */
str default_tls_clientcert =
		STR_NULL; /*!< File name: Default client certificate to use for curl TLS connection */
str default_tls_clientkey =
		STR_NULL; /*!< File name: Key in PEM format that belongs to client cert */
str default_cipher_suite_list = STR_NULL; /*!< List of allowed cipher suites */
unsigned int default_tls_version = 0;	  /*!< 0 = Use libcurl default */
unsigned int default_tls_verify_peer =
		1; /*!< 0 = Do not verify TLS server cert. 1 = Verify TLS cert (default) */
unsigned int default_tls_verify_host =
		2; /*!< 0 = Do not verify TLS server CN/SAN  2 = Verify TLS server CN/SAN (default) */
str default_http_proxy = STR_NULL;		  /*!< Default HTTP proxy to use */
unsigned int default_http_proxy_port = 0; /*!< Default HTTP proxy port to use */
unsigned int default_http_follow_redirect =
		0; /*!< Follow HTTP redirects CURLOPT_FOLLOWLOCATION */
unsigned int default_keep_connections =
		0; /*!< Keep http connections open for reuse */
str default_useragent = {CURL_USER_AGENT,
		CURL_USER_AGENT_LEN}; /*!< Default CURL useragent. Default "Kamailio Curl " */
unsigned int default_maxdatasize = 0; /*!< Default download size. 0=disabled */
unsigned int default_authmethod =
		CURLAUTH_BASIC
		| CURLAUTH_DIGEST; /*!< authentication method - Basic, Digest or both */

char *default_netinterface = 0; /*!< local network interface */

/*!< Default http query result mode
 * - 0: return full result
 * - 1: return first line only */
unsigned int default_query_result = 1;
/*!< Default download size for result of query function. 0=disabled (no limit) */
unsigned int default_query_maxdatasize = 0;

str http_client_config_file = STR_NULL;

static curl_version_info_data *curl_info;

/* Module management function prototypes */
static int mod_init(void);
static int child_init(int);
static void destroy(void);

/* Fixup functions to be defined later */
static int fixup_http_query_get(void **param, int param_no);
static int fixup_free_http_query_get(void **param, int param_no);
static int fixup_http_query_post(void **param, int param_no);
static int fixup_free_http_query_post(void **param, int param_no);
static int fixup_http_query_post_hdr(void **param, int param_no);
static int fixup_free_http_query_post_hdr(void **param, int param_no);

static int fixup_curl_connect(void **param, int param_no);
static int fixup_free_curl_connect(void **param, int param_no);
static int fixup_curl_connect_post(void **param, int param_no);
static int fixup_curl_connect_post_raw(void **param, int param_no);
static int fixup_free_curl_connect_post(void **param, int param_no);
static int fixup_free_curl_connect_post_raw(void **param, int param_no);
static int w_curl_connect_post(struct sip_msg *_m, char *_con, char *_url,
		char *_result, char *_ctype, char *_data);
static int w_curl_connect_post_raw(struct sip_msg *_m, char *_con, char *_url,
		char *_result, char *_ctype, char *_data);

static int fixup_curl_get_redirect(void **param, int param_no);
static int fixup_free_curl_get_redirect(void **param, int param_no);
static int w_curl_get_redirect(struct sip_msg *_m, char *_con, char *_result);

/* Wrappers for http_query to be defined later */
static int w_http_query(struct sip_msg *_m, char *_url, char *_result);
static int w_http_query_post(
		struct sip_msg *_m, char *_url, char *_post, char *_result);
static int w_http_query_post_hdr(struct sip_msg *_m, char *_url, char *_post,
		char *_hdrs, char *_result);
static int w_http_query_get_hdr(struct sip_msg *_m, char *_url, char *_body,
		char *_hdrs, char *_result);
static int w_curl_connect(
		struct sip_msg *_m, char *_con, char *_url, char *_result);

/* forward function */
static int curl_con_param(modparam_t type, void *val);
static int pv_parse_curlerror(pv_spec_p sp, str *in);
static int pv_get_curlerror(
		struct sip_msg *msg, pv_param_t *param, pv_value_t *res);

/* clang-format off */
/* Exported functions */
static cmd_export_t cmds[] = {
	{"http_client_query", (cmd_function)w_http_query, 2, fixup_http_query_get,
	 	fixup_free_http_query_get,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"http_client_query", (cmd_function)w_http_query_post, 3, fixup_http_query_post,
		fixup_free_http_query_post,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"http_client_query", (cmd_function)w_http_query_post_hdr, 4, fixup_http_query_post_hdr,
		fixup_free_http_query_post_hdr,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"http_client_get", (cmd_function)w_http_query_get_hdr, 4, fixup_http_query_post_hdr,
		fixup_free_http_query_post_hdr,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"http_connect", (cmd_function)w_curl_connect, 3, fixup_curl_connect,
		fixup_free_curl_connect,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"http_connect", (cmd_function)w_curl_connect_post, 5, fixup_curl_connect_post,
		fixup_free_curl_connect_post,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"http_connect_raw", (cmd_function)w_curl_connect_post_raw, 5, fixup_curl_connect_post_raw,
		fixup_free_curl_connect_post_raw,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"http_get_redirect", (cmd_function)w_curl_get_redirect, 2, fixup_curl_get_redirect,
		fixup_free_curl_get_redirect,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"bind_http_client",  (cmd_function)bind_httpc_api,  0, 0, 0, 0},
	{0,0,0,0,0,0}
};


/* Exported parameters */
static param_export_t params[] = {
	{"connection_timeout", PARAM_INT, &default_connection_timeout},
	{"cacert", PARAM_STRING,  &default_tls_cacert },
	{"client_cert", PARAM_STR, &default_tls_clientcert },
	{"client_key", PARAM_STR, &default_tls_clientkey },
	{"cipher_suites", PARAM_STR, &default_cipher_suite_list },
	{"tlsversion", PARAM_INT, &default_tls_version },
	{"verify_peer", PARAM_INT, &default_tls_verify_peer },
	{"verify_host", PARAM_INT, &default_tls_verify_host },
	{"httpproxyport", PARAM_INT, &default_http_proxy_port },
	{"httpproxy", PARAM_STR, &default_http_proxy},
	{"httpredirect", PARAM_INT, &default_http_follow_redirect },
	{"useragent", PARAM_STR,  &default_useragent },
	{"maxdatasize", PARAM_INT,  &default_maxdatasize },
	{"config_file", PARAM_STR,  &http_client_config_file },
	{"httpcon",  PARAM_STRING|USE_FUNC_PARAM, (void*)curl_con_param},
	{"authmethod", PARAM_INT, &default_authmethod },
	{"keep_connections", PARAM_INT, &default_keep_connections },
	{"query_result", PARAM_INT, &default_query_result },
	{"query_maxdatasize", PARAM_INT, &default_query_maxdatasize },
	{"netinterface", PARAM_STRING,  &default_netinterface },
	{0, 0, 0}
};


/*!
 * \brief Exported Pseudo variables
 */
static pv_export_t mod_pvs[] = {
	{{"curlerror", (sizeof("curlerror")-1)}, /* Curl error codes */
		PVT_OTHER, pv_get_curlerror, 0, pv_parse_curlerror, 0, 0, 0},

	{{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

/* Module interface */
struct module_exports exports = {
	"http_client",		/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,					/* RPC method exports */
	mod_pvs,					/* exported pseudo-variables */
	0,					/* response handling function */
	mod_init,			/* module initialization function */
	child_init,			/* per-child init function */
	destroy				/* module destroy function */
};
/* clang-format on */

counter_handle_t connections; /* Number of connection definitions */
counter_handle_t connok;	  /* Successful Connection attempts */
counter_handle_t connfail;	  /* Failed Connection attempts */


static int init_shmlock(void)
{
	return 0;
}


static void destroy_shmlock(void)
{
	;
}

/* Init counters */
static void curl_counter_init()
{
	counter_register(&connections, "httpclient", "connections", 0, 0, 0,
			"Counter of connection definitions (httpcon)", 0);
	counter_register(&connok, "httpclient", "connok", 0, 0, 0,
			"Counter of successful connections (200 OK)", 0);
	counter_register(&connfail, "httpclient", "connfail", 0, 0, 0,
			"Counter of failed connections (not 200 OK)", 0);
}


/* Module initialization function */
static int mod_init(void)
{

	LM_DBG("init curl module\n");

	/* Initialize curl */
	if(curl_global_init(CURL_GLOBAL_ALL)) {
		LM_ERR("curl_global_init failed\n");
		return -1;
	}
	curl_info = curl_version_info(CURLVERSION_NOW);

	if(curl_init_rpc() < 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(init_shmlock() != 0) {
		LM_CRIT("cannot initialize shared memory lock.\n");
		return -1;
	}

	curl_counter_init();
	counter_add(connections, curl_connection_count());

	if(default_tls_version >= CURL_SSLVERSION_LAST) {
		LM_WARN("tlsversion %d unsupported value. Using libcurl default\n",
				default_tls_version);
		default_tls_version = CURL_SSLVERSION_DEFAULT;
	}
	if(http_client_config_file.s != NULL) {
		if(http_client_load_config(&http_client_config_file) < 0) {
			LM_ERR("Failed to load http_client connections from [%.*s]\n",
					http_client_config_file.len, http_client_config_file.s);
			return -1;
		}
	}

	if(default_connection_timeout == 0) {
		LM_ERR("CURL connection timeout set to zero. Using default 4 secs\n");
		default_connection_timeout = 4;
	}
	if(default_http_proxy_port == 0) {
		LM_INFO("HTTP proxy port set to 0. Disabling HTTP proxy\n");
	}

	LM_DBG("**** init http_client module done. Curl version: %s SSL %s\n",
			curl_info->version, curl_info->ssl_version);
	LM_DBG("**** init http_client: Number of connection objects: %d \n",
			curl_connection_count());
	LM_DBG("**** init http_client: User Agent: %.*s \n", default_useragent.len,
			default_useragent.s);
	LM_DBG("**** init http_client: HTTPredirect: %d \n",
			default_http_follow_redirect);
	LM_DBG("**** init http_client: Client Cert: %.*s Key %.*s\n",
			default_tls_clientcert.len, default_tls_clientcert.s,
			default_tls_clientkey.len, default_tls_clientkey.s);
	LM_DBG("**** init http_client: CA Cert: %s \n", default_tls_cacert);
	LM_DBG("**** init http_client: Cipher Suites: %.*s \n",
			default_cipher_suite_list.len, default_cipher_suite_list.s);
	LM_DBG("**** init http_client: SSL Version: %d \n", default_tls_version);
	LM_DBG("**** init http_client: verifypeer: %d verifyhost: %d\n",
			default_tls_verify_peer, default_tls_verify_host);
	LM_DBG("**** init http_client: HTTP Proxy: %.*s Port %d\n",
			default_http_proxy.len, default_http_proxy.s,
			default_http_proxy_port);
	LM_DBG("**** init http_client: Auth method: %d \n", default_authmethod);
	LM_DBG("**** init http_client: Keep Connections open: %d \n",
			default_keep_connections);

	LM_DBG("**** Extra: Curl supports %s %s %s \n",
			(curl_info->features & CURL_VERSION_SSL ? "TLS" : ""),
			(curl_info->features & CURL_VERSION_IPV6 ? "IPv6" : ""),
			(curl_info->features & CURL_VERSION_IDN ? "IDN" : ""));
	return 0;
}

/*! Returns TRUE if curl supports TLS */
int curl_support_tls()
{
	return (curl_info->features & CURL_VERSION_SSL);
}

/*! Returns TRUE if curl supports IPv6 */
int curl_support_ipv6()
{
	return (curl_info->features & CURL_VERSION_IPV6);
}


/* Child initialization function */
static int child_init(int rank)
{
	int i = my_pid();

	if(rank == PROC_INIT || rank == PROC_MAIN || rank == PROC_TCP_MAIN) {
		return 0; /* do nothing for the main process */
	}
	LM_DBG("*** http_client module initializing process %d\n", i);

	return 0;
}


static void destroy(void)
{
	/* Cleanup curl */
	curl_global_cleanup();
	destroy_shmlock();
}


/**
 * parse httpcon module parameter
 */
int curl_con_param(modparam_t type, void *val)
{
	if(val == NULL) {
		goto error;
	}

	LM_DBG("**** HTTP_CLIENT got modparam httpcon \n");
	return curl_parse_param((char *)val);
error:
	return -1;
}

/* Fixup functions */

/*
 * Fix http_query params: url (string that may contain pvars) and
 * result (writable pvar).
 */
static int fixup_http_query_get(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_spve_null(param, 1);
	}

	if(param_no == 2) {
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("http_query: failed to fixup result pvar\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("http_query: result pvar is not writeble\n");
			return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Free http_query params.
 */
static int fixup_free_http_query_get(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_free_spve_null(param, 1);
	}

	if(param_no == 2) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("http_query: invalid parameter number <%d>\n", param_no);
	return -1;
}


/*
 * Fix curl_connect params: connection(string/pvar) url (string that may contain pvars) and
 * result (writable pvar).
 */
static int fixup_curl_connect(void **param, int param_no)
{

	if(param_no == 1) {
		/* We want char * strings */
		return 0;
	}
	/* URL and data may contain pvar */
	if(param_no == 2) {
		return fixup_spve_null(param, 1);
	}
	if(param_no == 3) {
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writeble\n");
			return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Fix curl_connect params when posting (5 parameters): 
 *	connection (string/pvar), url (string with pvars), content-type, 
 *      data (string/pvar, pvar)
 */
static int fixup_curl_connect_post(void **param, int param_no)
{

	if(param_no == 1 || param_no == 3) {
		/* We want char * strings */
		return 0;
	}
	/* URL and data may contain pvar */
	if(param_no == 2 || param_no == 4) {
		return fixup_spve_null(param, 1);
	}
	if(param_no == 5) {
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pseudo variable\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writeable\n");
			return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Fix curl_connect params when posting (5 parameters): 
 *	connection (string/pvar), url (string with pvars), content-type, 
 *      data (string(with no pvar parsing), pvar)
 */
static int fixup_curl_connect_post_raw(void **param, int param_no)
{

	if(param_no == 1 || param_no == 3 || param_no == 4) {
		/* We want char * strings */
		return 0;
	}
	/* URL and data may contain pvar */
	if(param_no == 2) {
		return fixup_spve_null(param, 1);
	}
	if(param_no == 5) {
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pseudo variable\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writeable\n");
			return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Free curl_connect params.
 */
static int fixup_free_curl_connect_post(void **param, int param_no)
{
	if(param_no == 1 || param_no == 3) {
		/* Char strings don't need freeing */
		return 0;
	}
	if(param_no == 2 || param_no == 4) {
		return fixup_free_spve_null(param, 1);
	}

	if(param_no == 5) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Free curl_connect params.
 */
static int fixup_free_curl_connect_post_raw(void **param, int param_no)
{
	if(param_no == 1 || param_no == 3 || param_no == 4) {
		/* Char strings don't need freeing */
		return 0;
	}
	if(param_no == 2) {
		return fixup_free_spve_null(param, 1);
	}

	if(param_no == 5) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Free curl_connect params.
 */
static int fixup_free_curl_connect(void **param, int param_no)
{
	if(param_no == 1) {
		/* Char strings don't need freeing */
		return 0;
	}
	if(param_no == 2) {
		return fixup_free_spve_null(param, 1);
	}

	if(param_no == 3) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Wrapper for Curl_connect (GET)
 */
static int ki_curl_connect_helper(
		sip_msg_t *_m, str *con, str *url, pv_spec_t *dst)
{
	str result = {NULL, 0};
	pv_value_t val;
	int ret = 0;

	ret = curl_con_query_url(_m, con, url, &result, NULL, NULL);

	val.rs = result;
	val.flags = PV_VAL_STR;
	if(dst->setf) {
		dst->setf(_m, &dst->pvp, (int)EQ_T, &val);
	} else {
		LM_WARN("target pv is not writable\n");
	}

	if(result.s != NULL)
		pkg_free(result.s);

	return (ret == 0) ? -1 : ret;
}

/*
 * Kemi wrapper for Curl_connect (GET)
 */
static int ki_curl_connect(sip_msg_t *_m, str *con, str *url, str *dpv)
{
	pv_spec_t *dst;

	dst = pv_cache_get(dpv);
	if(dst == NULL) {
		LM_ERR("failed to get pv spec for: %.*s\n", dpv->len, dpv->s);
		return -1;
	}
	if(dst->setf == NULL) {
		LM_ERR("target pv is not writable: %.*s\n", dpv->len, dpv->s);
		return -1;
	}

	return ki_curl_connect_helper(_m, con, url, dst);
}

/*
 * Cfg wrapper for Curl_connect (GET)
 */
static int w_curl_connect(sip_msg_t *_m, char *_con, char *_url, char *_result)
{
	str con = {NULL, 0};
	str url = {NULL, 0};
	pv_spec_t *dst;

	if(_con == NULL || _url == NULL || _result == NULL) {
		LM_ERR("http_connect: Invalid parameter\n");
		return -1;
	}
	con.s = _con;
	con.len = strlen(con.s);
	if(get_str_fparam(&url, _m, (gparam_p)_url) != 0) {
		LM_ERR("http_connect: url has no value\n");
		return -1;
	}

	LM_DBG("**** HTTP_CONNECT Connection %s URL %s Result var %s\n", _con, _url,
			_result);
	dst = (pv_spec_t *)_result;

	return ki_curl_connect_helper(_m, &con, &url, dst);
}

/*
 * Wrapper for Curl_connect (POST)
 */
static int ki_curl_connect_post_helper(sip_msg_t *_m, str *con, str *url,
		str *ctype, str *data, pv_spec_t *dst)
{
	str result = {NULL, 0};
	pv_value_t val;
	int ret = 0;

	ret = curl_con_query_url(_m, con, url, &result, ctype->s, data);

	val.rs = result;
	val.flags = PV_VAL_STR;
	if(dst->setf) {
		dst->setf(_m, &dst->pvp, (int)EQ_T, &val);
	} else {
		LM_WARN("target pv is not writable\n");
	}

	if(result.s != NULL)
		pkg_free(result.s);

	return (ret == 0) ? -1 : ret;
}

/*
 * Kemi wrapper for Curl_connect (POST)
 */
static int ki_curl_connect_post(
		sip_msg_t *_m, str *con, str *url, str *ctype, str *data, str *dpv)
{
	pv_spec_t *dst;

	dst = pv_cache_get(dpv);
	if(dst == NULL) {
		LM_ERR("failed to get pv spec for: %.*s\n", dpv->len, dpv->s);
		return -1;
	}
	if(dst->setf == NULL) {
		LM_ERR("target pv is not writable: %.*s\n", dpv->len, dpv->s);
		return -1;
	}

	return ki_curl_connect_post_helper(_m, con, url, ctype, data, dst);
}

/*
 * Wrapper for Curl_connect (POST) raw data (no pvar parsing inside the data)
 */
static int w_curl_connect_post_raw(struct sip_msg *_m, char *_con, char *_url,
		char *_ctype, char *_data, char *_result)
{
	str con = {NULL, 0};
	str url = {NULL, 0};
	str ctype = {NULL, 0};
	str data = {NULL, 0};
	pv_spec_t *dst;

	if(_con == NULL || _url == NULL || _ctype == NULL || _data == NULL
			|| _result == NULL) {
		LM_ERR("http_connect: Invalid parameters\n");
		return -1;
	}
	con.s = _con;
	con.len = strlen(con.s);

	if(get_str_fparam(&url, _m, (gparam_p)_url) != 0) {
		LM_ERR("http_connect: URL has no value\n");
		return -1;
	}

	ctype.s = _ctype;
	ctype.len = strlen(ctype.s);

	data.s = _data;
	data.len = strlen(data.s);

	LM_DBG("**** HTTP_CONNECT: Connection %s URL %s Result var %s\n", _con,
			_url, _result);
	dst = (pv_spec_t *)_result;

	return ki_curl_connect_post_helper(_m, &con, &url, &ctype, &data, dst);
}

/*
 * Wrapper for Curl_connect (POST)
 */
static int w_curl_connect_post(struct sip_msg *_m, char *_con, char *_url,
		char *_ctype, char *_data, char *_result)
{
	str con = {NULL, 0};
	str url = {NULL, 0};
	str ctype = {NULL, 0};
	str data = {NULL, 0};
	pv_spec_t *dst;

	if(_con == NULL || _url == NULL || _ctype == NULL || _data == NULL
			|| _result == NULL) {
		LM_ERR("http_connect: Invalid parameters\n");
		return -1;
	}
	con.s = _con;
	con.len = strlen(con.s);

	if(get_str_fparam(&url, _m, (gparam_p)_url) != 0) {
		LM_ERR("http_connect: URL has no value\n");
		return -1;
	}

	ctype.s = _ctype;
	ctype.len = strlen(ctype.s);

	if(get_str_fparam(&data, _m, (gparam_p)_data) != 0) {
		LM_ERR("http_connect: No post data given\n");
		return -1;
	}

	LM_DBG("**** HTTP_CONNECT: Connection %s URL %s Result var %s\n", _con,
			_url, _result);
	dst = (pv_spec_t *)_result;

	return ki_curl_connect_post_helper(_m, &con, &url, &ctype, &data, dst);
}

/*!
 * Fix http_query params: url (string that may contain pvars) and
 * result (writable pvar).
 */
static int fixup_http_query_post(void **param, int param_no)
{
	if((param_no == 1) || (param_no == 2)) {
		return fixup_spve_null(param, 1);
	}

	if(param_no == 3) {
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writeble\n");
			return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*!
 * Free http_query params.
 */
static int fixup_free_http_query_post(void **param, int param_no)
{
	if((param_no == 1) || (param_no == 2)) {
		return fixup_free_spve_null(param, 1);
	}

	if(param_no == 3) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Fix http_query params: url (string that may contain pvars) and
 * result (writable pvar).
 */
static int fixup_http_query_post_hdr(void **param, int param_no)
{
	if((param_no >= 1) && (param_no <= 3)) {
		return fixup_spve_null(param, 1);
	}

	if(param_no == 4) {
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writeble\n");
			return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Free http_query params.
 */
static int fixup_free_http_query_post_hdr(void **param, int param_no)
{
	if((param_no >= 1) && (param_no <= 3)) {
		return fixup_free_spve_null(param, 1);
	}

	if(param_no == 4) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*!
 * helper for HTTP-Query function
 */
static int ki_http_query_helper(
		sip_msg_t *_m, str *url, str *post, str *hdrs, pv_spec_t *dst)
{
	int ret = 0;
	str result = {NULL, 0};
	pv_value_t val;

	if(url == NULL || url->s == NULL) {
		LM_ERR("invalid url parameter\n");
		return -1;
	}
	ret = http_client_query(_m, url->s, &result,
			(post && post->s && post->len > 0) ? post->s : NULL,
			(hdrs && hdrs->s && hdrs->len > 0) ? hdrs->s : NULL);

	val.rs = result;
	val.flags = PV_VAL_STR;
	if(dst->setf) {
		dst->setf(_m, &dst->pvp, (int)EQ_T, &val);
	} else {
		LM_WARN("target pv is not writable\n");
	}

	if(result.s != NULL)
		pkg_free(result.s);

	return (ret == 0) ? -1 : ret;
}

static int ki_http_query_post_hdrs(
		sip_msg_t *_m, str *url, str *post, str *hdrs, str *dpv)
{
	pv_spec_t *dst;

	dst = pv_cache_get(dpv);
	if(dst == NULL) {
		LM_ERR("failed to get pv spec for: %.*s\n", dpv->len, dpv->s);
		return -1;
	}
	if(dst->setf == NULL) {
		LM_ERR("target pv is not writable: %.*s\n", dpv->len, dpv->s);
		return -1;
	}

	return ki_http_query_helper(_m, url, post, hdrs, dst);
}

static int ki_http_query_post(sip_msg_t *_m, str *url, str *post, str *dpv)
{
	return ki_http_query_post_hdrs(_m, url, post, NULL, dpv);
}

static int ki_http_query(sip_msg_t *_m, str *url, str *dpv)
{
	return ki_http_query_post_hdrs(_m, url, NULL, NULL, dpv);
}

/*!
 * Wrapper for HTTP-Query function for cfg script
 */
static int w_http_query_script(
		sip_msg_t *_m, char *_url, char *_post, char *_hdrs, char *_result)
{
	str url = {NULL, 0};
	str post = {NULL, 0};
	str hdrs = {NULL, 0};
	pv_spec_t *dst;

	if(get_str_fparam(&url, _m, (gparam_p)_url) != 0 || url.len <= 0) {
		LM_ERR("URL has no value\n");
		return -1;
	}
	if(_post && get_str_fparam(&post, _m, (gparam_p)_post) != 0) {
		LM_ERR("DATA has no value\n");
		return -1;
	} else {
		if(post.len == 0) {
			post.s = NULL;
		}
	}
	if(_hdrs && get_str_fparam(&hdrs, _m, (gparam_p)_hdrs) != 0) {
		LM_ERR("HDRS has no value\n");
		return -1;
	} else {
		if(hdrs.len == 0) {
			hdrs.s = NULL;
		}
	}
	dst = (pv_spec_t *)_result;

	return ki_http_query_helper(_m, &url, &post, &hdrs, dst);
}

/*!
 * Wrapper for HTTP-Query (GET)
 */
static int w_http_query(struct sip_msg *_m, char *_url, char *_result)
{
	return w_http_query_script(_m, _url, NULL, NULL, _result);
}

/*!
 * Wrapper for HTTP-Query (POST-Variant)
 */
static int w_http_query_post(
		struct sip_msg *_m, char *_url, char *_post, char *_result)
{
	return w_http_query_script(_m, _url, _post, NULL, _result);
}

/*!
 * Wrapper for HTTP-Query (HDRS-Variant)
 */
static int w_http_query_post_hdr(
		struct sip_msg *_m, char *_url, char *_post, char *_hdrs, char *_result)
{
	return w_http_query_script(_m, _url, _post, _hdrs, _result);
}

/*!
 * helper for HTTP-Query function
 */
static int ki_http_request_helper(
		sip_msg_t *_m, str *met, str *url, str *body, str *hdrs, pv_spec_t *dst)
{
	int ret = 0;
	str result = {NULL, 0};
	pv_value_t val;

	if(url == NULL || url->s == NULL) {
		LM_ERR("invalid url parameter\n");
		return -1;
	}
	ret = http_client_request(_m, url->s, &result,
			(body && body->s && body->len > 0) ? body->s : NULL,
			(hdrs && hdrs->s && hdrs->len > 0) ? hdrs->s : NULL,
			(met && met->s && met->len > 0) ? met->s : NULL);

	val.rs = result;
	val.flags = PV_VAL_STR;
	if(dst->setf) {
		dst->setf(_m, &dst->pvp, (int)EQ_T, &val);
	} else {
		LM_WARN("target pv is not writable\n");
	}

	if(result.s != NULL)
		pkg_free(result.s);

	return (ret == 0) ? -1 : ret;
}

/*!
 * KEMI function to perform GET with headers and body
 */
static int ki_http_get_hdrs(
		sip_msg_t *_m, str *url, str *body, str *hdrs, str *dpv)
{
	str met = str_init("GET");
	pv_spec_t *dst;

	dst = pv_cache_get(dpv);
	if(dst == NULL) {
		LM_ERR("failed to get pv spec for: %.*s\n", dpv->len, dpv->s);
		return -1;
	}
	if(dst->setf == NULL) {
		LM_ERR("target pv is not writable: %.*s\n", dpv->len, dpv->s);
		return -1;
	}

	return ki_http_request_helper(_m, &met, url, body, hdrs, dst);
}

/*!
 * Wrapper for HTTP-Query function for cfg script
 */
static int w_http_get_script(
		sip_msg_t *_m, char *_url, char *_body, char *_hdrs, char *_result)
{
	str met = str_init("GET");
	str url = {NULL, 0};
	str body = {NULL, 0};
	str hdrs = {NULL, 0};
	pv_spec_t *dst;

	if(get_str_fparam(&url, _m, (gparam_p)_url) != 0 || url.len <= 0) {
		LM_ERR("URL has no value\n");
		return -1;
	}
	if(_body && get_str_fparam(&body, _m, (gparam_p)_body) != 0) {
		LM_ERR("DATA has no value\n");
		return -1;
	} else {
		if(body.len == 0) {
			body.s = NULL;
		}
	}
	if(_hdrs && get_str_fparam(&hdrs, _m, (gparam_p)_hdrs) != 0) {
		LM_ERR("HDRS has no value\n");
		return -1;
	} else {
		if(hdrs.len == 0) {
			hdrs.s = NULL;
		}
	}
	dst = (pv_spec_t *)_result;

	return ki_http_request_helper(_m, &met, &url, &body, &hdrs, dst);
}

/*!
 * Wrapper for HTTP-GET (HDRS-Variant)
 */
static int w_http_query_get_hdr(
		sip_msg_t *_m, char *_url, char *_body, char *_hdrs, char *_result)
{
	return w_http_get_script(_m, _url, _body, _hdrs, _result);
}

/*!
 * Parse arguments to  pv $curlerror
 */
static int pv_parse_curlerror(pv_spec_p sp, str *in)
{
	int cerr = 0;
	if(sp == NULL || in == NULL || in->len <= 0)
		return -1;

	cerr = atoi(in->s);
	LM_DBG(" =====> CURL ERROR %d \n", cerr);
	sp->pvp.pvn.u.isname.name.n = cerr;

	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;
}

/*
 * PV - return curl error explanation as string
 */
static int pv_get_curlerror(
		struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	str curlerr;
	char *err = NULL;

	if(param == NULL) {
		return -1;
	}

	/* cURL error codes does not collide with HTTP codes */
	if(param->pvn.u.isname.name.n < 0 || param->pvn.u.isname.name.n > 999) {
		err = "Bad CURL error code";
	}
	if(param->pvn.u.isname.name.n > 99) {
		err = "HTTP result code";
	}
	if(err == NULL) {
		err = (char *)curl_easy_strerror(param->pvn.u.isname.name.n);
	}
	curlerr.s = err;
	curlerr.len = strlen(err);

	return pv_get_strval(msg, param, res, &curlerr);
}


/*
 * Fix curl_get_redirect params: connection(string/pvar) url (string that may contain pvars) and
 * result (writable pvar).
 */
static int fixup_curl_get_redirect(void **param, int param_no)
{
	if(param_no == 1) { /* Connection name */
		/* We want char * strings */
		return 0;
	}
	if(param_no == 2) { /* PVAR to store result in */
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pseudo variable\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pseudovariable is not writeable\n");
			return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Free curl_get_redirect params.
 */
static int fixup_free_curl_get_redirect(void **param, int param_no)
{
	if(param_no == 1) {
		/* Char strings don't need freeing */
		return 0;
	}
	if(param_no == 2) {
		return fixup_free_spve_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/*
 * Wrapper for Curl_redirect
 */
static int w_curl_get_redirect(struct sip_msg *_m, char *_con, char *_result)
{

	str con = {NULL, 0};
	str result = {NULL, 0};
	pv_spec_t *dst;
	pv_value_t val;
	int ret = 0;

	if(_con == NULL || _result == NULL) {
		LM_ERR("Invalid or missing parameter\n");
		return -1;
	}
	con.s = _con;
	con.len = strlen(con.s);

	LM_DBG("**** http_client get_redirect Connection %s Result var %s\n", _con,
			_result);

	ret = curl_get_redirect(_m, &con, &result);

	val.rs = result;
	val.flags = PV_VAL_STR;
	dst = (pv_spec_t *)_result;
	dst->setf(_m, &dst->pvp, (int)EQ_T, &val);

	if(result.s != NULL)
		pkg_free(result.s);

	return ret;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_http_client_exports[] = {
	{ str_init("http_client"), str_init("query"),
		SR_KEMIP_INT, ki_http_query,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("http_client"), str_init("query_post"),
		SR_KEMIP_INT, ki_http_query_post,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("http_client"), str_init("query_post_hdrs"),
		SR_KEMIP_INT, ki_http_query_post_hdrs,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("http_client"), str_init("get_hdrs"),
		SR_KEMIP_INT, ki_http_get_hdrs,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("http_client"), str_init("curl_connect"),
		SR_KEMIP_INT, ki_curl_connect,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("http_client"), str_init("curl_connect_post"),
		SR_KEMIP_INT, ki_curl_connect_post,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_http_client_exports);
	return 0;
}
