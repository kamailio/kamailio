/*
 * $Id$
 *
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define _XOPEN_SOURCE 4           /* strptime */
#define _XOPEN_SOURCE_EXTENDED 1  /* solaris */
#define _SVID_SOURCE 1            /* timegm */

#include <strings.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <signal.h>
#include <libxml/xmlreader.h>
#include "../../str.h"
#include "../../sr_module.h"
#include "../../error.h"
#include "../../usr_avp.h"
#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"
#include "../../parser/msg_parser.h"
#include "../../ut.h"
#include "../../dset.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../msg_translator.h"
#include "../../select.h"
#include "../../receive.h" /* needed by process_rpc / receive_msg() */
#include "../sl/sl.h"
#include "../../nonsip_hooks.h"
#include "../../action.h" /* run_actions */
#include "../../script_cb.h" /* exec_*_req_cb */
#include "../../route.h" /* route_get */
#include "http.h"

/*
 * FIXME: Decouple code and reason phrase from reply body
 *        Escape special characters in strings
 */

MODULE_VERSION

int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

static int process_xmlrpc(struct sip_msg* msg);
static int dispatch_rpc(struct sip_msg* msg, char* s1, char* s2);
static int xmlrpc_reply(struct sip_msg* msg, char* code, char* reason);
static int mod_init(void);

/* first line (w/o the version) of the sip msg created from the http xmlrpc */
#define XMLRPC_URI "sip:127.0.0.1:9"
#define XMLRPC_URI_LEN (sizeof(XMLRPC_URI)-1)

#define HTTP_GET		"GET"
#define HTTP_GET_LEN	(sizeof(HTTP_GET)-1)
#define HTTP_POST		"POST"
#define HTTP_POST_LEN	(sizeof(HTTP_POST)-1)
#define N_HTTP_GET		0x00746567U
#define N_HTTP_POST		0x74736f70U

#define LF "\n"

#define FAULT_PREFIX         \
"<?xml version=\"1.0\"?>" LF \
"<methodResponse>" LF        \
"<fault>" LF                 \
"<value>" LF                 \
"<struct>" LF                \
"<member>" LF                \
"<name>faultCode</name>" LF  \
"<value><int>"

#define FAULT_BODY            \
"</int></value>" LF           \
"</member>" LF                \
"<member>" LF                 \
"<name>faultString</name>" LF \
"<value><string>"

#define FAULT_SUFFIX   \
"</string></value>" LF \
"</member>" LF         \
"</struct>" LF         \
"</value>" LF          \
"</fault>" LF          \
"</methodResponse>"

#define SUCCESS_PREFIX       \
"<?xml version=\"1.0\"?>" LF \
"<methodResponse>" LF        \
"<params>" LF                \
"<param>" LF                 \
"<value>"

#define SUCCESS_SUFFIX \
"</value>" LF          \
"</param>" LF          \
"</params>" LF         \
"</methodResponse>"

static str fault_prefix   = STR_STATIC_INIT(FAULT_PREFIX);
static str fault_body     = STR_STATIC_INIT(FAULT_BODY);
static str fault_suffix   = STR_STATIC_INIT(FAULT_SUFFIX);
static str success_prefix = STR_STATIC_INIT(SUCCESS_PREFIX);
static str success_suffix = STR_STATIC_INIT(SUCCESS_SUFFIX);
static str lf             = STR_STATIC_INIT(LF);
static str int_prefix     = STR_STATIC_INIT("<int>");
static str int_suffix     = STR_STATIC_INIT("</int>");
static str double_prefix  = STR_STATIC_INIT("<double>");
static str double_suffix  = STR_STATIC_INIT("</double>");
static str string_prefix  = STR_STATIC_INIT("<string>");
static str string_suffix  = STR_STATIC_INIT("</string>");
static str date_prefix    = STR_STATIC_INIT("<dateTime.iso8601>");
static str date_suffix    = STR_STATIC_INIT("</dateTime.iso8601>");
static str bool_prefix    = STR_STATIC_INIT("<boolean>");
static str bool_suffix    = STR_STATIC_INIT("</boolean>");
static str value_prefix   = STR_STATIC_INIT("<value>");
static str value_suffix   = STR_STATIC_INIT("</value>");
static str array_prefix   = STR_STATIC_INIT("<array><data>" LF);
static str array_suffix   = STR_STATIC_INIT("</data></array>");
static str struct_prefix  = STR_STATIC_INIT("<struct>");
static str struct_suffix  = STR_STATIC_INIT("</struct>");
static str member_prefix  = STR_STATIC_INIT("<member>");
static str member_suffix  = STR_STATIC_INIT("</member>");
static str name_prefix    = STR_STATIC_INIT("<name>");
static str name_suffix    = STR_STATIC_INIT("</name>");


static struct garbage {
	enum {
		JUNK_XMLCHAR,
		JUNK_RPCSTRUCT
	} type;
	void* ptr;
	struct garbage* next;
} *waste_bin = 0;


struct xmlrpc_reply {
	int code;
	char* reason;
	str body;
	str buf;
};


/*
 * Global variables
 */
typedef struct rpc_ctx {
	struct sip_msg* msg;        /* The SIP/HTTP through which the RPC has been received */
	struct xmlrpc_reply reply;  /* XML-RPC reply */
	struct rpc_struct* structs; /* Structures to be added to the reply */
	int reply_sent;             /* The flag is set after a reply is sent */
	char* method;               /* Name of the method to call */
	unsigned int flags;         /* Flags, such as return value type */
	xmlDocPtr doc;              /* XML-RPC document */
	xmlNodePtr act_param;       /* Actual parameter */
} rpc_ctx_t;


struct rpc_struct {
	xmlNodePtr struct_in;           /* Pointer to the structure parameter */
	struct xmlrpc_reply struct_out; /* Structure to be sent in reply */
	struct xmlrpc_reply* reply;     /* Print errors here */
	int n;                          /* Number of structure members created */
	xmlDocPtr doc;                  /* XML-RPC document */
	int offset;                     /* Offset in the reply where the structure should be printed */
	struct rpc_struct* next;
};

static rpc_ctx_t ctx;

static void close_doc(rpc_ctx_t* ctx);
static void set_fault(struct xmlrpc_reply* reply, int code, char* fmt, ...);
static int fixup_xmlrpc_reply(void** param, int param_no);


static rpc_t func_param;

int enable_introspection = 1;
static char* xmlrpc_route=0; /* default is the main route */
sl_api_t sl;

static int xmlrpc_route_no=DEFAULT_RT;


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"dispatch_rpc", dispatch_rpc, 0, 0,                  REQUEST_ROUTE},
	{"xmlrpc_reply", xmlrpc_reply, 2, fixup_xmlrpc_reply, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"enable_instrospection", PARAM_INT, &enable_introspection},
	{"route", PARAM_STRING, &xmlrpc_route},
	{0, 0, 0}
};


struct module_exports exports = {
	"xmlrpc",
	cmds,           /* Exported commands */
	0,              /* Exported RPC methods */
	params,         /* Exported parameters */
	mod_init,       /* module initialization function */
	0,              /* response function*/
	0,              /* destroy function */
	0,              /* oncancel function */
	0               /* per-child init function */
};

/* XML-RPC reply helper functions */

#define ESC_LT "&lt;"
#define ESC_AMP "&amp;"

static int add_xmlrpc_reply_esc(struct xmlrpc_reply* reply, str* text)
{
    char* p;
    int i;

    for(i = 0; i < text->len; i++) {
	     /* 10 must be bigger than size of longest escape sequence */
	if (reply->body.len >= reply->buf.len - 10) { 
	    p = pkg_malloc(reply->buf.len + 1024);
	    if (!p) {
		set_fault(reply, 500, "Internal Server Error (No memory left)");
		ERR("No memory left: %d\n", reply->body.len + 1024);
		return -1;
	    }
	    memcpy(p, reply->body.s, reply->body.len);
	    pkg_free(reply->buf.s);
	    reply->buf.s = p;
	    reply->buf.len += 1024;
	    reply->body.s = p;
	}

	switch(text->s[i]) {
	case '<':
	    memcpy(reply->body.s + reply->body.len, ESC_LT, sizeof(ESC_LT) - 1);
	    reply->body.len += sizeof(ESC_LT) - 1;
	    break;

	case '&':
	    memcpy(reply->body.s + reply->body.len, ESC_AMP, sizeof(ESC_AMP) - 1);
	    reply->body.len += sizeof(ESC_AMP) - 1;
	    break;

	default:
	    reply->body.s[reply->body.len] = text->s[i];
	    reply->body.len++;
	    break;
	}
    }
    return 0;
}


static int add_xmlrpc_reply(struct xmlrpc_reply* reply, str* text)
{
	char* p;
	if (text->len > (reply->buf.len - reply->body.len)) {
		p = pkg_malloc(reply->buf.len + text->len + 1024);
		if (!p) {
			set_fault(reply, 500, "Internal Server Error (No memory left)");
			ERR("No memory left: %d\n", reply->buf.len + text->len + 1024);
			return -1;
		}
		memcpy(p, reply->body.s, reply->body.len);
		pkg_free(reply->buf.s);
		reply->buf.s = p;
		reply->buf.len += text->len + 1024;
		reply->body.s = p;
	}
	memcpy(reply->body.s + reply->body.len, text->s, text->len);
	reply->body.len += text->len;
	return 0;
}


static int add_xmlrpc_reply_offset(struct xmlrpc_reply* reply, unsigned int offset, str* text)
{
	char* p;
	if (text->len > (reply->buf.len - reply->body.len)) {
		p = pkg_malloc(reply->buf.len + text->len + 1024);
		if (!p) {
			set_fault(reply, 500, "Internal Server Error (No memory left)");
			ERR("No memory left: %d\n", reply->buf.len + text->len + 1024);
			return -1;
		}
		memcpy(p, reply->body.s, reply->body.len);
		pkg_free(reply->buf.s);
		reply->buf.s = p;
		reply->buf.len += text->len + 1024;
		reply->body.s = p;
	}
	memmove(reply->body.s + offset + text->len, reply->body.s + offset, reply->body.len - offset);
	memcpy(reply->body.s + offset, text->s, text->len);
	reply->body.len += text->len;
	return 0;
}



static unsigned int get_reply_len(struct xmlrpc_reply* reply)
{
	return reply->body.len;
}


/*
 * Reset XMLRPC reply, discard everything that has been written so far
 * and set start from the beginning
 */
static void reset_xmlrpc_reply(struct xmlrpc_reply* reply)
{
	reply->body.len = 0;
}


static int init_xmlrpc_reply(struct xmlrpc_reply* reply)
{
	reply->code = 200;
	reply->reason = "OK";
	reply->buf.s = pkg_malloc(1024);
	if (!reply->buf.s) {
		set_fault(reply, 500, "Internal Server Error (No memory left)");
		ERR("No memory left\n");
		return -1;
	}
	reply->buf.len = 1024;
	reply->body.s = reply->buf.s;
	reply->body.len = 0;
	return 0;
}

static void clean_xmlrpc_reply(struct xmlrpc_reply* reply)
{
	if (reply->buf.s) pkg_free(reply->buf.s);
}

static int build_fault_reply(struct xmlrpc_reply* reply)
{
	str reason_s, code_s;

	reason_s.s = reply->reason;
	reason_s.len = strlen(reply->reason);
	code_s.s = int2str(reply->code, &code_s.len);
	reset_xmlrpc_reply(reply);
	if (add_xmlrpc_reply(reply, &fault_prefix) < 0) return -1;
	if (add_xmlrpc_reply_esc(reply, &code_s) < 0) return -1;
	if (add_xmlrpc_reply(reply, &fault_body) < 0) return -1;
	if (add_xmlrpc_reply_esc(reply, &reason_s) < 0) return -1;
	if (add_xmlrpc_reply(reply, &fault_suffix) < 0) return -1;
	return 0;
}



/* Garbage collection */
static int add_garbage(int type, void* ptr, struct xmlrpc_reply* reply)
{
	struct garbage* p;

	p = (struct garbage*)pkg_malloc(sizeof(struct garbage));
	if (!p) {
		set_fault(reply, 500, "Internal Server Error (No memory left)");
		ERR("Not enough memory\n");
		return -1;
	}

	p->type = type;
	p->ptr = ptr;
	p->next = waste_bin;
        waste_bin = p;
	return 0;
}


static void collect_garbage(void)
{
	struct rpc_struct* s;
	struct garbage* p;
	     /* Collect garbage */
	while(waste_bin) {
		p = waste_bin;
		waste_bin = waste_bin->next;
		switch(p->type) {
		case JUNK_XMLCHAR:
			if (p->ptr) xmlFree(p->ptr);
			break;

		case JUNK_RPCSTRUCT:
			s = (struct rpc_struct*)p->ptr;
			if (s && s->struct_out.buf.s) pkg_free(s->struct_out.buf.s);
			if (s) pkg_free(s);
			break;

		default:
			ERR("BUG: Unsupported junk type\n");
		}
		pkg_free(p);
	}
}


/*
 * Extract XML-RPC query from a SIP/HTTP message
 */
static int get_rpc_document(str* doc, struct sip_msg* msg)
{
 	doc->s = get_body(msg);
	if (!doc->s) {
	        ERR("Error while extracting message body\n");
		return -1;
	}
	doc->len = strlen(doc->s);
	return 0;
}


/*
 * Send a pre-defined error
 */
static int send_reply(struct sip_msg* msg, str* body)
{
	if (add_lump_rpl(msg, body->s, body->len, LUMP_RPL_BODY) < 0) {
		ERR("Error while adding reply lump\n");
		return -1;
	}

	if (sl.reply(msg, 200, "OK") == -1) {
		ERR("Error while sending reply\n");
		return -1;
	}

	return 0;
}


static int print_structures(struct xmlrpc_reply* reply, struct rpc_struct* st)
{
	while(st) {
		     /* Close the structure first */
		if (add_xmlrpc_reply(&st->struct_out, &struct_suffix) < 0) return -1;
		if (add_xmlrpc_reply_offset(reply, st->offset, &st->struct_out.body) < 0) return -1;
		st = st->next;
	}
	return 0;
}


static int rpc_send(rpc_ctx_t* ctx)
{
	struct xmlrpc_reply* reply;

	if (ctx->reply_sent) return 1;

	reply = &ctx->reply;
	if (reply->code >= 300) {
		if (build_fault_reply(reply) < 0) return -1;
	} else {
		if (ctx->flags & RET_ARRAY && add_xmlrpc_reply(reply, &array_suffix) < 0) return -1;
		if (ctx->structs && print_structures(reply, ctx->structs) < 0) return -1;
		if (add_xmlrpc_reply(reply, &success_suffix) < 0) return -1;
	}
	if (send_reply(ctx->msg, &reply->body) < 0) return -1;
	ctx->reply_sent = 1;
	return 0;
}


#define REASON_BUF_LEN 1024

static void set_fault(struct xmlrpc_reply* reply, int code, char* fmt, ...)
{
	static char buf[REASON_BUF_LEN];
	va_list ap;

	reply->code = code;
	va_start(ap, fmt);
	vsnprintf(buf, REASON_BUF_LEN, fmt, ap);
	va_end(ap);
	reply->reason = buf;
}

static void rpc_fault(rpc_ctx_t* ctx, int code, char* fmt, ...)
{
	static char buf[REASON_BUF_LEN];
	va_list ap;

	ctx->reply.code = code;
	va_start(ap, fmt);
	vsnprintf(buf, REASON_BUF_LEN, fmt, ap);
	va_end(ap);
	ctx->reply.reason = buf;
}


static struct rpc_struct* new_rpcstruct(xmlDocPtr doc, xmlNodePtr structure, struct xmlrpc_reply* reply)
{
	struct rpc_struct* p;

	p = (struct rpc_struct*)pkg_malloc(sizeof(struct rpc_struct));
	if (!p) {
		set_fault(reply, 500, "Internal Server Error (No Memory Left");
		return 0;
	}
	memset(p, 0, sizeof(struct rpc_struct));
	p->struct_in = structure;

	p->reply = reply;
	p->n = 0;
	if (doc && structure) {
		     /* We will be parsing structure from request */
		p->doc = doc;
		p->struct_in = structure;
	} else {
		     /* We will build a reply structure */
		if (init_xmlrpc_reply(&p->struct_out) < 0) goto err;
		if (add_xmlrpc_reply(&p->struct_out, &struct_prefix) < 0) goto err;

	}
	if (add_garbage(JUNK_RPCSTRUCT, p, reply) < 0) goto err;
	return p;

 err:
	if (p->struct_out.buf.s) pkg_free(p->struct_out.buf.s);
	pkg_free(p);
	return 0;
}


static int print_value(struct xmlrpc_reply* res, struct xmlrpc_reply* err_reply, char fmt, va_list* ap)
{
	str prefix, body, suffix;
	str* sp;
	char buf[256];
	time_t dt;
	struct tm* t;

	switch(fmt) {
	case 'd':
		prefix = int_prefix;
		suffix = int_suffix;
		body.s = int2str(va_arg(*ap, int), &body.len);
		break;

	case 'f':
		prefix = double_prefix;
		suffix = double_suffix;
		body.s = buf;
		body.len = snprintf(buf, 256, "%f", va_arg(*ap, double));
		if (body.len < 0) {
			set_fault(err_reply, 400, "Error While Converting double");
			ERR("Error while converting double\n");
			goto err;
		}
		break;

	case 'b':
		prefix = bool_prefix;
		suffix = bool_suffix;
		body.len = 1;
		body.s = ((va_arg(*ap, int) == 0) ? "0" : "1");
		break;

	case 't':
		prefix = date_prefix;
		suffix = date_suffix;
		body.s = buf;
		body.len = sizeof("19980717T14:08:55") - 1;
		dt = va_arg(*ap, time_t);
		t = gmtime(&dt);
		if (strftime(buf, 256, "%Y%m%dT%H:%M:%S", t) == 0) {
			set_fault(err_reply, 400, "Error While Converting datetime");
			ERR("Error while converting time\n");
			goto err;
		}
		break;

	case 's':
		prefix = string_prefix;
		suffix = string_suffix;
		body.s = va_arg(*ap, char*);
		body.len = strlen(body.s);
		break;

	case 'S':
		prefix = string_prefix;
		suffix = string_suffix;
		sp = va_arg(*ap, str*);
		body = *sp;
		break;

	default:
		set_fault(err_reply, 500, "Bug In SER (Invalid formatting character)");
		ERR("Invalid formatting character\n");
		goto err;
	}

	if (add_xmlrpc_reply(res, &prefix) < 0) goto err;
	if (add_xmlrpc_reply_esc(res, &body) < 0) goto err;
	if (add_xmlrpc_reply(res, &suffix) < 0) goto err;
	return 0;
 err:
	return -1;
}


static int rpc_add(rpc_ctx_t* ctx, char* fmt, ...)
{
	void* void_ptr;
	va_list ap;
	struct xmlrpc_reply* reply;
	struct rpc_struct* p;

	va_start(ap, fmt);
	reply = &ctx->reply;

	while(*fmt) {
		if (ctx->flags & RET_ARRAY && add_xmlrpc_reply(reply, &value_prefix) < 0) goto err;
		if (*fmt == '{') {
			void_ptr = va_arg(ap, void**);
			p = new_rpcstruct(0, 0, reply);
			if (!p) goto err;
			*(struct rpc_struct**)void_ptr = p;
			p->offset = get_reply_len(reply);
			p->next = ctx->structs;
			ctx->structs = p;
		} else {
			if (print_value(reply, reply, *fmt, &ap) < 0) goto err;
		}

		if (ctx->flags & RET_ARRAY && add_xmlrpc_reply(reply, &value_suffix) < 0) goto err;
		if (add_xmlrpc_reply(reply, &lf) < 0) goto err;
		fmt++;
	}
	va_end(ap);
	return 0;
 err:
	va_end(ap);
	return -1;
}


/*
 * Convert XML-RPC time to time_t
 */
static time_t xmlrpc2time(const char* str)
{
	struct tm time;

	memset(&time, '\0', sizeof(struct tm));
	strptime(str, "%Y%m%dT%H:%M:%S", &time);
	time.tm_isdst = -1;
#ifdef HAVE_TIMEGM
	return timegm(&time);
#else
	return _timegm(&time);
#endif /* HAVE_TIMEGM */
}


static int get_int(int* val, struct xmlrpc_reply* reply, xmlDocPtr doc, xmlNodePtr value)
{
	int type;
	xmlNodePtr i4;
	char* val_str;

	if (!value || xmlStrcmp(value->name, BAD_CAST "value")) {
		set_fault(reply, 400, "Invalid parameter value");
		return -1;
	}

	i4 = value->xmlChildrenNode;
	if (!xmlStrcmp(i4->name, BAD_CAST "i4") || !xmlStrcmp(i4->name, BAD_CAST "int")) {
		type = 1;
	} else if (!xmlStrcmp(i4->name, BAD_CAST "boolean")) {
		type = 1;
	} else if (!xmlStrcmp(i4->name, BAD_CAST "dateTime.iso8601")) {
		type = 2;
	} else {
		set_fault(reply, 400, "Invalid Parameter Type");
		return -1;
	}

	val_str = (char*)xmlNodeListGetString(doc, i4->xmlChildrenNode, 1);
	if (!val_str) {
		set_fault(reply, 400, "Empty Parameter Value");
		return -1;
	}
	if (type == 1) {
		     /* Integer/bool conversion */
		*val = strtol(val_str, 0, 10);
	} else {
		*val = xmlrpc2time(val_str);
	}
	xmlFree(val_str);
	return 0;
}


static int get_double(double* val, struct xmlrpc_reply* reply, xmlDocPtr doc, xmlNodePtr value)
{
	xmlNodePtr dbl;
	char* val_str;

	if (!value || xmlStrcmp(value->name, BAD_CAST "value")) {
		set_fault(reply, 400, "Invalid Parameter Value");
		return -1;
	}

	dbl = value->xmlChildrenNode;
	if (!dbl || (xmlStrcmp(dbl->name, BAD_CAST "double") && xmlStrcmp(dbl->name, BAD_CAST "int") && xmlStrcmp(dbl->name, BAD_CAST "int4"))) {
		set_fault(reply, 400, "Invalid Parameter Type");
		return -1;
	}

	val_str = (char*)xmlNodeListGetString(doc, dbl->xmlChildrenNode, 1);
	if (!val_str) {
		set_fault(reply, 400, "Empty Double Parameter");
		return -1;
	}
	*val = strtod(val_str, 0);
	xmlFree(val_str);
	return 0;
}


static int get_string(char** val, struct xmlrpc_reply* reply, xmlDocPtr doc, xmlNodePtr value)
{
	static char* null_str = "";
	xmlNodePtr dbl;
	char* val_str;

	if (!value || xmlStrcmp(value->name, BAD_CAST "value")) {
		set_fault(reply, 400, "Invalid Parameter Value");
		return -1;
	}

	dbl = value->xmlChildrenNode;
	if (!dbl || xmlStrcmp(dbl->name, BAD_CAST "string")) {
		set_fault(reply, 400, "Invalid Parameter Type");
		return -1;
	}

	val_str = (char*)xmlNodeListGetString(doc, dbl->xmlChildrenNode, 1);
	if (!val_str) {
		*val = null_str;
		return 0;
	}

	if (add_garbage(JUNK_XMLCHAR, val_str, reply) < 0) return -1;
	*val = val_str;
	return 0;
}


static int rpc_scan(rpc_ctx_t* ctx, char* fmt, ...)
{
	int read;
	int fmt_len;
	int* int_ptr;
	char** char_ptr;
	str* str_ptr;
	double* double_ptr;
	void** void_ptr;
	xmlNodePtr value;
	struct xmlrpc_reply* reply;
	struct rpc_struct* p;

	va_list ap;

	reply = &ctx->reply;
	fmt_len = strlen(fmt);
	va_start(ap, fmt);
	read = 0;
	while(*fmt) {
		if (!ctx->act_param) goto error;
		value = ctx->act_param->xmlChildrenNode;

		switch(*fmt) {
		case 'b': /* Bool */
		case 't': /* Date and time */
		case 'd': /* Integer */
			int_ptr = va_arg(ap, int*);
			if (get_int(int_ptr, reply, ctx->doc, value) < 0) goto error;
			break;

		case 'f': /* double */
			double_ptr = va_arg(ap, double*);
			if (get_double(double_ptr, reply, ctx->doc, value) < 0) goto error;
			break;

		case 's': /* zero terminated string */
			char_ptr = va_arg(ap, char**);
			if (get_string(char_ptr, reply, ctx->doc, value) < 0) goto error;
			break;

		case 'S': /* str structure */
			str_ptr = va_arg(ap, str*);
			if (get_string(&str_ptr->s, reply, ctx->doc, value) < 0) goto error;
			str_ptr->len = strlen(str_ptr->s);
			break;

		case '{':
			void_ptr = va_arg(ap, void**);
			if (!value->xmlChildrenNode) goto error;
			p = new_rpcstruct(ctx->doc, value->xmlChildrenNode, reply);
			if (!p) goto error;
			*void_ptr = p;
			break;

		default:
			ERR("Invalid parameter type in formatting string: %c\n", *fmt);
			set_fault(reply, 500, "Server Internal Error (Invalid Formatting String)");
			goto error;
		}
		ctx->act_param = ctx->act_param->next;
		read++;
		fmt++;
	}
	va_end(ap);
	return read;

 error:
	va_end(ap);
	return -read;
}

#define RPC_BUF_SIZE 1024

static int rpc_printf(rpc_ctx_t* ctx, char* fmt, ...)
{
	int n, buf_size;
	char* buf;
	va_list ap;
	str s;
	struct xmlrpc_reply* reply;

	reply = &ctx->reply;
	buf = (char*)pkg_malloc(RPC_BUF_SIZE);
	if (!buf) {
		set_fault(reply, 500, "Internal Server Error (No memory left)");
		ERR("No memory left\n");
		return -1;
	}

	buf_size = RPC_BUF_SIZE;
	while (1) {
		     /* Try to print in the allocated space. */
		va_start(ap, fmt);
		n = vsnprintf(buf, buf_size, fmt, ap);
		va_end(ap);
		     /* If that worked, return the string. */
		if (n > -1 && n < buf_size) {
			s.s = buf;
			s.len = n;
			if (ctx->flags & RET_ARRAY && add_xmlrpc_reply(reply, &value_prefix) < 0) goto err;
			if (add_xmlrpc_reply(reply, &string_prefix) < 0) goto err;
			if (add_xmlrpc_reply_esc(reply, &s) < 0) goto err;
			if (add_xmlrpc_reply(reply, &string_suffix) < 0) goto err;
			if (ctx->flags & RET_ARRAY && add_xmlrpc_reply(reply, &value_suffix) < 0) goto err;
			if (add_xmlrpc_reply(reply, &lf) < 0) goto err;
			pkg_free(buf);
			return 0;
		}
		     /* Else try again with more space. */
		if (n > -1) {   /* glibc 2.1 */
			buf_size = n + 1; /* precisely what is needed */
		} else {          /* glibc 2.0 */
			buf_size *= 2;  /* twice the old size */
		}
		if ((buf = pkg_realloc(buf, buf_size)) == 0) {
			set_fault(reply, 500, "Internal Server Error (No memory left)");
			ERR("No memory left\n");
			goto err;
		}
	}
	return 0;
 err:
	if (buf) pkg_free(buf);
	return -1;
}

/* Structure manipulation functions */

/*
 * Find a structure member by name
 */
static int find_member(xmlNodePtr* value, xmlDocPtr doc, xmlNodePtr structure, struct xmlrpc_reply* reply, char* member_name)
{
	char* name_str;
	xmlNodePtr member, name;

	if (!structure) {
		set_fault(reply, 400, "Invalid Structure Parameter");
		return -1;
	}

	member = structure->xmlChildrenNode;
	while(member) {
		name = member->xmlChildrenNode;
		     /* Find <name> node in the member */
		while(name) {
			if (!xmlStrcmp(name->name, BAD_CAST "name")) break;
			name = name->next;
		}
		if (!name) {
			set_fault(reply, 400, "Member Name Not Found In Structure");
			return -1;
		}

		     /* Check the value of <name> node in the structure member */
		name_str = (char*)xmlNodeListGetString(doc, name->xmlChildrenNode, 1);
		if (!name_str) {
			set_fault(reply, 400, "Empty name Element of Structure Parameter");
			return -1;
		}
		if (strcmp(name_str, member_name)) {
			xmlFree(name_str);
			goto skip;
		}
		xmlFree(name_str);

		*value = member->xmlChildrenNode;
		while(*value) {
			if (!xmlStrcmp((*value)->name, BAD_CAST "value")) break;
			(*value) = (*value)->next;
		}
		if (!(*value)) {
			set_fault(reply, 400, "Member Value Not Found In Structure");
			return -1;
		}
		return 0;
	skip:
		member = member->next;
	}
	return 1;
}


static int rpc_struct_add(struct rpc_struct* s, char* fmt, ...)
{
	va_list ap;
	str member_name;
	struct xmlrpc_reply* reply;

	reply = &s->struct_out;

	va_start(ap, fmt);
	while(*fmt) {
		member_name.s = va_arg(ap, char*);
		member_name.len = (member_name.s ? strlen(member_name.s) : 0);

		if (add_xmlrpc_reply(reply, &member_prefix) < 0) goto err;
		if (add_xmlrpc_reply(reply, &name_prefix) < 0) goto err;
		if (add_xmlrpc_reply_esc(reply, &member_name) < 0) goto err;
		if (add_xmlrpc_reply(reply, &name_suffix) < 0) goto err;
		if (add_xmlrpc_reply(reply, &value_prefix) < 0) goto err;
		if (print_value(reply, reply, *fmt, &ap) < 0) goto err;
		if (add_xmlrpc_reply(reply, &value_suffix) < 0) goto err;
		if (add_xmlrpc_reply(reply, &member_suffix) < 0) goto err;
		fmt++;
	}

	va_end(ap);
	return 0;
 err:
	va_end(ap);
	return -1;
}


static int rpc_struct_printf(struct rpc_struct* s, char* member_name, char* fmt, ...)
{
	int n, buf_size;
	char* buf;
	va_list ap;
	str st, name;
	struct xmlrpc_reply* reply;
	struct xmlrpc_reply* out;

	out = &s->struct_out;
	buf = (char*)pkg_malloc(RPC_BUF_SIZE);
	reply = s->reply;
	if (!buf) {
		set_fault(reply, 500, "Internal Server Error (No memory left)");
		ERR("No memory left\n");
		return -1;
	}

	buf_size = RPC_BUF_SIZE;
	while (1) {
		     /* Try to print in the allocated space. */
		va_start(ap, fmt);
		n = vsnprintf(buf, buf_size, fmt, ap);
		va_end(ap);
		     /* If that worked, return the string. */
		if (n > -1 && n < buf_size) {
			st.s = buf;
			st.len = n;

			name.s = member_name;
			name.len = strlen(member_name);

			if (add_xmlrpc_reply(out, &member_prefix) < 0) goto err;
			if (add_xmlrpc_reply(out, &name_prefix) < 0) goto err;
			if (add_xmlrpc_reply_esc(out, &name) < 0) goto err;
			if (add_xmlrpc_reply(out, &name_suffix) < 0) goto err;
			if (add_xmlrpc_reply(out, &value_prefix) < 0) goto err;

			if (add_xmlrpc_reply(out, &string_prefix) < 0) goto err;
			if (add_xmlrpc_reply_esc(out, &st) < 0) goto err;
			if (add_xmlrpc_reply(out, &string_suffix) < 0) goto err;

			if (add_xmlrpc_reply(out, &value_suffix) < 0) goto err;
			if (add_xmlrpc_reply(out, &member_suffix) < 0) goto err;

			return 0;
		}
		     /* Else try again with more space. */
		if (n > -1) {   /* glibc 2.1 */
			buf_size = n + 1; /* precisely what is needed */
		} else {          /* glibc 2.0 */
			buf_size *= 2;  /* twice the old size */
		}
		if ((buf = pkg_realloc(buf, buf_size)) == 0) {
			set_fault(reply, 500, "Internal Server Error (No memory left)");
			ERR("No memory left\n");
			goto err;
		}
	}
	return 0;
 err:
	if (buf) pkg_free(buf);
	return -1;

}


static int rpc_struct_scan(struct rpc_struct* s, char* fmt, ...)
{
	int read;
	va_list ap;
	int* int_ptr;
	double* double_ptr;
	char** char_ptr;
	str* str_ptr;
	xmlNodePtr value;
	char* member_name;
	struct xmlrpc_reply* reply;
	int ret;

	read = 0;
	va_start(ap, fmt);
	while(*fmt) {
		member_name = va_arg(ap, char*);
		reply = s->reply;
		ret = find_member(&value, s->doc, s->struct_in, reply, member_name);
		if (ret != 0) goto error;

		switch(*fmt) {
		case 'b': /* Bool */
		case 't': /* Date and time */
		case 'd': /* Integer */
			int_ptr = va_arg(ap, int*);
			if (get_int(int_ptr, reply, s->doc, value) < 0) goto error;
			break;

		case 'f': /* double */
			double_ptr = va_arg(ap, double*);
			if (get_double(double_ptr, reply, s->doc, value) < 0) goto error;
			break;

		case 's': /* zero terminated string */
			char_ptr = va_arg(ap, char**);
			if (get_string(char_ptr, reply, s->doc, value) < 0) goto error;
			break;

		case 'S': /* str structure */
			str_ptr = va_arg(ap, str*);
			if (get_string(&str_ptr->s, reply, s->doc, value) < 0) goto error;
			str_ptr->len = strlen(str_ptr->s);
			break;
		default:
			ERR("Invalid parameter type in formatting string: %c\n", *fmt);
			return -1;
		}
		fmt++;
		read++;
	}
	va_end(ap);
	return read;
 error:
	va_end(ap);
	return -read;
}


/*
 * Start parsing XML-RPC document, get the name of the method
 * to be called and position the cursor at the first parameter
 * in the document
 */
static int open_doc(rpc_ctx_t* ctx, struct sip_msg* msg)
{
	str doc;
	xmlNodePtr root;
	xmlNodePtr cur;
	struct xmlrpc_reply* reply;

	reply = &ctx->reply;
	if (get_rpc_document(&doc, msg) < 0) {
		set_fault(reply, 400, "Malformed Message Body");
		ERR("Error extracting message body\n");
		return -1;
	}

	ctx->doc = xmlReadMemory(doc.s, doc.len, 0, 0,
				 XML_PARSE_NOBLANKS |
				 XML_PARSE_NONET |
				 XML_PARSE_NOCDATA);

	if (!ctx->doc) {
		set_fault(reply, 400, "Invalid XML-RPC Document");
		ERR("Invalid XML-RPC document: \n[%.*s]\n", doc.len, doc.s);
		goto err;
	}

	root = xmlDocGetRootElement(ctx->doc);
	if (!root) {
		set_fault(reply, 400, "Empty XML-RPC Document");
		ERR("Empty XML-RPC document\n");
		goto err;
	}

	if (xmlStrcmp(root->name, (const xmlChar*)"methodCall")) {
		set_fault(reply, 400, "Root Element Is Not methodCall");
		ERR("Root element is not methodCall\n");
		goto err;
	}

	cur = root->xmlChildrenNode;
	while(cur) {
		if (!xmlStrcmp(cur->name, (const xmlChar*)"methodName")) {
			ctx->method = (char*)xmlNodeListGetString(ctx->doc, cur->xmlChildrenNode, 1);
			if (!ctx->method) {
				set_fault(reply, 400, "Cannot Extract Method Name");
				ERR("Cannot extract method name\n");
				goto err;
			}
			break;
		}
		cur = cur->next;
	}
	if (!cur) {
		set_fault(reply, 400, "Method Name Not Found");
		ERR("Method name not found\n");
		goto err;
	}
	cur = root->xmlChildrenNode;
	while(cur) {
		if (!xmlStrcmp(cur->name, (const xmlChar*)"params")) {
			ctx->act_param = cur->xmlChildrenNode;
			break;
		}
		cur = cur->next;
	}
	if (!cur) ctx->act_param = 0;
	return 0;

 err:
	close_doc(ctx);
	return -1;
}

static void close_doc(rpc_ctx_t* ctx)
{
	if (ctx->method) xmlFree(ctx->method);
	if (ctx->doc) xmlFreeDoc(ctx->doc);
	ctx->method = 0;
	ctx->doc = 0;
}

static int init_context(rpc_ctx_t* ctx, struct sip_msg* msg)
{
	ctx->msg = msg;
	ctx->method = 0;
	ctx->reply_sent = 0;
	ctx->act_param = 0;
	ctx->doc = 0;
	ctx->structs = 0;
	if (init_xmlrpc_reply(&ctx->reply) < 0) return -1;
	add_xmlrpc_reply(&ctx->reply, &success_prefix);
	if (open_doc(ctx, msg) < 0) return -1;
	return 0;
}


static void clean_context(rpc_ctx_t* ctx)
{
	if (!ctx) return;
	clean_xmlrpc_reply(&ctx->reply);
	close_doc(ctx);
}



/* creates a sip msg (in "buffer" form) from a http xmlrpc request)
 *  returns 0 on error and a pkg_malloc'ed message buffer on success
 * NOTE: the result must be pkg_free()'ed when not needed anymore */
static char* http_xmlrpc2sip(struct sip_msg* msg, int* new_msg_len)
{
	unsigned int len;
	char* via;
	char* new_msg;
	char* p;
	unsigned int via_len;
	str ip, port;
	struct hostport hp;
	struct dest_info dst;
	
	/* create a via */
	ip.s = ip_addr2a(&msg->rcv.src_ip);
	ip.len = strlen(ip.s);
	port.s = int2str(msg->rcv.src_port, &port.len);
	hp.host = &ip;
	hp.port = &port;
	init_dst_from_rcv(&dst, &msg->rcv);
	via = via_builder(&via_len, &dst, 0, 0, &hp);
	if (via==0){
		DEBUG("failed to build via\n");
		return 0;
	}
	len=msg->first_line.u.request.method.len+1 /* space */+XMLRPC_URI_LEN+
			1 /* space */ + msg->first_line.u.request.version.len+
			CRLF_LEN+via_len+(msg->len-msg->first_line.len);
	p=new_msg=pkg_malloc(len+1);
	if (new_msg==0){
		DEBUG("memory allocation failure (%d bytes)\n", len);
		pkg_free(via);
		return 0;
	}
	/* new message:
	 * <orig_http_method> sip:127.0.0.1:9 HTTP/1.x 
	 * Via: <faked via>
	 * <orig. http message w/o the first line>
	 */
	memcpy(p, msg->first_line.u.request.method.s, 
				msg->first_line.u.request.method.len);
	p+=msg->first_line.u.request.method.len;
	*p=' ';
	p++;
	memcpy(p, XMLRPC_URI, XMLRPC_URI_LEN);
	p+=XMLRPC_URI_LEN;
	*p=' ';
	p++;
	memcpy(p, msg->first_line.u.request.version.s,
				msg->first_line.u.request.version.len);
	p+=msg->first_line.u.request.version.len;
	memcpy(p, CRLF, CRLF_LEN);
	p+=CRLF_LEN;
	memcpy(p, via, via_len);
	p+=via_len;
	memcpy(p, msg->first_line.line.s + msg->first_line.len, msg->len - msg->first_line.len);
	new_msg[len]=0; /* null terminate, required by receive_msg() */
	pkg_free(via);
	*new_msg_len=len;
	return new_msg;
}



/* emulate receive_msg for an xmlrpc request */
static int em_receive_request(struct sip_msg* orig_msg, 
								char* new_buf, unsigned int new_len)
{
	struct sip_msg tmp_msg;
	struct sip_msg* msg;
	
	if (new_buf && new_len){
		memset(&tmp_msg, 0, sizeof(struct sip_msg));
		tmp_msg.buf=new_buf;
		tmp_msg.len=new_len;
		tmp_msg.rcv=orig_msg->rcv;
		tmp_msg.id=orig_msg->id;
		tmp_msg.set_global_address=orig_msg->set_global_address;
		tmp_msg.set_global_port=orig_msg->set_global_port;
		if (parse_msg(new_buf, new_len, &tmp_msg)!=0){
			ERR("xmlrpc: parse_msg failed\n");
			goto error;
		}
		msg=&tmp_msg;
	}else{
		msg=orig_msg;
	}
	
	/* not needed, performed by the "real" receive_msg()
	clear_branches();
	reset_static_buffer();
	*/
	if ((msg->first_line.type!=SIP_REQUEST) || (msg->via1==0) ||
		(msg->via1->error!=PARSE_OK)){
		BUG("xmlrpc: strange message: %.*s\n", msg->len, msg->buf);
		goto error;
	}
	if (exec_pre_req_cb(msg)==0)
		goto end; /* drop request */
	/* exec routing script */
	if (run_actions(main_rt.rlist[xmlrpc_route_no], msg)<0){
		WARN("xmlrpc: error while trying script\n");
		goto end;
	}
end:
	exec_post_req_cb(msg); /* needed for example if tm is used */
	/* reset_avps(); non needed, performed by the real receive_msg */
	if (msg!=orig_msg) /* avoid double free (freed from receive_msg too) */
		free_sip_msg(msg);
	return 0;
error:
	return -1;
}



static int process_xmlrpc(struct sip_msg* msg)
{
	char* fake_msg;
	int fake_msg_len;
	unsigned char* method;
	unsigned int method_len;
	unsigned int n_method;
	
	if (IS_HTTP(msg)){
		method=(unsigned char*)msg->first_line.u.request.method.s;
		method_len=msg->first_line.u.request.method.len;
		/* first line is always > 4, so it's always safe to try to read the
		 * 1st 4 bytes from method, even if method is shorter*/
		n_method=method[0]+(method[1]<<8)+(method[2]<<16)+(method[3]<<24);
		n_method|=0x20202020;
		n_method&= ((method_len<4)*(1U<<method_len*8)-1);
		/* accept only GET or POST */
		if ((n_method==N_HTTP_GET) || 
			((n_method==N_HTTP_POST) && (method_len==HTTP_POST_LEN))){
			if (msg->via1==0){
				/* create a fake sip message */
				fake_msg=http_xmlrpc2sip(msg, &fake_msg_len);
				if (fake_msg==0){
					ERR("xmlrpc: out of memory\n");
				}else{
					/* send it */
					DBG("new fake xml msg created (%d bytes):\n<%.*s>\n",
							fake_msg_len, fake_msg_len, fake_msg);
					em_receive_request(msg, fake_msg, fake_msg_len);
					/* ignore the return code */
					pkg_free(fake_msg);
				}
				return NONSIP_MSG_DROP; /* we "ate" the message, 
										   stop processing */
			}else{ /* the message has a via */
				DBG("http xml msg unchanged (%d bytes):\n<%.*s>\n",
						msg->len, msg->len, msg->buf);
				em_receive_request(msg, 0, 0);
				return NONSIP_MSG_DROP;
			}
		}else{
			ERR("xmlrpc: bad HTTP request method: \"%.*s\"\n",
					msg->first_line.u.request.method.len,
					msg->first_line.u.request.method.s);
			/* the message was for us, but it is an error */
			return NONSIP_MSG_ERROR; 
#if 0
			/* FIXME: temporary test only, remove when non-sip hook available*/
			DEBUG("xmlrpc:  HTTP request method not recognized: \"%.*s\"\n",
					msg->first_line.u.request.method.len,
					msg->first_line.u.request.method.s);
			return 1;
#endif
		}
	}
	return NONSIP_MSG_PASS; /* message not for us, maybe somebody 
								   else needs it */
}



static int dispatch_rpc(struct sip_msg* msg, char* s1, char* s2)
{
	rpc_export_t* exp;
	int ret = 1;

	if (init_context(&ctx, msg) < 0) goto skip;

	exp = find_rpc_export(ctx.method, 0);
	if (!exp || !exp->function) {
		rpc_fault(&ctx, 500, "Method Not Found");
		goto skip;
	}
	ctx.flags = exp->flags;
	if (exp->flags & RET_ARRAY && add_xmlrpc_reply(&ctx.reply, &array_prefix) < 0) goto skip;
	exp->function(&func_param, &ctx);

 skip:
	     /* The function may have sent the reply itself */
	if (!ctx.reply_sent) {
		ret = rpc_send(&ctx);
	}
	clean_context(&ctx);
	collect_garbage();
	if (ret < 0) return -1;
	else return 1;
}


static int xmlrpc_reply(struct sip_msg* msg, char* p1, char* p2)
{
        str reason;
	static str succ = STR_STATIC_INIT("1");
	struct xmlrpc_reply reply;

	memset(&reply, 0, sizeof(struct xmlrpc_reply));
	if (init_xmlrpc_reply(&reply) < 0) return -1;

	if (get_int_fparam(&reply.code, msg, (fparam_t*)p1) < 0) return -1;
	if (get_str_fparam(&reason, msg, (fparam_t*)p2) < 0) return -1;

	reply.reason = as_asciiz(&reason);
	if (reply.reason == NULL) {
	    ERR("No memory left\n");
	    return -1;
	}

	if (reply.code >= 300) { 
		if (build_fault_reply(&reply) < 0) goto error;
	} else {
		if (add_xmlrpc_reply(&reply, &success_prefix) < 0) goto error;
		if (add_xmlrpc_reply(&reply, &int_prefix) < 0) goto error;
		if (add_xmlrpc_reply_esc(&reply, &succ) < 0) goto error;
		if (add_xmlrpc_reply(&reply, &int_suffix) < 0) goto error;
		if (add_xmlrpc_reply(&reply, &success_suffix) < 0) return -1;
	}
	if (send_reply(msg, &reply.body) < 0) goto error;
	if (reply.reason) pkg_free(reply.reason);
	clean_xmlrpc_reply(&reply);
	return 1;
 error:
	if (reply.reason) pkg_free(reply.reason);
	clean_xmlrpc_reply(&reply);
	return -1;
}


static int select_method(str* res, struct select* s, struct sip_msg* msg)
{
	static char buf[1024];
	str doc;
	xmlDocPtr xmldoc;
	xmlNodePtr cur;
	char* method;

	xmldoc = 0;
	method = 0;

	if (get_rpc_document(&doc, msg) < 0) goto err;
	xmldoc = xmlReadMemory(doc.s, doc.len, 0, 0, XML_PARSE_NOBLANKS | XML_PARSE_NONET | XML_PARSE_NOCDATA);
	
	if (!xmldoc) goto err;
	cur = xmlDocGetRootElement(xmldoc);
	if (!cur) goto err;
	if (xmlStrcmp(cur->name, (const xmlChar*)"methodCall")) goto err;
	cur = cur->xmlChildrenNode;
	while(cur) {
		if (!xmlStrcmp(cur->name, (const xmlChar*)"methodName")) {
			method = (char*)xmlNodeListGetString(xmldoc, cur->xmlChildrenNode, 1);
			if (!method) goto err;
			break;
		}
		cur = cur->next;
	}
	if (!cur) goto err;
	res->len = strlen(method);
	if (res->len >= 1024) goto err;
	memcpy(buf, method, res->len);
	res->s = buf;
	return 0;
 err:
	if (method) xmlFree(method);
	if (xmldoc) xmlFreeDoc(xmldoc);
	return -1;
}

static ABSTRACT_F(select_xmlrpc);

select_row_t xmlrpc_sel[] = {
        { NULL,          SEL_PARAM_STR, STR_STATIC_INIT("xmlrpc"), select_xmlrpc, SEL_PARAM_EXPECTED},
        { select_xmlrpc, SEL_PARAM_STR, STR_STATIC_INIT("method"), select_method, 0},
        { NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};


static int mod_init(void)
{
	bind_sl_t bind_sl;
	struct nonsip_hook nsh;
	int route_no;
	
	/* try to fix the xmlrpc route */
	if (xmlrpc_route){
		route_no=route_get(&main_rt, xmlrpc_route);
		if (route_no==-1){
			ERR("xmlrpc: failed to fix route \"%s\": route_get() failed\n",
					xmlrpc_route);
			return -1;
		}
		if (main_rt.rlist[route_no]==0){
			WARN("xmlrpc: xmlrpc route \"%s\" is empty / doesn't exist\n",
					xmlrpc_route);
		}
		xmlrpc_route_no=route_no;
	}

             /*
              * We will need sl_send_reply from stateless
	      * module for sending replies
	      */
        bind_sl = (bind_sl_t)find_export("bind_sl", 0, 0);
	if (!bind_sl) {
		ERR("This module requires sl module\n");
		return -1;
	}
	if (bind_sl(&sl) < 0) return -1;

	func_param.send = (rpc_send_f)rpc_send;
	func_param.fault = (rpc_fault_f)rpc_fault;
	func_param.add = (rpc_add_f)rpc_add;
	func_param.scan = (rpc_scan_f)rpc_scan;
	func_param.printf = (rpc_printf_f)rpc_printf;
	func_param.struct_add = (rpc_struct_add_f)rpc_struct_add;
	func_param.struct_scan = (rpc_struct_scan_f)rpc_struct_scan;
	func_param.struct_printf = (rpc_struct_printf_f)rpc_struct_printf;
	register_select_table(xmlrpc_sel);
	
	/* register non-sip hooks */
	memset(&nsh, 0, sizeof(nsh));
	nsh.name="xmlrpc";
	nsh.destroy=0;
	nsh.on_nonsip_req=process_xmlrpc;
	if (register_nonsip_msg_hook(&nsh)<0){
		ERR("Failed to register non sip msg hooks\n");
		return -1;
	}
	return 0;
}


static int fixup_xmlrpc_reply(void** param, int param_no)
{
	int ret;

	if (param_no == 1) {
		ret = fix_param(FPARAM_AVP, param);
		if (ret <= 0) return ret;		
		return fix_param(FPARAM_INT, param);
	} else if (param_no == 2) {
	        return fixup_var_str_12(param, 2);
	}
	return 0;
}

