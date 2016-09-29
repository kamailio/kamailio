/*
 * http_client Module
 * Copyright (C) 2015 Edvina AB, Olle E. Johansson
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

#include "../../mod_fix.h"
#include "../../sr_module.h"
#include "../../ut.h"
#include "../../resolve.h"
#include "../../locking.h"
#include "../../script_cb.h"
#include "../../mem/shm_mem.h"
#include "../../lib/srdb1/db.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"
#include "../../config.h"
#include "../../lvalue.h"

#include "functions.h"
#include "curlcon.h"
#include "curlrpc.h"
#include "curl_api.h"

MODULE_VERSION

#define CURL_USER_AGENT  NAME  " (" VERSION " (" ARCH "/" OS_QUOTED "))"
#define CURL_USER_AGENT_LEN (sizeof(CURL_USER_AGENT)-1)

/* Module parameter variables */
unsigned int	default_connection_timeout = 4;
char		*default_tls_cacert = NULL;		/*!< File name: Default CA cert to use for curl TLS connection */
str		default_tls_clientcert = STR_NULL;		/*!< File name: Default client certificate to use for curl TLS connection */
str		default_tls_clientkey = STR_NULL;		/*!< File name: Key in PEM format that belongs to client cert */
str		default_cipher_suite_list = STR_NULL;		/*!< List of allowed cipher suites */
unsigned int	default_tls_version = 0;		/*!< 0 = Use libcurl default */
unsigned int	default_tls_verify_peer = 1;		/*!< 0 = Do not verify TLS server cert. 1 = Verify TLS cert (default) */
unsigned int	default_tls_verify_host = 2;		/*!< 0 = Do not verify TLS server CN/SAN  2 = Verify TLS server CN/SAN (default) */
str 		default_http_proxy = STR_NULL;		/*!< Default HTTP proxy to use */
unsigned int	default_http_proxy_port = 0;		/*!< Default HTTP proxy port to use */
unsigned int	default_http_follow_redirect = 0;	/*!< Follow HTTP redirects CURLOPT_FOLLOWLOCATION */
str 		default_useragent = { CURL_USER_AGENT, CURL_USER_AGENT_LEN };	/*!< Default CURL useragent. Default "Kamailio Curl " */
unsigned int	default_maxdatasize = 0;		/*!< Default download size. 0=disabled */
unsigned int 	default_authmethod = CURLAUTH_BASIC | CURLAUTH_DIGEST;		/*!< authentication method - Basic, Digest or both */

str		http_client_config_file = STR_NULL;

static curl_version_info_data *curl_info;

/* Module management function prototypes */
static int mod_init(void);
static int child_init(int);
static void destroy(void);

/* Fixup functions to be defined later */
static int fixup_http_query_get(void** param, int param_no);
static int fixup_free_http_query_get(void** param, int param_no);
static int fixup_http_query_post(void** param, int param_no);
static int fixup_free_http_query_post(void** param, int param_no);

static int fixup_curl_connect(void** param, int param_no);
static int fixup_free_curl_connect(void** param, int param_no);
static int fixup_curl_connect_post(void** param, int param_no);
static int fixup_free_curl_connect_post(void** param, int param_no);

/* Wrappers for http_query to be defined later */
static int w_http_query(struct sip_msg* _m, char* _url, char* _result);
static int w_http_query_post(struct sip_msg* _m, char* _url, char* _post, char* _result);
static int w_curl_connect(struct sip_msg* _m, char* _con, char * _url, char* _result);
static int w_curl_connect_post(struct sip_msg* _m, char* _con, char * _url, char* _result, char* _ctype, char* _data);

/* forward function */
static int curl_con_param(modparam_t type, void* val);
static int pv_parse_curlerror(pv_spec_p sp, str *in);
static int pv_get_curlerror(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);

/* Exported functions */
static cmd_export_t cmds[] = {
    {"http_client_query", (cmd_function)w_http_query, 2, fixup_http_query_get,
     fixup_free_http_query_get,
     REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
    {"http_client_query", (cmd_function)w_http_query_post, 3, fixup_http_query_post,
     fixup_free_http_query_post,
     REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
    {"http_connect", (cmd_function)w_curl_connect, 3, fixup_curl_connect,
     fixup_free_curl_connect,
     REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
    {"http_connect", (cmd_function)w_curl_connect_post, 5, fixup_curl_connect_post,
     fixup_free_curl_connect_post,
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
	{"authmetod", PARAM_INT, &default_authmethod },
    	{0, 0, 0}
};


/*!
 * \brief Exported Pseudo variables
 */
static pv_export_t mod_pvs[] = {
    {{"curlerror", (sizeof("curlerror")-1)}, /* Curl error codes */
     PVT_OTHER, pv_get_curlerror, 0,
	pv_parse_curlerror, 0, 0, 0},
    {{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

/* Module interface */
struct module_exports exports = {
    "http_client",
    DEFAULT_DLFLAGS, /* dlopen flags */
    cmds,      /* Exported functions */
    params,    /* Exported parameters */
    0,         /* exported statistics */
    0,   	/* exported MI functions */
    mod_pvs,         /* exported pseudo-variables */
    0,         /* extra processes */
    mod_init,  /* module initialization function */
    0,         /* response function*/
    destroy,   /* destroy function */
    child_init /* per-child init function */
};

counter_handle_t connections;	/* Number of connection definitions */
counter_handle_t connok;	/* Successful Connection attempts */
counter_handle_t connfail;	/* Failed Connection attempts */



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
        counter_register(&connections, "httpclient", "connections", 0, 0, 0, "Counter of connection definitions (httpcon)", 0);
        counter_register(&connok, "httpclient", "connok", 0, 0, 0, "Counter of successful connections (200 OK)", 0);
        counter_register(&connfail, "httpclient", "connfail", 0, 0, 0, "Counter of failed connections (not 200 OK)", 0);
}


/* Module initialization function */
static int mod_init(void)
{

	LM_DBG("init curl module\n");

	/* Initialize curl */
	if (curl_global_init(CURL_GLOBAL_ALL)) {
		LM_ERR("curl_global_init failed\n");
		return -1;
	}
	curl_info = curl_version_info(CURLVERSION_NOW);

	if(curl_init_rpc() < 0)
        {
                LM_ERR("failed to register RPC commands\n");
                return -1;
        }

	if (init_shmlock() != 0) {
		LM_CRIT("cannot initialize shmlock.\n");
		return -1;
	}

	curl_counter_init();
	counter_add(connections, curl_connection_count());

	if (default_tls_version >= CURL_SSLVERSION_LAST) {
		LM_WARN("tlsversion %d unsupported value. Using libcurl default\n", default_tls_version);
		default_tls_version = CURL_SSLVERSION_DEFAULT;
	}
	if (http_client_config_file.s != NULL)
	{
		if (http_client_load_config(&http_client_config_file) < 0)
		{
			LM_ERR("Failed to load http_client connections from [%.*s]\n", http_client_config_file.len, http_client_config_file.s);
			return -1;
		}
	}

	if (default_connection_timeout == 0) {
		LM_ERR("CURL connection timeout set to zero. Using default 4 secs\n");
		default_connection_timeout = 4;
	}
	if (default_http_proxy_port == 0) {
		LM_INFO("HTTP proxy port set to 0. Disabling HTTP proxy\n");
	}


	LM_DBG("**** init curl module done. Curl version: %s SSL %s\n", curl_info->version, curl_info->ssl_version);
	LM_DBG("**** init curl: Number of connection objects: %d \n", curl_connection_count());
	LM_DBG("**** init curl: User Agent: %.*s \n", default_useragent.len, default_useragent.s);
	LM_DBG("**** init curl: HTTPredirect: %d \n", default_http_follow_redirect);
	LM_DBG("**** init curl: Client Cert: %.*s Key %.*s\n", default_tls_clientcert.len, default_tls_clientcert.s, default_tls_clientkey.len, default_tls_clientkey.s);
	LM_DBG("**** init curl: CA Cert: %s \n", default_tls_cacert);
	LM_DBG("**** init curl: Cipher Suites: %.*s \n", default_cipher_suite_list.len, default_cipher_suite_list.s);
	LM_DBG("**** init curl: SSL Version: %d \n", default_tls_version);
	LM_DBG("**** init curl: verifypeer: %d verifyhost: %d\n", default_tls_verify_peer, default_tls_verify_host);
	LM_DBG("**** init curl: HTTP Proxy: %.*s Port %d\n", default_http_proxy.len, default_http_proxy.s, default_http_proxy_port);

	LM_DBG("Extra: Curl supports %s %s %s \n",
			(curl_info->features & CURL_VERSION_SSL ? "SSL" : ""),
			(curl_info->features & CURL_VERSION_IPV6 ? "IPv6" : ""),
			(curl_info->features & CURL_VERSION_IDN ? "IDN" : "")
		 );
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
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN) {
		return 0; /* do nothing for the main process */
	}

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
	return curl_parse_param((char*)val);
error:
	return -1;

}

/* Fixup functions */

/*
 * Fix http_query params: url (string that may contain pvars) and
 * result (writable pvar).
 */
static int fixup_http_query_get(void** param, int param_no)
{
    if (param_no == 1) {
	return fixup_spve_null(param, 1);
    }

    if (param_no == 2) {
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
 * Free http_query params.
 */
static int fixup_free_http_query_get(void** param, int param_no)
{
    if (param_no == 1) {
        return fixup_free_spve_null(param, 1);
    }

    if (param_no == 2) {
	return fixup_free_pvar_null(param, 1);
    }

    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}


/*
 * Fix curl_connect params: connection(string/pvar) url (string that may contain pvars) and
 * result (writable pvar).
 */
static int fixup_curl_connect(void** param, int param_no)
{

    if (param_no == 1) {
	/* We want char * strings */
	return 0;
    }
    /* URL and data may contain pvar */
    if (param_no == 2) {
        return fixup_spve_null(param, 1);
    }
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
 * Fix curl_connect params when posting (5 parameters):
 *	connection (string/pvar), url (string with pvars), content-type,
 *      data (string/pvar, pvar)
 */
static int fixup_curl_connect_post(void** param, int param_no)
{

    if (param_no == 1 || param_no == 3) {
	/* We want char * strings */
	return 0;
    }
    /* URL and data may contain pvar */
    if (param_no == 2 || param_no == 4) {
        return fixup_spve_null(param, 1);
    }
    if (param_no == 5) {
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
 * Free curl_connect params.
 */
static int fixup_free_curl_connect_post(void** param, int param_no)
{
    if (param_no == 1 || param_no == 3) {
	/* Char strings don't need freeing */
	return 0;
    }
    if (param_no == 2 || param_no == 4) {
        return fixup_free_spve_null(param, 1);
    }

    if (param_no == 5) {
	return fixup_free_pvar_null(param, 1);
    }

    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}

/*
 * Free curl_connect params.
 */
static int fixup_free_curl_connect(void** param, int param_no)
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
static int w_curl_connect(struct sip_msg* _m, char* _con, char * _url, char* _result) {
/* int curl_con_query_url(struct sip_msg* _m, const str *connection, const str* _url, str* _result, const str *contenttype, const str* _post); */

	str con = {NULL,0};
	str url = {NULL,0};
	str result = {NULL,0};
	pv_spec_t *dst;
	pv_value_t val;
	int ret = 0;

	if (_con == NULL || _url == NULL || _result == NULL) {
		LM_ERR("Invalid parameter\n");
	}
	con.s = _con;
	con.len = strlen(con.s);

	if (get_str_fparam(&url, _m, (gparam_p)_url) != 0) {
		LM_ERR("_url has no value\n");
		return -1;
	}

	LM_DBG("**** Curl Connection %s URL %s Result var %s\n", _con, _url, _result);

	ret = curl_con_query_url(_m, &con, &url, &result, NULL, NULL);

	val.rs = result;
	val.flags = PV_VAL_STR;
	dst = (pv_spec_t *)_result;
	dst->setf(_m, &dst->pvp, (int)EQ_T, &val);

	if (result.s != NULL)
		pkg_free(result.s);

	return (ret==0)?-1:ret;
}

/*
 * Wrapper for Curl_connect (POST)
 */
static int w_curl_connect_post(struct sip_msg* _m, char* _con, char * _url, char* _ctype, char* _data, char *_result) {
	str con = {NULL,0};
	str url = {NULL,0};
	str data = {NULL, 0};
	str result = {NULL,0};
	pv_spec_t *dst;
	pv_value_t val;
	int ret = 0;

	if (_con == NULL || _url == NULL || _data == NULL || _result == NULL) {
		LM_ERR("Invalid parameter\n");
	}
	con.s = _con;
	con.len = strlen(con.s);

	if (get_str_fparam(&url, _m, (gparam_p)_url) != 0) {
		LM_ERR("_url has no value\n");
		return -1;
	}
	if (get_str_fparam(&data, _m, (gparam_p)_data) != 0) {
		LM_ERR("_data has no value\n");
		return -1;
	}

	LM_DBG("**** Curl Connection %s URL %s Result var %s\n", _con, _url, _result);

	ret = curl_con_query_url(_m, &con, &url, &result, _ctype, &data);

	val.rs = result;
	val.flags = PV_VAL_STR;
	dst = (pv_spec_t *)_result;
	dst->setf(_m, &dst->pvp, (int)EQ_T, &val);

	if (result.s != NULL)
		pkg_free(result.s);

	return (ret==0)?-1:ret;
}


/*!
 * Fix http_query params: url (string that may contain pvars) and
 * result (writable pvar).
 */
static int fixup_http_query_post(void** param, int param_no)
{
    if ((param_no == 1) || (param_no == 2)) {
	return fixup_spve_null(param, 1);
    }

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

/*!
 * Free http_query params.
 */
static int fixup_free_http_query_post(void** param, int param_no)
{
    if ((param_no == 1) || (param_no == 2)) {
        return fixup_free_spve_null(param, 1);
    }

    if (param_no == 3) {
	return fixup_free_pvar_null(param, 1);
    }

    LM_ERR("invalid parameter number <%d>\n", param_no);
    return -1;
}

/*!
 * Wrapper for HTTP-Query (GET)
 */
static int w_http_query(struct sip_msg* _m, char* _url, char* _result) {
	int ret = 0;
	str url = {NULL, 0};
	str result = {NULL, 0};
	pv_spec_t *dst;
	pv_value_t val;

	if (get_str_fparam(&url, _m, (gparam_p)_url) != 0) {
		LM_ERR("_url has no value\n");
		return -1;
	}

	ret = http_query(_m, url.s, &result, NULL);

	val.rs = result;
	val.flags = PV_VAL_STR;
	dst = (pv_spec_t *)_result;
	dst->setf(_m, &dst->pvp, (int)EQ_T, &val);

	if (result.s != NULL)
		pkg_free(result.s);
	return (ret==0)?-1:ret;
}


/*!
 * Wrapper for HTTP-Query (POST-Variant)
 */
static int w_http_query_post(struct sip_msg* _m, char* _url, char* _post, char* _result) {
	int ret = 0;
	str url = {NULL, 0};
	str post = {NULL, 0};
	str result = {NULL, 0};
	pv_spec_t *dst;
	pv_value_t val;

	if (get_str_fparam(&url, _m, (gparam_p)_url) != 0) {
		LM_ERR("_url has no value\n");
		return -1;
	}
	if (get_str_fparam(&post, _m, (gparam_p)_post) != 0) {
		LM_ERR("_data has no value\n");
		return -1;
	}

	ret = http_query(_m, url.s, &result, post.s);
	val.rs = result;
	val.flags = PV_VAL_STR;
	dst = (pv_spec_t *)_result;
	dst->setf(_m, &dst->pvp, (int)EQ_T, &val);

	if (result.s != NULL)
		pkg_free(result.s);
	return (ret==0)?-1:ret;
}

/*!
 * Parse arguments to  pv $curlerror
 */
static int pv_parse_curlerror(pv_spec_p sp, str *in)
{
	int cerr  = 0;
	if(sp==NULL || in==NULL || in->len<=0)
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
static int pv_get_curlerror(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	str curlerr;
	char *err = NULL;

	if(param==NULL) {
		return -1;
	}

	/* cURL error codes does not collide with HTTP codes */
	if (param->pvn.u.isname.name.n < 0 || param->pvn.u.isname.name.n > 999 ) {
		err = "Bad CURL error code";
	}
	if (param->pvn.u.isname.name.n > 99) {
		err = "HTTP result code";
	}
	if (err == NULL) {
		err = (char *) curl_easy_strerror(param->pvn.u.isname.name.n);
	}
	curlerr.s = err;
	curlerr.len = strlen(err);

	return pv_get_strval(msg, param, res, &curlerr);
}

