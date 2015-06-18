/**
 * Copyright (C) 2014 Daniel-Constantin Mierla (asipto.com)
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

#include "../../ver.h"
#include "../../trim.h"
#include "../../pt.h"
#include "../../sr_module.h"
#include "../../mod_fix.h"
#include "../../nonsip_hooks.h"
#include "../../cfg/cfg_struct.h"
#include "../../modules/xhttp/api.h"

#include "jsonrpc-s_mod.h"

/** @addtogroup jsonrpc-s
 * @ingroup modules
 * @{
 *
 * <h1>Overview of Operation</h1>
 * This module provides jsonrpc over http server implementation.
 */

/** @file
 *
 * This is the main file of jsonrpc-s module which contains all the functions
 * related to http processing, as well as the module interface.
 */

MODULE_VERSION


#define jsonrpc_malloc	pkg_malloc
#define jsonrpc_free	pkg_free

static str JSONRPC_REASON_OK = str_init("OK");
static str JSONRPC_CONTENT_TYPE_HTML = str_init("application/json");

/* FIFO server vars */
static char *jsonrpc_fifo = NULL;				/*!< FIFO file name */
static char *jsonrpc_fifo_reply_dir = "/tmp/"; 	/*!< dir where reply fifos are allowed */
static int  jsonrpc_fifo_uid = -1;				/*!< Fifo default UID */
static char *jsonrpc_fifo_uid_s = 0;			/*!< Fifo default User ID name */
static int  jsonrpc_fifo_gid = -1;				/*!< Fifo default Group ID */
static char *jsonrpc_fifo_gid_s = 0;			/*!< Fifo default Group ID name */
static int  jsonrpc_fifo_mode = S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP; /* Default file mode rw-rw---- */

static int  jsonrpc_transport = 0;				/*!< 0 - all available; 1 - http; 2 - fifo */

static int jsonrpc_pretty_format = 0;

static int jsonrpc_register_rpc(void);

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);
static int jsonrpc_dispatch(sip_msg_t* msg, char* s1, char* s2);
static int jsonrpc_exec(sip_msg_t* msg, char* cmd, char* s2);

static int jsonrpc_init_fifo_file(void);
static void jsonrpc_fifo_process(int rank);

/** The context of the jsonrpc request being processed.
 *
 * This is a global variable that records the context of the jsonrpc request
 * being currently processed.
 * @sa rpc_ctx
 */
static jsonrpc_ctx_t _jsonrpc_ctx;

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
	{"fifo_name",        PARAM_STRING, &jsonrpc_fifo},
	{"fifo_mode",        PARAM_INT,	   &jsonrpc_fifo_mode},
	{"fifo_group",       PARAM_STRING, &jsonrpc_fifo_gid_s},
	{"fifo_group",       PARAM_INT,    &jsonrpc_fifo_gid},
	{"fifo_user",        PARAM_STRING, &jsonrpc_fifo_uid_s},
	{"fifo_user",        PARAM_INT,    &jsonrpc_fifo_uid},
	{"fifo_reply_dir",   PARAM_STRING, &jsonrpc_fifo_reply_dir},
	{"transport",        PARAM_INT,    &jsonrpc_transport},
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
struct module_exports exports= {
	"jsonrpc-s",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,			/* exported statistics */
	0,			/* exported MI functions */
	mod_pvs,	/* exported pseudo-variables */
	0,			/* extra processes */
	mod_init,	/* module initialization function */
	0,
	mod_destroy,/* module destroy function */
	child_init	/* per-child init function */
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

typedef struct jsonrpc_plain_reply {
	int rcode;         /**< reply code */
	str rtext;         /**< reply reason text */
	str rbody;             /**< reply body */
} jsonrpc_plain_reply_t;

static jsonrpc_plain_reply_t _jsonrpc_plain_reply;

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
static void jsonrpc_fault(jsonrpc_ctx_t* ctx, int code, char* fmt, ...)
{
	va_list ap;

	ctx->http_code = code;
	va_start(ap, fmt);
	vsnprintf(jsonrpc_error_buf, JSONRPC_ERROR_REASON_BUF_LEN, fmt, ap);
	va_end(ap);
	ctx->http_text.len = strlen(jsonrpc_error_buf);
	ctx->http_text.s = jsonrpc_error_buf;
	if(ctx->error_code == 0) ctx->error_code = -32000;

	return;
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
				srjson_AddStrStrToObject(ctx->jrpl, nj,
					"message", 7, "Unexpected Error", 16);
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
	nj = srjson_GetObjectItem(ctx->jreq, ctx->jreq->root, "id");
	if(nj!=NULL) {
		if(nj->valuestring!=NULL) {
			srjson_AddStrStrToObject(ctx->jrpl, ctx->jrpl->root,
					"id", 2,
					nj->valuestring, strlen(nj->valuestring));
		} else {
			srjson_AddNumberToObject(ctx->jrpl, ctx->jrpl->root, "id",
					nj->valueint);
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
		if(ctx->msg) {
			xhttp_api.reply(ctx->msg, ctx->http_code, &ctx->http_text,
				&JSONRPC_CONTENT_TYPE_HTML, &rbuf);
		} else {
			jsonrpc_set_plain_reply(ctx->http_code, &ctx->http_text, &rbuf,
					ctx->jrpl->free_fn);
			rbuf.s=NULL;
		}
	} else {
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
	struct tm* t;
	str *sp;

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
	case 'b':
		nj = srjson_CreateBool(ctx->jrpl, ((va_arg(*ap, int)==0)?0:1));
		break;
	case 't':
		dt = va_arg(*ap, time_t);
		t = gmtime(&dt);
		if (strftime(buf, JSONRPC_PRINT_VALUE_BUF_LEN,
				"%Y%m%dT%H:%M:%S", t) == 0) {
			LM_ERR("Error while converting time\n");
			return NULL;
		}
		nj = srjson_CreateString(ctx->jrpl, buf);
		break;
	case 's':
		nj = srjson_CreateString(ctx->jrpl, va_arg(*ap, char*));
		break;
	case 'S':
		sp = va_arg(*ap, str*);
		nj = srjson_CreateStr(ctx->jrpl, sp->s, sp->len);
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
	char **char_ptr;
	double *double_ptr;
	str *str_ptr;
	int mandatory_param = 1;
	int modifiers = 0;
	int auto_convert = 0;
	char* orig_fmt;
	va_list ap;
	str stmp;

	if(ctx->req_node==NULL)
		return 0;

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
		case 't': /* Date and time */
		case 'd': /* Integer */
			int_ptr = va_arg(ap, int*);
			*int_ptr = ctx->req_node->valueint;
			break;
		case 'u': /* Integer */
			uint_ptr = va_arg(ap, unsigned int*);
			*uint_ptr = (unsigned int)ctx->req_node->valueint;
			break;
		case 'f': /* double */
			double_ptr = va_arg(ap, double*);
			*double_ptr = ctx->req_node->valuedouble;
			break;
		case 's': /* zero terminated string */
			char_ptr = va_arg(ap, char**);
			if(ctx->req_node->type==srjson_String) {
				*char_ptr = ctx->req_node->valuestring;
			} else if(auto_convert == 1) {
				if(ctx->req_node->type==srjson_Number) {
					*char_ptr = int2str(ctx->req_node->valueint, &stmp.len);
				} else {
					*char_ptr = NULL;
					goto error;
				}
			} else {
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
					str_ptr->s = int2str(ctx->req_node->valueint,
							&str_ptr->len);
				} else {
					str_ptr->s = NULL;
					str_ptr->len = 0;
					goto error;
				}
			} else {
				str_ptr->s = NULL;
				str_ptr->len = 0;
				goto error;
			}
			break;
		case '{':
		case '[':
			LM_ERR("Unsupported param type '%c'\n", *fmt);
			jsonrpc_fault(ctx, 500, "Unsupported param type");
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
	if(*fmt && mandatory_param==1)
		goto error;

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

	ctx = &_jsonrpc_ctx;
	if(ctx->jrpl==NULL) {
		LM_ERR("reply object not initialized in rpl context\n");
		return -1;
	}

	va_start(ap, fmt);
	while(*fmt) {
		mname.s = va_arg(ap, char*);
		mname.len = (mname.s?strlen(mname.s):0);

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
		if(isobject) {
			/* add as member to object */
			srjson_AddItemToObject(ctx->jrpl, jnode, mname.s, nj);
		} else {
			/* wrap member in a new object and add to array */
			wj = srjson_CreateObject(ctx->jrpl);
			if(wj==NULL) {
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

	ctx = &_jsonrpc_ctx;
	if(ctx->jrpl==NULL) {
		LM_ERR("reply object not initialized in rpl context\n");
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

	ctx = &_jsonrpc_ctx;
	if(ctx->jrpl==NULL) {
		LM_ERR("reply object not initialized in rpl context\n");
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


/** Returns the RPC capabilities supported by the xmlrpc driver.
 */
static rpc_capabilities_t jsonrpc_capabilities(jsonrpc_ctx_t* ctx)
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
static struct rpc_delayed_ctx* jsonrpc_delayed_ctx_new(jsonrpc_ctx_t* ctx)
{
	return NULL;
}


/** Closes a "delayed reply" context and sends the reply.
 * If no reply has been sent the reply will be built and sent automatically.
 * See the notes from rpc_new_delayed_ctx()
 */
static void jsonrpc_delayed_ctx_close(struct rpc_delayed_ctx* dctx)
{
	return;
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

static int mod_init(void)
{
	int sep;
	int len;
	char *p;

	/* bind the XHTTP API */
	if(jsonrpc_transport==0 || jsonrpc_transport==1) {
		if (xhttp_load_api(&xhttp_api) < 0) {
			if(jsonrpc_transport==1) {
				LM_ERR("cannot bind to XHTTP API\n");
				return -1;
			} else {
				memset(&xhttp_api, 0, sizeof(xhttp_api_t));
			}
		}
	}
	if(jsonrpc_transport==0 || jsonrpc_transport==2) {
		if(jsonrpc_fifo != NULL && *jsonrpc_fifo!=0) {
			if(*jsonrpc_fifo != '/') {
				if(runtime_dir!=NULL && *runtime_dir!=0) {
					len = strlen(runtime_dir);
					sep = 0;
					if(runtime_dir[len-1]!='/') {
						sep = 1;
					}
					len += sep + strlen(jsonrpc_fifo);
					p = pkg_malloc(len + 1);
					if(p==NULL) {
						LM_ERR("no more pkg\n");
						return -1;
					}
					strcpy(p, runtime_dir);
					if(sep) strcat(p, "/");
					strcat(p, jsonrpc_fifo);
					jsonrpc_fifo = p;
					LM_DBG("fifo path is [%s]\n", jsonrpc_fifo);
				}
			}
		}
		if(jsonrpc_init_fifo_file()<0) {
			if(jsonrpc_transport==2) {
				LM_ERR("cannot initialize fifo transport\n");
				return -1;
			} else {
				jsonrpc_fifo = NULL;
			}
		}
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
	int pid;

	if (rank==PROC_MAIN) {
		if(jsonrpc_fifo != NULL) {
			pid=fork_process(PROC_NOCHLDINIT, "JSONRPC-S FIFO", 1);
			if (pid<0)
				return -1; /* error */
			if(pid==0) {
				/* child */

				/* initialize the config framework */
				if (cfg_child_init())
					return -1;

				jsonrpc_fifo_process(1);
			}
		}
	}

	if(rank==PROC_MAIN || rank==PROC_TCP_MAIN)
	{
		return 0; /* do nothing for the main process */
	}

	return 0;
}

static void mod_destroy(void)
{
	int n;
	struct stat filestat;

	if(jsonrpc_fifo==NULL)
		return;

	/* destroying the fifo file */
	n=stat(jsonrpc_fifo, &filestat);
	if (n==0){
		/* FIFO exist, delete it (safer) if not config check */
		if(config_check==0) {
			if (unlink(jsonrpc_fifo)<0){
				LM_ERR("cannot delete the fifo (%s): %s\n",
					jsonrpc_fifo, strerror(errno));
				goto error;
			}
		}
	} else if (n<0 && errno!=ENOENT) {
		LM_ERR("FIFO stat failed: %s\n", strerror(errno));
		goto error;
	}

	return;
error:
	return;
}

static int jsonrpc_dispatch(sip_msg_t* msg, char* s1, char* s2)
{
	rpc_export_t* rpce;
	jsonrpc_ctx_t* ctx;
	int ret = 0;
	srjson_t *nj = NULL;
	str val;

	if(!IS_HTTP(msg)) {
		LM_DBG("Got non HTTP msg\n");
		return NONSIP_MSG_PASS;
	}

	/* initialize jsonrpc context */
	ctx = &_jsonrpc_ctx;
	memset(ctx, 0, sizeof(jsonrpc_ctx_t));
	ctx->msg = msg;
	/* parse the jsonrpc request */
	ctx->jreq = srjson_NewDoc(NULL);
	if(ctx->jreq==NULL) {
		LM_ERR("Failed to init the json document\n");
		return NONSIP_MSG_PASS;
	}

	ctx->jreq->buf.s = get_body(msg);
	ctx->jreq->buf.len = strlen(ctx->jreq->buf.s);
	ctx->jreq->root = srjson_Parse(ctx->jreq, ctx->jreq->buf.s);
	if(ctx->jreq->root == NULL)
	{
		LM_ERR("invalid json doc [[%s]]\n", ctx->jreq->buf.s);
		return NONSIP_MSG_PASS;
	}
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
	rpce = find_rpc_export(ctx->method, 0);
	if (!rpce || !rpce->function) {
		LM_ERR("method callback not found [%.*s]\n", val.len, val.s);
		jsonrpc_fault(ctx, 500, "Method Not Found");
		goto send_reply;
	}
	ctx->flags = rpce->flags;
	nj = srjson_GetObjectItem(ctx->jreq, ctx->jreq->root, "params");
	if(nj!=NULL && nj->type!=srjson_Array && nj->type!=srjson_Object) {
		LM_ERR("params field is not an array or object\n");
		goto send_reply;
	}
	if(nj!=NULL) ctx->req_node = nj->child;
	rpce->function(&func_param, ctx);

send_reply:
	if (!ctx->reply_sent) {
		ret = jsonrpc_send(ctx);
	}
	jsonrpc_clean_context(ctx);
	if (ret < 0) return -1;
	return 1;
}


static int jsonrpc_exec_ex(str *cmd, str *rpath)
{
	rpc_export_t* rpce;
	jsonrpc_ctx_t* ctx;
	int ret;
	srjson_t *nj = NULL;
	str val;
	str scmd;

	scmd = *cmd;

	/* initialize jsonrpc context */
	ctx = &_jsonrpc_ctx;
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
	if(ctx->jreq->root == NULL)
	{
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
	/* run jsonrpc command */
	nj = srjson_GetObjectItem(ctx->jreq, ctx->jreq->root, "method");
	if(nj==NULL) {
		LM_ERR("missing jsonrpc method field in request\n");
		goto send_reply;
	}
	val.s = nj->valuestring;
	val.len = strlen(val.s);
	ctx->method = val.s;
	rpce = find_rpc_export(ctx->method, 0);
	if (!rpce || !rpce->function) {
		LM_ERR("method callback not found [%.*s]\n", val.len, val.s);
		jsonrpc_fault(ctx, 500, "Method Not Found");
		goto send_reply;
	}
	ctx->flags = rpce->flags;
	nj = srjson_GetObjectItem(ctx->jreq, ctx->jreq->root, "params");
	if(nj!=NULL && nj->type!=srjson_Array && nj->type!=srjson_Object) {
		LM_ERR("params field is not an array or object\n");
		goto send_reply;
	}
	if(nj!=NULL) ctx->req_node = nj->child;
	rpce->function(&func_param, ctx);
	ret = 1;

send_reply:
	if (!ctx->reply_sent) {
		ret = jsonrpc_send(ctx);
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
	return jsonrpc_exec_ex(&scmd, NULL);
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
	str sval;
	int ival = 0;
	int ret;
	ret = rpc->scan(ctx, "S*d", &sval, &ival);
	if(ret>0) {
		LM_DBG("READ STR: %.*s\n", sval.len, sval.s);
		rpc->add(ctx, "S", &sval);
	}
	if(ret>1) {
		LM_DBG("READ INT: %d\n", ival);
		rpc->add(ctx, "d", ival);
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

/* FIFO TRANSPORT */

/*! \brief Initialize fifo transport */
static int jsonrpc_init_fifo_file(void)
{
	int n;
	struct stat filestat;

	/* checking the jsonrpc_fifo module param */
	if (jsonrpc_fifo==NULL || *jsonrpc_fifo == 0) {
		jsonrpc_fifo=NULL;
		LM_DBG("No fifo configured\n");
		return 0;
	}

	LM_DBG("testing if fifo file exists ...\n");
	n=stat(jsonrpc_fifo, &filestat);
	if (n==0) {
		/* FIFO exist, delete it (safer) if no config check */
		if(config_check==0) {
			if (unlink(jsonrpc_fifo)<0){
				LM_ERR("Cannot delete old fifo (%s): %s\n",
					jsonrpc_fifo, strerror(errno));
				return -1;
			}
		}
	} else if (n<0 && errno!=ENOENT){
		LM_ERR("MI FIFO stat failed: %s\n", strerror(errno));
		return -1;
	}

	/* checking the fifo_reply_dir param */
	if(!jsonrpc_fifo_reply_dir || *jsonrpc_fifo_reply_dir == 0) {
		LM_ERR("fifo_reply_dir parameter is empty\n");
		return -1;
	}

	/* Check if the directory for the reply fifo exists */
	n = stat(jsonrpc_fifo_reply_dir, &filestat);
	if(n < 0){
		LM_ERR("Directory stat for MI Fifo reply failed: %s\n", strerror(errno));
		return -1;
	}

	if(S_ISDIR(filestat.st_mode) == 0){
		LM_ERR("fifo_reply_dir parameter is not a directory\n");
		return -1;
	}

	/* check fifo_mode */
	if(!jsonrpc_fifo_mode){
		LM_WARN("cannot specify fifo_mode = 0, forcing it to rw-------\n");
		jsonrpc_fifo_mode = S_IRUSR | S_IWUSR;
	}

	if (jsonrpc_fifo_uid_s){
		if (user2uid(&jsonrpc_fifo_uid, &jsonrpc_fifo_gid, jsonrpc_fifo_uid_s)<0){
			LM_ERR("Bad user name %s\n", jsonrpc_fifo_uid_s);
			return -1;
		}
	}

	if (jsonrpc_fifo_gid_s){
		if (group2gid(&jsonrpc_fifo_gid, jsonrpc_fifo_gid_s)<0){
			LM_ERR("Bad group name %s\n", jsonrpc_fifo_gid_s);
			return -1;
		}
	}

	/* add space for one extra process */
	register_procs(1);

	/* add child to update local config framework structures */
	cfg_register_child(1);

	return 0;
}


static int  jsonrpc_fifo_read = 0;
static int  jsonrpc_fifo_write = 0;
#define JSONRPC_MAX_FILENAME	128
static char *jsonrpc_reply_fifo_s = NULL;
static int jsonrpc_reply_fifo_len = 0;

/*! \brief Initialize Fifo server */
FILE *jsonrpc_init_fifo_server(char *fifo_name, int fifo_mode,
						int fifo_uid, int fifo_gid, char* fifo_reply_dir)
{
	FILE *fifo_stream;
	long opt;

	/* create FIFO ... */
	if ((mkfifo(fifo_name, fifo_mode)<0)) {
		LM_ERR("Can't create FIFO: %s (mode=%d)\n", strerror(errno), fifo_mode);
		return 0;
	}

	LM_DBG("FIFO created @ %s\n", fifo_name );

	if ((chmod(fifo_name, fifo_mode)<0)) {
		LM_ERR("Can't chmod FIFO: %s (mode=%d)\n", strerror(errno), fifo_mode);
		return 0;
	}

	if ((fifo_uid!=-1) || (fifo_gid!=-1)){
		if (chown(fifo_name, fifo_uid, fifo_gid)<0){
			LM_ERR("Failed to change the owner/group for %s  to %d.%d; %s[%d]\n",
				fifo_name, fifo_uid, fifo_gid, strerror(errno), errno);
			return 0;
		}
	}

	LM_DBG("fifo %s opened, mode=%o\n", fifo_name, fifo_mode );

	/* open it non-blocking or else wait here until someone
	 * opens it for writing */
	jsonrpc_fifo_read=open(fifo_name, O_RDONLY|O_NONBLOCK, 0);
	if (jsonrpc_fifo_read<0) {
		LM_ERR("Can't open fifo %s for reading - fifo_read did not open: %s\n", fifo_name, strerror(errno));
		return 0;
	}

	fifo_stream = fdopen(jsonrpc_fifo_read, "r");
	if (fifo_stream==NULL) {
		LM_ERR("fdopen failed on %s: %s\n", fifo_name, strerror(errno));
		return 0;
	}

	/* make sure the read fifo will not close */
	jsonrpc_fifo_write=open(fifo_name, O_WRONLY|O_NONBLOCK, 0);
	if (jsonrpc_fifo_write<0) {
		LM_ERR("fifo_write did not open: %s\n", strerror(errno));
		return 0;
	}
	/* set read fifo blocking mode */
	if ((opt=fcntl(jsonrpc_fifo_read, F_GETFL))==-1){
		LM_ERR("fcntl(F_GETFL) failed: %s [%d]\n", strerror(errno), errno);
		return 0;
	}
	if (fcntl(jsonrpc_fifo_read, F_SETFL, opt & (~O_NONBLOCK))==-1){
		LM_ERR("cntl(F_SETFL) failed: %s [%d]\n", strerror(errno), errno);
		return 0;
	}

	jsonrpc_reply_fifo_s = pkg_malloc(JSONRPC_MAX_FILENAME);
	if (jsonrpc_reply_fifo_s==NULL) {
		LM_ERR("no more private memory\n");
		return 0;
	}

	/* init fifo reply dir buffer */
	jsonrpc_reply_fifo_len = strlen(fifo_reply_dir);
	memcpy( jsonrpc_reply_fifo_s, jsonrpc_fifo_reply_dir, jsonrpc_reply_fifo_len);

	return fifo_stream;
}

/*! \brief Read input on fifo */
int jsonrpc_read_stream(char *b, int max, FILE *stream, int *lread)
{
	int retry_cnt;
	int len;
	char *p;
	int sstate;
	int pcount;
	int pfound;
	int stype;

	sstate = 0;
	retry_cnt=0;

	*lread = 0;
	p = b;
	pcount = 0;
	pfound = 0;
	stype = 0;

	while(1) {
		len = fread (p, 1, 1, stream);
		if(len==0) {
			LM_ERR("fifo server fread failed: %s\n", strerror(errno));
			/* on Linux, sometimes returns ESPIPE -- give
			   it few more chances
			*/
			if (errno==ESPIPE) {
				retry_cnt++;
				if (retry_cnt>4)
					return -1;
				continue;
			}
			/* interrupted by signal or ... */
			if ((errno==EINTR)||(errno==EAGAIN))
				continue;
			return -1;
		}
		if(*p=='"' && (sstate==0 || stype==1)) {
			if(*lread>0) {
				if(*(p-1)!='\\') {
					sstate = (sstate+1) % 2;
					stype = 1;
				}
			} else {
				sstate = (sstate+1) % 2;
				stype = 1;
			}
		} else if(*p=='\'' && (sstate==0 || stype==2)) {
			if(*lread>0) {
				if(*(p-1)!='\\') {
					sstate = (sstate+1) % 2;
					stype = 2;
				}
			} else {
				sstate = (sstate+1) % 2;
				stype = 2;
			}
		} else if(*p=='{') {
			if(sstate==0) {
				pfound = 1;
				pcount++;
			}
		} else if(*p=='}') {
			if(sstate==0)
				pcount--;
		}
		*lread = *lread + 1;
		if(*lread>=max-1) {
			LM_WARN("input data too large (%d)\n", *lread);
			return -1;
		}
		p++;
		if(pfound==1 && pcount==0) {
			*p = 0;
			return 0;
		}
	}

	return -1;
}

/*! \brief reply fifo security checks:
 *
 * checks if fd is a fifo, is not hardlinked and it's not a softlink
 * opened file descriptor + file name (for soft link check)
 * \return 0 if ok, <0 if not */
static int jsonrpc_fifo_check(int fd, char* fname)
{
	struct stat fst;
	struct stat lst;
	
	if (fstat(fd, &fst)<0){
		LM_ERR("security: fstat on %s failed: %s\n", fname, strerror(errno));
		return -1;
	}
	/* check if fifo */
	if (!S_ISFIFO(fst.st_mode)){
		LM_ERR("security: %s is not a fifo\n", fname);
		return -1;
	}
	/* check if hard-linked */
	if (fst.st_nlink>1){
		LM_ERR("security: fifo_check: %s is hard-linked %d times\n", fname, (unsigned)fst.st_nlink);
		return -1;
	}

	/* lstat to check for soft links */
	if (lstat(fname, &lst)<0){
		LM_ERR("security: lstat on %s failed: %s\n", fname, strerror(errno));
		return -1;
	}
	if (S_ISLNK(lst.st_mode)){
		LM_ERR("security: fifo_check: %s is a soft link\n", fname);
		return -1;
	}
	/* if this is not a symbolic link, check to see if the inode didn't
	 * change to avoid possible sym.link, rm sym.link & replace w/ fifo race
	 */
	if ((lst.st_dev!=fst.st_dev)||(lst.st_ino!=fst.st_ino)){
		LM_ERR("security: fifo_check: inode/dev number differ: %d %d (%s)\n",
			(int)fst.st_ino, (int)lst.st_ino, fname);
		return -1;
	}
	/* success */
	return 0;
}

#define JSONRPC_REPLY_RETRIES 4
FILE *jsonrpc_open_reply_fifo(str *srpath)
{
	int retries=JSONRPC_REPLY_RETRIES;
	int fifofd;
	FILE *file_handle;
	int flags;

	if ( memchr(srpath->s, '.', srpath->len)
			|| memchr(srpath->s, '/', srpath->len)
			|| memchr(srpath->s, '\\', srpath->len) ) {
		LM_ERR("Forbidden reply fifo filename: %.*s\n",
				srpath->len, srpath->s);
		return 0;
	}

	if (jsonrpc_reply_fifo_len + srpath->len + 1 > JSONRPC_MAX_FILENAME) {
		LM_ERR("Reply fifo filename too long %d\n",
				jsonrpc_reply_fifo_len + srpath->len);
		return 0;
	}

	memcpy(jsonrpc_reply_fifo_s + jsonrpc_reply_fifo_len, srpath->s, srpath->len);
	jsonrpc_reply_fifo_s[jsonrpc_reply_fifo_len + srpath->len]=0;


tryagain:
	/* open non-blocking to make sure that a broken client will not 
	 * block the FIFO server forever */
	fifofd=open( jsonrpc_reply_fifo_s, O_WRONLY | O_NONBLOCK );
	if (fifofd==-1) {
		/* retry several times if client is not yet ready for getting
		   feedback via a reply pipe
		*/
		if (errno==ENXIO) {
			/* give up on the client - we can't afford server blocking */
			if (retries==0) {
				LM_ERR("no client at %s\n", jsonrpc_reply_fifo_s );
				return 0;
			}
			/* don't be noisy on the very first try */
			if (retries != JSONRPC_REPLY_RETRIES)
				LM_DBG("mi_fifo retry countdown: %d\n", retries );
			sleep_us( 80000 );
			retries--;
			goto tryagain;
		}
		/* some other opening error */
		LM_ERR("open error (%s): %s\n", jsonrpc_reply_fifo_s, strerror(errno));
		return 0;
	}

	/* security checks: is this really a fifo?, is 
	 * it hardlinked? is it a soft link? */
	if (jsonrpc_fifo_check(fifofd, jsonrpc_reply_fifo_s)<0)
		goto error;

	/* we want server blocking for big writes */
	if ( (flags=fcntl(fifofd, F_GETFL, 0))<0) {
		LM_ERR("pipe (%s): getfl failed: %s\n", jsonrpc_reply_fifo_s, strerror(errno));
		goto error;
	}
	flags&=~O_NONBLOCK;
	if (fcntl(fifofd, F_SETFL, flags)<0) {
		LM_ERR("pipe (%s): setfl cntl failed: %s\n", jsonrpc_reply_fifo_s, strerror(errno));
		goto error;
	}

	/* create an I/O stream */
	file_handle=fdopen( fifofd, "w");
	if (file_handle==NULL) {
		LM_ERR("open error (%s): %s\n",
			jsonrpc_reply_fifo_s, strerror(errno));
		goto error;
	}
	return file_handle;
error:
	close(fifofd);
	return 0;
}

#define JSONRPC_BUF_IN_SIZE	4096
static void jsonrpc_run_fifo_server(FILE *fifo_stream)
{
	FILE *reply_stream;
	char buf_in[JSONRPC_BUF_IN_SIZE];
	char buf_rpath[128];
	int lread;
	str scmd;
	str srpath;
	int nw;

	while(1) {
		/* update the local config framework structures */
		cfg_update();

		reply_stream = NULL;
		lread = 0;
		if(jsonrpc_read_stream(buf_in, JSONRPC_BUF_IN_SIZE,
					fifo_stream, &lread)<0 || lread<=0) {
			LM_DBG("failed to get the json document from fifo stream\n");
			continue;
		}
		scmd.s = buf_in;
		scmd.len = lread;
		trim(&scmd);
		LM_DBG("preparing to execute fifo jsonrpc [%.*s]\n", scmd.len, scmd.s);
		srpath.s = buf_rpath;
		srpath.len = 128;
		if(jsonrpc_exec_ex(&scmd, &srpath)<0) {
			LM_ERR("failed to execute the json document from fifo stream\n");
			continue;
		}

		LM_DBG("command executed - result: [%.*s] [%d] [%p] [%.*s]\n",
				srpath.len, srpath.s,
				_jsonrpc_plain_reply.rcode,
				_jsonrpc_plain_reply.rbody.s,
				_jsonrpc_plain_reply.rbody.len, _jsonrpc_plain_reply.rbody.s);
		if(srpath.len>0) {
			reply_stream = jsonrpc_open_reply_fifo(&srpath);
			if (reply_stream==NULL) {
				LM_ERR("cannot open reply fifo: %.*s\n", srpath.len, srpath.s);
				continue;
			}
			nw = fwrite(_jsonrpc_plain_reply.rbody.s, 1,
						_jsonrpc_plain_reply.rbody.len, reply_stream);
			if(nw < _jsonrpc_plain_reply.rbody.len) {
				LM_ERR("failed to write the reply to fifo: %d out of %d\n",
						nw, _jsonrpc_plain_reply.rbody.len);
			}
			fclose(reply_stream);

		}
	}
	return;
}

static void jsonrpc_fifo_process(int rank)
{
	FILE *fifo_stream;

	LM_DBG("new process with pid = %d created\n",getpid());

	fifo_stream = jsonrpc_init_fifo_server( jsonrpc_fifo, jsonrpc_fifo_mode,
		jsonrpc_fifo_uid, jsonrpc_fifo_gid, jsonrpc_fifo_reply_dir);
	if ( fifo_stream==NULL ) {
		LM_CRIT("The function jsonrpc_init_fifo_server returned with error!!!\n");
		exit(-1);
	}

	jsonrpc_run_fifo_server( fifo_stream );

	LM_CRIT("the function jsonroc_fifo_server returned with error!!!\n");
	exit(-1);
}
