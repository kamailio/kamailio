/*
 * Copyright (C) 2012 VoIP Embedded, Inc.
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
 * \brief XHTTP_PI :: Module interface (main file)
 * \ingroup xhttp_pi
 * Module: \ref xhttp_pi
 *
 * This is the main file of xhttp_pi module which contains all the functions
 * related to http processing, as well as the module interface.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "../../trim.h"
#include "../../sr_module.h"
#include "../../nonsip_hooks.h"
#include "../../modules/xhttp/api.h"
#include "../../rpc_lookup.h"
#include "xhttp_pi.h"
#include "xhttp_pi_fnc.h"
#include "http_db_handler.h"

/** @addtogroup xhttp_pi
 * @ingroup modules
 * @{
 *
 * <h1>Overview of Operation</h1>
 * This module provides a web interface for DB provisioning web interface.
 * It is built on top of the xhttp API module.
 */


MODULE_VERSION

str XHTTP_PI_REASON_OK = str_init("OK");
str XHTTP_PI_CONTENT_TYPE_TEXT_HTML = str_init("text/html");


extern ph_framework_t *ph_framework_data;


static int mod_init(void);
static int destroy(void);
static int xhttp_pi_dispatch(sip_msg_t* msg, char* s1, char* s2);


/** The context of the xhttp_pi request being processed.
 *
 * This is a global variable that records the context of the xhttp_pi request
 * being currently processed.
 * @sa pi_ctx
 */
static pi_ctx_t ctx;

static xhttp_api_t xhttp_api;

gen_lock_t* ph_lock;


str xhttp_pi_root = str_init("pi");
str filename = STR_NULL;

int buf_size = 0;
char error_buf[ERROR_REASON_BUF_LEN];

static cmd_export_t cmds[] = {
	{"dispatch_xhttp_pi",(cmd_function)xhttp_pi_dispatch,0,0,0,REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"xhttp_pi_root",	PARAM_STR,	&xhttp_pi_root},
	{"xhttp_pi_buf_size",	INT_PARAM,	&buf_size},
	{"framework",	PARAM_STR,	&filename},
	{0, 0, 0}
};

static rpc_export_t rpc_methods[];

/** module exports */
struct module_exports exports= {
	"xhttp_pi",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,		/* exported statistics */
	0,		/* exported MI functions */
	0,		/* exported pseudo-variables */
	0,		/* extra processes */
	mod_init,	/* module initialization function */
	0,
	(destroy_function) destroy,	/* destroy function */
	NULL	/* per-child init function */
};



/** Implementation of pi_fault function required by the management API.
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
static void pi_fault(pi_ctx_t* ctx, int code, char* fmt, ...)
{
	va_list ap;
	struct xhttp_pi_reply *reply = &ctx->reply;

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


static int pi_send(pi_ctx_t* ctx)
{
	struct xhttp_pi_reply* reply;

	if (ctx->reply_sent) return 1;

	reply = &ctx->reply;

	if (0!=ph_run_pi_cmd(ctx)){
		LM_DBG("pi_fault(500,\"Internal Server Error\"\n");
		pi_fault(ctx, 500, "Internal Server Error");
	}

	ctx->reply_sent = 1;
	if (reply->body.len) {
		xhttp_api.reply(ctx->msg, reply->code, &reply->reason,
			&XHTTP_PI_CONTENT_TYPE_TEXT_HTML, &reply->body);
	}
	else {
		LM_DBG("xhttp_api.reply(%p, %d, %.*s, %.*s, %.*s)\n",
			ctx->msg, reply->code,
			(&reply->reason)->len, (&reply->reason)->s,
			(&XHTTP_PI_CONTENT_TYPE_TEXT_HTML)->len,
			(&XHTTP_PI_CONTENT_TYPE_TEXT_HTML)->s,
			(&reply->reason)->len, (&reply->reason)->s);
		xhttp_api.reply(ctx->msg, reply->code, &reply->reason,
			&XHTTP_PI_CONTENT_TYPE_TEXT_HTML, &reply->reason);
	}

	if (reply->buf.s) {
		pkg_free(reply->buf.s);
		reply->buf.s = NULL;
		reply->buf.len = 0;
	}
	if (ctx->arg.s) {
        pkg_free(ctx->arg.s);
        ctx->arg.s = NULL;
        ctx->arg.len = 0;
    }

	return 0;
}

/** Initialize xhttp_pi reply data structure.
 *
 * This function initializes the data structure that contains all data related
 * to the xhttp_pi reply being created. The function must be called before any
 * other function that adds data to the reply.
 * @param ctx pi_ctx_t structure to be initialized.
 * @return 0 on success, a negative number on error.
 */
static int init_xhttp_pi_reply(pi_ctx_t *ctx)
{
	struct xhttp_pi_reply *reply = &ctx->reply;

	reply->code = 200;
	reply->reason = XHTTP_PI_REASON_OK;
	reply->buf.s = pkg_malloc(buf_size);
	if (!reply->buf.s) {
		LM_ERR("oom\n");
		pi_fault(ctx, 500, "Internal Server Error (No memory left)");
		return -1;
	}
	reply->buf.len = buf_size;
	reply->body.s = reply->buf.s;
	reply->body.len = 0;
	return 0;
}



int ph_init_async_lock(void)
{
	ph_lock = lock_alloc();
		if (ph_lock==NULL) {
		LM_ERR("failed to create lock\n");
		return -1;
	}
	if (lock_init(ph_lock)==NULL) {
		LM_ERR("failed to init lock\n");
		return -1;
	}
	return 0;
}


void ph_destroy_async_lock(void)
{
	if (ph_lock) {
		lock_destroy(ph_lock);
		lock_dealloc(ph_lock);
	}
}

static int mod_init(void)
{
	int i;

	if (rpc_register_array(rpc_methods)!=0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	/* bind the XHTTP API */
	if (xhttp_load_api(&xhttp_api) < 0) {
		LM_ERR("cannot bind to XHTTP API\n");
		return -1;
	}

	/* Check xhttp_pi_buf_size param */
	if (buf_size == 0)
		buf_size = pkg_mem_size/3;

	/* Check xhttp_pi_root param */
	for(i=0;i<xhttp_pi_root.len;i++){
		if ( !isalnum(xhttp_pi_root.s[i]) && xhttp_pi_root.s[i]!='_') {
			LM_ERR("bad xhttp_pi_root param [%.*s], char [%c] "
				"- use only alphanumerical chars\n",
				xhttp_pi_root.len, xhttp_pi_root.s,
				xhttp_pi_root.s[i]);
			return -1;
		}
	}

	/* Check framework param */
	if (!filename.s || filename.len<=0) {
		LM_ERR("missing framework\n");
		return -1;
	}

		/* building a cache of pi module commands */
		if (0!=ph_init_cmds(&ph_framework_data, filename.s))
			return -1;
		for(i=0;i<ph_framework_data->ph_db_urls_size;i++){
			LM_DBG("initializing db[%d] [%s]\n",
				i, ph_framework_data->ph_db_urls[i].db_url.s);
			if (init_http_db(ph_framework_data, i)!=0) {
				LM_ERR("failed to initialize the DB support\n");
				return -1;
			}
			if (connect_http_db(ph_framework_data, i)) {
				LM_ERR("failed to connect to database\n");
				return -1;
			}
		}

	/* Build async lock */
	if (ph_init_async_lock() != 0) return -1;

	return 0;
}


int destroy(void)
{
	destroy_http_db(ph_framework_data);
	ph_destroy_async_lock();
	return 0;
}


static int xhttp_pi_dispatch(sip_msg_t* msg, char* s1, char* s2)
{
	str arg = {NULL, 0};
	int ret = 0;
	int i;

	if(!IS_HTTP(msg)) {
		LM_DBG("Got non HTTP msg\n");
		return NONSIP_MSG_PASS;
	}

	/* Init xhttp_pi context */
	if (ctx.reply.buf.s) LM_ERR("Unexpected buf value [%p][%d]\n",
				ctx.reply.buf.s, ctx.reply.buf.len);
	memset(&ctx, 0, sizeof(pi_ctx_t));
	ctx.msg = msg;
	ctx.mod = ctx.cmd = -1;
	if (init_xhttp_pi_reply(&ctx) < 0) goto send_reply;

	lock_get(ph_lock);
	LM_DBG("ph_framework_data: [%p]->[%p]\n", &ph_framework_data, ph_framework_data);
	/* Extract arguments from url */
	if (0!=ph_parse_url(&msg->first_line.u.request.uri,
				&ctx.mod, &ctx.cmd, &arg)){
		pi_fault(&ctx, 500, "Bad URL");
		goto send_reply;
	}

	if (arg.s) {
		if (arg.len) {
			/* Unescape args */
			ctx.arg.s = pkg_malloc((arg.len)*sizeof(char));
			if (ctx.arg.s==NULL){
				LM_ERR("oom\n");
				pi_fault(&ctx, 500, "Internal Server Error (oom)");
				goto send_reply;
			}
			for(i=0;i<arg.len;i++) if (arg.s[i]=='+') arg.s[i]=' ';
			if (0>un_escape(&arg, &ctx.arg)) {
				LM_ERR("unable to escape [%.*s]\n", arg.len, arg.s);
				pi_fault(&ctx, 500, "Bad arg in URL");
				goto send_reply;
			}
		}
		ctx.arg_received = 1;
	} else {
		LM_DBG("Got no arg\n");
		goto send_reply;
	}
	
send_reply:
	LM_DBG("ph_framework_data: [%p]->[%p]\n", &ph_framework_data, ph_framework_data);
	lock_release(ph_lock);
	if (!ctx.reply_sent) {
		ret = pi_send(&ctx);
	}
	if (ret < 0) return -1;
	return 0;
}


/* rpc function documentation */
static const char *rpc_reload_doc[2] = {
	"Reload the xml framework", 0
};

/* rpc function implementations */
static void rpc_reload(rpc_t *rpc, void *c) {
	lock_get(ph_lock);
	if (0!=ph_init_cmds(&ph_framework_data, filename.s)) {
		rpc->rpl_printf(c, "Reload failed");
	} else {
		rpc->rpl_printf(c, "Reload OK");
	}
	lock_release(ph_lock);
	return;
}

static rpc_export_t rpc_methods[] = {
	{"xhttp_pi.reload", rpc_reload, rpc_reload_doc, 0},
	{0, 0, 0, 0}
};

