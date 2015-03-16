/*
 * Copyright (C) 2011 VoIP Embedded, Inc.
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "../../ver.h"
#include "../../trim.h"
#include "../../sr_module.h"
#include "../../nonsip_hooks.h"
#include "../../modules/xhttp/api.h"
#include "xhttp_rpc.h"
#include "xhttp_rpc_fnc.h"

/** @addtogroup xhttp_rpc
 * @ingroup modules
 * @{
 *
 * <h1>Overview of Operation</h1>
 * This module provides a web interface for RPC management interface.
 * It is built on top of the xhttp API module.
 */

/** @file
 *
 * This is the main file of xhttp_rpc module which contains all the functions
 * related to http processing, as well as the module interface.
 */

MODULE_VERSION

str XHTTP_RPC_REASON_OK = str_init("OK");
str XHTTP_RPC_CONTENT_TYPE_TEXT_HTML = str_init("text/html");


xhttp_rpc_mod_cmds_t *xhttp_rpc_mod_cmds = NULL;
int xhttp_rpc_mod_cmds_size = 0;

/* FIXME: this should be initialized in ../../ver.c */
int full_version_len;
int ver_name_len;

static int mod_init(void);
static int child_init(int rank);
static int xhttp_rpc_dispatch(sip_msg_t* msg, char* s1, char* s2);


/** The context of the xhttp_rpc request being processed.
 *
 * This is a global variable that records the context of the xhttp_rpc request
 * being currently processed.
 * @sa rpc_ctx
 */
static rpc_ctx_t ctx;

static xhttp_api_t xhttp_api;

/** Pointers to the functions that implement the RPC interface
 * of xhttp_rpc module
 */
static rpc_t func_param;

str xhttp_rpc_root = str_init("rpc");
int buf_size = 0;
char error_buf[ERROR_REASON_BUF_LEN];

static cmd_export_t cmds[] = {
	{"dispatch_xhttp_rpc",(cmd_function)xhttp_rpc_dispatch,0,0,0,REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"xhttp_rpc_root",	PARAM_STR,	&xhttp_rpc_root},
	{"xhttp_rpc_buf_size",	INT_PARAM,	&buf_size},
	{0, 0, 0}
};

/** module exports */
struct module_exports exports= {
	"xhttp_rpc",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,		/* exported statistics */
	0,		/* exported MI functions */
	0,		/* exported pseudo-variables */
	0,		/* extra processes */
	mod_init,	/* module initialization function */
	0,
	0,
	child_init	/* per-child init function */
};


/** Implementation of rpc_fault function required by the management API.
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
static void rpc_fault(rpc_ctx_t* ctx, int code, char* fmt, ...)
{
	va_list ap;
	struct xhttp_rpc_reply *reply = &ctx->reply;

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


/**
 */
static void free_data_struct(struct rpc_data_struct *rpc_d)
{
	struct rpc_data_struct *ds;

	if (!rpc_d) {
		LM_ERR("Atempting to free NULL rpc_data_struct\n");
		return;
	}
	while (rpc_d) {
		ds = rpc_d->next;
		pkg_free(rpc_d);
		rpc_d = ds;
	}
	return;
}


/**
 */
static struct rpc_data_struct *new_data_struct(rpc_ctx_t* ctx)
{
	struct rpc_data_struct *ds;

	if (!ctx) return NULL;
	ds = (struct rpc_data_struct*)pkg_malloc(sizeof(struct rpc_data_struct));
	if (!ds) {
		rpc_fault(ctx, 500, "Internal Server Error (oom)");
		return NULL;
	}
	memset(ds, 0, sizeof(struct rpc_data_struct));
	ds->ctx = ctx;

	return ds;
}


/** Initialize xhttp_rpc reply data structure.
 *
 * This function initializes the data structure that contains all data related
 * to the xhttp_rpc reply being created. The function must be called before any
 * other function that adds data to the reply.
 * @param ctx rpc_ctx_t structure to be initialized.
 * @return 0 on success, a negative number on error.
 */
static int init_xhttp_rpc_reply(rpc_ctx_t *ctx)
{
	struct xhttp_rpc_reply *reply = &ctx->reply;

	reply->code = 200;
	reply->reason = XHTTP_RPC_REASON_OK;
	reply->buf.s = pkg_malloc(buf_size);
	if (!reply->buf.s) {
		LM_ERR("oom\n");
		rpc_fault(ctx, 500, "Internal Server Error (No memory left)");
		return -1;
	}
	reply->buf.len = buf_size;
	reply->body.s = reply->buf.s;
	reply->body.len = 0;
	return 0;
}


/** Implementation of rpc_send function required by the management API.
 *
 * This is the function that will be called whenever a management function
 * asks the management interface to send the reply to the client.
 * The SIP/HTTP reply sent to
 * the client will be always 200 OK, if an error ocurred on the server then it
 * will be indicated in the html document in body.
 *
 * @param ctx A pointer to the context structure of the xhttp_rpc request that
 *            generated the reply.
 * @return 1 if the reply was already sent, 0 on success, a negative number on
 *            error
 */
static int rpc_send(rpc_ctx_t* ctx)
{
	struct xhttp_rpc_reply* reply;

	if (ctx->reply_sent) return 1;

	reply = &ctx->reply;

	if (0!=xhttp_rpc_build_page(ctx)){
		rpc_fault(ctx, 500, "Internal Server Error");
	}

	ctx->reply_sent = 1;
	if (reply->body.len)
		xhttp_api.reply(ctx->msg, reply->code, &reply->reason,
			&XHTTP_RPC_CONTENT_TYPE_TEXT_HTML, &reply->body);
	else
		xhttp_api.reply(ctx->msg, reply->code, &reply->reason,
			&XHTTP_RPC_CONTENT_TYPE_TEXT_HTML, &reply->reason);

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
	if (ctx->data_structs) {
		free_data_struct(ctx->data_structs);
		ctx->data_structs = NULL;
	}

	return 0;
}


/** Converts the variables provided in parameter ap according to formatting
 * string provided in parameter fmt into HTML format.
 *
 * This function takes the parameters provided in ap parameter and creates
 * HTML formatted parameters that will be put in the html document.
 * The format of input parameters is described in formatting string
 * fmt which follows the syntax of the management API. In the case of
 * an error the function will generate an error reply in err_reply parameter
 * instead.
 * @param ctx An error reply document will be generated here if the
 *                  function encounters a problem while processing input
 *                  parameters.
 * @param fmt Formatting string of the management API.
 * @param ap A pointer to the array of input parameters.
 *
 */
static int print_value(rpc_ctx_t* ctx, char fmt, va_list* ap, str *id)

{
	str body;
	str *sp;
	char buf[PRINT_VALUE_BUF_LEN];
	time_t dt;
	struct tm* t;

	switch(fmt) {
	case 'd':
		body.s = sint2str(va_arg(*ap, int), &body.len);
		break;
	case 'f':
		body.s = buf;
		body.len = snprintf(buf, PRINT_VALUE_BUF_LEN,
				"%f", va_arg(*ap, double));
		if (body.len < 0) {
			LM_ERR("Error while converting double\n");
			return -1;
		}
		break;
	case 'b':
		body.len = 1;
		body.s = ((va_arg(*ap, int)==0)?"0":"1");
		break;
	case 't':
		body.s = buf;
		body.len = sizeof("19980717T14:08:55") - 1;
		dt = va_arg(*ap, time_t);
		t = gmtime(&dt);
		if (strftime(buf, PRINT_VALUE_BUF_LEN,
				"%Y%m%dT%H:%M:%S", t) == 0) {
			LM_ERR("Error while converting time\n");
			return -1;
		}
		break;
	case 's':
		body.s = va_arg(*ap, char*);
		body.len = strlen(body.s);
		break;
	case 'S':
		sp = va_arg(*ap, str*);
		body = *sp;
		break;
	default:
		body.len = 0;
		body.s = NULL;
		LM_ERR("Invalid formatting character [%c]\n", fmt);
		return -1;
	}
	if (0!=xhttp_rpc_build_content(ctx, &body, id)) {
		rpc_fault(ctx, 500, "Internal Server Error");
		return -1;
	}
	return 0;
}


/** Implementation of rpc_add function required by the management API.
 *
 * This function will be called when an RPC management function calls
 * rpc->add to add a parameter to the xhttp_rpc reply being generated.
 */
static int rpc_add(rpc_ctx_t* ctx, char* fmt, ...)
{
	void **void_ptr;
	struct rpc_data_struct *ds;
	va_list ap;

	if (0!=xhttp_rpc_build_content(ctx, NULL, NULL)) {
		rpc_fault(ctx, 500, "Internal Server Error");
		return -1;
	}
	va_start(ap, fmt);
	while(*fmt) {
		if (*fmt == '{' || *fmt == '[') {
			void_ptr = va_arg(ap, void**);
			ds = new_data_struct(ctx);
			if (!ds) goto err;
			if (ctx->data_structs) free_data_struct(ctx->data_structs);
			ctx->data_structs = ds;
			*void_ptr = ds;
		} else {
			if (print_value(ctx, *fmt, &ap, NULL) < 0) goto err;
		}
		fmt++;
	}
	va_end(ap);
	return 0;
err:
	va_end(ap);
	return -1;
}


/** Implementation of rpc->scan function required by the management API.
 *
 * This is the function that will be called whenever a management function
 * calls rpc->scan to get the value of parameter from the xhttp_rpc
 * request. This function will extract the current parameter from the xhttp_rpc
 * URL and attempts to convert it to the type requested by the management
 * function that called it.
 */
static int rpc_scan(rpc_ctx_t* ctx, char* fmt, ...)
{
	int *int_ptr;
	char **char_ptr;
	double *double_ptr;
	str *str_ptr;

	str arg;

	int mandatory_param = 1;
	int modifiers = 0;
	char* orig_fmt;
	va_list ap;

	orig_fmt=fmt;
	va_start(ap, fmt);
	while(*fmt) {
		switch(*fmt) {
		case '*': /* start of optional parameters */
			mandatory_param = 0;
			modifiers++;
			fmt++;
			continue;
			break;
		case '.': /* autoconvert */
			modifiers++;
			fmt++;
			continue;
			break;
		case 'b': /* Bool */
		case 't': /* Date and time */
		case 'd': /* Integer */
			xhttp_rpc_get_next_arg(ctx, &arg);
			if (arg.len==0)
				goto read_error;
			int_ptr = va_arg(ap, int*);
			*int_ptr = strtol(arg.s, 0, 0);
			break;
		case 'f': /* double */
			xhttp_rpc_get_next_arg(ctx, &arg);
			if (arg.len==0)
				goto read_error;
			double_ptr = va_arg(ap, double*);
			*double_ptr = strtod(arg.s, 0);
			break;
		case 's': /* zero terminated string */
			xhttp_rpc_get_next_arg(ctx, &arg);
			if (arg.len==0)
				goto read_error;
			char_ptr = va_arg(ap, char**);
			*char_ptr = arg.s;
			break;
		case 'S': /* str structure */
			xhttp_rpc_get_next_arg(ctx, &arg);
			if (arg.len==0)
				goto read_error;
			str_ptr = va_arg(ap, str*);
			*str_ptr = arg;
			break;
		case '{':
			xhttp_rpc_get_next_arg(ctx, &arg);
			if (arg.len==0)
				goto read_error;
			LM_ERR("Unsupported param type [{]\n");
			rpc_fault(ctx, 500, "Unsupported param type [{]");
			goto error;
			break;
		default:
			LM_ERR("Invalid param type in formatting string: [%c]\n", *fmt);
			rpc_fault(ctx, 500,
				"Internal Server Error (inval formatting str)");
			goto error;
		}
		fmt++;
	}
	va_end(ap);
	return (int)(fmt-orig_fmt)-modifiers;
read_error:
	if (mandatory_param) rpc_fault(ctx, 400, "Invalid parameter value");
error:
	va_end(ap);
	return -((int)(fmt-orig_fmt)-modifiers);
}


/** Implementation of rpc_rpl_printf function required by the management API.
 *
 * This function will be called whenever an RPC management function calls
 * rpc-printf to add a parameter to the xhttp_rpc reply being constructed.
 */
static int rpc_rpl_printf(rpc_ctx_t* ctx, char* fmt, ...)
{
	int n, size;
	char *p;
	va_list ap;

	if (0!=xhttp_rpc_build_content(ctx, NULL, NULL)) {
		rpc_fault(ctx, 500, "Internal Server Error");
		return -1;
	}

	p = ctx->reply.body.s + ctx->reply.body.len;
	size = ctx->reply.buf.len - ctx->reply.body.len;
	va_start(ap, fmt);
	n = vsnprintf(p, size, fmt, ap);
	va_end(ap);
	if (n > -1 && n < size) {
		ctx->reply.body.len += n;
		p += n;
	} else {
		LM_ERR("oom\n");
		rpc_fault(ctx, 500, "Internal Server Error (oom)");
		return -1;
	}
	if (0!=xhttp_rpc_insert_break(ctx)) {
		LM_ERR("oom\n");
		rpc_fault(ctx, 500, "Internal Server Error (oom)");
		return -1;
	}

	return 0;
}


/** Adds a new member to structure.
 */
static int rpc_struct_add(struct rpc_data_struct* rpc_s, char* fmt, ...)
{
	va_list ap;
	void **void_ptr;
	str member_name;
	rpc_ctx_t *ctx = rpc_s->ctx;
	struct rpc_data_struct *ds, *s;

	if (!ctx) {
		LM_ERR("Invalid context\n");
		return -1;
	}
	if (!ctx->data_structs) {
		LM_ERR("Invalid structs\n");
		return -1;
	}
	s = ds = ctx->data_structs;
	ctx->struc_depth = 0;
	while (s) {
		if (s == rpc_s) {
			if (s->next) {
				free_data_struct(s->next);
				s->next = NULL;
			}
			break;
		}
		ctx->struc_depth++;
		ds = s;
		s = s->next;
	}
	if (!s)
		s = ds;
	va_start(ap, fmt);
	while(*fmt) {
		member_name.s = va_arg(ap, char*);
		member_name.len = (member_name.s?strlen(member_name.s):0);
		if (*fmt == '{' || *fmt == '[') {
			void_ptr = va_arg(ap, void**);
			ds = new_data_struct(ctx);
			if (!ds) goto err;
			s->next = ds;
			*void_ptr = ds;
			if (0!=xhttp_rpc_build_content(ctx, NULL, &member_name))
				goto err;
		} else {
			if (print_value(ctx, *fmt, &ap, &member_name) < 0) goto err;
		}
		fmt++;
	}
	va_end(ap);
	return 0;
err:
	va_end(ap);
	return -1;
}


static int rpc_struct_scan(void* s, char* fmt, ...)
{
	LM_ERR("Not implemented\n");
	return -1;
}


/** Create a new member from formatting string and add it to a structure.
 */
static int rpc_struct_printf(void* s, char* member_name, char* fmt, ...)
{
	LM_ERR("Not implemented\n");
	return -1;
}


/** Returns the RPC capabilities supported by the xmlrpc driver.
 */
static rpc_capabilities_t rpc_capabilities(rpc_ctx_t* ctx)
{
	/* No support for async commands.
	 */
	return 0;
}


/** Returns a new "delayed reply" context.
 * Creates a new delayed reply context in shm and returns it.
 * @return 0 - not supported, already replied, or no more memory;
 *         !=0 pointer to the special delayed ctx.
 * Note1: one should use the returned ctx reply context to build a reply and
 *  when finished call rpc_delayed_ctx_close().
 * Note2: adding pieces to the reply in different processes is not supported.
 */
static struct rpc_delayed_ctx* rpc_delayed_ctx_new(rpc_ctx_t* ctx)
{
	return NULL;
}


/** Closes a "delayed reply" context and sends the reply.
 * If no reply has been sent the reply will be built and sent automatically.
 * See the notes from rpc_new_delayed_ctx()
 */
static void rpc_delayed_ctx_close(struct rpc_delayed_ctx* dctx)
{
	return;
}


static int mod_init(void)
{
	int i;

	/* bind the XHTTP API */
	if (xhttp_load_api(&xhttp_api) < 0) {
		LM_ERR("cannot bind to XHTTP API\n");
		return -1;
	}

	/* Check xhttp_rpc_buf_size param */
	if (buf_size == 0)
		buf_size = pkg_mem_size/3;

	/* Check xhttp_rpc_root param */
	for(i=0;i<xhttp_rpc_root.len;i++){
		if ( !isalnum(xhttp_rpc_root.s[i]) && xhttp_rpc_root.s[i]!='_') {
			LM_ERR("bad xhttp_rpc_root param [%.*s], char [%c] "
				"- use only alphanumerical chars\n",
				xhttp_rpc_root.len, xhttp_rpc_root.s,
				xhttp_rpc_root.s[i]);
			return -1;
		}
	}

	memset(&func_param, 0, sizeof(func_param));
	func_param.send = (rpc_send_f)rpc_send;
	func_param.fault = (rpc_fault_f)rpc_fault;
	func_param.add = (rpc_add_f)rpc_add;
	func_param.scan = (rpc_scan_f)rpc_scan;
	func_param.rpl_printf = (rpc_rpl_printf_f)rpc_rpl_printf;
	func_param.struct_add = (rpc_struct_add_f)rpc_struct_add;
	/* use rpc_struct_add for array_add */
	func_param.array_add = (rpc_struct_add_f)rpc_struct_add;
	func_param.struct_scan = (rpc_struct_scan_f)rpc_struct_scan;
	func_param.struct_printf = (rpc_struct_printf_f)rpc_struct_printf;
	func_param.capabilities = (rpc_capabilities_f)rpc_capabilities;
	func_param.delayed_ctx_new = (rpc_delayed_ctx_new_f)rpc_delayed_ctx_new;
	func_param.delayed_ctx_close =
		(rpc_delayed_ctx_close_f)rpc_delayed_ctx_close;

	return 0;
}

static int child_init(int rank)
{
	int i, j;
	int len;
	xhttp_rpc_mod_cmds_t *cmds;
	/* rpc_export_t *rpc_e; */

	if(rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0; /* do nothing for the main process */

	if (rank==PROC_INIT)
	{
		/* building a cache of rpc module commands */
		xhttp_rpc_mod_cmds =
			(xhttp_rpc_mod_cmds_t*)pkg_malloc(sizeof(xhttp_rpc_mod_cmds_t));
		if (xhttp_rpc_mod_cmds==NULL){
			LM_ERR("oom\n");
			return -1;
		}
		xhttp_rpc_mod_cmds->rpc_e_index = 0;
		xhttp_rpc_mod_cmds->mod.s = NULL;
		xhttp_rpc_mod_cmds->mod.len = 0;
		xhttp_rpc_mod_cmds->size = 0;
		xhttp_rpc_mod_cmds_size = 1;
		cmds = xhttp_rpc_mod_cmds;
		for(i=0; i<rpc_sarray_crt_size; i++){
			len = strlen(rpc_sarray[i]->name);
			j = 0;
			while (j<len && rpc_sarray[i]->name[j]!='.')
				j++;
			if (j==len) {
				LM_DBG("dropping invalid command format [%.*s]\n",
						len, rpc_sarray[i]->name);
			} else {
				if (cmds->mod.len==0) {
					/* this is the first module */
					cmds->rpc_e_index = i;
					cmds->mod.s = (char*)&rpc_sarray[i]->name[0];
					cmds->mod.len = j;
					cmds->size++;
				} else if (cmds->mod.len==j &&
					strncmp(cmds->mod.s,
						(char*)&rpc_sarray[i]->name[0],
						j)==0){
					cmds->size++;
				} else {
					cmds = (xhttp_rpc_mod_cmds_t*)
						pkg_realloc(xhttp_rpc_mod_cmds,
							(xhttp_rpc_mod_cmds_size+1)*
							sizeof(xhttp_rpc_mod_cmds_t));
					if (cmds==NULL){
						LM_ERR("oom\n");
						return -1;
					}
					xhttp_rpc_mod_cmds = cmds;
					cmds = &xhttp_rpc_mod_cmds[xhttp_rpc_mod_cmds_size];
					cmds->rpc_e_index = i;
					cmds->mod.s = (char*)&rpc_sarray[i]->name[0];
					cmds->mod.len = j;
					xhttp_rpc_mod_cmds_size++;
					cmds->size = 1;
				}
			}
		}
		/*
		for(i=0; i<xhttp_rpc_mod_cmds_size; i++){
			for (j=0; j<xhttp_rpc_mod_cmds[i].size; j++){
				rpc_e = rpc_sarray[xhttp_rpc_mod_cmds[i].rpc_e_index+j];
				LM_DBG("[%p] => [%p]->[%.*s] [%p]->[%s]\n",
					rpc_e,
					xhttp_rpc_mod_cmds[i].mod.s,
					xhttp_rpc_mod_cmds[i].mod.len,
					xhttp_rpc_mod_cmds[i].mod.s,
					rpc_e->name,
					rpc_e->name);
			}
		}
		*/
	}

	full_version_len = strlen(full_version);
	ver_name_len = strlen(ver_name);
	return 0;
}


static int xhttp_rpc_dispatch(sip_msg_t* msg, char* s1, char* s2)
{
	rpc_export_t* rpc_e;
	str arg = {NULL, 0};
	int ret = 0;
	int i;

	if(!IS_HTTP(msg)) {
		LM_DBG("Got non HTTP msg\n");
		return NONSIP_MSG_PASS;
	}

	/* Init xhttp_rpc context */
	if (ctx.reply.buf.s) LM_ERR("Unexpected buf value [%p][%d]\n",
				ctx.reply.buf.s, ctx.reply.buf.len);
	memset(&ctx, 0, sizeof(rpc_ctx_t));
	ctx.msg = msg;
	ctx.mod = ctx.cmd = -1;
	if (init_xhttp_rpc_reply(&ctx) < 0) goto send_reply;

	/* Extract arguments from url */
	if (0!=xhttp_rpc_parse_url(&msg->first_line.u.request.uri,
				&ctx.mod, &ctx.cmd, &arg)){
		rpc_fault(&ctx, 500, "Bad URL");
		goto send_reply;
	}

	if (arg.s) {
		if (arg.len) {
			/* Unescape args */
			ctx.arg.s = pkg_malloc((arg.len+1)*sizeof(char));
			if (ctx.arg.s==NULL){
				LM_ERR("oom\n");
				rpc_fault(&ctx, 500, "Internal Server Error (oom)");
				goto send_reply;
			}
			for(i=0;i<arg.len;i++) if (arg.s[i]=='+') arg.s[i]=' ';
			if (0>un_escape(&arg, &ctx.arg)) {
				LM_ERR("unable to escape [%.*s]\n", arg.len, arg.s);
				rpc_fault(&ctx, 500, "Bad arg in URL");
				goto send_reply;
			}
			ctx.arg.s[ctx.arg.len] = '\0';
			ctx.arg.len++;
			ctx.arg2scan = ctx.arg;
		}
		ctx.arg_received = 1;
	} else {
		goto send_reply;
	}
	
	/*
	rpc_e=find_rpc_export((char*)rpc_sarray[xhttp_rpc_mod_cmds[ctx.mod].rpc_e_index+ctx.cmd]->name, 0);
	if ((rpc_e==NULL) || (rpc_e->function==NULL)){
		LM_ERR("Unable to find rpc command [%s]\n",
		rpc_sarray[xhttp_rpc_mod_cmds[ctx.mod].rpc_e_index+ctx.cmd]->name);
		rpc_fault(&ctx, 500, "Method not found");
		goto send_reply;
	}
	*/
	rpc_e=rpc_sarray[xhttp_rpc_mod_cmds[ctx.mod].rpc_e_index+ctx.cmd];
	rpc_e->function(&func_param, &ctx);

send_reply:
	if (!ctx.reply_sent) {
		ret = rpc_send(&ctx);
	}
	if (ret < 0) return -1;
	return 0;
}

