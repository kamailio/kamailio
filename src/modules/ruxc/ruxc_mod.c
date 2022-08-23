/**
 * Copyright (C) 2021 Daniel-Constantin Mierla (asipto.com)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <ruxc.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/data_lump.h"
#include "../../core/str_list.h"
#include "../../core/lvalue.h"
#include "../../core/kemi.h"


MODULE_VERSION

static int _ruxc_http_timeout = 5000;
static int _ruxc_http_timeout_connect = 5000;
static int _ruxc_http_timeout_read = 5000;
static int _ruxc_http_timeout_write = 5000;
static int _ruxc_http_tlsmode = 0;
static int _ruxc_http_reuse = 0;
static int _ruxc_http_retry = 0;
static int _ruxc_http_logtype = 0;
static int _ruxc_http_debug = 0;

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int fixup_ruxc_http_get(void **param, int param_no);
static int fixup_free_ruxc_http_get(void **param, int param_no);
static int w_ruxc_http_get(struct sip_msg *_msg, char *_url,
		char *_hdrs, char *_result);

static int fixup_ruxc_http_post(void **param, int param_no);
static int fixup_free_ruxc_http_post(void **param, int param_no);
static int w_ruxc_http_post(struct sip_msg *_msg, char *_url,
		char *_body, char *_hdrs, char *_result);

static int w_ruxc_http_delete(struct sip_msg *_msg, char *_url,
		char *_body, char *_hdrs, char *_result);

typedef struct ruxc_data {
	str value;
	int ret;
} ruxc_data_t;

/* clang-format off */
static cmd_export_t cmds[]={
	{"ruxc_http_get", (cmd_function)w_ruxc_http_get, 3, fixup_ruxc_http_get,
		fixup_free_ruxc_http_get, ANY_ROUTE},
	{"ruxc_http_post", (cmd_function)w_ruxc_http_post, 4, fixup_ruxc_http_post,
		fixup_free_ruxc_http_post, ANY_ROUTE},
	{"ruxc_http_delete", (cmd_function)w_ruxc_http_delete, 4, fixup_ruxc_http_post,
		fixup_free_ruxc_http_post, ANY_ROUTE},

	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"http_timeout",         PARAM_INT,   &_ruxc_http_timeout},
	{"http_timeout_connect", PARAM_INT,   &_ruxc_http_timeout_connect},
	{"http_timeout_read",    PARAM_INT,   &_ruxc_http_timeout_read},
	{"http_timeout_write",   PARAM_INT,   &_ruxc_http_timeout_write},
	{"http_tlsmode",         PARAM_INT,   &_ruxc_http_tlsmode},
	{"http_reuse",           PARAM_INT,   &_ruxc_http_reuse},
	{"http_retry",           PARAM_INT,   &_ruxc_http_retry},
	{"http_logtype",         PARAM_INT,   &_ruxc_http_logtype},
	{"http_debug",           PARAM_INT,   &_ruxc_http_debug},

	{0, 0, 0}
};

struct module_exports exports = {
	"ruxc",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,              /* exported RPC methods */
	0,              /* exported pseudo-variables */
	0,              /* response function */
	mod_init,       /* module initialization function */
	child_init,     /* per child init function */
	mod_destroy    	/* destroy function */
};
/* clang-format on */


/**
 * init module function
 */
static int mod_init(void)
{
	if(_ruxc_http_logtype==1 && log_stderr==1) {
		LM_INFO("setting http logtype to 0\n");
		_ruxc_http_logtype = 0;
	} else if(_ruxc_http_logtype==0 && log_stderr==0) {
		LM_INFO("setting http logtype to 1\n");
		_ruxc_http_logtype = 1;
	}
	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	return;
}

/**
 *
 */
static void ruxc_request_params_init(RuxcHTTPRequest *v_http_request)
{
	v_http_request->timeout = _ruxc_http_timeout;
	v_http_request->timeout_connect = _ruxc_http_timeout_connect;
	v_http_request->timeout_read = _ruxc_http_timeout_read;
	v_http_request->timeout_write = _ruxc_http_timeout_write;
	v_http_request->tlsmode = _ruxc_http_tlsmode;
	v_http_request->reuse = _ruxc_http_reuse;
	v_http_request->retry = _ruxc_http_retry;
	v_http_request->logtype = _ruxc_http_logtype;
	v_http_request->debug = _ruxc_http_debug;
}

/**
 *
 */
static int ki_ruxc_http_get_helper(sip_msg_t *_msg, str *url, str *hdrs,
		pv_spec_t *dst)
{
	RuxcHTTPRequest v_http_request = {0};
	RuxcHTTPResponse v_http_response = {0};
	pv_value_t val = {0};
	int ret;

	ruxc_request_params_init(&v_http_request);

	v_http_request.url = url->s;
	v_http_request.url_len = url->len;

	if(hdrs!=NULL && hdrs->s!=NULL && hdrs->len>0) {
		v_http_request.headers = hdrs->s;
		v_http_request.headers_len = hdrs->len;
	}

	ruxc_http_get(&v_http_request, &v_http_response);

	if(v_http_response.retcode < 0) {
		LM_ERR("failed to perform http get - retcode: %d\n", v_http_response.retcode);
		ret = v_http_response.retcode;
	} else {
		if(v_http_response.resdata != NULL &&  v_http_response.resdata_len>0) {
			LM_DBG("response code: %d - data len: %d - data: [%.*s]\n",
					v_http_response.rescode, v_http_response.resdata_len,
					v_http_response.resdata_len, v_http_response.resdata);
			val.rs.s = v_http_response.resdata;
			val.rs.len = v_http_response.resdata_len;
			val.flags = PV_VAL_STR;
			if(dst->setf) {
				dst->setf(_msg, &dst->pvp, (int)EQ_T, &val);
			} else {
				LM_WARN("target pv is not writable\n");
			}
		}
		ret = v_http_response.rescode;
	}
	ruxc_http_response_release(&v_http_response);

	return (ret!=0)?ret:-2;
}

/**
 *
 */
static int ki_ruxc_http_get(sip_msg_t *_msg, str *url, str *hdrs, str *dpv)
{
	pv_spec_t *dst;

	dst = pv_cache_get(dpv);
	if(dst==NULL) {
		LM_ERR("failed to get pv spec for: %.*s\n", dpv->len, dpv->s);
		return -1;
	}
	if(dst->setf==NULL) {
		LM_ERR("target pv is not writable: %.*s\n", dpv->len, dpv->s);
		return -1;
	}
	return ki_ruxc_http_get_helper(_msg, url, hdrs, dst);
}

/**
 *
 */
static int w_ruxc_http_get(struct sip_msg *_msg, char *_url,
		char *_hdrs, char *_result)
{
	str url = {NULL, 0};
	str hdrs = {NULL, 0};
	pv_spec_t *dst;

	if(get_str_fparam(&url, _msg, (gparam_p)_url) != 0 || url.len <= 0) {
		LM_ERR("URL has no value\n");
		return -1;
	}
	if(_hdrs && get_str_fparam(&hdrs, _msg, (gparam_p)_hdrs) != 0) {
		LM_ERR("HDRS has no value\n");
		return -1;
	} else {
		if(hdrs.len == 0) {
			hdrs.s = NULL;
		}
	}
	dst = (pv_spec_t *)_result;

	return ki_ruxc_http_get_helper(_msg, &url, &hdrs, dst);
}

/**
 *
 */
static int ki_ruxc_http_post_helper(sip_msg_t *_msg, str *url, str *body, str *hdrs,
		pv_spec_t *dst)
{
	RuxcHTTPRequest v_http_request = {0};
	RuxcHTTPResponse v_http_response = {0};
	pv_value_t val = {0};
	int ret;

	ruxc_request_params_init(&v_http_request);

	v_http_request.url = url->s;
	v_http_request.url_len = url->len;

	if(body!=NULL && body->s!=NULL && body->len>0) {
		v_http_request.data = body->s;
		v_http_request.data_len = body->len;
	}

	if(hdrs!=NULL && hdrs->s!=NULL && hdrs->len>0) {
		v_http_request.headers = hdrs->s;
		v_http_request.headers_len = hdrs->len;
	}

	ruxc_http_post(&v_http_request, &v_http_response);

	if(v_http_response.retcode < 0) {
		LM_ERR("failed to perform http get - retcode: %d\n", v_http_response.retcode);
		ret = v_http_response.retcode;
	} else {
		if(v_http_response.resdata != NULL &&  v_http_response.resdata_len>0) {
			LM_DBG("response code: %d - data len: %d - data: [%.*s]\n",
					v_http_response.rescode, v_http_response.resdata_len,
					v_http_response.resdata_len, v_http_response.resdata);
			val.rs.s = v_http_response.resdata;
			val.rs.len = v_http_response.resdata_len;
			val.flags = PV_VAL_STR;
			if(dst->setf) {
				dst->setf(_msg, &dst->pvp, (int)EQ_T, &val);
			} else {
				LM_WARN("target pv is not writable\n");
			}
		}
		ret = v_http_response.rescode;
	}
	ruxc_http_response_release(&v_http_response);

	return (ret!=0)?ret:-2;
}

/**
 *
 */
static int ki_ruxc_http_post(sip_msg_t *_msg, str *url, str *body, str *hdrs, str *dpv)
{
	pv_spec_t *dst;

	dst = pv_cache_get(dpv);
	if(dst==NULL) {
		LM_ERR("failed to get pv spec for: %.*s\n", dpv->len, dpv->s);
		return -1;
	}
	if(dst->setf==NULL) {
		LM_ERR("target pv is not writable: %.*s\n", dpv->len, dpv->s);
		return -1;
	}
	return ki_ruxc_http_post_helper(_msg, url, body, hdrs, dst);
}

/**
 *
 */
static int w_ruxc_http_post(struct sip_msg *_msg, char *_url,
		char *_body, char *_hdrs, char *_result)
{
	str url = {NULL, 0};
	str body = {NULL, 0};
	str hdrs = {NULL, 0};
	pv_spec_t *dst;

	if(get_str_fparam(&url, _msg, (gparam_p)_url) != 0 || url.len <= 0) {
		LM_ERR("URL has no value\n");
		return -1;
	}
	if(_body && get_str_fparam(&body, _msg, (gparam_p)_body) != 0) {
		LM_ERR("DATA body has no value\n");
		return -1;
	} else {
		if(body.len == 0) {
			body.s = NULL;
		}
	}
	if(_hdrs && get_str_fparam(&hdrs, _msg, (gparam_p)_hdrs) != 0) {
		LM_ERR("HDRS has no value\n");
		return -1;
	} else {
		if(hdrs.len == 0) {
			hdrs.s = NULL;
		}
	}
	dst = (pv_spec_t *)_result;

	return ki_ruxc_http_post_helper(_msg, &url, &body, &hdrs, dst);
}

/**
 *
 */
static int ki_ruxc_http_delete_helper(sip_msg_t *_msg, str *url, str *body, str *hdrs,
		pv_spec_t *dst)
{
	RuxcHTTPRequest v_http_request = {0};
	RuxcHTTPResponse v_http_response = {0};
	pv_value_t val = {0};
	int ret;

	ruxc_request_params_init(&v_http_request);

	v_http_request.url = url->s;
	v_http_request.url_len = url->len;

	if(body!=NULL && body->s!=NULL && body->len>0) {
		v_http_request.data = body->s;
		v_http_request.data_len = body->len;
	}

	if(hdrs!=NULL && hdrs->s!=NULL && hdrs->len>0) {
		v_http_request.headers = hdrs->s;
		v_http_request.headers_len = hdrs->len;
	}

	ruxc_http_delete(&v_http_request, &v_http_response);

	if(v_http_response.retcode < 0) {
		LM_ERR("failed to perform http get - retcode: %d\n", v_http_response.retcode);
		ret = v_http_response.retcode;
	} else {
		if(v_http_response.resdata != NULL &&  v_http_response.resdata_len>0) {
			LM_DBG("response code: %d - data len: %d - data: [%.*s]\n",
					v_http_response.rescode, v_http_response.resdata_len,
					v_http_response.resdata_len, v_http_response.resdata);
			val.rs.s = v_http_response.resdata;
			val.rs.len = v_http_response.resdata_len;
			val.flags = PV_VAL_STR;
			if(dst->setf) {
				dst->setf(_msg, &dst->pvp, (int)EQ_T, &val);
			} else {
				LM_WARN("target pv is not writable\n");
			}
		}
		ret = v_http_response.rescode;
	}
	ruxc_http_response_release(&v_http_response);

	return (ret!=0)?ret:-2;
}

/**
 *
 */
static int ki_ruxc_http_delete(sip_msg_t *_msg, str *url, str *body, str *hdrs, str *dpv)
{
	pv_spec_t *dst;

	dst = pv_cache_get(dpv);
	if(dst==NULL) {
		LM_ERR("failed to get pv spec for: %.*s\n", dpv->len, dpv->s);
		return -1;
	}
	if(dst->setf==NULL) {
		LM_ERR("target pv is not writable: %.*s\n", dpv->len, dpv->s);
		return -1;
	}
	return ki_ruxc_http_delete_helper(_msg, url, body, hdrs, dst);
}

/**
 *
 */
static int w_ruxc_http_delete(struct sip_msg *_msg, char *_url,
		char *_body, char *_hdrs, char *_result)
{
	str url = {NULL, 0};
	str body = {NULL, 0};
	str hdrs = {NULL, 0};
	pv_spec_t *dst;

	if(get_str_fparam(&url, _msg, (gparam_p)_url) != 0 || url.len <= 0) {
		LM_ERR("URL has no value\n");
		return -1;
	}
	if(_body && get_str_fparam(&body, _msg, (gparam_p)_body) != 0) {
		LM_ERR("DATA body has no value\n");
		return -1;
	} else {
		if(body.len == 0) {
			body.s = NULL;
		}
	}
	if(_hdrs && get_str_fparam(&hdrs, _msg, (gparam_p)_hdrs) != 0) {
		LM_ERR("HDRS has no value\n");
		return -1;
	} else {
		if(hdrs.len == 0) {
			hdrs.s = NULL;
		}
	}
	dst = (pv_spec_t *)_result;

	return ki_ruxc_http_delete_helper(_msg, &url, &body, &hdrs, dst);
}

/**
 *
 */
static int fixup_ruxc_http_get(void **param, int param_no)
{
	if((param_no >= 1) && (param_no <= 2)) {
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

/**
 *
 */
static int fixup_free_ruxc_http_get(void **param, int param_no)
{
	if((param_no >= 1) && (param_no <= 2)) {
		return fixup_free_spve_null(param, 1);
	}

	if(param_no == 3) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/**
 *
 */
static int fixup_ruxc_http_post(void **param, int param_no)
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

/**
 *
 */
static int fixup_free_ruxc_http_post(void **param, int param_no)
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

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_ruxc_exports[] = {
	{ str_init("ruxc"), str_init("http_get"),
		SR_KEMIP_INT, ki_ruxc_http_get,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ruxc"), str_init("http_post"),
		SR_KEMIP_INT, ki_ruxc_http_post,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("ruxc"), str_init("http_delete"),
		SR_KEMIP_INT, ki_ruxc_http_delete,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_ruxc_exports);
	return 0;
}
