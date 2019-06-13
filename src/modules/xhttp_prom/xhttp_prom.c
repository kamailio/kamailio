/*
 * Copyright (C) 2012 VoIP Embedded, Inc.
 *
 * Copyright (C) 2019 Vicente Hernando (Sonoc)
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

#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/str.h"
#include "../../modules/xhttp/api.h"
#include "../../core/nonsip_hooks.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include "xhttp_prom.h"
#include "prom.h"
#include "prom_metric.h"

/** @addtogroup xhttp_prom
 * @ingroup modules
 * @{
 *
 * <h1>Overview of Operation</h1>
 * This module provides a web interface for Prometheus server.
 * It is built on top of the xhttp API module.
 */

/** @file
 *
 * This is the main file of xhttp_prom module which contains all the functions
 * related to http processing, as well as the module interface.
 */

MODULE_VERSION

/* Declaration of static functions. */

str XHTTP_PROM_REASON_OK = str_init("OK");
str XHTTP_PROM_CONTENT_TYPE_TEXT_HTML = str_init("text/plain; version=0.0.4");

static rpc_export_t rpc_cmds[];
static int mod_init(void);
static void mod_destroy(void);
static int w_prom_check_uri(sip_msg_t* msg);
static int w_prom_dispatch(sip_msg_t* msg);
static int w_prom_counter_reset_l0(struct sip_msg* msg, char* pname);
static int w_prom_counter_reset_l1(struct sip_msg* msg, char* pname, char *l1);
static int w_prom_counter_reset_l2(struct sip_msg* msg, char* pname, char *l1, char *l2);
static int w_prom_counter_reset_l3(struct sip_msg* msg, char* pname, char *l1, char *l2, char *l3);
static int w_prom_gauge_reset_l0(struct sip_msg* msg, char* pname);
static int w_prom_gauge_reset_l1(struct sip_msg* msg, char* pname, char *l1);
static int w_prom_gauge_reset_l2(struct sip_msg* msg, char* pname, char *l1, char *l2);
static int w_prom_gauge_reset_l3(struct sip_msg* msg, char* pname, char *l1, char *l2, char *l3);
static int w_prom_counter_inc_l0(struct sip_msg* msg, char *pname, char* pnumber);
static int w_prom_counter_inc_l1(struct sip_msg* msg, char *pname, char* pnumber, char *l1);
static int w_prom_counter_inc_l2(struct sip_msg* msg, char *pname, char* pnumber, char *l1, char *l2);
static int w_prom_counter_inc_l3(struct sip_msg* msg, char *pname, char* pnumber, char *l1, char *l2, char *l3);
static int w_prom_gauge_set_l0(struct sip_msg* msg, char *pname, char* pnumber);
static int w_prom_gauge_set_l1(struct sip_msg* msg, char *pname, char* pnumber, char *l1);
static int w_prom_gauge_set_l2(struct sip_msg* msg, char *pname, char* pnumber, char *l1, char *l2);
static int w_prom_gauge_set_l3(struct sip_msg* msg, char *pname, char* pnumber, char *l1, char *l2, char *l3);
static int fixup_metric_reset(void** param, int param_no);
static int fixup_counter_inc(void** param, int param_no);

int prom_counter_param(modparam_t type, void *val);
int prom_gauge_param(modparam_t type, void *val);

/** The context of the xhttp_prom request being processed.
 *
 * This is a global variable that records the context of the xhttp_prom request
 * being currently processed.
 * @sa prom_ctx
 */
static prom_ctx_t ctx;

static xhttp_api_t xhttp_api;

/* It does not show Kamailio statistics by default. */
str xhttp_prom_stats = str_init("");

int buf_size = 0;
int timeout_minutes = 0;
char error_buf[ERROR_REASON_BUF_LEN];

/* module commands */
static cmd_export_t cmds[] = {
	{"prom_check_uri",(cmd_function)w_prom_check_uri,0,0,0,
	 REQUEST_ROUTE|EVENT_ROUTE},
	{"prom_dispatch",(cmd_function)w_prom_dispatch,0,0,0,
	 REQUEST_ROUTE|EVENT_ROUTE},
	{"prom_counter_reset", (cmd_function)w_prom_counter_reset_l0, 1, fixup_metric_reset,
	 0, ANY_ROUTE},
	{"prom_counter_reset", (cmd_function)w_prom_counter_reset_l1, 2, fixup_metric_reset,
	 0, ANY_ROUTE},
	{"prom_counter_reset", (cmd_function)w_prom_counter_reset_l2, 3, fixup_metric_reset,
	 0, ANY_ROUTE},
	{"prom_counter_reset", (cmd_function)w_prom_counter_reset_l3, 4, fixup_metric_reset,
	 0, ANY_ROUTE},
	{"prom_gauge_reset", (cmd_function)w_prom_gauge_reset_l0, 1, fixup_metric_reset,
	 0, ANY_ROUTE},
	{"prom_gauge_reset", (cmd_function)w_prom_gauge_reset_l1, 2, fixup_metric_reset,
	 0, ANY_ROUTE},
	{"prom_gauge_reset", (cmd_function)w_prom_gauge_reset_l2, 3, fixup_metric_reset,
	 0, ANY_ROUTE},
	{"prom_gauge_reset", (cmd_function)w_prom_gauge_reset_l3, 4, fixup_metric_reset,
	 0, ANY_ROUTE},
	{"prom_counter_inc", (cmd_function)w_prom_counter_inc_l0, 2, fixup_counter_inc,
	 0, ANY_ROUTE},
	{"prom_counter_inc", (cmd_function)w_prom_counter_inc_l1, 3, fixup_counter_inc,
	 0, ANY_ROUTE},
	{"prom_counter_inc", (cmd_function)w_prom_counter_inc_l2, 4, fixup_counter_inc,
	 0, ANY_ROUTE},
	{"prom_counter_inc", (cmd_function)w_prom_counter_inc_l3, 5, fixup_counter_inc,
	 0, ANY_ROUTE},
	{"prom_gauge_set", (cmd_function)w_prom_gauge_set_l0, 2, fixup_metric_reset,
	 0, ANY_ROUTE},
	{"prom_gauge_set", (cmd_function)w_prom_gauge_set_l1, 3, fixup_metric_reset,
	 0, ANY_ROUTE},
	{"prom_gauge_set", (cmd_function)w_prom_gauge_set_l2, 4, fixup_metric_reset,
	 0, ANY_ROUTE},
	{"prom_gauge_set", (cmd_function)w_prom_gauge_set_l3, 5, fixup_metric_reset,
	 0, ANY_ROUTE},
	{ 0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"xhttp_prom_buf_size",	INT_PARAM,	&buf_size},
	{"xhttp_prom_stats",	PARAM_STR,	&xhttp_prom_stats},
	{"prom_counter",        PARAM_STRING|USE_FUNC_PARAM, (void*)prom_counter_param},
	{"prom_gauge",          PARAM_STRING|USE_FUNC_PARAM, (void*)prom_gauge_param},
	{"xhttp_prom_timeout",	INT_PARAM,	&timeout_minutes},
	{0, 0, 0}
};

struct module_exports exports = {
	"xhttp_prom",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,              /* exported RPC methods */
	0,         	/* exported pseudo-variables */
	0,              /* response function */
	mod_init,       /* module initialization function */
	0,		/* per child init function */
	mod_destroy     /* destroy function */
};

/** Implementation of prom_fault function required by the management API.
 *
 * This function will be called whenever a management function
 * indicates that an error ocurred while it was processing the request. The
 * function takes the reply code and reason phrase as parameters, these will
 * be put in the body of the reply.
 *
 * @param ctx A pointer to the context structure of the request being
 *            processed.
 * @param code Reason code.
 * @param fmt Formatting string used to build the reason phrase.
 */
static void prom_fault(prom_ctx_t* ctx, int code, char* fmt, ...)
{
	va_list ap;
	struct xhttp_prom_reply *reply = &ctx->reply;

	reply->code = code;
	va_start(ap, fmt);
	vsnprintf(error_buf, ERROR_REASON_BUF_LEN, fmt, ap);
	va_end(ap);
	reply->reason.len = strlen(error_buf);
	reply->reason.s = error_buf;
	/* reset body so we can print the error */
	reply->body.len = 0;

	return;
}

static int mod_init(void)
{
	/* Register RPC commands. */
	if (rpc_register_array(rpc_cmds) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	/* bind the XHTTP API */
	if (xhttp_load_api(&xhttp_api) < 0) {
		LM_ERR("cannot bind to XHTTP API\n");
		return -1;
	}

	/* Check xhttp_prom_buf_size param */
	if (buf_size == 0)
		buf_size = pkg_mem_size/3;

	/* Check xhttp_prom_timeout param */
	if (timeout_minutes == 0) {
		timeout_minutes = 60;
	}

	/* Initialize Prometheus metrics. */
	if (prom_metric_init(timeout_minutes)) {
		LM_ERR("Cannot initialize Prometheus metrics\n");
		return -1;
	}

	return 0;
}

static void mod_destroy(void)
{
	LM_DBG("cleaning up\n");

	prom_metric_close();
}

/**
 * Parse parameters to create a counter.
 */
int prom_counter_param(modparam_t type, void *val)
{
	return prom_counter_create((char*)val);
}

/**
 * Parse parameters to create a gauge.
 */
int prom_gauge_param(modparam_t type, void *val)
{
	return prom_gauge_create((char*)val);
}

#define PROMETHEUS_URI "/metrics"
static str prom_uri = str_init(PROMETHEUS_URI);

static int ki_xhttp_prom_check_uri(sip_msg_t* msg)
{
	if(msg==NULL) {
		LM_ERR("No message\n");
		return -1;
	}

	str *uri = &msg->first_line.u.request.uri;
	LM_DBG("URI: %.*s\n", uri->len, uri->s);
	
	if (STR_EQ(*uri, prom_uri)) {
		LM_DBG("URI matches: %.*s\n", uri->len, uri->s);
		/* Return True */
		return 1;
	}

	/* Return False */
	LM_DBG("URI does not match: %.*s (%.*s)\n", uri->len, uri->s,
		   prom_uri.len, prom_uri.s);
	return 0;
}

static int w_prom_check_uri(sip_msg_t* msg)
{
	if(msg==NULL) {
		LM_ERR("No message\n");
		return -1;
	}

	str *uri = &msg->first_line.u.request.uri;
	LM_DBG("URI: %.*s\n", uri->len, uri->s);
	
	if (STR_EQ(*uri, prom_uri)) {
		LM_DBG("URI matches: %.*s\n", uri->len, uri->s);
		/* Return True */
		return 1;
	}

	/* Return False */
	LM_DBG("URI does not match: %.*s (%.*s)\n", uri->len, uri->s,
		   prom_uri.len, prom_uri.s);
	return -1;
}

/** Initialize xhttp_prom reply data structure.
 *
 * This function initializes the data structure that contains all data related
 * to the xhttp_prom reply being created. The function must be called before any
 * other function that adds data to the reply.
 * @param ctx prom_ctx_t structure to be initialized.
 * @return 0 on success, a negative number on error.
 */
static int init_xhttp_prom_reply(prom_ctx_t *ctx)
{
	struct xhttp_prom_reply *reply = &ctx->reply;

	reply->code = 200;
	reply->reason = XHTTP_PROM_REASON_OK;
	reply->buf.s = pkg_malloc(buf_size);
	if (!reply->buf.s) {
		LM_ERR("oom\n");
		prom_fault(ctx, 500, "Internal Server Error (No memory left)");
		return -1;
	}
	reply->buf.len = buf_size;
	reply->body.s = reply->buf.s;
	reply->body.len = 0;
	return 0;
}

/**
 * Free buffer in reply.
 */
static void xhttp_prom_reply_free(prom_ctx_t *ctx)
{
	struct xhttp_prom_reply* reply;
	reply = &ctx->reply;
	
	if (reply->buf.s) {
		pkg_free(reply->buf.s);
		reply->buf.s = NULL;
		reply->buf.len = 0;
	}

	/* if (ctx->arg.s) { */
	/* 	pkg_free(ctx->arg.s); */
	/* 	ctx->arg.s = NULL; */
	/* 	ctx->arg.len = 0; */
	/* } */
	/* if (ctx->data_structs) { */
	/* 	free_data_struct(ctx->data_structs); */
	/* 	ctx->data_structs = NULL; */
	/* } */
}

static int prom_send(prom_ctx_t* ctx)
{
	struct xhttp_prom_reply* reply;

	if (ctx->reply_sent) return 1;

	reply = &ctx->reply;

	if (prom_stats_get(ctx, &xhttp_prom_stats)){
		LM_DBG("prom_fault(500,\"Internal Server Error\"\n");
		prom_fault(ctx, 500, "Internal Server Error");
	}

	ctx->reply_sent = 1;
	if (reply->body.len)
		xhttp_api.reply(ctx->msg, reply->code, &reply->reason,
			&XHTTP_PROM_CONTENT_TYPE_TEXT_HTML, &reply->body);
	else {
		LM_DBG("xhttp_api.reply(%p, %d, %.*s, %.*s, %.*s)\n",
			   ctx->msg, reply->code,
			   (&reply->reason)->len, (&reply->reason)->s,
			   (&XHTTP_PROM_CONTENT_TYPE_TEXT_HTML)->len,
			   (&XHTTP_PROM_CONTENT_TYPE_TEXT_HTML)->s,
			   (&reply->reason)->len, (&reply->reason)->s);
		
		xhttp_api.reply(ctx->msg, reply->code, &reply->reason,
						&XHTTP_PROM_CONTENT_TYPE_TEXT_HTML, &reply->reason);
	}

	xhttp_prom_reply_free(ctx);
	
	return 0;
}


static int ki_xhttp_prom_dispatch(sip_msg_t* msg)
{
	int ret = 0;

	if(msg==NULL) {
		LM_ERR("No message\n");
		return -1;
	}

	if(!IS_HTTP(msg)) {
		LM_DBG("Got non HTTP msg\n");
		return NONSIP_MSG_PASS;
	}

	/* Init xhttp_prom context */
	if (ctx.reply.buf.s) {
		LM_ERR("Unexpected buf value [%p][%d]\n",
			   ctx.reply.buf.s, ctx.reply.buf.len);

		/* Something happened and this memory was not freed. */
		xhttp_prom_reply_free(&ctx);
	}
	memset(&ctx, 0, sizeof(prom_ctx_t));
	ctx.msg = msg;
	if (init_xhttp_prom_reply(&ctx) < 0) {
		goto send_reply;
	}

send_reply:
	if (!ctx.reply_sent) {
		ret = prom_send(&ctx);
	}
	if (ret < 0) {
		return -1;
	}
	return 0;
}

static int w_prom_dispatch(sip_msg_t* msg)
{
	return ki_xhttp_prom_dispatch(msg);
}

static int fixup_metric_reset(void** param, int param_no)
{
	return fixup_spve_null(param, 1);
}

/* static int fixup_free_metric_reset(void** param, int param_no) */
/* { */
/* 	return fixup_free_spve_null(param, 1); */
/* } */

/**
 * Reset a counter (No labels)
 */
static int ki_xhttp_prom_counter_reset_l0(struct sip_msg* msg, str *s_name)
{
	if (s_name == NULL || s_name->s == NULL || s_name->len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if (prom_counter_reset(s_name, NULL, NULL, NULL)) {
		LM_ERR("Cannot reset counter: %.*s\n", s_name->len, s_name->s);
		return -1;
	}

	LM_DBG("Counter %.*s reset\n", s_name->len, s_name->s);
	return 1;
}

/**
 * Reset a counter (1 label)
 */
static int ki_xhttp_prom_counter_reset_l1(struct sip_msg* msg, str *s_name, str *l1)
{
	if (s_name == NULL || s_name->s == NULL || s_name->len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if (l1 == NULL || l1->s == NULL || l1->len == 0) {
		LM_ERR("Invalid l1 string\n");
		return -1;
	}

	if (prom_counter_reset(s_name, l1, NULL, NULL)) {
		LM_ERR("Cannot reset counter: %.*s (%.*s)\n", s_name->len, s_name->s,
			   l1->len, l1->s);
		return -1;
	}

	LM_DBG("Counter %.*s (%.*s) reset\n", s_name->len, s_name->s,
		   l1->len, l1->s);

	return 1;
}

/**
 * Reset a counter (2 labels)
 */
static int ki_xhttp_prom_counter_reset_l2(struct sip_msg* msg, str *s_name, str *l1, str *l2)
{
	if (s_name == NULL || s_name->s == NULL || s_name->len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if (l1 == NULL || l1->s == NULL || l1->len == 0) {
		LM_ERR("Invalid l1 string\n");
		return -1;
	}

	if (l2 == NULL || l2->s == NULL || l2->len == 0) {
		LM_ERR("Invalid l2 string\n");
		return -1;
	}

	if (prom_counter_reset(s_name, l1, l2, NULL)) {
		LM_ERR("Cannot reset counter: %.*s (%.*s, %.*s)\n", s_name->len, s_name->s,
			   l1->len, l1->s,
			   l2->len, l2->s
			);
		return -1;
	}

	LM_DBG("Counter %.*s (%.*s, %.*s) reset\n", s_name->len, s_name->s,
		   l1->len, l1->s,
		   l2->len, l2->s
		);

	return 1;
}

/**
 * Reset a counter (3 labels)
 */
static int ki_xhttp_prom_counter_reset_l3(struct sip_msg* msg, str *s_name, str *l1, str *l2,
										  str *l3)
{
	if (s_name == NULL || s_name->s == NULL || s_name->len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if (l1 == NULL || l1->s == NULL || l1->len == 0) {
		LM_ERR("Invalid l1 string\n");
		return -1;
	}

	if (l2 == NULL || l2->s == NULL || l2->len == 0) {
		LM_ERR("Invalid l2 string\n");
		return -1;
	}

	if (l3 == NULL || l3->s == NULL || l3->len == 0) {
		LM_ERR("Invalid l3 string\n");
		return -1;
	}

	if (prom_counter_reset(s_name, l1, l2, l3)) {
		LM_ERR("Cannot reset counter: %.*s (%.*s, %.*s, %.*s)\n", s_name->len, s_name->s,
			   l1->len, l1->s,
			   l2->len, l2->s,
			   l3->len, l3->s
			);
		return -1;
	}

	LM_DBG("Counter %.*s (%.*s, %.*s, %.*s) reset\n", s_name->len, s_name->s,
		   l1->len, l1->s,
		   l2->len, l2->s,
		   l3->len, l3->s
		);

	return 1;
}

/**
 * Reset a counter.
 */
static int w_prom_counter_reset(struct sip_msg* msg, char* pname, char *l1, char *l2,
								char *l3)
{
	str s_name;

	if (pname == NULL) {
		LM_ERR("Invalid parameter\n");
		return -1;
	}

	if (get_str_fparam(&s_name, msg, (gparam_t*)pname)!=0) {
		LM_ERR("No counter name\n");
		return -1;
	}
	if (s_name.s == NULL || s_name.len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	str l1_str, l2_str, l3_str;
	if (l1 != NULL) {
		if (get_str_fparam(&l1_str, msg, (gparam_t*)l1)!=0) {
			LM_ERR("No label l1 in counter\n");
			return -1;
		}
		if (l1_str.s == NULL || l1_str.len == 0) {
			LM_ERR("Invalid l1 string\n");
			return -1;
		}

		if (l2 != NULL) {
			if (get_str_fparam(&l2_str, msg, (gparam_t*)l2)!=0) {
				LM_ERR("No label l2 in counter\n");
				return -1;
			}
			if (l2_str.s == NULL || l2_str.len == 0) {
				LM_ERR("Invalid l2 string\n");
				return -1;
			}

			if (l3 != NULL) {
				if (get_str_fparam(&l3_str, msg, (gparam_t*)l3)!=0) {
					LM_ERR("No label l3 in counter\n");
					return -1;
				}
				if (l3_str.s == NULL || l3_str.len == 0) {
					LM_ERR("Invalid l3 string\n");
					return -1;
				}
			} /* if l3 != NULL */
			
		} else {
			l3 = NULL;
		} /* if l2 != NULL */
		
	} else {
		l2 = NULL;
		l3 = NULL;
	} /* if l1 != NULL */
	
	if (prom_counter_reset(&s_name,
						   (l1!=NULL)?&l1_str:NULL,
						   (l2!=NULL)?&l2_str:NULL,
						   (l3!=NULL)?&l3_str:NULL
			)) {
		LM_ERR("Cannot reset counter: %.*s\n", s_name.len, s_name.s);
		return -1;
	}

	LM_DBG("Counter %.*s reset\n", s_name.len, s_name.s);
	return 1;
}

/**
 * Reset a counter (no labels)
 */
static int w_prom_counter_reset_l0(struct sip_msg* msg, char* pname)
{
  return w_prom_counter_reset(msg, pname, NULL, NULL, NULL);
}

/**
 * Reset a counter (one label)
 */
static int w_prom_counter_reset_l1(struct sip_msg* msg, char* pname, char *l1)
{
  return w_prom_counter_reset(msg, pname, l1, NULL, NULL);
}

/**
 * Reset a counter (two labels)
 */
static int w_prom_counter_reset_l2(struct sip_msg* msg, char* pname, char *l1, char *l2)
{
  return w_prom_counter_reset(msg, pname, l1, l2, NULL);
}

/**
 * Reset a counter (three labels)
 */
static int w_prom_counter_reset_l3(struct sip_msg* msg, char* pname, char *l1, char *l2,
	char *l3)
{
  return w_prom_counter_reset(msg, pname, l1, l2, l3);
}

/**
 * Reset a gauge (No labels)
 */
static int ki_xhttp_prom_gauge_reset_l0(struct sip_msg* msg, str *s_name)
{
	if (s_name == NULL || s_name->s == NULL || s_name->len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if (prom_gauge_reset(s_name, NULL, NULL, NULL)) {
		LM_ERR("Cannot reset gauge: %.*s\n", s_name->len, s_name->s);
		return -1;
	}

	LM_DBG("Gauge %.*s reset\n", s_name->len, s_name->s);
	return 1;
}

/**
 * Reset a gauge (1 label)
 */
static int ki_xhttp_prom_gauge_reset_l1(struct sip_msg* msg, str *s_name, str *l1)
{
	if (s_name == NULL || s_name->s == NULL || s_name->len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if (l1 == NULL || l1->s == NULL || l1->len == 0) {
		LM_ERR("Invalid l1 string\n");
		return -1;
	}

	if (prom_gauge_reset(s_name, l1, NULL, NULL)) {
		LM_ERR("Cannot reset gauge: %.*s (%.*s)\n", s_name->len, s_name->s,
			   l1->len, l1->s);
		return -1;
	}

	LM_DBG("Gauge %.*s (%.*s) reset\n", s_name->len, s_name->s,
		   l1->len, l1->s);

	return 1;
}

/**
 * Reset a gauge (2 labels)
 */
static int ki_xhttp_prom_gauge_reset_l2(struct sip_msg* msg, str *s_name, str *l1, str *l2)
{
	if (s_name == NULL || s_name->s == NULL || s_name->len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if (l1 == NULL || l1->s == NULL || l1->len == 0) {
		LM_ERR("Invalid l1 string\n");
		return -1;
	}

	if (l2 == NULL || l2->s == NULL || l2->len == 0) {
		LM_ERR("Invalid l2 string\n");
		return -1;
	}

	if (prom_gauge_reset(s_name, l1, l2, NULL)) {
		LM_ERR("Cannot reset gauge: %.*s (%.*s, %.*s)\n", s_name->len, s_name->s,
			   l1->len, l1->s,
			   l2->len, l2->s
			);
		return -1;
	}

	LM_DBG("Gauge %.*s (%.*s, %.*s) reset\n", s_name->len, s_name->s,
		   l1->len, l1->s,
		   l2->len, l2->s
		);

	return 1;
}

/**
 * Reset a gauge (3 labels)
 */
static int ki_xhttp_prom_gauge_reset_l3(struct sip_msg* msg, str *s_name, str *l1, str *l2,
										  str *l3)
{
	if (s_name == NULL || s_name->s == NULL || s_name->len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if (l1 == NULL || l1->s == NULL || l1->len == 0) {
		LM_ERR("Invalid l1 string\n");
		return -1;
	}

	if (l2 == NULL || l2->s == NULL || l2->len == 0) {
		LM_ERR("Invalid l2 string\n");
		return -1;
	}

	if (l3 == NULL || l3->s == NULL || l3->len == 0) {
		LM_ERR("Invalid l3 string\n");
		return -1;
	}

	if (prom_gauge_reset(s_name, l1, l2, l3)) {
		LM_ERR("Cannot reset gauge: %.*s (%.*s, %.*s, %.*s)\n", s_name->len, s_name->s,
			   l1->len, l1->s,
			   l2->len, l2->s,
			   l3->len, l3->s
			);
		return -1;
	}

	LM_DBG("Gauge %.*s (%.*s, %.*s, %.*s) reset\n", s_name->len, s_name->s,
		   l1->len, l1->s,
		   l2->len, l2->s,
		   l3->len, l3->s
		);

	return 1;
}

/**
 * Reset a gauge.
 */
static int w_prom_gauge_reset(struct sip_msg* msg, char* pname, char *l1, char *l2,
								char *l3)
{
	str s_name;

	if (pname == NULL) {
		LM_ERR("Invalid parameter\n");
		return -1;
	}

	if (get_str_fparam(&s_name, msg, (gparam_t*)pname)!=0) {
		LM_ERR("No gauge name\n");
		return -1;
	}
	if (s_name.s == NULL || s_name.len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	str l1_str, l2_str, l3_str;
	if (l1 != NULL) {
		if (get_str_fparam(&l1_str, msg, (gparam_t*)l1)!=0) {
			LM_ERR("No label l1 in gauge\n");
			return -1;
		}
		if (l1_str.s == NULL || l1_str.len == 0) {
			LM_ERR("Invalid l1 string\n");
			return -1;
		}

		if (l2 != NULL) {
			if (get_str_fparam(&l2_str, msg, (gparam_t*)l2)!=0) {
				LM_ERR("No label l2 in gauge\n");
				return -1;
			}
			if (l2_str.s == NULL || l2_str.len == 0) {
				LM_ERR("Invalid l2 string\n");
				return -1;
			}

			if (l3 != NULL) {
				if (get_str_fparam(&l3_str, msg, (gparam_t*)l3)!=0) {
					LM_ERR("No label l3 in gauge\n");
					return -1;
				}
				if (l3_str.s == NULL || l3_str.len == 0) {
					LM_ERR("Invalid l3 string\n");
					return -1;
				}
			} /* if l3 != NULL */
			
		} else {
			l3 = NULL;
		} /* if l2 != NULL */
		
	} else {
		l2 = NULL;
		l3 = NULL;
	} /* if l1 != NULL */
	
	if (prom_gauge_reset(&s_name,
						 (l1!=NULL)?&l1_str:NULL,
						 (l2!=NULL)?&l2_str:NULL,
						 (l3!=NULL)?&l3_str:NULL
			)) {
		LM_ERR("Cannot reset gauge: %.*s\n", s_name.len, s_name.s);
		return -1;
	}

	LM_DBG("Gauge %.*s reset\n", s_name.len, s_name.s);
	return 1;
}

/**
 * Reset a gauge (no labels)
 */
static int w_prom_gauge_reset_l0(struct sip_msg* msg, char* pname)
{
  return w_prom_gauge_reset(msg, pname, NULL, NULL, NULL);
}

/**
 * Reset a gauge (one label)
 */
static int w_prom_gauge_reset_l1(struct sip_msg* msg, char* pname, char *l1)
{
  return w_prom_gauge_reset(msg, pname, l1, NULL, NULL);
}

/**
 * Reset a gauge (two labels)
 */
static int w_prom_gauge_reset_l2(struct sip_msg* msg, char* pname, char *l1, char *l2)
{
  return w_prom_gauge_reset(msg, pname, l1, l2, NULL);
}

/**
 * Reset a gauge (three labels)
 */
static int w_prom_gauge_reset_l3(struct sip_msg* msg, char* pname, char *l1, char *l2,
	char *l3)
{
  return w_prom_gauge_reset(msg, pname, l1, l2, l3);
}

static int fixup_counter_inc(void** param, int param_no)
{
	if (param_no == 1 || param_no == 2) {
		return fixup_spve_igp(param, param_no);
	} else {
		return fixup_spve_null(param, 1);
	}
}

/* static int fixup_free_counter_inc(void** param, int param_no) */
/* { */
/* 	if (param_no == 1 || param_no == 2) { */
/* 		return fixup_free_spve_igp(param, param_no); */
/* 	} else { */
/* 		return fixup_free_spve_null(param, 1); */
/* 	} */
/* } */

/**
 * Add an integer to a counter (No labels).
 */
static int ki_xhttp_prom_counter_inc_l0(struct sip_msg* msg, str *s_name, int number)
{
	if (s_name == NULL || s_name->s == NULL || s_name->len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if(number < 0) {
		LM_ERR("invalid negative number parameter\n");
		return -1;
	}

	if (prom_counter_inc(s_name, number, NULL, NULL, NULL)) {
		LM_ERR("Cannot add number: %d to counter: %.*s\n", number, s_name->len, s_name->s);
		return -1;
	}

	LM_DBG("Added %d to counter %.*s\n", number, s_name->len, s_name->s);
	return 1;
}

/**
 * Add an integer to a counter (1 label).
 */
static int ki_xhttp_prom_counter_inc_l1(struct sip_msg* msg, str *s_name, int number, str *l1)
{
	if (s_name == NULL || s_name->s == NULL || s_name->len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if(number < 0) {
		LM_ERR("invalid negative number parameter\n");
		return -1;
	}

	if (l1 == NULL || l1->s == NULL || l1->len == 0) {
		LM_ERR("Invalid l1 string\n");
		return -1;
	}

	if (prom_counter_inc(s_name, number, l1, NULL, NULL)) {
		LM_ERR("Cannot add number: %d to counter: %.*s (%.*s)\n",
			   number, s_name->len, s_name->s,
			   l1->len, l1->s
			);
		return -1;
	}

	LM_DBG("Added %d to counter %.*s (%.*s)\n", number,
		   s_name->len, s_name->s,
		   l1->len, l1->s
		);

	return 1;
}

/**
 * Add an integer to a counter (2 labels).
 */
static int ki_xhttp_prom_counter_inc_l2(struct sip_msg* msg, str *s_name, int number,
										str *l1, str *l2)
{
	if (s_name == NULL || s_name->s == NULL || s_name->len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if(number < 0) {
		LM_ERR("invalid negative number parameter\n");
		return -1;
	}

	if (l1 == NULL || l1->s == NULL || l1->len == 0) {
		LM_ERR("Invalid l1 string\n");
		return -1;
	}

	if (l2 == NULL || l2->s == NULL || l2->len == 0) {
		LM_ERR("Invalid l2 string\n");
		return -1;
	}

	if (prom_counter_inc(s_name, number, l1, l2, NULL)) {
		LM_ERR("Cannot add number: %d to counter: %.*s (%.*s, %.*s)\n",
			   number, s_name->len, s_name->s,
			   l1->len, l1->s,
			   l2->len, l2->s
			);
		return -1;
	}

	LM_DBG("Added %d to counter %.*s (%.*s, %.*s)\n", number,
		   s_name->len, s_name->s,
		   l1->len, l1->s,
		   l2->len, l2->s
		);

	return 1;
}

/**
 * Add an integer to a counter (3 labels).
 */
static int ki_xhttp_prom_counter_inc_l3(struct sip_msg* msg, str *s_name, int number,
										str *l1, str *l2, str *l3)
{
	if (s_name == NULL || s_name->s == NULL || s_name->len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if(number < 0) {
		LM_ERR("invalid negative number parameter\n");
		return -1;
	}

	if (l1 == NULL || l1->s == NULL || l1->len == 0) {
		LM_ERR("Invalid l1 string\n");
		return -1;
	}

	if (l2 == NULL || l2->s == NULL || l2->len == 0) {
		LM_ERR("Invalid l2 string\n");
		return -1;
	}

	if (l3 == NULL || l3->s == NULL || l3->len == 0) {
		LM_ERR("Invalid l3 string\n");
		return -1;
	}

	if (prom_counter_inc(s_name, number, l1, l2, l3)) {
		LM_ERR("Cannot add number: %d to counter: %.*s (%.*s, %.*s, %.*s)\n",
			   number, s_name->len, s_name->s,
			   l1->len, l1->s,
			   l2->len, l2->s,
			   l3->len, l3->s
			);
		return -1;
	}

	LM_DBG("Added %d to counter %.*s (%.*s, %.*s, %.*s)\n", number,
		   s_name->len, s_name->s,
		   l1->len, l1->s,
		   l2->len, l2->s,
		   l3->len, l3->s
		);

	return 1;
}

/**
 * Add an integer to a counter.
 */
static int w_prom_counter_inc(struct sip_msg* msg, char *pname, char* pnumber,
							  char *l1, char *l2, char *l3)
{
	int number;
	str s_name;

	if (pname == NULL || pnumber == 0) {
		LM_ERR("Invalid parameters\n");
		return -1;
	}

	if (get_str_fparam(&s_name, msg, (gparam_t*)pname)!=0) {
		LM_ERR("No counter name\n");
		return -1;
	}
	if (s_name.s == NULL || s_name.len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if(get_int_fparam(&number, msg, (gparam_p)pnumber)!=0) {
		LM_ERR("no number\n");
		return -1;
	}
	if(number < 0) {
		LM_ERR("invalid negative number parameter\n");
		return -1;
	}

	str l1_str, l2_str, l3_str;
	if (l1 != NULL) {
		if (get_str_fparam(&l1_str, msg, (gparam_t*)l1)!=0) {
			LM_ERR("No label l1 in counter\n");
			return -1;
		}
		if (l1_str.s == NULL || l1_str.len == 0) {
			LM_ERR("Invalid l1 string\n");
			return -1;
		}

		if (l2 != NULL) {
			if (get_str_fparam(&l2_str, msg, (gparam_t*)l2)!=0) {
				LM_ERR("No label l2 in counter\n");
				return -1;
			}
			if (l2_str.s == NULL || l2_str.len == 0) {
				LM_ERR("Invalid l2 string\n");
				return -1;
			}

			if (l3 != NULL) {
				if (get_str_fparam(&l3_str, msg, (gparam_t*)l3)!=0) {
					LM_ERR("No label l3 in counter\n");
					return -1;
				}
				if (l3_str.s == NULL || l3_str.len == 0) {
					LM_ERR("Invalid l3 string\n");
					return -1;
				}
			} /* if l3 != NULL */
			
		} else {
			l3 = NULL;
		} /* if l2 != NULL */
		
	} else {
		l2 = NULL;
		l3 = NULL;
	} /* if l1 != NULL */

	if (prom_counter_inc(&s_name, number,
						   (l1!=NULL)?&l1_str:NULL,
						   (l2!=NULL)?&l2_str:NULL,
						   (l3!=NULL)?&l3_str:NULL
						 )) {
		LM_ERR("Cannot add number: %d to counter: %.*s\n", number, s_name.len, s_name.s);
		return -1;
	}

	LM_DBG("Added %d to counter %.*s\n", number, s_name.len, s_name.s);
	return 1;
}

/**
 * Add an integer to a counter (no labels)
 */
static int w_prom_counter_inc_l0(struct sip_msg* msg, char *pname, char* pnumber)
{
	return w_prom_counter_inc(msg, pname, pnumber, NULL, NULL, NULL);
}

/**
 * Add an integer to a counter (1 labels)
 */
static int w_prom_counter_inc_l1(struct sip_msg* msg, char *pname, char* pnumber,
								 char *l1)
{
	return w_prom_counter_inc(msg, pname, pnumber, l1, NULL, NULL);
}

/**
 * Add an integer to a counter (2 labels)
 */
static int w_prom_counter_inc_l2(struct sip_msg* msg, char *pname, char* pnumber,
								 char *l1, char *l2)
{
	return w_prom_counter_inc(msg, pname, pnumber, l1, l2, NULL);
}

/**
 * Add an integer to a counter (3 labels)
 */
static int w_prom_counter_inc_l3(struct sip_msg* msg, char *pname, char* pnumber,
								 char *l1, char *l2, char *l3)
{
	return w_prom_counter_inc(msg, pname, pnumber, l1, l2, l3);
}

/**
 * Parse a string and convert to double.
 *
 * /param s_number pointer to number string.
 * /param number double passed as reference.
 *
 * /return 0 on success.
 * On error value pointed by pnumber is undefined.
 */
static int double_parse_str(str *s_number, double *pnumber)
{
	char *s = NULL;
	
	if (!s_number || !s_number->s || s_number->len == 0) {
		LM_ERR("Bad s_number to convert to double\n");
		goto error;
	}

	if (!pnumber) {
		LM_ERR("No double passed by reference\n");
		goto error;
	}

	/* We generate a zero terminated string. */

	/* We set last character to zero to get a zero terminated string. */
	int len = s_number->len;
	s = pkg_malloc(len + 1);
	if (!s) {
		LM_ERR("Out of pkg memory\n");
		goto error;
	}
	memcpy(s, s_number->s, len);
	s[len] = '\0'; /* Zero terminated string. */

	/* atof function does not check for errors. */
	double num = atof(s);
	LM_DBG("double number (%.*s) -> %f\n", len, s, num);

	*pnumber = num;
	pkg_free(s);
	return 0;

error:
	if (s) {
		pkg_free(s);
	}
	return -1;
}

/**
 * Set a number to a gauge (No labels).
 */
static int ki_xhttp_prom_gauge_set_l0(struct sip_msg* msg, str *s_name, str *s_number)
{
	if (s_name == NULL || s_name->s == NULL || s_name->len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if (s_number == NULL || s_number->s == NULL || s_number->len == 0) {
		LM_ERR("Invalid number string\n");
		return -1;
	}

	double number;
	if (double_parse_str(s_number, &number)) {
		LM_ERR("Cannot parse double\n");
		return -1;
	}

	if (prom_gauge_set(s_name, number, NULL, NULL, NULL)) {
		LM_ERR("Cannot assign number: %f to gauge: %.*s\n", number, s_name->len, s_name->s);
		return -1;
	}

	LM_DBG("Assigned %f to gauge %.*s\n", number, s_name->len, s_name->s);
	return 1;
}

/**
 * Assign a number to a gauge (1 label).
 */
static int ki_xhttp_prom_gauge_set_l1(struct sip_msg* msg, str *s_name, str *s_number, str *l1)
{
	if (s_name == NULL || s_name->s == NULL || s_name->len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if (s_number == NULL || s_number->s == NULL || s_number->len == 0) {
		LM_ERR("Invalid number string\n");
		return -1;
	}

	double number;
	if (double_parse_str(s_number, &number)) {
		LM_ERR("Cannot parse double\n");
		return -1;
	}

	if (l1 == NULL || l1->s == NULL || l1->len == 0) {
		LM_ERR("Invalid l1 string\n");
		return -1;
	}

	if (prom_gauge_set(s_name, number, l1, NULL, NULL)) {
		LM_ERR("Cannot assign number: %f to gauge: %.*s (%.*s)\n",
			   number, s_name->len, s_name->s,
			   l1->len, l1->s
			);
		return -1;
	}

	LM_DBG("Assign %f to gauge %.*s (%.*s)\n", number,
		   s_name->len, s_name->s,
		   l1->len, l1->s
		);
	return 1;
}

/**
 * Assign a number to a gauge (2 labels).
 */
static int ki_xhttp_prom_gauge_set_l2(struct sip_msg* msg, str *s_name, str *s_number,
									  str *l1, str *l2)
{
	if (s_name == NULL || s_name->s == NULL || s_name->len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if (s_number == NULL || s_number->s == NULL || s_number->len == 0) {
		LM_ERR("Invalid number string\n");
		return -1;
	}

	double number;
	if (double_parse_str(s_number, &number)) {
		LM_ERR("Cannot parse double\n");
		return -1;
	}

	if (l1 == NULL || l1->s == NULL || l1->len == 0) {
		LM_ERR("Invalid l1 string\n");
		return -1;
	}

	if (l2 == NULL || l2->s == NULL || l2->len == 0) {
		LM_ERR("Invalid l2 string\n");
		return -1;
	}

	if (prom_gauge_set(s_name, number, l1, l2, NULL)) {
		LM_ERR("Cannot assign number: %f to gauge: %.*s (%.*s, %.*s)\n",
			   number, s_name->len, s_name->s,
			   l1->len, l1->s,
			   l2->len, l2->s
			);
		return -1;
	}

	LM_DBG("Assign %f to gauge %.*s (%.*s, %.*s)\n", number,
		   s_name->len, s_name->s,
		   l1->len, l1->s,
		   l2->len, l2->s
		);

	return 1;
}

/**
 * Assign a number to a gauge (3 labels).
 */
static int ki_xhttp_prom_gauge_set_l3(struct sip_msg* msg, str *s_name, str *s_number,
									  str *l1, str *l2, str *l3)
{
	if (s_name == NULL || s_name->s == NULL || s_name->len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if (s_number == NULL || s_number->s == NULL || s_number->len == 0) {
		LM_ERR("Invalid number string\n");
		return -1;
	}

	double number;
	if (double_parse_str(s_number, &number)) {
		LM_ERR("Cannot parse double\n");
		return -1;
	}

	if (l1 == NULL || l1->s == NULL || l1->len == 0) {
		LM_ERR("Invalid l1 string\n");
		return -1;
	}

	if (l2 == NULL || l2->s == NULL || l2->len == 0) {
		LM_ERR("Invalid l2 string\n");
		return -1;
	}

	if (l3 == NULL || l3->s == NULL || l3->len == 0) {
		LM_ERR("Invalid l3 string\n");
		return -1;
	}

	if (prom_gauge_set(s_name, number, l1, l2, l3)) {
		LM_ERR("Cannot assign number: %f to gauge: %.*s (%.*s, %.*s, %.*s)\n",
			   number, s_name->len, s_name->s,
			   l1->len, l1->s,
			   l2->len, l2->s,
			   l3->len, l3->s
			);
		return -1;
	}

	LM_DBG("Assign %f to gauge %.*s (%.*s, %.*s, %.*s)\n", number,
		   s_name->len, s_name->s,
		   l1->len, l1->s,
		   l2->len, l2->s,
		   l3->len, l3->s
		);

	return 1;
}

/**
 * Assign a number to a gauge.
 */
static int w_prom_gauge_set(struct sip_msg* msg, char *pname, char* pnumber,
							char *l1, char *l2, char *l3)
{
    str s_number;
	str s_name;

	if (pname == NULL || pnumber == 0) {
		LM_ERR("Invalid parameters\n");
		return -1;
	}

	if (get_str_fparam(&s_name, msg, (gparam_t*)pname)!=0) {
		LM_ERR("No gauge name\n");
		return -1;
	}
	if (s_name.s == NULL || s_name.len == 0) {
		LM_ERR("Invalid name string\n");
		return -1;
	}

	if (get_str_fparam(&s_number, msg, (gparam_t*)pnumber)!=0) {
		LM_ERR("No gauge number\n");
		return -1;
	}
	if (s_number.s == NULL || s_number.len == 0) {
		LM_ERR("Invalid number string\n");
		return -1;
	}

	double number;
	if (double_parse_str(&s_number, &number)) {
		LM_ERR("Cannot parse double\n");
		return -1;
	}

	str l1_str, l2_str, l3_str;
	if (l1 != NULL) {
		if (get_str_fparam(&l1_str, msg, (gparam_t*)l1)!=0) {
			LM_ERR("No label l1 in counter\n");
			return -1;
		}
		if (l1_str.s == NULL || l1_str.len == 0) {
			LM_ERR("Invalid l1 string\n");
			return -1;
		}

		if (l2 != NULL) {
			if (get_str_fparam(&l2_str, msg, (gparam_t*)l2)!=0) {
				LM_ERR("No label l2 in counter\n");
				return -1;
			}
			if (l2_str.s == NULL || l2_str.len == 0) {
				LM_ERR("Invalid l2 string\n");
				return -1;
			}

			if (l3 != NULL) {
				if (get_str_fparam(&l3_str, msg, (gparam_t*)l3)!=0) {
					LM_ERR("No label l3 in counter\n");
					return -1;
				}
				if (l3_str.s == NULL || l3_str.len == 0) {
					LM_ERR("Invalid l3 string\n");
					return -1;
				}
			} /* if l3 != NULL */
			
		} else {
			l3 = NULL;
		} /* if l2 != NULL */
		
	} else {
		l2 = NULL;
		l3 = NULL;
	} /* if l1 != NULL */

	if (prom_gauge_set(&s_name, number,
					   (l1!=NULL)?&l1_str:NULL,
					   (l2!=NULL)?&l2_str:NULL,
					   (l3!=NULL)?&l3_str:NULL
			)) {
		LM_ERR("Cannot assign number: %f to gauge: %.*s\n", number, s_name.len, s_name.s);
		return -1;
	}

	LM_DBG("Assign %f to gauge %.*s\n", number, s_name.len, s_name.s);
	return 1;
}

/**
 * Assign a number to a gauge (no labels)
 */
static int w_prom_gauge_set_l0(struct sip_msg* msg, char *pname, char* pnumber)
{
	return w_prom_gauge_set(msg, pname, pnumber, NULL, NULL, NULL);
}

/**
 * Assign a number to a gauge (1 labels)
 */
static int w_prom_gauge_set_l1(struct sip_msg* msg, char *pname, char* pnumber,
							   char *l1)
{
	return w_prom_gauge_set(msg, pname, pnumber, l1, NULL, NULL);
}

/**
 * Assign a number to a gauge (2 labels)
 */
static int w_prom_gauge_set_l2(struct sip_msg* msg, char *pname, char* pnumber,
							   char *l1, char *l2)
{
	return w_prom_gauge_set(msg, pname, pnumber, l1, l2, NULL);
}

/**
 * Assign a number to a gauge (3 labels)
 */
static int w_prom_gauge_set_l3(struct sip_msg* msg, char *pname, char* pnumber,
							   char *l1, char *l2, char *l3)
{
	return w_prom_gauge_set(msg, pname, pnumber, l1, l2, l3);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_xhttp_prom_exports[] = {
	{ str_init("xhttp_prom"), str_init("dispatch"),
		SR_KEMIP_INT, ki_xhttp_prom_dispatch,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("check_uri"),
		SR_KEMIP_INT, ki_xhttp_prom_check_uri,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("counter_reset_l0"),
	    SR_KEMIP_INT, ki_xhttp_prom_counter_reset_l0,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("counter_reset_l1"),
	    SR_KEMIP_INT, ki_xhttp_prom_counter_reset_l1,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("counter_reset_l2"),
	    SR_KEMIP_INT, ki_xhttp_prom_counter_reset_l2,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("counter_reset_l3"),
	    SR_KEMIP_INT, ki_xhttp_prom_counter_reset_l3,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("gauge_reset_l0"),
	    SR_KEMIP_INT, ki_xhttp_prom_gauge_reset_l0,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("gauge_reset_l1"),
	    SR_KEMIP_INT, ki_xhttp_prom_gauge_reset_l1,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("gauge_reset_l2"),
	    SR_KEMIP_INT, ki_xhttp_prom_gauge_reset_l2,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("gauge_reset_l3"),
	    SR_KEMIP_INT, ki_xhttp_prom_gauge_reset_l3,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("counter_inc_l0"),
	    SR_KEMIP_INT, ki_xhttp_prom_counter_inc_l0,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("counter_inc_l1"),
	    SR_KEMIP_INT, ki_xhttp_prom_counter_inc_l1,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("counter_inc_l2"),
	    SR_KEMIP_INT, ki_xhttp_prom_counter_inc_l2,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("counter_inc_l3"),
	    SR_KEMIP_INT, ki_xhttp_prom_counter_inc_l3,
		{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("gauge_set_l0"),
	    SR_KEMIP_INT, ki_xhttp_prom_gauge_set_l0,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("gauge_set_l1"),
	    SR_KEMIP_INT, ki_xhttp_prom_gauge_set_l1,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("gauge_set_l2"),
	    SR_KEMIP_INT, ki_xhttp_prom_gauge_set_l2,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("xhttp_prom"), str_init("gauge_set_l3"),
	    SR_KEMIP_INT, ki_xhttp_prom_gauge_set_l3,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_xhttp_prom_exports);
	return 0;
}

/* RPC commands. */

/* NOTE: I rename ctx variable in rpc functions to avoid collision with global ctx. */

static void rpc_prom_counter_reset(rpc_t *rpc, void *ct)
{
	str s_name;

	if (rpc->scan(ct, "S", &s_name) < 1) {
		rpc->fault(ct, 400, "required counter identifier");
		return;
	}

	if (s_name.len == 0 || s_name.s == NULL) {
		rpc->fault(ct, 400, "invalid counter identifier");
		return;
	}

	str l1, l2, l3;
	int res;
	res = rpc->scan(ct, "*SSS", &l1, &l2, &l3);
	if (res == 0) {
		/* No labels */
		if (prom_counter_reset(&s_name, NULL, NULL, NULL)) {
			LM_ERR("Cannot reset counter: %.*s\n", s_name.len, s_name.s);
			rpc->fault(ct, 500, "Failed to reset counter: %.*s", s_name.len, s_name.s);
			return;
		}
		LM_DBG("Counter reset: (%.*s)\n", s_name.len, s_name.s);
		
	} else if (res == 1) {
		if (prom_counter_reset(&s_name, &l1, NULL, NULL)) {
			LM_ERR("Cannot reset counter: %.*s (%.*s)\n", s_name.len, s_name.s,
				   l1.len, l1.s);
			rpc->fault(ct, 500, "Failed to reset counter: %.*s (%.*s)", s_name.len, s_name.s,
					   l1.len, l1.s);
			return;
		}
		LM_DBG("Counter reset: %.*s (%.*s)\n", s_name.len, s_name.s,
			   l1.len, l1.s);

	} else if (res == 2) {
		if (prom_counter_reset(&s_name, &l1, &l2, NULL)) {
			LM_ERR("Cannot reset counter: %.*s (%.*s, %.*s)\n", s_name.len, s_name.s,
				   l1.len, l1.s,
				   l2.len, l2.s);
			rpc->fault(ct, 500, "Failed to reset counter: %.*s (%.*s, %.*s)",
					   s_name.len, s_name.s,
					   l1.len, l1.s,
					   l2.len, l2.s
				);
			return;
		}
		LM_DBG("Counter reset: %.*s (%.*s, %.*s)\n", s_name.len, s_name.s,
			   l1.len, l1.s,
			   l2.len, l2.s);

	} else if (res == 3) {
		if (prom_counter_reset(&s_name, &l1, &l2, &l3)) {
			LM_ERR("Cannot reset counter: %.*s (%.*s, %.*s, %.*s)\n", s_name.len, s_name.s,
				   l1.len, l1.s,
				   l2.len, l2.s,
				   l3.len, l3.s
				);
			rpc->fault(ct, 500, "Failed to reset counter: %.*s (%.*s, %.*s, %.*s)",
					   s_name.len, s_name.s,
					   l1.len, l1.s,
					   l2.len, l2.s,
					   l3.len, l3.s
				);
			return;
		}
		LM_DBG("Counter reset: %.*s (%.*s, %.*s, %.*s)\n", s_name.len, s_name.s,
			   l1.len, l1.s,
			   l2.len, l2.s,
			   l3.len, l3.s
			);

	} else {
		LM_ERR("Strange return value: %d\n", res);
		rpc->fault(ct, 500, "Strange return value: %d", res);

	} /* if res == 0 */
		
	return;
}

static void rpc_prom_counter_inc(rpc_t *rpc, void *ct)
{
	str s_name;

	if (rpc->scan(ct, "S", &s_name) < 1) {
		rpc->fault(ct, 400, "required counter identifier");
		return;
	}

	if (s_name.len == 0 || s_name.s == NULL) {
		rpc->fault(ct, 400, "invalid counter identifier");
		return;
	}

	int number;
	if (rpc->scan(ct, "d", &number) < 1) {
		rpc->fault(ct, 400, "required number argument");
		return;
	}
	if(number < 0) {
		LM_ERR("invalid negative number parameter\n");
		return;
	}

	str l1, l2, l3;
	int res;
	res = rpc->scan(ct, "*SSS", &l1, &l2, &l3);
	if (res == 0) {
		/* No labels */
		if (prom_counter_inc(&s_name, number, NULL, NULL, NULL)) {
			LM_ERR("Cannot add %d to counter: %.*s\n", number, s_name.len, s_name.s);
			rpc->fault(ct, 500, "Failed to add %d to counter: %.*s", number,
					   s_name.len, s_name.s);
			return;
		}
		LM_DBG("Added %d to counter: (%.*s)\n", number, s_name.len, s_name.s);
		
	} else if (res == 1) {
		if (prom_counter_inc(&s_name, number, &l1, NULL, NULL)) {
			LM_ERR("Cannot add %d to counter: %.*s (%.*s)\n", number, s_name.len, s_name.s,
				   l1.len, l1.s);
			rpc->fault(ct, 500, "Failed to add %d to counter: %.*s (%.*s)",
					   number, s_name.len, s_name.s,
					   l1.len, l1.s);
			return;
		}
		LM_DBG("Added %d to counter: %.*s (%.*s)\n", number, s_name.len, s_name.s,
			   l1.len, l1.s);

	} else if (res == 2) {
		if (prom_counter_inc(&s_name, number, &l1, &l2, NULL)) {
			LM_ERR("Cannot add %d to counter: %.*s (%.*s, %.*s)\n", number,
				   s_name.len, s_name.s,
				   l1.len, l1.s,
				   l2.len, l2.s);
			rpc->fault(ct, 500, "Failed to add %d to counter: %.*s (%.*s, %.*s)",
					   number, s_name.len, s_name.s,
					   l1.len, l1.s,
					   l2.len, l2.s
				);
			return;
		}
		LM_DBG("Added %d to counter: %.*s (%.*s, %.*s)\n", number, s_name.len, s_name.s,
			   l1.len, l1.s,
			   l2.len, l2.s);

	} else if (res == 3) {
		if (prom_counter_inc(&s_name, number, &l1, &l2, &l3)) {
			LM_ERR("Cannot add %d to counter: %.*s (%.*s, %.*s, %.*s)\n",
				   number, s_name.len, s_name.s,
				   l1.len, l1.s,
				   l2.len, l2.s,
				   l3.len, l3.s
				);
			rpc->fault(ct, 500, "Failed to add %d to counter: %.*s (%.*s, %.*s, %.*s)",
					   number, s_name.len, s_name.s,
					   l1.len, l1.s,
					   l2.len, l2.s,
					   l3.len, l3.s
				);
			return;
		}
		LM_DBG("Added %d to counter: %.*s (%.*s, %.*s, %.*s)\n", number, s_name.len, s_name.s,
			   l1.len, l1.s,
			   l2.len, l2.s,
			   l3.len, l3.s
			);

	} else {
		LM_ERR("Strange return value: %d\n", res);
		rpc->fault(ct, 500, "Strange return value: %d", res);

	} /* if res == 0 */

	return;
}

static void rpc_prom_gauge_reset(rpc_t *rpc, void *ct)
{
	str s_name;

	if (rpc->scan(ct, "S", &s_name) < 1) {
		rpc->fault(ct, 400, "required gauge identifier");
		return;
	}

	if (s_name.len == 0 || s_name.s == NULL) {
		rpc->fault(ct, 400, "invalid gauge identifier");
		return;
	}

	str l1, l2, l3;
	int res;
	res = rpc->scan(ct, "*SSS", &l1, &l2, &l3);
	if (res == 0) {
		/* No labels */
		if (prom_gauge_reset(&s_name, NULL, NULL, NULL)) {
			LM_ERR("Cannot reset gauge: %.*s\n", s_name.len, s_name.s);
			rpc->fault(ct, 500, "Failed to reset gauge: %.*s", s_name.len, s_name.s);
			return;
		}
		LM_DBG("Gauge reset: (%.*s)\n", s_name.len, s_name.s);
		
	} else if (res == 1) {
		if (prom_gauge_reset(&s_name, &l1, NULL, NULL)) {
			LM_ERR("Cannot reset gauge: %.*s (%.*s)\n", s_name.len, s_name.s,
				   l1.len, l1.s);
			rpc->fault(ct, 500, "Failed to reset gauge: %.*s (%.*s)", s_name.len, s_name.s,
					   l1.len, l1.s);
			return;
		}
		LM_DBG("Gauge reset: %.*s (%.*s)\n", s_name.len, s_name.s,
			   l1.len, l1.s);

	} else if (res == 2) {
		if (prom_gauge_reset(&s_name, &l1, &l2, NULL)) {
			LM_ERR("Cannot reset gauge: %.*s (%.*s, %.*s)\n", s_name.len, s_name.s,
				   l1.len, l1.s,
				   l2.len, l2.s);
			rpc->fault(ct, 500, "Failed to reset gauge: %.*s (%.*s, %.*s)",
					   s_name.len, s_name.s,
					   l1.len, l1.s,
					   l2.len, l2.s
				);
			return;
		}
		LM_DBG("Gauge reset: %.*s (%.*s, %.*s)\n", s_name.len, s_name.s,
			   l1.len, l1.s,
			   l2.len, l2.s);

	} else if (res == 3) {
		if (prom_gauge_reset(&s_name, &l1, &l2, &l3)) {
			LM_ERR("Cannot reset gauge: %.*s (%.*s, %.*s, %.*s)\n", s_name.len, s_name.s,
				   l1.len, l1.s,
				   l2.len, l2.s,
				   l3.len, l3.s
				);
			rpc->fault(ct, 500, "Failed to reset gauge: %.*s (%.*s, %.*s, %.*s)",
					   s_name.len, s_name.s,
					   l1.len, l1.s,
					   l2.len, l2.s,
					   l3.len, l3.s
				);
			return;
		}
		LM_DBG("Gauge reset: %.*s (%.*s, %.*s, %.*s)\n", s_name.len, s_name.s,
			   l1.len, l1.s,
			   l2.len, l2.s,
			   l3.len, l3.s
			);

	} else {
		LM_ERR("Strange return value: %d\n", res);
		rpc->fault(ct, 500, "Strange return value: %d", res);

	} /* if res == 0 */
		
	return;
}

static void rpc_prom_gauge_set(rpc_t *rpc, void *ct)
{
	str s_name;

	if (rpc->scan(ct, "S", &s_name) < 1) {
		rpc->fault(ct, 400, "required gauge identifier");
		return;
	}

	if (s_name.len == 0 || s_name.s == NULL) {
		rpc->fault(ct, 400, "invalid gauge identifier");
		return;
	}

	double number;
	if (rpc->scan(ct, "f", &number) < 1) {
		rpc->fault(ct, 400, "required number argument");
		return;
	}

	str l1, l2, l3;
	int res;
	res = rpc->scan(ct, "*SSS", &l1, &l2, &l3);
	if (res == 0) {
		/* No labels */
		if (prom_gauge_set(&s_name, number, NULL, NULL, NULL)) {
			LM_ERR("Cannot assign %f to gauge %.*s\n", number, s_name.len, s_name.s);
			rpc->fault(ct, 500, "Failed to assign %f gauge: %.*s", number,
					   s_name.len, s_name.s);
			return;
		}
		LM_DBG("Assigned %f to gauge (%.*s)\n", number, s_name.len, s_name.s);
		
	} else if (res == 1) {
		if (prom_gauge_set(&s_name, number, &l1, NULL, NULL)) {
			LM_ERR("Cannot assign %f to gauge %.*s (%.*s)\n", number, s_name.len, s_name.s,
				   l1.len, l1.s);
			rpc->fault(ct, 500, "Failed to assign %f to gauge: %.*s (%.*s)",
					   number, s_name.len, s_name.s,
					   l1.len, l1.s);
			return;
		}
		LM_DBG("Assigned %f to gauge: %.*s (%.*s)\n", number, s_name.len, s_name.s,
			   l1.len, l1.s);

	} else if (res == 2) {
		if (prom_gauge_set(&s_name, number, &l1, &l2, NULL)) {
			LM_ERR("Cannot assign %f to gauge: %.*s (%.*s, %.*s)\n", number,
				   s_name.len, s_name.s,
				   l1.len, l1.s,
				   l2.len, l2.s);
			rpc->fault(ct, 500, "Failed to assign %f to gauge: %.*s (%.*s, %.*s)",
					   number, s_name.len, s_name.s,
					   l1.len, l1.s,
					   l2.len, l2.s
				);
			return;
		}
		LM_DBG("Assigned %f to gauge: %.*s (%.*s, %.*s)\n", number, s_name.len, s_name.s,
			   l1.len, l1.s,
			   l2.len, l2.s);

	} else if (res == 3) {
		if (prom_gauge_set(&s_name, number, &l1, &l2, &l3)) {
			LM_ERR("Cannot assign %f to gauge: %.*s (%.*s, %.*s, %.*s)\n",
				   number, s_name.len, s_name.s,
				   l1.len, l1.s,
				   l2.len, l2.s,
				   l3.len, l3.s
				);
			rpc->fault(ct, 500, "Failed to assign %f to gauge: %.*s (%.*s, %.*s, %.*s)",
					   number, s_name.len, s_name.s,
					   l1.len, l1.s,
					   l2.len, l2.s,
					   l3.len, l3.s
				);
			return;
		}
		LM_DBG("Assigned %f to gauge: %.*s (%.*s, %.*s, %.*s)\n",
			   number, s_name.len, s_name.s,
			   l1.len, l1.s,
			   l2.len, l2.s,
			   l3.len, l3.s
			);

	} else {
		LM_ERR("Strange return value: %d\n", res);
		rpc->fault(ct, 500, "Strange return value: %d", res);

	} /* if res == 0 */

	return;
}

static void rpc_prom_metric_list_print(rpc_t *rpc, void *ct)
{
	/* We reuse ctx->reply for the occasion. */
	if (init_xhttp_prom_reply(&ctx) < 0) {
		goto clean;
	}

	if (prom_metric_list_print(&ctx)) {
		LM_ERR("Cannot print a list of metrics\n");
		goto clean;
	}

	/* Convert to zero terminated string. */
	struct xhttp_prom_reply* reply;
	reply = &(ctx.reply);
	reply->body.s[reply->body.len] = '\0';

	/* Print content of reply buffer. */
	if (rpc->rpl_printf(ct, reply->body.s) < 0) {
		LM_ERR("Error printing RPC response\n");
		goto clean;
	}
	
clean:

	xhttp_prom_reply_free(&ctx);
	
	return;
}

static const char* rpc_prom_counter_reset_doc[2] = {
	"Reset a counter based on its identifier",
	0
};

static const char* rpc_prom_counter_inc_doc[2] = {
	"Add a number (greater or equal to zero) to a counter based on its identifier",
	0
};

static const char* rpc_prom_gauge_reset_doc[2] = {
	"Reset a gauge based on its identifier",
	0
};

static const char* rpc_prom_gauge_set_doc[2] = {
	"Set a gauge to a number based on its identifier",
	0
};

static const char* rpc_prom_metric_list_print_doc[2] = {
	"Print a list showing all user defined metrics",
	0
};

static rpc_export_t rpc_cmds[] = {
	{"xhttp_prom.counter_reset", rpc_prom_counter_reset, rpc_prom_counter_reset_doc, 0},
	{"xhttp_prom.counter_inc", rpc_prom_counter_inc, rpc_prom_counter_inc_doc, 0},
	{"xhttp_prom.gauge_reset", rpc_prom_gauge_reset, rpc_prom_gauge_reset_doc, 0},
	{"xhttp_prom.gauge_set", rpc_prom_gauge_set, rpc_prom_gauge_set_doc, 0},
	{"xhttp_prom.metric_list_print", rpc_prom_metric_list_print, rpc_prom_metric_list_print_doc, 0},
	{0, 0, 0, 0}
};
