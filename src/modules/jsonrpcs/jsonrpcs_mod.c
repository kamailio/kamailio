/**
 * Copyright (C) 2014-2017 Daniel-Constantin Mierla (asipto.com)
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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../../core/ver.h"
#include "../../core/trim.h"
#include "../../core/pt.h"
#include "../../core/sr_module.h"
#include "../../core/mod_fix.h"
#include "../../core/nonsip_hooks.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/resolve.h"
#include "../../core/ip_addr.h"
#include "../../core/sip_msg_clone.h"
#include "../../core/data_lump.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/kemi.h"
#include "../../modules/xhttp/api.h"

#include "jsonrpcs_mod.h"

/** @file
 *
 * This is the main file of jsonrpc-s module which contains all the functions
 * related to http processing, as well as the module interface.
 */

/** @addtogroup jsonrpc-s
 * @ingroup modules
 *
 * <h1>Overview of Operation</h1>
 * This module provides jsonrpc over http server implementation.
 * @{
 */

MODULE_VERSION


#define jsonrpc_malloc	pkg_malloc
#define jsonrpc_free	pkg_free

static str JSONRPC_REASON_OK = str_init("OK");
static str JSONRPC_CONTENT_TYPE_HTML = str_init("application/json");

/*!< 0 - all available; 1 - http; 2 - fifo; 4 - datagram */
static int jsonrpc_transport = 6; /* fifo + datagram */

static int jsonrpc_pretty_format = 1;

static int jsonrpc_register_rpc(void);

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);
static int jsonrpc_dispatch(sip_msg_t* msg, char* s1, char* s2);
static int jsonrpc_exec(sip_msg_t* msg, char* cmd, char* s2);

/* FIFO server parameters */
extern char *jsonrpc_fifo;				/*!< FIFO file name */
extern char *jsonrpc_fifo_reply_dir; 	/*!< dir where reply fifos are allowed */
extern int  jsonrpc_fifo_uid;				/*!< Fifo default UID */
extern char *jsonrpc_fifo_uid_s;			/*!< Fifo default User ID name */
extern int  jsonrpc_fifo_gid;				/*!< Fifo default Group ID */
extern char *jsonrpc_fifo_gid_s;			/*!< Fifo default Group ID name */
extern int  jsonrpc_fifo_mode; /* Default file mode rw-rw---- */
/* fifo function prototypes */
extern int jsonrpc_init_fifo_file(void);
extern int jsonrpc_fifo_mod_init(void);
extern int jsonrpc_fifo_child_init(int rank);
extern int jsonrpc_fifo_destroy(void);

/* DATAGRAM server parameters */
extern char *jsonrpc_dgram_socket;
extern int jsonrpc_dgram_workers;
extern int jsonrpc_dgram_timeout;
extern int  jsonrpc_dgram_unix_socket_uid;
extern char *jsonrpc_dgram_unix_socket_uid_s;
extern int  jsonrpc_dgram_unix_socket_gid;
extern char *jsonrpc_dgram_unix_socket_gid_s;
extern int jsonrpc_dgram_unix_socket_mode;
/* datagram function prototypes */
extern int jsonrpc_dgram_mod_init(void);
extern int jsonrpc_dgram_child_init(int rank);
extern int jsonrpc_dgram_destroy(void);

/* tcp server parameters */
extern char *jsonrpc_tcp_socket;

int jsonrpc_tcp_mod_init(void);
int jsonrpc_tcp_child_init(int rank);

/** The context of the jsonrpc request being processed.
 *
 * This is a global variable that records the context of the jsonrpc request
 * being currently processed.
 * @sa rpc_ctx
 */
static jsonrpc_ctx_t _jsonrpc_ctx_global;
static jsonrpc_ctx_t *_jsonrpc_ctx_active = NULL;

static xhttp_api_t xhttp_api;

/** Pointers to the functions that implement the RPC interface
 * of jsonrpc module
 */
static rpc_t func_param;

#define JSONRPC_ERROR_REASON_BUF_LEN	128
#define JSONRPC_PRINT_VALUE_BUF_LEN		1024

char jsonrpc_error_buf[JSONRPC_ERROR_REASON_BUF_LEN];

static cmd_export_t cmds[] = {
	{"jsonrpc_dispatch", (cmd_function)jsonrpc_dispatch, 0, 0, 0,
		REQUEST_ROUTE},
	{"jsonrpc_exec",     (cmd_function)jsonrpc_exec, 1, fixup_spve_null, 0,
		ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"pretty_format",    PARAM_INT,    &jsonrpc_pretty_format},
	{"transport",        PARAM_INT,    &jsonrpc_transport},
	{"fifo_name",        PARAM_STRING, &jsonrpc_fifo},
	{"fifo_mode",        PARAM_INT,	   &jsonrpc_fifo_mode},
	{"fifo_group",       PARAM_STRING, &jsonrpc_fifo_gid_s},
	{"fifo_group",       PARAM_INT,    &jsonrpc_fifo_gid},
	{"fifo_user",        PARAM_STRING, &jsonrpc_fifo_uid_s},
	{"fifo_user",        PARAM_INT,    &jsonrpc_fifo_uid},
	{"fifo_reply_dir",   PARAM_STRING, &jsonrpc_fifo_reply_dir},
	{"dgram_socket",     PARAM_STRING, &jsonrpc_dgram_socket},
	{"dgram_workers",    PARAM_INT,    &jsonrpc_dgram_workers},
	{"dgram_timeout",    PARAM_INT,    &jsonrpc_dgram_timeout},
	{"dgram_mode",       PARAM_INT,    &jsonrpc_dgram_unix_socket_mode},
	{"dgram_group",      PARAM_STRING, &jsonrpc_dgram_unix_socket_gid_s},
	{"dgram_group",      PARAM_INT,    &jsonrpc_dgram_unix_socket_gid},
	{"dgram_user",       PARAM_STRING, &jsonrpc_dgram_unix_socket_uid_s},
	{"dgram_user",       PARAM_INT,    &jsonrpc_dgram_unix_socket_uid},
	{"tcp_socket",       PARAM_STRING, &jsonrpc_tcp_socket},

	{0, 0, 0}
};

static int jsonrpc_pv_get_jrpl(sip_msg_t *msg, pv_param_t *param, pv_value_t *res);
static int jsonrpc_pv_parse_jrpl_name(pv_spec_t *sp, str *in);

static pv_export_t mod_pvs[] = {
	{ {"jsonrpl",  sizeof("jsonrpl")-1}, PVT_OTHER,  jsonrpc_pv_get_jrpl,    0,
			jsonrpc_pv_parse_jrpl_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

/** module exports */
struct module_exports exports = {
	"jsonrpcs",      /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	mod_pvs,         /* pseudo-variables exports */
	0,               /* response handling function */
	mod_init,        /* module init function */
	child_init,      /* per-child init function */
	mod_destroy      /* module destroy function */
};


typedef struct jsonrpc_error {
	int code;
	str text;
} jsonrpc_error_t;

static jsonrpc_error_t _jsonrpc_error_table[] = {
	{ -32700, { "Parse Error", 11 } },
	{ -32600, { "Invalid Request", 15 } },
	{ -32601, { "Method Not Found", 16 } },
	{ -32602, { "Invalid Parameters", 18 } },
	{ -32603, { "Internal Error", 14 } },
	{ -32000, { "Execution Error", 15 } },
	{0, { 0, 0 } }
};

static char _jsonrpcs_stored_id[64];

char *jsonrpcs_stored_id_get(void)
{
	return _jsonrpcs_stored_id;
}

static jsonrpc_plain_reply_t _jsonrpc_plain_reply;

jsonrpc_plain_reply_t* jsonrpc_plain_reply_get(void)
{
	return &_jsonrpc_plain_reply;
}

static void jsonrpc_set_plain_reply(int rcode, str *rtext, str *rbody,
					void (*free_fn)(void*))
{
	if(_jsonrpc_plain_reply.rbody.s) {
		free_fn(_jsonrpc_plain_reply.rbody.s);
	}
	_jsonrpc_plain_reply.rcode = rcode;
	_jsonrpc_plain_reply.rtext = *rtext;
	if(rbody) {
		_jsonrpc_plain_reply.rbody = *rbody;
	} else {
		_jsonrpc_plain_reply.rbody.s = NULL;
		_jsonrpc_plain_reply.rbody.len = 0;
	}
}

static void jsonrpc_reset_plain_reply(void (*free_fn)(void*))
{
	if(_jsonrpc_plain_reply.rbody.s) {
		free_fn(_jsonrpc_plain_reply.rbody.s);
	}
	memset(&_jsonrpc_plain_reply, 0, sizeof(jsonrpc_plain_reply_t));
}


/** Initialize jsonrpc reply data structure.
 *
 * This function initializes the data structure that contains all data related
 * to the jsonrpc reply being created. The function must be called before any
 * other function that adds data to the reply.
 * @param ctx jsonrpc_ctx_t structure to be initialized.
 * @return 0 on success, a negative number on error.
 */
static int jsonrpc_init_reply(jsonrpc_ctx_t *ctx)
{
	ctx->http_code = 200;
	ctx->http_text = JSONRPC_REASON_OK;
	ctx->jrpl = srjson_NewDoc(NULL);
	if(ctx->jrpl==NULL) {
		LM_ERR("Failed to init the reply json document\n");
		return -1;
	}
	ctx->jrpl->root = srjson_CreateObject(ctx->jrpl);
	if(ctx->jrpl->root==NULL) {
		LM_ERR("Failed to init the reply json root node\n");
		return -1;
	}
	srjson_AddStrStrToObject(ctx->jrpl, ctx->jrpl->root,
					"jsonrpc", 7,
					"2.0", 3);

	return 0;
}


/** if this a delayed reply context,
 * and it's never been use before, initialize it */
static int jsonrpc_delayed_reply_ctx_init(jsonrpc_ctx_t* ctx)
{
	if  ((ctx->flags & JSONRPC_DELAYED_CTX_F)
			&& (ctx->jrpl==0)) {
		if (jsonrpc_init_reply(ctx) < 0)
			return -1;
		jsonrpc_reset_plain_reply(ctx->jrpl->free_fn);
		_jsonrpc_ctx_active = ctx;
	}
	return 0;
}

/** Implementation of rpc_fault function required by the management API.
 *
 * This function will be called whenever a management function
 * indicates that an error occurred while it was processing the request. The
 * function takes the reply code and reason phrase as parameters, these will
 * be put in the body of the reply.
 *
 * @param ctx A pointer to the context structure of the request being
 *            processed.
 * @param code Reason code.
 * @param fmt Formatting string used to build the reason phrase.
 */
static void jsonrpc_fault(jsonrpc_ctx_t* ctx, int code, char* fmt, ...)
{
	va_list ap;

	jsonrpc_delayed_reply_ctx_init(ctx);

	if(code <= 100) {
		ctx->http_code = 500;
	} else {
		ctx->http_code = code;
	}
	va_start(ap, fmt);
	vsnprintf(jsonrpc_error_buf, JSONRPC_ERROR_REASON_BUF_LEN, fmt, ap);
	va_end(ap);
	ctx->error_text.len = strlen(jsonrpc_error_buf);
	ctx->error_text.s = jsonrpc_error_buf;
	ctx->http_text.len = ctx->error_text.len;
	ctx->http_text.s = jsonrpc_error_buf;
	if(code == 0) {
		ctx->error_code = -32000;
	} else {
		ctx->error_code = code;
	}

	return;
}


/** Implementation of rpc_send function required by the management API.
 *
 * This is the function that will be called whenever a management function
 * asks the management interface to send the reply to the client.
 * The SIP/HTTP reply sent to
 * the client will be always 200 OK, if an error occurred on the server then it
 * will be indicated in the html document in body.
 *
 * @param ctx A pointer to the context structure of the jsonrpc request that
 *            generated the reply.
 * @param mode If the jsonrpc id should be stored in global buffer
 * @return 1 if the reply was already sent, 0 on success, a negative number on
 *            error
 */
static int jsonrpc_send_mode(jsonrpc_ctx_t* ctx, int mode)
{
	srjson_t *nj = NULL;
	int i;
	str rbuf;

	if (ctx->reply_sent) return 1;

	ctx->reply_sent = 1;

	if(ctx->error_code != 0) {
		/* fault handling */
		nj = srjson_CreateObject(ctx->jrpl);
		if(nj!=NULL) {
			srjson_AddNumberToObject(ctx->jrpl, nj, "code",
					ctx->error_code);
			for(i=0; _jsonrpc_error_table[i].code!=0
					&& _jsonrpc_error_table[i].code!=ctx->error_code; i++);
			if(_jsonrpc_error_table[i].code!=0) {
				srjson_AddStrStrToObject(ctx->jrpl, nj,
					"message", 7,
					_jsonrpc_error_table[i].text.s,
					_jsonrpc_error_table[i].text.len);
			} else {
				if(ctx->error_text.len>0) {
					srjson_AddStrStrToObject(ctx->jrpl, nj,
							"message", 7,
							ctx->error_text.s, ctx->error_text.len);
				} else {
					srjson_AddStrStrToObject(ctx->jrpl, nj,
							"message", 7, "Unexpected Error", 16);
				}
			}
			srjson_AddItemToObject(ctx->jrpl, ctx->jrpl->root, "error", nj);
		}
	} else {
		nj = srjson_GetObjectItem(ctx->jrpl, ctx->jrpl->root, "result");
		if(nj==NULL) {
			if (!ctx->rpl_node) {
				if(ctx->flags & RET_ARRAY) {
					ctx->rpl_node = srjson_CreateArray(ctx->jrpl);
				} else {
					ctx->rpl_node = srjson_CreateObject(ctx->jrpl);
				}
				if(ctx->rpl_node == 0) {
					LM_ERR("failed to create the root array node\n");
				}
			}
			srjson_AddItemToObject(ctx->jrpl, ctx->jrpl->root,
				"result", ctx->rpl_node);
			ctx->rpl_node = 0;
		}
	}
	if(ctx->jreq!=NULL && ctx->jreq->root!=NULL) {
		nj = srjson_GetObjectItem(ctx->jreq, ctx->jreq->root, "id");
		if(nj!=NULL) {
			if(nj->valuestring!=NULL) {
				srjson_AddStrStrToObject(ctx->jrpl, ctx->jrpl->root,
						"id", 2,
						nj->valuestring, strlen(nj->valuestring));
				if(mode==1) {
					snprintf(_jsonrpcs_stored_id, 62, "\"%s\"", nj->valuestring);
				}
			} else {
				srjson_AddNumberToObject(ctx->jrpl, ctx->jrpl->root, "id",
						nj->valuedouble);
				if(mode==1) {
					snprintf(_jsonrpcs_stored_id, 62, "%lld",
							(long long int)nj->valuedouble);
				}
			}
		}
	} else {
		if(ctx->jsrid_type == 1) {
			srjson_AddStrStrToObject(ctx->jrpl, ctx->jrpl->root,
					"id", 2,
					ctx->jsrid_val, strlen(ctx->jsrid_val));
			if(mode==1) {
				snprintf(_jsonrpcs_stored_id, 62, "\"%s\"", ctx->jsrid_val);
			}
		} else if(ctx->jsrid_type == 2) {
			srjson_AddNumberToObject(ctx->jrpl, ctx->jrpl->root, "id",
					(double)(*(long*)ctx->jsrid_val));
			if(mode==1) {
				snprintf(_jsonrpcs_stored_id, 62, "%ld", *((long*)ctx->jsrid_val));
			}
		}
	}

	if(jsonrpc_pretty_format==0) {
		rbuf.s = srjson_PrintUnformatted(ctx->jrpl, ctx->jrpl->root);
	} else {
		rbuf.s = srjson_Print(ctx->jrpl, ctx->jrpl->root);
	}
	if(rbuf.s!=NULL) {
		rbuf.len = strlen(rbuf.s);
	}
	if (rbuf.s!=NULL) {
		LM_DBG("sending response with body: %p - %d %.*s\n", ctx->msg,
				ctx->http_code, ctx->http_text.len, ctx->http_text.s);
		if(ctx->msg) {
			xhttp_api.reply(ctx->msg, ctx->http_code, &ctx->http_text,
				&JSONRPC_CONTENT_TYPE_HTML, &rbuf);
		} else {
			jsonrpc_set_plain_reply(ctx->http_code, &ctx->http_text, &rbuf,
					ctx->jrpl->free_fn);
			rbuf.s=NULL;
		}
	} else {
		LM_DBG("sending response without body: %p - %d %.*s\n", ctx->msg,
				ctx->http_code, ctx->http_text.len, ctx->http_text.s);
		if(ctx->msg) {
			xhttp_api.reply(ctx->msg, ctx->http_code, &ctx->http_text,
					NULL, NULL);
		} else {
			jsonrpc_set_plain_reply(ctx->http_code, &ctx->http_text, NULL,
					ctx->jrpl->free_fn);
		}
	}
	if (rbuf.s!=NULL) {
		ctx->jrpl->free_fn(rbuf.s);
	}

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
 * @param ctx A pointer to the context structure of the jsonrpc request that
 *            generated the reply.
 * @return 1 if the reply was already sent, 0 on success, a negative number on
 *            error
 */
static int jsonrpc_send(jsonrpc_ctx_t* ctx)
{
	return jsonrpc_send_mode(ctx, 0);
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
static srjson_t* jsonrpc_print_value(jsonrpc_ctx_t* ctx, char fmt, va_list* ap)

{
	srjson_t *nj = NULL;
	char buf[JSONRPC_PRINT_VALUE_BUF_LEN];
	time_t dt;
	struct tm t;
	str *sp;
	char *cp;

	switch(fmt) {
	case 'd':
		nj = srjson_CreateNumber(ctx->jrpl, va_arg(*ap, int));
		break;
	case 'u':
		nj = srjson_CreateNumber(ctx->jrpl, va_arg(*ap, unsigned int));
		break;
	case 'f':
		nj = srjson_CreateNumber(ctx->jrpl, va_arg(*ap, double));
		break;
	case 'l':
		nj = srjson_CreateNumber(ctx->jrpl, va_arg(*ap, long));
		break;
	case 'j':
		nj = srjson_CreateNumber(ctx->jrpl, va_arg(*ap, unsigned long));
		break;
	case 'L':
		nj = srjson_CreateNumber(ctx->jrpl, va_arg(*ap, long long));
		break;
	case 'J':
		nj = srjson_CreateNumber(ctx->jrpl, va_arg(*ap, unsigned long long));
		break;
	case 'b':
		nj = srjson_CreateBool(ctx->jrpl, ((va_arg(*ap, int)==0)?0:1));
		break;
	case 't':
		dt = va_arg(*ap, time_t);
		gmtime_r(&dt, &t);
		if (strftime(buf, JSONRPC_PRINT_VALUE_BUF_LEN,
				"%Y%m%dT%H:%M:%S", &t) == 0) {
			LM_ERR("Error while converting time\n");
			return NULL;
		}
		nj = srjson_CreateString(ctx->jrpl, buf);
		break;
	case 's':
		cp = va_arg(*ap, char*);
		if(cp!=NULL) {
			nj = srjson_CreateString(ctx->jrpl, cp);
		} else {
			nj = srjson_CreateNull(ctx->jrpl);
		}
		break;
	case 'S':
		sp = va_arg(*ap, str*);
		if(sp!=NULL && sp->s!=NULL) {
			nj = srjson_CreateStr(ctx->jrpl, sp->s, sp->len);
		} else {
			nj = srjson_CreateNull(ctx->jrpl);
		}
		break;
	default:
		LM_ERR("Invalid formatting character [%c]\n", fmt);
		return NULL;
	}
	return nj;
}



/** Implementation of rpc_add function required by the management API.
 *
 * This function will be called when an RPC management function calls
 * rpc->add to add a parameter to the jsonrpc reply being generated.
 */
static int jsonrpc_add(jsonrpc_ctx_t* ctx, char* fmt, ...)
{
	srjson_t *nj = NULL;
	void **void_ptr;
	va_list ap;

	jsonrpc_delayed_reply_ctx_init(ctx);

	va_start(ap, fmt);
	while(*fmt) {
		if (*fmt == '{' || *fmt == '[') {
			void_ptr = va_arg(ap, void**);
			if (*fmt == '{') {
				nj = srjson_CreateObject(ctx->jrpl);
			} else {
				nj = srjson_CreateArray(ctx->jrpl);
			}
			*void_ptr = nj;
		} else {
			nj = jsonrpc_print_value(ctx, *fmt, &ap);
		}

		if(nj==NULL) goto err;
		if(ctx->flags & RET_ARRAY) {
			if (ctx->rpl_node==NULL) {
				ctx->rpl_node = srjson_CreateArray(ctx->jrpl);
				if(ctx->rpl_node == 0) {
					LM_ERR("failed to create the root array node\n");
					goto err;
				}
			}
			srjson_AddItemToArray(ctx->jrpl, ctx->rpl_node, nj);
		} else {
			if (ctx->rpl_node) srjson_Delete(ctx->jrpl, ctx->rpl_node);
			ctx->rpl_node = nj;
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
 * calls rpc->scan to get the value of parameter from the jsonrpc
 * request. This function will extract the current parameter from the jsonrpc
 * URL and attempts to convert it to the type requested by the management
 * function that called it.
 */
static int jsonrpc_scan(jsonrpc_ctx_t* ctx, char* fmt, ...)
{
	int *int_ptr;
	unsigned int *uint_ptr;
	long *long_ptr;
	unsigned long *ulong_ptr;
	long long *llong_ptr;
	unsigned long long *ullong_ptr;
	char **char_ptr;
	double *double_ptr;
	str *str_ptr;
	int mandatory_param = 1;
	int modifiers = 0;
	int auto_convert = 0;
	char* orig_fmt;
	va_list ap;
	str stmp;

	if(ctx->req_node==NULL) {
		LM_DBG("no request node\n");
		return 0;
	}

	orig_fmt=fmt;
	va_start(ap, fmt);
	while(*fmt && ctx->req_node) {
		switch(*fmt) {
		case '*': /* start of optional parameters */
			mandatory_param = 0;
			modifiers++;
			fmt++;
			continue;
		case '.': /* autoconvert */
			modifiers++;
			fmt++;
			auto_convert = 1;
			continue;
		case 'b': /* Bool */
			uint_ptr = va_arg(ap, unsigned int*);
			*uint_ptr = SRJSON_GET_UINT(ctx->req_node);
			break;
		case 't': /* Date and time */
			uint_ptr = va_arg(ap, unsigned int*);
			*uint_ptr = SRJSON_GET_UINT(ctx->req_node);
			break;
		case 'd': /* Integer */
			int_ptr = va_arg(ap, int*);
			*int_ptr = SRJSON_GET_INT(ctx->req_node);
			break;
		case 'u': /* Unsigned Integer */
			uint_ptr = va_arg(ap, unsigned int*);
			*uint_ptr = SRJSON_GET_UINT(ctx->req_node);
			break;
		case 'f': /* double */
			double_ptr = va_arg(ap, double*);
			*double_ptr = ctx->req_node->valuedouble;
			break;
		case 'l': /* Long */
			long_ptr = va_arg(ap, long*);
			*long_ptr = SRJSON_GET_LONG(ctx->req_node);
			break;
		case 'j': /* Unsigned Long */
			ulong_ptr = va_arg(ap, unsigned long*);
			*ulong_ptr = SRJSON_GET_ULONG(ctx->req_node);
			break;
		case 'L': /* Long Long */
			llong_ptr = va_arg(ap, long long*);
			*llong_ptr = SRJSON_GET_LLONG(ctx->req_node);
			break;
		case 'J': /* Unsigned Long Long */
			ullong_ptr = va_arg(ap, unsigned long long*);
			*ullong_ptr = SRJSON_GET_ULLONG(ctx->req_node);
			break;
		case 's': /* zero terminated string */
			char_ptr = va_arg(ap, char**);
			if(ctx->req_node->type==srjson_String) {
				*char_ptr = ctx->req_node->valuestring;
			} else if(auto_convert == 1) {
				if(ctx->req_node->type==srjson_Number) {
					*char_ptr = int2str(SRJSON_GET_ULONG(ctx->req_node),
							&stmp.len);
				} else {
					LM_ERR("field is not a number to auto-convert - type %d\n",
							ctx->req_node->type);
					*char_ptr = NULL;
					goto error;
				}
			} else {
				LM_ERR("field is not a string - type %d\n",
							ctx->req_node->type);
				*char_ptr = NULL;
				goto error;
			}
			break;
		case 'S': /* str structure */
			str_ptr = va_arg(ap, str*);
			if(ctx->req_node->type==srjson_String) {
				str_ptr->s = ctx->req_node->valuestring;
				str_ptr->len = strlen(ctx->req_node->valuestring);
			} else if(auto_convert == 1) {
				if(ctx->req_node->type==srjson_Number) {
					str_ptr->s = int2str(SRJSON_GET_ULONG(ctx->req_node),
							&str_ptr->len);
				} else {
					LM_ERR("field is not a number to auto-convert - type %d\n",
							ctx->req_node->type);
					str_ptr->s = NULL;
					str_ptr->len = 0;
					goto error;
				}
			} else {
				LM_ERR("field is not a string - type %d\n",
							ctx->req_node->type);
				str_ptr->s = NULL;
				str_ptr->len = 0;
				goto error;
			}
			break;
		case '{':
		case '[':
			LM_ERR("Unsupported param type '%c'\n", *fmt);
			jsonrpc_fault(ctx, 400, "Unsupported param type");
			goto error;
		default:
			LM_ERR("Invalid param type in formatting string: [%c]\n", *fmt);
			jsonrpc_fault(ctx, 500,
				"Internal Server Error (inval formatting str)");
			goto error;
		}
		fmt++;
		auto_convert = 0;
		ctx->req_node = ctx->req_node->next;
	}
	/* error if there is still a scan char type and it is not optional */
	if(*fmt && *fmt!='*' && mandatory_param==1) {
		LM_ERR("no more fields to scan\n");
		goto error;
	}

	va_end(ap);
	return (int)(fmt-orig_fmt)-modifiers;
error:
	va_end(ap);
	return -((int)(fmt-orig_fmt)-modifiers);
}


/** Implementation of rpc_rpl_printf function required by the management API.
 *
 * This function will be called whenever an RPC management function calls
 * rpc-printf to add a parameter to the jsonrpc reply being constructed.
 */
static int jsonrpc_rpl_printf(jsonrpc_ctx_t* ctx, char* fmt, ...)
{
	int n, buf_size;
	char *buf = 0;
	char tbuf[JSONRPC_PRINT_VALUE_BUF_LEN];
	va_list ap;
	srjson_t *nj = NULL;

	jsonrpc_delayed_reply_ctx_init(ctx);

	buf = tbuf;
	buf_size = JSONRPC_PRINT_VALUE_BUF_LEN;
	while (1) {
		/* try to print in the allocated space. */
		va_start(ap, fmt);
		n = vsnprintf(buf, buf_size, fmt, ap);
		va_end(ap);
		/* if that worked, return the string. */
		if (n > -1 && n < buf_size) {
			nj = srjson_CreateString(ctx->jrpl, buf);
			if(nj==NULL) {
				LM_ERR("failed to create the value node\n");
				if(buf && buf!=tbuf) jsonrpc_free(buf);
				return -1;
			}
			if(ctx->flags & RET_ARRAY) {
				if (ctx->rpl_node==NULL) {
					ctx->rpl_node = srjson_CreateArray(ctx->jrpl);
					if(ctx->rpl_node == 0) {
						LM_ERR("failed to create the root array node\n");
						if(buf && buf!=tbuf) jsonrpc_free(buf);
						return -1;
					}
				}
				srjson_AddItemToArray(ctx->jrpl, ctx->rpl_node, nj);
			} else {
				if (ctx->rpl_node) srjson_Delete(ctx->jrpl, ctx->rpl_node);
				ctx->rpl_node = nj;
			}
			if(buf && buf!=tbuf) jsonrpc_free(buf);
			return 0;
		}
		/* else try again with more space. */
		if (n > -1) {   /* glibc 2.1 */
			buf_size = n + 1; /* precisely what is needed */
		} else {          /* glibc 2.0 */
			buf_size *= 2;  /* twice the old size */
		}
		if(buf && buf!=tbuf) jsonrpc_free(buf);
		if ((buf = jsonrpc_malloc(buf_size)) == 0) {
			jsonrpc_fault(ctx, 500, "Internal Server Error (No memory left)");
			LM_ERR("no memory left for rpc printf\n");
			return -1;
		}
	}
}


/** Adds a new member to structure.
 */
static int jsonrpc_struct_add(srjson_t *jnode, char* fmt, ...)
{
	srjson_t *nj = NULL;
	srjson_t *wj = NULL;
	jsonrpc_ctx_t* ctx;
	va_list ap;
	void **void_ptr;
	str mname;
	int isobject;

	if(jnode==NULL) {
		LM_ERR("invalid json node parameter\n");
		return -1;
	}
	if(jnode->type!=srjson_Object && jnode->type!=srjson_Array) {
		LM_ERR("json node parameter is not object or array (%d)\n",
				jnode->type);
		return -1;
	}
	isobject = (jnode->type==srjson_Object);

	ctx = _jsonrpc_ctx_active;
	if(ctx==NULL || ctx->jrpl==NULL) {
		LM_ERR("reply object not initialized in rpl context %p - flags 0x%x\n",
				ctx, (ctx)?ctx->flags:0);
		return -1;
	}

	va_start(ap, fmt);
	while(*fmt) {
		mname.s = va_arg(ap, char*);
		mname.len = (mname.s?strlen(mname.s):0);
		if(mname.s==NULL) mname.s = "";

		if (*fmt == '{' || *fmt == '[') {
			void_ptr = va_arg(ap, void**);
			if (*fmt == '{') {
				nj = srjson_CreateObject(ctx->jrpl);
			} else {
				nj = srjson_CreateArray(ctx->jrpl);
			}
			*void_ptr = nj;
		} else {
			nj = jsonrpc_print_value(ctx, *fmt, &ap);
		}

		if(nj==NULL) {
			LM_ERR("failed to print the value (%c)\n", *fmt);
			goto err;
		}
		if(isobject) {
			/* add as member to object */
			srjson_AddItemToObject(ctx->jrpl, jnode, mname.s, nj);
		} else {
			/* wrap member in a new object and add to array */
			wj = srjson_CreateObject(ctx->jrpl);
			if(wj==NULL) {
				LM_ERR("failed to create object (%c)\n", *fmt);
				srjson_Delete(ctx->jrpl, nj);
				goto err;
			}
			srjson_AddItemToObject(ctx->jrpl, wj, mname.s, nj);
			srjson_AddItemToArray(ctx->jrpl, jnode, wj);
		}
		fmt++;
	}
	va_end(ap);
	return 0;
err:
	va_end(ap);
	return -1;
}


/** Adds a new member to structure.
 */
static int jsonrpc_array_add(srjson_t *jnode, char* fmt, ...)
{
	srjson_t *nj = NULL;
	jsonrpc_ctx_t* ctx;
	va_list ap;
	void **void_ptr;

	if(jnode==NULL) {
		LM_ERR("invalid json node parameter\n");
		return -1;
	}
	if(jnode->type!=srjson_Array) {
		LM_ERR("json node parameter is not array (%d)\n", jnode->type);
		return -1;
	}

	ctx = _jsonrpc_ctx_active;
	if(ctx==NULL || ctx->jrpl==NULL) {
		LM_ERR("reply object not initialized in rpl context %p - flags 0x%x\n",
				ctx, (ctx)?ctx->flags:0);
		return -1;
	}

	va_start(ap, fmt);
	while(*fmt) {
		if (*fmt == '{' || *fmt == '[') {
			void_ptr = va_arg(ap, void**);
			if (*fmt == '{') {
				nj = srjson_CreateObject(ctx->jrpl);
			} else {
				nj = srjson_CreateArray(ctx->jrpl);
			}
			*void_ptr = nj;
		} else {
			nj = jsonrpc_print_value(ctx, *fmt, &ap);
		}

		if(nj==NULL) goto err;
		srjson_AddItemToArray(ctx->jrpl, jnode, nj);
		fmt++;
	}
	va_end(ap);
	return 0;
err:
	va_end(ap);
	return -1;
}


static int jsonrpc_struct_scan(void* s, char* fmt, ...)
{
	LM_ERR("Not implemented\n");
	return -1;
}


/** Create a new member from formatting string and add it to a structure.
 */
static int jsonrpc_struct_printf(srjson_t *jnode, char* mname, char* fmt, ...)
{
	jsonrpc_ctx_t* ctx;
	int n, buf_size;
	char *buf = 0;
	char tbuf[JSONRPC_PRINT_VALUE_BUF_LEN];
	va_list ap;
	srjson_t *nj = NULL;

	if(jnode==NULL || mname==NULL) {
		LM_ERR("invalid json node or member name parameter (%p/%p)\n",
				jnode, mname);
		return -1;
	}
	if(jnode->type!=srjson_Object) {
		LM_ERR("json node parameter is not object (%d)\n", jnode->type);
		return -1;
	}

	ctx = _jsonrpc_ctx_active;
	if(ctx==NULL || ctx->jrpl==NULL) {
		LM_ERR("reply object not initialized in rpl context %p - flags 0x%x\n",
				ctx, (ctx)?ctx->flags:0);
		return -1;
	}

	buf = tbuf;
	buf_size = JSONRPC_PRINT_VALUE_BUF_LEN;
	while (1) {
		/* try to print in the allocated space. */
		va_start(ap, fmt);
		n = vsnprintf(buf, buf_size, fmt, ap);
		va_end(ap);
		/* if that worked, return the string. */
		if (n > -1 && n < buf_size) {
			nj = srjson_CreateString(ctx->jrpl, buf);
			if(nj==NULL) {
				LM_ERR("failed to create the value node\n");
				if(buf && buf!=tbuf) jsonrpc_free(buf);
				return -1;
			}
			srjson_AddItemToObject(ctx->jrpl, jnode, mname, nj);
			if(buf && buf!=tbuf) jsonrpc_free(buf);
			return 0;
		}
		/* else try again with more space. */
		if (n > -1) {   /* glibc 2.1 */
			buf_size = n + 1; /* precisely what is needed */
		} else {          /* glibc 2.0 */
			buf_size *= 2;  /* twice the old size */
		}
		if(buf && buf!=tbuf) jsonrpc_free(buf);
		if ((buf = jsonrpc_malloc(buf_size)) == 0) {
			jsonrpc_fault(ctx, 500, "Internal Server Error (No memory left)");
			LM_ERR("no memory left for rpc printf\n");
			return -1;
		}
	}
	return -1;
}


static void jsonrpc_clean_context(jsonrpc_ctx_t* ctx)
{
	if (!ctx) return;
	srjson_DeleteDoc(ctx->jreq);
	if(ctx->rpl_node!=NULL) {
		srjson_Delete(ctx->jrpl, ctx->rpl_node);
		ctx->rpl_node = NULL;
	}
	srjson_DeleteDoc(ctx->jrpl);
}


/** Returns the RPC capabilities supported by the xmlrpc driver.
 */
static rpc_capabilities_t jsonrpc_capabilities(jsonrpc_ctx_t* ctx)
{
	/* support for async commands - delayed response */
	return RPC_DELAYED_REPLY;
}


/** Returns a new "delayed reply" context.
 * Creates a new delayed reply context in shm and returns it.
 * @return 0 - not supported, already replied, or no more memory;
 *         !=0 pointer to the special delayed ctx.
 * Note1: one should use the returned ctx reply context to build a reply and
 *  when finished call rpc_delayed_ctx_close().
 * Note2: adding pieces to the reply in different processes is not supported.
 */
static struct rpc_delayed_ctx* jsonrpc_delayed_ctx_new(jsonrpc_ctx_t* ctx)
{
	struct rpc_delayed_ctx* ret;
	int size;
	jsonrpc_ctx_t* r_ctx;
	sip_msg_t* shm_msg;
	int len;
	srjson_t *nj = NULL;

	ret=0;
	shm_msg=0;
	len = 0;

	if (ctx->reply_sent) {
		LM_ERR("response already sent - cannot create a delayed context\n");
		return 0; /* no delayed reply if already replied */
	}

	if (ctx->transport!=JSONRPC_TRANS_HTTP) {
		LM_ERR("delayed response implemented only for HTTP transport\n");
		return 0;
	}

	if(ctx->jreq==NULL || ctx->jreq->root==NULL) {
		LM_ERR("invalid context attributes\n");
		return 0;
	}

	nj = srjson_GetObjectItem(ctx->jreq, ctx->jreq->root, "id");
	if(nj==NULL) {
		LM_ERR("id attribute is missing\n");
		return 0;
	}
	if(nj->valuestring!=NULL && strlen(nj->valuestring)>JSONRPC_ID_SIZE-1) {
		LM_ERR("id attribute is too long (%lu/%d)\n", (unsigned long)strlen(nj->valuestring),
				JSONRPC_ID_SIZE);
		return 0;
	}
	/* clone the sip msg */
	if(ctx->msg!=NULL) {
		shm_msg=sip_msg_shm_clone(ctx->msg, &len, 1);
		if (shm_msg==0)
			goto error;
	}

	/* alloc into one block */
	size=ROUND_POINTER(sizeof(*ret))+sizeof(jsonrpc_ctx_t);
	if ((ret=shm_malloc(size))==0)
		goto error;
	memset(ret, 0, size);
	ret->rpc=func_param;
	ret->reply_ctx=(char*)ret+ROUND_POINTER(sizeof(*ret));
	r_ctx=ret->reply_ctx;
	r_ctx->flags=ctx->flags | JSONRPC_DELAYED_CTX_F;
	r_ctx->transport=ctx->transport;
	ctx->flags |= JSONRPC_DELAYED_REPLY_F;
	r_ctx->msg=shm_msg;
	r_ctx->msg_shm_block_size=len;

	if(nj->valuestring!=NULL) {
		strcpy(r_ctx->jsrid_val, nj->valuestring);
		r_ctx->jsrid_type = 1;
	} else {
		*(long*)r_ctx->jsrid_val = (long)nj->valuedouble;
		r_ctx->jsrid_type = 2;
	}

	return ret;
error:
	if (shm_msg)
		shm_free(shm_msg);
	if (ret)
		shm_free(ret);
	return NULL;
}


/** Closes a "delayed reply" context and sends the reply.
 * If no reply has been sent the reply will be built and sent automatically.
 * See the notes from rpc_new_delayed_ctx()
 */
static void jsonrpc_delayed_ctx_close(struct rpc_delayed_ctx* dctx)
{
	jsonrpc_ctx_t* r_ctx;
	hdr_field_t* hdr;

	r_ctx=dctx->reply_ctx;
	if (unlikely(!(r_ctx->flags & JSONRPC_DELAYED_CTX_F))){
		BUG("reply ctx not marked as async/delayed\n");
		goto error;
	}

	if (jsonrpc_delayed_reply_ctx_init(r_ctx)<0)
		goto error;

	if (!r_ctx->reply_sent){
		jsonrpc_send(r_ctx);
	}
error:
	jsonrpc_clean_context(r_ctx);
	if(r_ctx->msg) {
		/* free added lumps (rpc_send adds a body lump) */
		del_nonshm_lump( &(r_ctx->msg->add_rm) );
		del_nonshm_lump( &(r_ctx->msg->body_lumps) );
		del_nonshm_lump_rpl( &(r_ctx->msg->reply_lump) );
		/* free header's parsed structures
		 * that were added by failure handlers */
		for( hdr=r_ctx->msg->headers ; hdr ; hdr=hdr->next ) {
			if ( hdr->parsed && hdr_allocs_parse(hdr) &&
					(hdr->parsed<(void*)r_ctx->msg ||
					hdr->parsed>=(void*)(r_ctx->msg+r_ctx->msg_shm_block_size))) {
				/* header parsed filed doesn't point inside uas.request memory
				 * chunk -> it was added by failure funcs.-> free it as pkg */
				DBG("removing hdr->parsed %d\n", hdr->type);
				clean_hdr_field(hdr);
				hdr->parsed = 0;
			}
		}
		shm_free(r_ctx->msg);
	}
	r_ctx->msg=0;
	dctx->reply_ctx=0;
	shm_free(dctx);
	_jsonrpc_ctx_active = NULL;

	return;
}


static int mod_init(void)
{
	memset(&xhttp_api, 0, sizeof(xhttp_api_t));

	_jsonrpcs_stored_id[0] = '0';
	_jsonrpcs_stored_id[1] = '\0';

	/* bind the XHTTP API */
	if(jsonrpc_transport==0 || (jsonrpc_transport&1)) {
		if (xhttp_load_api(&xhttp_api) < 0) {
			if(jsonrpc_transport&1) {
				LM_ERR("cannot bind to XHTTP API\n");
				return -1;
			} else {
				memset(&xhttp_api, 0, sizeof(xhttp_api_t));
			}
		}
	}
	/* prepare fifo transport */
	if(jsonrpc_transport==0 || (jsonrpc_transport&2)) {
		if(jsonrpc_fifo != NULL && *jsonrpc_fifo!=0) {
			LM_DBG("preparing to listen on fifo file: %s\n",
					jsonrpc_fifo);
			if(jsonrpc_fifo_mod_init()<0) {
				if(jsonrpc_transport&2) {
					LM_ERR("cannot initialize fifo transport\n");
					return -1;
				} else {
					jsonrpc_fifo = NULL;
				}
			}
		} else {
			jsonrpc_fifo = NULL;
		}
	} else {
		jsonrpc_fifo = NULL;
	}
	/* prepare datagram transport */
	if(jsonrpc_transport==0 || (jsonrpc_transport&4)) {
		if(jsonrpc_dgram_socket!=NULL && *jsonrpc_dgram_socket!='\0') {
			LM_DBG("preparing to listen on datagram socket: %s\n",
					jsonrpc_dgram_socket);
			if(jsonrpc_dgram_mod_init()<0) {
				if(jsonrpc_transport&4) {
					LM_ERR("cannot initialize datagram transport\n");
					return -1;
				} else {
					jsonrpc_dgram_socket = NULL;
				}
			}
		} else {
			jsonrpc_dgram_socket = NULL;
		}
	} else {
		jsonrpc_dgram_socket = NULL;
	}
	/* prepare tcp transport */
	if(jsonrpc_transport==0 || (jsonrpc_transport&8)) {
		if(jsonrpc_tcp_socket!=NULL && *jsonrpc_tcp_socket!='\0') {
			LM_DBG("preparing to listen on tcp socket: %s\n",
					jsonrpc_tcp_socket);
			if(jsonrpc_tcp_mod_init()<0) {
				if(jsonrpc_transport&8) {
					LM_ERR("cannot initialize tcp transport\n");
					return -1;
				} else {
					jsonrpc_tcp_socket = NULL;
				}
			}
		} else {
			jsonrpc_tcp_socket = NULL;
		}
	} else {
		jsonrpc_tcp_socket = NULL;
	}

	memset(&func_param, 0, sizeof(func_param));
	func_param.send              = (rpc_send_f)jsonrpc_send;
	func_param.fault             = (rpc_fault_f)jsonrpc_fault;
	func_param.add               = (rpc_add_f)jsonrpc_add;
	func_param.scan              = (rpc_scan_f)jsonrpc_scan;
	func_param.rpl_printf        = (rpc_rpl_printf_f)jsonrpc_rpl_printf;
	func_param.struct_add        = (rpc_struct_add_f)jsonrpc_struct_add;
	func_param.array_add         = (rpc_struct_add_f)jsonrpc_array_add;
	func_param.struct_scan       = (rpc_struct_scan_f)jsonrpc_struct_scan;
	func_param.struct_printf     = (rpc_struct_printf_f)jsonrpc_struct_printf;
	func_param.capabilities      = (rpc_capabilities_f)jsonrpc_capabilities;
	func_param.delayed_ctx_new   = (rpc_delayed_ctx_new_f)jsonrpc_delayed_ctx_new;
	func_param.delayed_ctx_close =
		(rpc_delayed_ctx_close_f)jsonrpc_delayed_ctx_close;

	jsonrpc_register_rpc();

	memset(&_jsonrpc_plain_reply, 0, sizeof(jsonrpc_plain_reply_t));
	return 0;
}

static int child_init(int rank)
{
	if (rank==PROC_MAIN) {
		if(jsonrpc_fifo != NULL) {
			if(jsonrpc_fifo_child_init(rank)<0) {
				LM_ERR("failed to init fifo worker\n");
				return -1;
			}
		}
		if(jsonrpc_dgram_socket!=NULL) {
			if(jsonrpc_dgram_child_init(rank)<0) {
				LM_ERR("failed to init datagram workers\n");
				return -1;
			}
		}
		if(jsonrpc_tcp_socket!=NULL) {
			if(jsonrpc_tcp_child_init(rank)<0) {
				LM_ERR("failed to init tcp worker\n");
				return -1;
			}
		}
	}

	return 0;
}

/**
 *
 */
static void mod_destroy(void)
{
	jsonrpc_fifo_destroy();
	jsonrpc_dgram_destroy();

	return;
}

/**
 *
 */
static int ki_jsonrpcs_dispatch(sip_msg_t* msg)
{
	rpc_exportx_t* rpce;
	jsonrpc_ctx_t* ctx;
	int ret = 0;
	srjson_t *nj = NULL;
	str val;
	unsigned int rdata;

	if(!IS_HTTP(msg)) {
		LM_DBG("Got non HTTP msg\n");
		return NONSIP_MSG_PASS;
	}

	if(xhttp_api.reply==NULL) {
		LM_ERR("jsonrpc over http not initialized - check transport param\n");
		return NONSIP_MSG_ERROR;
	}

	/* initialize jsonrpc context */
	_jsonrpc_ctx_active = &_jsonrpc_ctx_global;
	ctx = _jsonrpc_ctx_active;
	memset(ctx, 0, sizeof(jsonrpc_ctx_t));
	ctx->msg = msg;
	/* parse the jsonrpc request */
	ctx->jreq = srjson_NewDoc(NULL);
	if(ctx->jreq==NULL) {
		LM_ERR("Failed to init the json document\n");
		return NONSIP_MSG_ERROR;
	}

	ctx->jreq->buf.s = get_body(msg);
	ctx->jreq->buf.len = strlen(ctx->jreq->buf.s);
	ctx->jreq->root = srjson_Parse(ctx->jreq, ctx->jreq->buf.s);
	if(ctx->jreq->root == NULL)
	{
		LM_ERR("invalid json doc [[%s]]\n", ctx->jreq->buf.s);
		srjson_DeleteDoc(ctx->jreq);
		return NONSIP_MSG_ERROR;
	}
	ctx->transport = JSONRPC_TRANS_HTTP;
	if (jsonrpc_init_reply(ctx) < 0) goto send_reply;

	/* sanity checks on jsonrpc request */
	nj = srjson_GetObjectItem(ctx->jreq, ctx->jreq->root, "jsonrpc");
	if(nj==NULL || nj->valuestring==NULL) {
		LM_ERR("missing or invalid jsonrpc field in request\n");
		goto send_reply;
	}
	val.s = nj->valuestring;
	val.len = strlen(val.s);
	if(val.len!=3 || strncmp(val.s, "2.0", 3)!=0) {
		LM_ERR("unsupported jsonrpc version [%.*s]\n", val.len, val.s);
		goto send_reply;
	}
	/* run jsonrpc command */
	nj = srjson_GetObjectItem(ctx->jreq, ctx->jreq->root, "method");
	if(nj==NULL || nj->valuestring==NULL) {
		LM_ERR("missing or invalid jsonrpc method field in request\n");
		goto send_reply;
	}
	val.s = nj->valuestring;
	val.len = strlen(val.s);
	ctx->method = val.s;
	rpce = rpc_lookupx(val.s, val.len, &rdata);
	if (!rpce || !rpce->r.function) {
		LM_ERR("method callback not found [%.*s]\n", val.len, val.s);
		jsonrpc_fault(ctx, 500, "Method Not Found");
		goto send_reply;
	}
	if (rdata & RPC_EXEC_DELTA) {
		LM_ERR("execution of command [%.*s] is limited by delta [%d]\n",
				val.len, val.s, ksr_rpc_exec_delta);
		jsonrpc_fault(ctx, 500, "Command Executed Too Fast");
		goto send_reply;
	}
	ctx->flags = rpce->r.flags;
	nj = srjson_GetObjectItem(ctx->jreq, ctx->jreq->root, "params");
	if(nj!=NULL && nj->type!=srjson_Array && nj->type!=srjson_Object) {
		LM_ERR("params field is not an array or object\n");
		goto send_reply;
	}
	if(nj!=NULL) ctx->req_node = nj->child;
	rpce->r.function(&func_param, ctx);

send_reply:
	if (!ctx->reply_sent && !(ctx->flags&JSONRPC_DELAYED_REPLY_F)) {
		ret = jsonrpc_send(ctx);
	}
	jsonrpc_clean_context(ctx);
	if (ret < 0) return -1;
	return 1;
}


/**
 *
 */
static int jsonrpc_dispatch(sip_msg_t* msg, char* s1, char* s2)
{
	return ki_jsonrpcs_dispatch(msg);
}

int jsonrpc_exec_ex(str *cmd, str *rpath, str *spath)
{
	rpc_exportx_t* rpce;
	jsonrpc_ctx_t* ctx;
	int ret;
	srjson_t *nj = NULL;
	str val;
	str scmd;
	unsigned int rdata = 0;
	int mode;

	scmd = *cmd;

	/* initialize jsonrpc context */
	_jsonrpc_ctx_active = &_jsonrpc_ctx_global;
	ctx = _jsonrpc_ctx_active;
	memset(ctx, 0, sizeof(jsonrpc_ctx_t));
	ctx->msg = NULL; /* mark it not send a reply out */
	/* parse the jsonrpc request */
	ctx->jreq = srjson_NewDoc(NULL);
	if(ctx->jreq==NULL) {
		LM_ERR("Failed to init the json document\n");
		return -1;
	}
	ctx->jreq->buf = scmd;
	ctx->jreq->root = srjson_Parse(ctx->jreq, ctx->jreq->buf.s);
	if(ctx->jreq->root == NULL) {
		LM_ERR("invalid json doc [[%.*s]]\n",
				ctx->jreq->buf.len, ctx->jreq->buf.s);
		return -1;
	}
	ret = -1;
	if (jsonrpc_init_reply(ctx) < 0) goto send_reply;
	jsonrpc_reset_plain_reply(ctx->jrpl->free_fn);


	/* sanity checks on jsonrpc request */
	nj = srjson_GetObjectItem(ctx->jreq, ctx->jreq->root, "jsonrpc");
	if(nj==NULL) {
		LM_ERR("missing jsonrpc field in request\n");
		goto send_reply;
	}
	val.s = nj->valuestring;
	val.len = strlen(val.s);
	if(val.len!=3 || strncmp(val.s, "2.0", 3)!=0) {
		LM_ERR("unsupported jsonrpc version [%.*s]\n", val.len, val.s);
		goto send_reply;
	}
	/* reply name */
	if(rpath!=NULL) {
		if(rpath->s==NULL || rpath->len<=0) {
			LM_ERR("empty buffer to store the reply name\n");
			goto send_reply;
		}
		nj = srjson_GetObjectItem(ctx->jreq, ctx->jreq->root, "reply_name");
		if(nj==NULL) {
			LM_ERR("missing reply_name field in request\n");
			goto send_reply;
		}
		val.s = nj->valuestring;
		val.len = strlen(val.s);
		if(val.len>=rpath->len) {
			LM_ERR("no space to store reply_name field\n");
			goto send_reply;
		}
		strncpy(rpath->s, val.s, val.len);
		rpath->s[val.len] = 0;
		rpath->len = val.len;
	}
	/* store file path */
	if(spath!=NULL) {
		if(spath->s==NULL || spath->len<=0) {
			LM_ERR("empty buffer to store the output file path\n");
			goto send_reply;
		}
		nj = srjson_GetObjectItem(ctx->jreq, ctx->jreq->root, "store_path");
		if(nj==NULL) {
			LM_DBG("store path not provided in request\n");
			spath->len = 0;
		} else {
			val.s = nj->valuestring;
			val.len = strlen(val.s);
			if(val.len > 0) {
				if(val.len>=spath->len) {
					LM_ERR("no space to store path field\n");
					goto send_reply;
				}
				strncpy(spath->s, val.s, val.len);
				spath->s[val.len] = 0;
				spath->len = val.len;
				mode = 1;
			} else {
				spath->len = 0;
			}
		}
	}

	/* run jsonrpc command */
	nj = srjson_GetObjectItem(ctx->jreq, ctx->jreq->root, "method");
	if(nj==NULL) {
		LM_ERR("missing jsonrpc method field in request\n");
		goto send_reply;
	}
	val.s = nj->valuestring;
	val.len = strlen(val.s);
	ctx->method = val.s;
	rpce = rpc_lookupx(val.s, val.len, &rdata);
	if (!rpce || !rpce->r.function) {
		LM_ERR("method callback not found [%.*s]\n", val.len, val.s);
		jsonrpc_fault(ctx, 500, "Method Not Found");
		goto send_reply;
	}
	if (rdata & RPC_EXEC_DELTA) {
		LM_ERR("execution of command [%.*s] is limited by delta [%d]\n",
				val.len, val.s, ksr_rpc_exec_delta);
		jsonrpc_fault(ctx, 500, "Command Executed Too Fast");
		goto send_reply;
	}
	ctx->flags = rpce->r.flags;
	nj = srjson_GetObjectItem(ctx->jreq, ctx->jreq->root, "params");
	if(nj!=NULL && nj->type!=srjson_Array && nj->type!=srjson_Object) {
		LM_ERR("params field is not an array or object\n");
		goto send_reply;
	}
	if(nj!=NULL) ctx->req_node = nj->child;
	rpce->r.function(&func_param, ctx);
	ret = 1;

send_reply:
	if (!ctx->reply_sent) {
		ret = jsonrpc_send_mode(ctx, mode);
	}
	jsonrpc_clean_context(ctx);
	if (ret < 0) return -1;
	return 1;
}

static int jsonrpc_exec(sip_msg_t* msg, char* cmd, char* s2)
{
	str scmd;

	if(fixup_get_svalue(msg, (gparam_t*)cmd, &scmd)<0 || scmd.len<=0) {
		LM_ERR("cannot get the rpc command parameter\n");
		return -1;
	}
	return jsonrpc_exec_ex(&scmd, NULL, NULL);
}
/**
 *
 */
static const char* jsonrpc_rpc_echo_doc[2] = {
	"Sample echo command",
	0
};

/**
 *
 */
static void jsonrpc_rpc_echo(rpc_t* rpc, void* ctx)
{
	str sval = {"", 0};
	int ival = 0;

	if(rpc->scan(ctx, "*.S", &sval)>0) {
		LM_DBG("READ STR: %.*s\n", sval.len, sval.s);
		rpc->add(ctx, "S", &sval);
		if(rpc->scan(ctx, "*.d", &ival)>0) {
			LM_DBG("READ INT: %d\n", ival);
			rpc->add(ctx, "d", ival);
		}
	} else {
		LM_DBG("no parameters\n");
	}
}
/**
 *
 */
static rpc_export_t jsonrpc_rpc[] = {
	{"jsonrpc.echo", jsonrpc_rpc_echo,  jsonrpc_rpc_echo_doc,       RET_ARRAY},
	{0, 0, 0, 0}
};

/**
 *
 */
static int jsonrpc_register_rpc(void)
{
	if (rpc_register_array(jsonrpc_rpc)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
static int jsonrpc_pv_get_jrpl(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	switch(param->pvn.u.isname.name.n)
	{
		case 0:
			return pv_get_uintval(msg, param, res,
					(unsigned int)_jsonrpc_plain_reply.rcode);
		case 1:
			if(_jsonrpc_plain_reply.rtext.s==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_jsonrpc_plain_reply.rtext);
		case 2:
			if(_jsonrpc_plain_reply.rbody.s==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_jsonrpc_plain_reply.rbody);
		default:
			return pv_get_null(msg, param, res);
	}
}

/**
 *
 */
static int jsonrpc_pv_parse_jrpl_name(pv_spec_t *sp, str *in)
{
	if(in->len!=4) {
		LM_ERR("unknown inner name [%.*s]\n", in->len, in->s);
		return -1;
	}
	if(strncmp(in->s, "code", 4)==0) {
		sp->pvp.pvn.u.isname.name.n = 0;
	} else if(strncmp(in->s, "text", 4)==0) {
		sp->pvp.pvn.u.isname.name.n = 1;
	} else if(strncmp(in->s, "body", 4)==0) {
		sp->pvp.pvn.u.isname.name.n = 2;
	} else {
		LM_ERR("unknown inner name [%.*s]\n", in->len, in->s);
		return -1;
	}
	return 0;
}

/**
 *
 */
static int ki_jsonrpcs_exec(sip_msg_t *msg, str *scmd)
{
	return jsonrpc_exec_ex(scmd, NULL, NULL);
}

/**
 *
 */
static sr_kemi_xval_t _sr_kemi_jsonrpcs_xval = {0};

/**
 *
 */
static sr_kemi_xval_t* ki_jsonrpcs_response(sip_msg_t *msg)
{
	if(_jsonrpc_plain_reply.rbody.s==NULL) {
		sr_kemi_xval_null(&_sr_kemi_jsonrpcs_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_sr_kemi_jsonrpcs_xval;
	}
	_sr_kemi_jsonrpcs_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_jsonrpcs_xval.v.s = _jsonrpc_plain_reply.rbody;
	return &_sr_kemi_jsonrpcs_xval;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_jsonrpcs_exports[] = {
	{ str_init("jsonrpcs"), str_init("dispatch"),
		SR_KEMIP_INT, ki_jsonrpcs_dispatch,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("jsonrpcs"), str_init("exec"),
		SR_KEMIP_INT, ki_jsonrpcs_exec,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("jsonrpcs"), str_init("execx"),
		SR_KEMIP_INT, ki_jsonrpcs_exec,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("jsonrpcs"), str_init("response"),
		SR_KEMIP_XVAL, ki_jsonrpcs_response,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_jsonrpcs_exports);
	return 0;
}

/** @} */
