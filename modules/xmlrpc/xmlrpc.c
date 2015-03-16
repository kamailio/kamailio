/*
 * Copyright (C) 2005 iptelorg GmbH
 * Written by Jan Janak <jan@iptel.org>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version
 *
 * Kamailio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
/*This define breaks on Solaris OS */
#ifndef __OS_solaris
	#define _XOPEN_SOURCE 4           /* strptime */
#endif
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
#include "../../modules/sl/sl.h"
#include "../../nonsip_hooks.h"
#include "../../action.h" /* run_actions */
#include "../../script_cb.h" /* exec_*_script_cb */
#include "../../route.h" /* route_get */
#include "../../sip_msg_clone.h" /* sip_msg_shm_clone */
#include "http.h"

/** @addtogroup xmlrpc
 * @ingroup modules
 * @{
 *
 * <h1>Overview of Operation</h1> 
 * This module provides XML-RPC based interface to management functions in
 * SER. You can send XML-RPC requests to SER when the module is loaded and
 * configured and it will send XML-RPC replies back.  XML-RPC requests are
 * encoded as XML documents in the body of HTTP requests. Due to similarity
 * between HTTP and SIP SER can easily parse HTTP requests and extract the XML
 * document from their body.
 *
 * When you load this module into SER, it will register a callback function
 * that will be called whenever the SER core receives a request with method it
 * does not understand. The main callback function is process_xmlrpc(). The
 * function first verifies if the protocol identifier inside the request is
 * HTTP and whether the request method is either GET or POST. If both
 * conditions are met then it will signal to the SER core that it is
 * processing the request, otherwise it will reject the request and the SER
 * core will pass the requests to other callbacks if they exist.
 *
 * As the next step the request will be converted from HTTP request to a SIP
 * request to ensure that it can be processed by SER and its modules. The
 * conversion will modify the URI in the Request-URI of the request, the new
 * URI will be a SIP URI. In addition to that it will add a fake Via header
 * field and copy all remaining header fields from the original HTTP request.
 * The conversion is implemented in http_xmlrpc2sip() function.
 * 
 * After the conversion the module will execute the route statement whose
 * number is configured in "route" module parameter. That route stament may
 * perform additional security checks and when it ensures that the client is
 * authorized to execute management functions then it will call dispatch_rpc()
 * module function provided by this module.
 *
 * dispatch_rpc() function extracts the XML-RPC document from the body of the
 * request to determine the name of the method to be called and then it
 * searches through the list of all management functions to find a function
 * with matching name. If such a function is found then dispatch_rpc() will
 * pass control to the function to handle the request. dispatch_rpc() will
 * send a reply back to the client when the management function terminates, if
 * the function did not do that explicitly.
 * 
 * <h2>Memory Management</h2> 
 * The module provides implementation for all the functions required by the
 * management interface in SER, such as rpc->rpl_printf, rpc->add, rpc->struct_add
 * and so on. Whenever the management function calls one of the functions then
 * corresponding function in this module will be called to handle the request.
 *
 * The implementation functions build the reply, that will be sent to the
 * client, as they execute and they need to allocate memory to do that. That
 * memory must be freed again after the reply has been sent to the client. To
 * remember all the memory regions allocated during the execution of the
 * management function all functions within this module record all allocated
 * memory in the global variable called waste_bin. dispatch_rpc() functions
 * executes function collect_garbage() after the reply has been sent to the
 * client to free all memory that was allocated from the management function.
 * that was executed.
 *
 * <h2>Request Context</h2> 
 * Before the module calls a management function it prepares a structure
 * called context. The context is defined in structure rpc_ctx and it is
 * passed as one of parameter to the management function being called. The
 * context contains all the data that is needed during the execution of the
 * management function, such as the pointer to the request being processed, a
 * pointer to the reply being built, and so on.
 *
 * Another parameter to the management function being called is a structure
 * that contains pointers to all implementation functions. This structure is
 * of type rpc_t, this module keeps one global variable of that type called
 * func_param and a pointer to that variable is passed to all management
 * functions. The global variable is initialized in mod_init().
 */

/** @file 
 *
 * This is the main file of XMLRPC SER module which contains all the functions
 * related to XML-RPC processing, as well as the module interface.
 */

/*
 * FIXME: Decouple code and reason phrase from reply body
 *        Escape special characters in strings
 */

MODULE_VERSION

#if defined (__OS_darwin) || defined (__OS_freebsd)
/* redeclaration of functions from stdio.h throws errors */
#else
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
#endif

static int process_xmlrpc(sip_msg_t* msg);
static int dispatch_rpc(sip_msg_t* msg, char* s1, char* s2);
static int xmlrpc_reply(sip_msg_t* msg, char* code, char* reason);
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

/** The beginning of XML document indicating an error.
 *
 * This is the beginning of the XML document that will be sent back to the
 * client when the server encountered an error.  It will be immediately
 * followed by a reason phrase.
 */
#define FAULT_PREFIX         \
"<?xml version=\"1.0\"?>" LF \
"<methodResponse>" LF        \
"<fault>" LF                 \
"<value>" LF                 \
"<struct>" LF                \
"<member>" LF                \
"<name>faultCode</name>" LF  \
"<value><int>"


/** The text of XML document indicating error that goes between reason code
 * and reason phrase.
 */
#define FAULT_BODY            \
"</int></value>" LF           \
"</member>" LF                \
"<member>" LF                 \
"<name>faultString</name>" LF \
"<value><string>"


/** The end of XML document that indicates an error.  
 *
 * This is the closing part of the XML-RPC document that indicates an error on
 * the server.
 */
#define FAULT_SUFFIX   \
"</string></value>" LF \
"</member>" LF         \
"</struct>" LF         \
"</value>" LF          \
"</fault>" LF          \
"</methodResponse>"


/** The beginning of XML-RPC reply sent to the client.
 */
#define SUCCESS_PREFIX       \
"<?xml version=\"1.0\"?>" LF \
"<methodResponse>" LF        \
"<params>" LF                \
"<param>" LF                 \
"<value>"


/** The closing part of XML-RPC reply document sent to
 * the client.
 */
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

/** Garbage collection data structure.
 *
 * This is the data structure used by the garbage collector in this module.
 * When the xmlrpc SER module identifies the management function to be called,
 * it calls corresponding function in SER. The function being called adds data
 * to the reply, that will be later sent to the client, as it executes. This
 * module needs to allocate memory for such data and the memory will be
 * re-claimed after the reply was sent out.  All the memory allocated this way
 * is recorded in this data structure so that it can be identified and
 * re-claimed later (when the reply is being sent out).
 * 
 */
static struct garbage {
	enum {
		JUNK_XMLCHAR,
		JUNK_RPCSTRUCT,    /**< This type indicates that the memory block was
						   * allocated for the RPC structure data type, this
						   * type needs to be freed differently as it may
						   * contain more allocated memory blocks
						   */
		JUNK_PKGCHAR 	  /** This type indicates a mxr_malloc'ed string */
	} type;               /**< Type of the memory block */
	void* ptr;            /**< Pointer to the memory block obtained from
							 mxr_malloc */
	struct garbage* next; /**< The linked list of all allocated memory
							 blocks */
} *waste_bin = 0;


/** Representation of the XML-RPC reply being constructed.
 *  
 * This data structure describes the XML-RPC reply that is being constructed
 * and will be sent to the client.
 */
struct xmlrpc_reply {
	int code;     /**< Reply code which indicates the type of the reply */
	char* reason; /**< Reason phrase text which provides human-readable
				   * description that augments the reply code */
	str body;     /**< The XML-RPC document body built so far */
	str buf;      /**< The memory buffer allocated for the reply, this is
				   * where the body attribute of the structure points to
				   */
};


/** The context of the XML-RPC request being processed.
 * 
 * This is the data structure that contains all data related to the XML-RPC
 * request being processed, such as the reply code and reason, data to be sent
 * to the client in the reply, and so on.
 *
 * There is always one context per XML-RPC request.
 */
typedef struct rpc_ctx {
	sip_msg_t* msg;        /**< The SIP/HTTP through which the RPC has been
							  received */
	struct xmlrpc_reply reply;  /**< XML-RPC reply to be sent to the client */
	struct rpc_struct* structs; /**< Structures to be added to the reply */
	int msg_shm_block_size; /**< non-zero for delayed reply contexts with
								shm cloned msgs */
	int reply_sent;             /**< The flag is set after a reply is sent,
								   this prevents a single reply being sent
								   twice */
	char* method;               /**< Name of the management function to be
								   called */
	unsigned int flags;         /**< Various flags, such as return value
								   type */
	xmlDocPtr doc;              /**< Pointer to the XML-RPC request
								   document */
	xmlNodePtr act_param;       /**< Pointer to the parameter being processed
								   in the XML-RPC request document */
} rpc_ctx_t;


/* extra rpc_ctx_t flags */
/* first 8 bits reserved for rpc flags (e.g. RET_ARRAY) */
#define XMLRPC_DELAYED_CTX_F	256
#define XMLRPC_DELAYED_REPLY_F	512

/** The structure represents a XML-RPC document structure.
 *
 * This is the data structure that represents XML-RPC structures that are sent
 * to the client in the XML-RPC reply documents. A XML-RPC document structure
 * is compound consting of name-value pairs.
 * @sa http://www.xml-rpc.com
 */
struct rpc_struct {
	int vtype;
	xmlNodePtr struct_in;           /**< Pointer to the structure parameter */
	struct xmlrpc_reply struct_out; /**< Structure to be sent in reply */
	struct xmlrpc_reply* reply;     /**< Print errors here */
	int n;                          /**< Number of structure members
									   created */
	xmlDocPtr doc;                  /**< XML-RPC document */
	int offset;                     /**< Offset in the reply where the
									   structure should be printed */
	struct rpc_struct* nnext;	/**< nested structure support - a recursive list of nested structrures */
	struct rpc_struct* parent;	/**< access to parent structure - used for flattening structure before reply */
	struct rpc_struct* next;
};


/** The context of the XML-RPC request being processed.
 *
 * This is a global variable that records the context of the XML-RPC request
 * being currently processed.  
 * @sa rpc_ctx
 */
static rpc_ctx_t ctx;

static void close_doc(rpc_ctx_t* ctx);
static void set_fault(struct xmlrpc_reply* reply, int code, char* fmt, ...);
static int fixup_xmlrpc_reply(void** param, int param_no);

/** Pointers to the functions that implement the RPC interface
 * of xmlrpc SER module
 */
static rpc_t func_param;

/** Enable/disable additional introspection methods.  If set to 1 then the
 * functions defined in http://scripts.incutio.com/xmlrpc/introspection.html
 * will be available on the server. If set to 0 then the functions will be
 * disabled.
 */
static char* xmlrpc_route=0; /* default is the main route */


/** Reference to the sl (stateless replies) module of SER The sl module of SER
 * is needed so that the xmlrpc SER module can send replies back to clients
 */
sl_api_t slb;

static int xmlrpc_route_no=DEFAULT_RT;
/* if set, try autoconverting to the requested type if possible
  (e.g. convert 1 to "1" if string is requested) */
static int autoconvert=0;
/* in replies, escape CR to &#xD (according to the xml specs) */
static int escape_cr=1; /* default on */
/* convert double LF to CR LF (when on, LFLF becomes an escape for CRLF, needed
 with some xmlrpc clients that are not escaping CR to &#xD; )*/
static int lflf2crlf=0; /* default off */
/* do not register for non-sip requests */
static int xmlrpc_mode = 0;

static char* xmlrpc_url_match = NULL;
static regex_t xmlrpc_url_match_regexp;
static char* xmlrpc_url_skip = NULL;
static regex_t xmlrpc_url_skip_regexp;


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
	{"route",             PARAM_STRING, &xmlrpc_route},
	{"autoconversion",    PARAM_INT,    &autoconvert},
	{"escape_cr",         PARAM_INT,    &escape_cr},
	{"double_lf_to_crlf", PARAM_INT,    &lflf2crlf},
	{"mode",              PARAM_INT,    &xmlrpc_mode},
	{"url_match",         PARAM_STRING, &xmlrpc_url_match},
	{"url_skip",          PARAM_STRING, &xmlrpc_url_skip},
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
#define ESC_CR  "&#xD;"


static void clean_context(rpc_ctx_t* ctx);


/** Adds arbitrary text to the XML-RPC reply being constructed, special
 * characters < and & will be escaped.
 *
 * This function adds arbitrary text to the body of the XML-RPC reply being
 * constructed. Note well that the function does not check whether the XML
 * document being constructed is well-formed or valid. Use with care.
 *
 * @param reply Pointer to the structure representing the XML-RPC reply
 *              being constructed.
 * @param text The text to be appended to the XML-RPC reply.
 * @return -1 on error, 0 if the text was added successfuly.
 * @sa add_xmlrpc_reply()
 */
static int add_xmlrpc_reply_esc(struct xmlrpc_reply* reply, str* text)
{
    char* p;
    int i;

    for(i = 0; i < text->len; i++) {
		/* 10 must be bigger than size of longest escape sequence */
		if (reply->body.len >= reply->buf.len - 10) { 
			p = mxr_malloc(reply->buf.len + 1024);
			if (!p) {
				set_fault(reply, 500, 
						  "Internal Server Error (No memory left)");
				ERR("No memory left: %d\n", reply->body.len + 1024);
				return -1;
			}
			memcpy(p, reply->body.s, reply->body.len);
			mxr_free(reply->buf.s);
			reply->buf.s = p;
			reply->buf.len += 1024;
			reply->body.s = p;
		}
		
		switch(text->s[i]) {
		case '<':
			memcpy(reply->body.s + reply->body.len, ESC_LT, 
				   sizeof(ESC_LT) - 1);
			reply->body.len += sizeof(ESC_LT) - 1;
			break;
			
		case '&':
			memcpy(reply->body.s + reply->body.len, ESC_AMP, 
				   sizeof(ESC_AMP) - 1);
			reply->body.len += sizeof(ESC_AMP) - 1;
			break;
			
		case '\r':
			if (likely(escape_cr)){
				memcpy(reply->body.s + reply->body.len, ESC_CR,
					sizeof(ESC_CR) - 1);
				reply->body.len += sizeof(ESC_CR) - 1;
				break;
			}
			/* no break */
		default:
			reply->body.s[reply->body.len] = text->s[i];
			reply->body.len++;
			break;
		}
    }
    return 0;
}

/** Add arbitrary text to the XML-RPC reply being constructed, no escaping
 * done.
 * 
 * This is a more efficient version of add_xmlrpc_reply_esc(), the function
 * appends arbitrary text to the end of the XML-RPC reply being constructed,
 * but the text must not contain any characters that need to be escaped in
 * XML, such as < and & (or the characters must be escaped already).
 *
 * @param reply Pointer to the structure representing the XML-RPC reply
 *              being constructed.
 * @param text The text to be appended to the XML-RPC reply.
 * @return -1 on error, 0 if the text was added successfuly.
 * @sa add_xmlrpc_reply_esc()
 */
static int add_xmlrpc_reply(struct xmlrpc_reply* reply, str* text)
{
	char* p;
	if (text->len > (reply->buf.len - reply->body.len)) {
		p = mxr_malloc(reply->buf.len + text->len + 1024);
		if (!p) {
			set_fault(reply, 500, "Internal Server Error (No memory left)");
			ERR("No memory left: %d\n", reply->buf.len + text->len + 1024);
			return -1;
		}
		memcpy(p, reply->body.s, reply->body.len);
		mxr_free(reply->buf.s);
		reply->buf.s = p;
		reply->buf.len += text->len + 1024;
		reply->body.s = p;
	}
	memcpy(reply->body.s + reply->body.len, text->s, text->len);
	reply->body.len += text->len;
	return 0;
}

/** Adds arbitrary text to the XML-RPC reply being constructed, the text will
 * be inserted at a specified offset within the XML-RPC reply.
 *
 * This function inserts arbitrary text in the XML-RPC reply that is being
 * constructed, unlike add_xmlrp_reply(), this function will not append the
 * text at the end of the reply, but it will insert the text in the middle of
 * the reply at the position provided to the function in "offset"
 * parameter. The function does not escape special characters and thus the
 * text must not contain such characters (or the must be escaped already).
 *
 * @param reply The XML-RPC reply structure representing the reply being
 *              constructed.
 * @param offset The position of the first character where the text should be
 *               inserted. 
 * @param text The text to be inserted.
 * @return 0 of the text was inserted successfuly, a negative number on error.
 */
static int add_xmlrpc_reply_offset(struct xmlrpc_reply* reply, unsigned int offset, str* text)
{
	char* p;
	if (text->len > (reply->buf.len - reply->body.len)) {
		p = mxr_malloc(reply->buf.len + text->len + 1024);
		if (!p) {
			set_fault(reply, 500, "Internal Server Error (No memory left)");
			ERR("No memory left: %d\n", reply->buf.len + text->len + 1024);
			return -1;
		}
		memcpy(p, reply->body.s, reply->body.len);
		mxr_free(reply->buf.s);
		reply->buf.s = p;
		reply->buf.len += text->len + 1024;
		reply->body.s = p;
	}
	memmove(reply->body.s + offset + text->len, reply->body.s + offset, 
			reply->body.len - offset);
	memcpy(reply->body.s + offset, text->s, text->len);
	reply->body.len += text->len;
	return 0;
}


/** Returns the current length of the XML-RPC reply body.
 *
 * @param reply The XML-RPC reply being constructed
 * @return Number of bytes of the XML-RPC reply body.
 */
static unsigned int get_reply_len(struct xmlrpc_reply* reply)
{
	return reply->body.len;
}


/* Resets XMLRPC reply body.
 *
 * This function discards everything that has been written so far and starts
 * constructing the XML-RPC reply body from the beginning.
 *
 * @param reply The XML-RPC reply being constructed.
 */
static void reset_xmlrpc_reply(struct xmlrpc_reply* reply)
{
	reply->body.len = 0;
}

/** Initialize XML-RPC reply data structure.
 *
 * This function initializes the data structure that contains all data related
 * to the XML-RPC reply being created. The function must be called before any
 * other function that adds data to the reply.
 * @param reply XML-RPC reply structure to be initialized.
 * @return 0 on success, a negative number on error.
 */
static int init_xmlrpc_reply(struct xmlrpc_reply* reply)
{
	reply->code = 200;
	reply->reason = "OK";
	reply->buf.s = mxr_malloc(1024);
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

/** Clear the XML-RPC reply code and sets it back to a success reply.
 *
 * @param reply XML-RPC reply structure to be cleared.
 */
static void clear_xmlrpc_reply(struct xmlrpc_reply* reply)
{
	reply->code = 200;
	reply->reason = "OK";
}


/* if this a delayed reply context, and it's never been use before, fix it */
static int fix_delayed_reply_ctx(rpc_ctx_t* ctx)
{
	if  ((ctx->flags & XMLRPC_DELAYED_CTX_F) && (ctx->reply.buf.s==0)){
		if (init_xmlrpc_reply(&ctx->reply) <0) return -1;
		add_xmlrpc_reply(&ctx->reply, &success_prefix);
		if (ctx->flags & RET_ARRAY)
			return add_xmlrpc_reply(&ctx->reply, &array_prefix);
	}
	return 0;
}



/** Free all memory used by the XML-RPC reply structure. */
static void clean_xmlrpc_reply(struct xmlrpc_reply* reply)
{
	if (reply->buf.s) mxr_free(reply->buf.s);
}

/** Create XML-RPC reply that indicates an error to the caller.
 *
 * This function is used to build the XML-RPC reply body that indicates that
 * an error ocurred on the server. It is called when a management function in
 * SER reports an error. The reply will contain the reason code and reason
 * phrase text provided by the management function that indicated the error.
 */
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


/** Add a memory registion to the list of memory blocks that
 * need to be re-claimed later.
 *
 * @param type The type of the memory block (ordinary text or structure).
 * @param ptr A pointer to the memory block.
 * @param reply The XML-RPC the memory block is associated with.
 * @return 0 on success, a negative number on error.
 * @sa collect_garbage()
 */
static int add_garbage(int type, void* ptr, struct xmlrpc_reply* reply)
{
	struct garbage* p;

	p = (struct garbage*)mxr_malloc(sizeof(struct garbage));
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

/** Re-claims all memory allocated in the process of building XML-RPC
 * reply.
 */
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
			if (s && s->struct_out.buf.s) mxr_free(s->struct_out.buf.s);
			if (s) mxr_free(s);
			break;

		case JUNK_PKGCHAR:
			if (p->ptr){
				mxr_free(p->ptr);
				p->ptr=0;
			}
			break;

		default:
			ERR("BUG: Unsupported junk type\n");
		}
		mxr_free(p);
	}
}


/** Extract XML-RPC query from a SIP/HTTP message.
 *
 * @param doc A pointer to string descriptor that will be filled
 *            with the pointer to the beginning of the XML-RPC
 *            document and length of the document.
 * @param msg A structure representing the SIP/HTTP message 
 *            carrying the XML-RPC document in body.
 */
static int get_rpc_document(str* doc, sip_msg_t* msg)
{
 	doc->s = get_body(msg);
	if (!doc->s) {
	        ERR("Error while extracting message body\n");
		return -1;
	}
	doc->len = strlen(doc->s);
	return 0;
}


/** Send a reply to the client with given body.
 *
 * This function sends a 200 OK reply back to the client, the body of the
 * reply will contain text provided to the function in "body" parameter.
 *
 * @param msg The request that generated the reply.
 * @param body The text that will be put in the body of the reply.
 */
static int send_reply(sip_msg_t* msg, str* body)
{
	if (add_lump_rpl(msg, body->s, body->len, LUMP_RPL_BODY) < 0) {
		ERR("Error while adding reply lump\n");
		return -1;
	}

	if (slb.zreply(msg, 200, "OK") == -1) {
		ERR("Error while sending reply\n");
		return -1;
	}

	return 0;
}

static int flatten_nests(struct rpc_struct* st, struct xmlrpc_reply* reply) {
	if (!st)
		return 1;

	if (!st->nnext) {
		if(st->vtype == RET_ARRAY) {
			if (add_xmlrpc_reply(&st->struct_out, &array_suffix) < 0) return -1;
		} else {
			if (add_xmlrpc_reply(&st->struct_out, &struct_suffix) < 0) return -1;
		}
		if (add_xmlrpc_reply_offset(&st->parent->struct_out, st->offset, &st->struct_out.body) < 0) return -1;
	} else {
		flatten_nests(st->nnext, reply);
		if(st->vtype == RET_ARRAY) {
			if (add_xmlrpc_reply(&st->struct_out, &array_suffix) < 0) return -1;
		} else {
			if (add_xmlrpc_reply(&st->struct_out, &struct_suffix) < 0) return -1;
		}
		if (add_xmlrpc_reply_offset(&st->parent->struct_out, st->offset, &st->struct_out.body) < 0) return -1;
	}
	return 1;
}

static int print_structures(struct xmlrpc_reply* reply, 
							struct rpc_struct* st)
{
	while(st) {
		     /* Close the structure first */
		if(st->vtype == RET_ARRAY) {
			if (add_xmlrpc_reply(&st->struct_out, &array_suffix) < 0) return -1;
		} else {
			if (add_xmlrpc_reply(&st->struct_out, &struct_suffix) < 0) return -1;
		}
		if (flatten_nests(st->nnext, &st->struct_out) < 0) return -1;
		if (add_xmlrpc_reply_offset(reply, st->offset, &st->struct_out.body) < 0) return -1;
		st = st->next;
	}
	return 0;
}

/** Implementation of rpc_send function required by the management API in SER.
 *
 * This is the function that will be called whenever a management function in
 * SER asks the management interface to send the reply to the client. The
 * function will generate the XML-RPC document, put it in body of a SIP
 * response and send the response to the client. The SIP/HTTP reply sent to
 * the client will be always 200 OK, if an error ocurred on the server then it
 * will be indicated in the XML document in body.
 *
 * @param ctx A pointer to the context structure of the XML-RPC request that
 *            generated the reply.  
 * @return 1 if the reply was already sent, 0 on success, a negative number on
 *            error
 */
static int rpc_send(rpc_ctx_t* ctx)
{
	struct xmlrpc_reply* reply;

	if (ctx->reply_sent) return 1;

	reply = &ctx->reply;
	if (reply->code >= 300) {
		if (build_fault_reply(reply) < 0) return -1;
	} else {
		if (ctx->flags & RET_ARRAY && 
			add_xmlrpc_reply(reply, &array_suffix) < 0) return -1;
		if (ctx->structs && 
			print_structures(reply, ctx->structs) < 0) return -1;
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

/** Implementation of rpc_fault function required by the management API in 
 * SER.
 *
 * This function will be called whenever a management function in SER
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
	static char buf[REASON_BUF_LEN];
	va_list ap;

	ctx->reply.code = code;
	va_start(ap, fmt);
	vsnprintf(buf, REASON_BUF_LEN, fmt, ap);
	va_end(ap);
	ctx->reply.reason = buf;
}

/** Create and initialize a new rpc_structure data structure.
 *
 * This function allocates and initializes memory for a new rpc_struct
 * structure. If the caller provided non-NULL pointers in doc and structure
 * parameters then the structure is coming from an XML-RPC request. If either
 * of the pointers is NULL then we are creating a structure that will be
 * attached to a XML-RPC reply sent to the client. The memory allocated in
 * this function will be added to the garbage collection list.
 *
 * @param doc A pointer to the XML-RPC request document or NULL if we create
 *            a structure that will be put in a reply.
 * @param structure A pointer to opening tag of the structure in the XML-RPC
 *                  request document or NULL if we create a structure that
 *                  will be put in a XML-RPC reply.
 * @param reply A pointer to xml_reply structure, NULL if it is a structure
 *              coming from a XML-RPC request.
 */
static struct rpc_struct* new_rpcstruct(xmlDocPtr doc, xmlNodePtr structure, 
										struct xmlrpc_reply* reply, int vtype)
{
	struct rpc_struct* p;

	p = (struct rpc_struct*)mxr_malloc(sizeof(struct rpc_struct));
	if (!p) {
		set_fault(reply, 500, "Internal Server Error (No Memory Left");
		return 0;
	}
	memset(p, 0, sizeof(struct rpc_struct));
	p->struct_in = structure;

	p->reply = reply;
	p->n = 0;
	p->vtype = vtype;
	if (doc && structure) {
		     /* We will be parsing structure from request */
		p->doc = doc;
		p->struct_in = structure;
	} else {
		     /* We will build a reply structure */
		if (init_xmlrpc_reply(&p->struct_out) < 0) goto err;
		if(vtype==RET_ARRAY) {
			if (add_xmlrpc_reply(&p->struct_out, &array_prefix) < 0) goto err;
		} else {
			if (add_xmlrpc_reply(&p->struct_out, &struct_prefix) < 0) goto err;
		}
	}
	if (add_garbage(JUNK_RPCSTRUCT, p, reply) < 0) goto err;
	return p;

 err:
	if (p->struct_out.buf.s) mxr_free(p->struct_out.buf.s);
	mxr_free(p);
	return 0;
}

/** Converts the variables provided in parameter ap according to formatting
 * string provided in parameter fmt into parameters in XML-RPC format.
 *
 * This function takes the parameters provided in ap parameter and creates
 * XML-RPC formatted parameters that will be put in the document in res
 * parameter. The format of input parameters is described in formatting string
 * fmt which follows the syntax of the management API in SER. In the case of
 * an error the function will generate an error reply in err_reply parameter
 * instead.
 * @param res A pointer to the XML-RPC result structure where the parameters
 *            will be written.
 * @param err_reply An error reply document will be generated here if the
 *                  function encounters a problem while processing input
 *                  parameters.
 * @param fmt Formatting string of the management API in SER.
 * @param ap A pointer to the array of input parameters.
 *
 */
static int print_value(struct xmlrpc_reply* res, 
					   struct xmlrpc_reply* err_reply, char fmt, va_list* ap)
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
		body.s = sint2str(va_arg(*ap, int), &body.len);
		break;

	case 'u':
		prefix = int_prefix;
		suffix = int_suffix;
		body.s = int2str(va_arg(*ap, unsigned int), &body.len);
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
		ERR("Invalid formatting character [%c]\n", fmt);
		goto err;
	}

	if (add_xmlrpc_reply(res, &prefix) < 0) goto err;
	if (add_xmlrpc_reply_esc(res, &body) < 0) goto err;
	if (add_xmlrpc_reply(res, &suffix) < 0) goto err;
	return 0;
 err:
	return -1;
}

/** Implementation of rpc_add function required by the management API in SER.
 *
 * This function will be called when a management function in SER calls
 * rpc->add to add a parameter to the XML-RPC reply being generated.
 */
static int rpc_add(rpc_ctx_t* ctx, char* fmt, ...)
{
	void* void_ptr;
	va_list ap;
	struct xmlrpc_reply* reply;
	struct rpc_struct* p;

	fix_delayed_reply_ctx(ctx);
	va_start(ap, fmt);
	reply = &ctx->reply;

	while(*fmt) {
		if (ctx->flags & RET_ARRAY && 
			add_xmlrpc_reply(reply, &value_prefix) < 0) goto err;
		if (*fmt == '{' || *fmt == '[') {
			void_ptr = va_arg(ap, void**);
			p = new_rpcstruct(0, 0, reply, (*fmt=='[')?RET_ARRAY:0);
			if (!p) goto err;
			*(struct rpc_struct**)void_ptr = p;
			p->offset = get_reply_len(reply);
			p->next = ctx->structs;
			ctx->structs = p;
		} else {
			if (print_value(reply, reply, *fmt, &ap) < 0) goto err;
		}

		if (ctx->flags & RET_ARRAY && 
			add_xmlrpc_reply(reply, &value_suffix) < 0) goto err;
		if (add_xmlrpc_reply(reply, &lf) < 0) goto err;
		fmt++;
	}
	va_end(ap);
	return 0;
 err:
	va_end(ap);
	return -1;
}


/** Convert time in XML-RPC format to time_t */
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



/* get_* flags: */
#define GET_X_AUTOCONV 1
#define GET_X_NOREPLY 2
#define GET_X_LFLF2CRLF 4  /* replace "\n\n" with "\r\n" */

/* xml value types */
enum xmlrpc_val_type{
	XML_T_STR,
	XML_T_TXT,
	XML_T_INT,
	XML_T_BOOL,
	XML_T_DATE,
	XML_T_DOUBLE,
	XML_T_ERR=-1
};



/** Returns the XML-RPC value type.
 * @return value type (>= on success, XML_T_ERR on error/unknown type)
 */
static enum xmlrpc_val_type xml_get_type(xmlNodePtr value)
{
	if (!xmlStrcmp(value->name, BAD_CAST "string")){
		return XML_T_STR;
	} else if (!xmlStrcmp(value->name, BAD_CAST "text")) {
		return XML_T_TXT;
	} else if ( !xmlStrcmp(value->name, BAD_CAST "i4") ||
				!xmlStrcmp(value->name, BAD_CAST "int")) {
		return XML_T_INT;
	} else if (!xmlStrcmp(value->name, BAD_CAST "boolean")) {
		return XML_T_BOOL;
	} else if (!xmlStrcmp(value->name, BAD_CAST "dateTime.iso8601")) {
		return XML_T_DATE;
	}else if (!(xmlStrcmp(value->name, BAD_CAST "double"))){
		return XML_T_DOUBLE;
	}
	return XML_T_ERR;
}



/** Converts an XML-RPC encoded parameter into integer if possible.
 *
 * This function receives a pointer to a parameter encoded in XML-RPC format
 * and tries to convert the value of the parameter into integer.  Only
 * &lt;i4&gt;, &lt;int&gt;, &lt;boolean&gt;, &lt;dateTime.iso8601&gt; XML-RPC
 * parameters can be converted to integer, attempts to conver other types will
 * fail.
 * @param val A pointer to an integer variable where the result will be 
 *            stored.  
 * @param reply A pointer to XML-RPC reply being constructed (used to 
 *        indicate conversion errors).  
 * @param doc A pointer to the XML-RPC request document.  
 * @param value A pointer to the element containing the parameter to be 
 *              converted within the document.
 * @param flags : GET_X_AUTOCONV - try autoconverting
 *                GET_X_NOREPLY - do not reply
 * @return <0 on error, 0 on success
 */
static int get_int(int* val, struct xmlrpc_reply* reply, 
				   xmlDocPtr doc, xmlNodePtr value, int flags)
{
	enum xmlrpc_val_type type;
	int ret;
	xmlNodePtr i4;
	char* val_str;
	char* end_ptr;

	if (!value || xmlStrcmp(value->name, BAD_CAST "value")) {
		if (!(flags & GET_X_NOREPLY))
			set_fault(reply, 400, "Invalid parameter value");
		return -1;
	}

	i4 = value->xmlChildrenNode;
	if (!i4){
		if (!(flags & GET_X_NOREPLY))
			set_fault(reply, 400, "Invalid Parameter Type");
		return -1;
	}
	type=xml_get_type(i4);
	switch(type){
		case XML_T_INT:
		case XML_T_BOOL:
		case XML_T_DATE:
			break;
		case XML_T_DOUBLE:
		case XML_T_STR:
		case XML_T_TXT:
			if (flags & GET_X_AUTOCONV)
				break;
		case XML_T_ERR:
			if (!(flags & GET_X_NOREPLY))
				set_fault(reply, 400, "Invalid Parameter Type");
			return -1;
	}
	if (type == XML_T_TXT)
		val_str = (char*)i4->content;
	else
		val_str = (char*)xmlNodeListGetString(doc, i4->xmlChildrenNode, 1);
	if (!val_str) {
		if (!(flags & GET_X_NOREPLY))
			set_fault(reply, 400, "Empty Parameter Value");
		return -1;
	}
	ret=0;
	switch(type){
		case XML_T_INT:
		case XML_T_BOOL:
		case XML_T_STR:
		case XML_T_TXT:
			/* Integer/bool conversion */
			*val = strtol(val_str, &end_ptr, 10);
			if (val_str==end_ptr)
				ret=-1;
			break;
		case XML_T_DATE:
			*val = xmlrpc2time(val_str);
			break;
		case XML_T_DOUBLE:
			*val = (int)strtod(val_str, &end_ptr);
			if (val_str==end_ptr)
				ret=-1;
			break;
		case XML_T_ERR:
			*val=0;
			ret=-1;
			break;
	}
	xmlFree(val_str);
	if (ret==-1 && !(flags & GET_X_NOREPLY))
		set_fault(reply, 400, "Invalid Value");
	return ret;
}



/** Converts an XML-RPC encoded parameter into double if possible.
 *
 * This function receives a pointer to a parameter encoded in XML-RPC format
 * and tries to convert the value of the parameter into double.  Only
 * &lt;i4&gt;, &lt;int&gt;, &lt;double&gt; XML-RPC parameters can be converted
 * to double, attempts to conver other types will fail.
 * @param val A pointer to an integer variable where the result will be 
 *            stored.
 * @param reply A pointer to XML-RPC reply being constructed (used to indicate
 *              conversion errors).
 * @param doc A pointer to the XML-RPC request document.
 * @param value A pointer to the element containing the parameter to be 
 *              converted within the document.
 * @param flags : GET_X_AUTOCONV - try autoconverting
 *                GET_X_NOREPLY - do not reply
 * @return <0 on error, 0 on success
 */
static int get_double(double* val, struct xmlrpc_reply* reply, 
					  xmlDocPtr doc, xmlNodePtr value, int flags)
{
	xmlNodePtr dbl;
	char* val_str;
	char* end_ptr;
	enum xmlrpc_val_type type;
	int ret;

	if (!value || xmlStrcmp(value->name, BAD_CAST "value")) {
		if (!(flags & GET_X_NOREPLY))
			set_fault(reply, 400, "Invalid Parameter Value");
		return -1;
	}

	dbl = value->xmlChildrenNode;
	if (!dbl){
		if (!(flags & GET_X_NOREPLY))
			set_fault(reply, 400, "Invalid Parameter Type");
		return -1;
	}
	type=xml_get_type(dbl);
	switch(type){
		case XML_T_DOUBLE:
		case XML_T_INT:
			break;
		case XML_T_BOOL:
		case XML_T_DATE:
		case XML_T_STR:
		case XML_T_TXT:
			if (flags & GET_X_AUTOCONV)
				break;
		case XML_T_ERR:
			if (!(flags & GET_X_NOREPLY))
				set_fault(reply, 400, "Invalid Parameter Type");
			return -1;
	}
	if (type == XML_T_TXT)
		val_str = (char*)dbl->content;
	else
		val_str = (char*)xmlNodeListGetString(doc, dbl->xmlChildrenNode, 1);
	if (!val_str) {
		if (!(flags & GET_X_NOREPLY))
			set_fault(reply, 400, "Empty Double Parameter");
		return -1;
	}
	ret=0;
	switch(type){
		case XML_T_DOUBLE:
		case XML_T_INT:
		case XML_T_BOOL:
		case XML_T_STR:
		case XML_T_TXT:
			*val = strtod(val_str, &end_ptr);
			if (val_str==end_ptr)
				ret=-1;
			break;
		case XML_T_DATE:
			*val = (double)xmlrpc2time(val_str);
			break;
		case XML_T_ERR:
			*val=0;
			ret=-1;
			break;
	}
	xmlFree(val_str);
	if (ret==-1 && !(flags & GET_X_NOREPLY))
		set_fault(reply, 400, "Invalid Value");
	return ret;
}


/** Convert a parameter encoded in XML-RPC to a zero terminated string.
 *
 * @param val A pointer to a char* variable where the result will be 
 *            stored (the result is dynamically allocated, but it's garbage
 *            collected, so it doesn't have to be freed)
 * @param reply A pointer to XML-RPC reply being constructed (used to indicate
 *              conversion errors).
 * @param doc A pointer to the XML-RPC request document.
 * @param value A pointer to the element containing the parameter to be 
 *              converted within the document.
 * @param flags 
 *              - GET_X_AUTOCONV - try autoconverting
 *              - GET_X_LFLF2CRLF - replace double '\\n' with `\\r\\n'
 *              - GET_X_NOREPLY - do not reply
 * @return <0 on error, 0 on success
 */
static int get_string(char** val, struct xmlrpc_reply* reply, 
					  xmlDocPtr doc, xmlNodePtr value, int flags)
{
	static char* null_str = "";
	xmlNodePtr dbl;
	char* val_str;
	char* end_ptr;
	char* s;
	char* p;
	int i;
	int len;
	enum xmlrpc_val_type type;
	int ret;

	if (!value || xmlStrcmp(value->name, BAD_CAST "value")) {
		if (!(flags & GET_X_NOREPLY))
			set_fault(reply, 400, "Invalid Parameter Value");
		return -1;
	}

	dbl = value->xmlChildrenNode;
	if (!dbl){
		if (!(flags & GET_X_NOREPLY))
			set_fault(reply, 400, "Invalid Parameter Type");
		return -1;
	}
	type=xml_get_type(dbl);
	switch(type){
		case XML_T_STR:
		case XML_T_TXT:
			break;
		case XML_T_INT:
		case XML_T_BOOL:
		case XML_T_DATE:
		case XML_T_DOUBLE:
			if (flags & GET_X_AUTOCONV)
				break;
		case XML_T_ERR:
			if (!(flags & GET_X_NOREPLY))
				set_fault(reply, 400, "Invalid Parameter Type");
			return -1;
	}
	if (type == XML_T_TXT)
		val_str = (char*)dbl->content;
	else
		val_str = (char*)xmlNodeListGetString(doc, dbl->xmlChildrenNode, 1);

	if (!val_str) {
		if (type==XML_T_STR || type==XML_T_TXT){
			*val = null_str;
			return 0;
		}else{
			if (!(flags & GET_X_NOREPLY))
				set_fault(reply, 400, "Empty Parameter Value");
			return -1;
		}
	}
	ret=0;
	switch(type){
		case XML_T_STR:
		case XML_T_TXT:
			if (flags & GET_X_LFLF2CRLF){
				p=val_str;
				while(*p){
					if (*p=='\n' && *(p+1)=='\n'){
						*p='\r';
						p+=2;
						continue;
					}
					p++;
				}
			}
			/* no break */
		case XML_T_DATE:  /* no special conversion */
		case XML_T_DOUBLE: /* no special conversion */
			if (add_garbage(JUNK_XMLCHAR, val_str, reply) < 0){
				xmlFree(val_str);
				return -1;
			}
			*val = val_str;
			break;
		case XML_T_INT:
		case XML_T_BOOL:
			/* convert str to int an back to str */
			i = strtol(val_str, &end_ptr, 10);
			if (val_str==end_ptr){
				ret=-1;
			}else{
				s=sint2str(i, &len);
				p=mxr_malloc(len+1);
				if (p && add_garbage(JUNK_PKGCHAR, p, reply) == 0){
					memcpy(p, s, len);
					p[len]=0;
					*val=p;
				}else{
					ret=-1;
					if (p) mxr_free(p);
				}
			}
			xmlFree(val_str);
			break;
		case XML_T_ERR:
			xmlFree(val_str);
			ret=-1;
			break;
	}
	return ret;
}



/** Implementation of rpc->scan function required by the management API in
 * SER.
 *
 * This is the function that will be called whenever a management function in
 * SER calls rpc->scan to get the value of parameter from the XML-RPC
 * request. This function will extract the current parameter from the XML-RPC
 * document and attempts to convert it to the type requested by the management
 * function that called it.
 */
static int rpc_scan(rpc_ctx_t* ctx, char* fmt, ...)
{
	int read;
	int ival;
	int* int_ptr;
	unsigned int* uint_ptr;
	char** char_ptr;
	str* str_ptr;
	double* double_ptr;
	void** void_ptr;
	xmlNodePtr value;
	struct xmlrpc_reply* reply;
	struct rpc_struct* p;
	int modifiers;
	int f;
	va_list ap;
	int nofault;

	reply = &ctx->reply;
	/* clear the previously saved error code */
	clear_xmlrpc_reply(reply);

	va_start(ap, fmt);
	modifiers=0;
	read = 0;
	nofault = 0;
	f=(autoconvert?GET_X_AUTOCONV:0) |
		(lflf2crlf?GET_X_LFLF2CRLF:0);
	while(*fmt) {
		if (!ctx->act_param) goto error;
		value = ctx->act_param->xmlChildrenNode;

		switch(*fmt) {
		case '*': /* start of optional parameters */
			modifiers++;
			read++;
			fmt++;
			nofault=1;
			f|=GET_X_NOREPLY;
			continue; /* do not advance ctx->act-param */
		case '.': /* autoconvert */
			modifiers++;
			read++;
			fmt++;
			f|=GET_X_AUTOCONV;
			continue; /* do not advance ctx->act-param */
		case 'b': /* Bool */
		case 't': /* Date and time */
		case 'd': /* Integer */
			int_ptr = va_arg(ap, int*);
			if (get_int(int_ptr, reply, ctx->doc, value, f) < 0) goto error;
			break;

		case 'u': /* Integer */
			uint_ptr = va_arg(ap, unsigned int*);
			if (get_int(&ival, reply, ctx->doc, value, f) < 0) goto error;
			*uint_ptr = (unsigned int)ival;
			break;
			
		case 'f': /* double */
			double_ptr = va_arg(ap, double*);
			if (get_double(double_ptr, reply, ctx->doc, value, f) < 0) {
				goto error;
			}
			break;

		case 's': /* zero terminated string */
			char_ptr = va_arg(ap, char**);
			if (get_string(char_ptr, reply, ctx->doc, value, f) < 0)
				goto error;
			break;

		case 'S': /* str structure */
			str_ptr = va_arg(ap, str*);
			if (get_string(&str_ptr->s, reply, ctx->doc, value, f) < 0) {
				goto error;
			}
			str_ptr->len = strlen(str_ptr->s);
			break;

		case '{':
			void_ptr = va_arg(ap, void**);
			if (!value->xmlChildrenNode) goto error;
			p = new_rpcstruct(ctx->doc, value->xmlChildrenNode, reply, 0);
			if (!p) goto error;
			*void_ptr = p;
			break;

		default:
			ERR("Invalid parameter type in formatting string: %c\n", *fmt);
			set_fault(reply, 500, 
					  "Server Internal Error (Invalid Formatting String)");
			goto error;
		}
		ctx->act_param = ctx->act_param->next;
		/* clear autoconv if not globally on */
		f=autoconvert?GET_X_AUTOCONV:(f&~GET_X_AUTOCONV);
		read++;
		fmt++;
	}
	va_end(ap);
	return read-modifiers;

 error:
	va_end(ap);
	if(nofault==0)
		return -(read-modifiers);
	else
		return read-modifiers;
}

#define RPC_BUF_SIZE 1024


/** Implementation of rpc_rpl_printf function required by the management API in
 *	SER.
 *
 * This function will be called whenever a management function in SER calls
 * rpc-printf to add a parameter to the XML-RPC reply being constructed.
 */
static int rpc_rpl_printf(rpc_ctx_t* ctx, char* fmt, ...)
{
	int n, buf_size;
	char* buf;
	va_list ap;
	str s;
	struct xmlrpc_reply* reply;

	fix_delayed_reply_ctx(ctx);
	reply = &ctx->reply;
	buf = (char*)mxr_malloc(RPC_BUF_SIZE);
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
			if (ctx->flags & RET_ARRAY && 
				add_xmlrpc_reply(reply, &value_prefix) < 0) goto err;
			if (add_xmlrpc_reply(reply, &string_prefix) < 0) goto err;
			if (add_xmlrpc_reply_esc(reply, &s) < 0) goto err;
			if (add_xmlrpc_reply(reply, &string_suffix) < 0) goto err;
			if (ctx->flags & RET_ARRAY && 
				add_xmlrpc_reply(reply, &value_suffix) < 0) goto err;
			if (add_xmlrpc_reply(reply, &lf) < 0) goto err;
			mxr_free(buf);
			return 0;
		}
		     /* Else try again with more space. */
		if (n > -1) {   /* glibc 2.1 */
			buf_size = n + 1; /* precisely what is needed */
		} else {          /* glibc 2.0 */
			buf_size *= 2;  /* twice the old size */
		}
		if ((buf = mxr_realloc(buf, buf_size)) == 0) {
			set_fault(reply, 500, "Internal Server Error (No memory left)");
			ERR("No memory left\n");
			goto err;
		}
	}
	return 0;
 err:
	if (buf) mxr_free(buf);
	return -1;
}

/* Structure manipulation functions */

/** Find a structure member by name.
 */
static int find_member(xmlNodePtr* value, xmlDocPtr doc, xmlNodePtr structure,
					   struct xmlrpc_reply* reply, char* member_name)
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

/** Adds a new member to structure.
 */
static int rpc_struct_add(struct rpc_struct* s, char* fmt, ...)
{
	va_list ap;
	str member_name;
	struct xmlrpc_reply* reply;
	void* void_ptr;
	struct rpc_struct* p, *tmp;

	reply = &s->struct_out;

	va_start(ap, fmt);
	while(*fmt) {
		member_name.s = va_arg(ap, char*);
		member_name.len = (member_name.s ? strlen(member_name.s) : 0);

		if(s->vtype==RET_ARRAY && *fmt == '{') {
			if (add_xmlrpc_reply(reply, &value_prefix) < 0) goto err;
			if (add_xmlrpc_reply(reply, &struct_prefix) < 0) goto err;
		}
		if (add_xmlrpc_reply(reply, &member_prefix) < 0) goto err;
		if (add_xmlrpc_reply(reply, &name_prefix) < 0) goto err;
		if (add_xmlrpc_reply_esc(reply, &member_name) < 0) goto err;
		if (add_xmlrpc_reply(reply, &name_suffix) < 0) goto err;
		if (add_xmlrpc_reply(reply, &value_prefix) < 0) goto err;
		if (*fmt == '{' || *fmt == '[') {
			void_ptr = va_arg(ap, void**);
			p = new_rpcstruct(0, 0, s->reply, (*fmt=='[')?RET_ARRAY:0);
			if (!p)
				goto err;
			*(struct rpc_struct**) void_ptr = p;
			p->offset = get_reply_len(reply);
			p->parent = s;
			if (!s->nnext) {
				s->nnext = p;
			} else {
				for (tmp = s; tmp->nnext; tmp=tmp->nnext);
				tmp->nnext = p;
			}
		} else {
			if (print_value(reply, reply, *fmt, &ap) < 0) goto err;
		}
		if (add_xmlrpc_reply(reply, &value_suffix) < 0) goto err;
		if (add_xmlrpc_reply(reply, &member_suffix) < 0) goto err;
		if(s->vtype==RET_ARRAY && *fmt == '{') {
			if (add_xmlrpc_reply(reply, &struct_suffix) < 0) goto err;
			if (add_xmlrpc_reply(reply, &value_suffix) < 0) goto err;
		}
		fmt++;
	}

	va_end(ap);
	return 0;
 err:
	va_end(ap);
	return -1;
}

/** Adds a new value to an array.
 */
static int rpc_array_add(struct rpc_struct* s, char* fmt, ...)
{
	va_list ap;
	struct xmlrpc_reply* reply;
	void* void_ptr;
	struct rpc_struct* p, *tmp;

	reply = &s->struct_out;
	if(s->vtype!=RET_ARRAY) {
		LM_ERR("parent structure is not an array\n");
		goto err;
	}

	va_start(ap, fmt);
	while(*fmt) {
		if (*fmt == '{' || *fmt == '[') {
			void_ptr = va_arg(ap, void**);
			p = new_rpcstruct(0, 0, s->reply, (*fmt=='[')?RET_ARRAY:0);
			if (!p)
				goto err;
			*(struct rpc_struct**) void_ptr = p;
			p->offset = get_reply_len(reply);
			p->parent = s;
			if (!s->nnext) {
				s->nnext = p;
			} else {
				for (tmp = s; tmp->nnext; tmp=tmp->nnext);
				tmp->nnext = p;
			}
		} else {
			if (print_value(reply, reply, *fmt, &ap) < 0) goto err;
		}
		fmt++;
	}

	va_end(ap);
	return 0;
 err:
	va_end(ap);
	return -1;
}

/** Create a new member from formatting string and add it to a structure.
 */
static int rpc_struct_printf(struct rpc_struct* s, char* member_name, 
							 char* fmt, ...)
{
	int n, buf_size;
	char* buf;
	va_list ap;
	str st, name;
	struct xmlrpc_reply* reply;
	struct xmlrpc_reply* out;

	out = &s->struct_out;
	buf = (char*)mxr_malloc(RPC_BUF_SIZE);
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
		if ((buf = mxr_realloc(buf, buf_size)) == 0) {
			set_fault(reply, 500, "Internal Server Error (No memory left)");
			ERR("No memory left\n");
			goto err;
		}
	}
	return 0;
 err:
	if (buf) mxr_free(buf);
	return -1;

}


static int rpc_struct_scan(struct rpc_struct* s, char* fmt, ...)
{
	int read;
	int ival;
	va_list ap;
	int* int_ptr;
	unsigned int* uint_ptr;
	double* double_ptr;
	char** char_ptr;
	str* str_ptr;
	xmlNodePtr value;
	char* member_name;
	struct xmlrpc_reply* reply;
	int ret;
	int f;

	read = 0;
	f=(autoconvert?GET_X_AUTOCONV:0) |
		(lflf2crlf?GET_X_LFLF2CRLF:0);
	va_start(ap, fmt);
	while(*fmt) {
		member_name = va_arg(ap, char*);
		reply = s->reply;
		/* clear the previously saved error code */
		clear_xmlrpc_reply(reply);
		ret = find_member(&value, s->doc, s->struct_in, reply, member_name);
		if (ret != 0) goto error;

		switch(*fmt) {
		case 'b': /* Bool */
		case 't': /* Date and time */
		case 'd': /* Integer */
			int_ptr = va_arg(ap, int*);
			if (get_int(int_ptr, reply, s->doc, value, f) < 0) goto error;
			break;

		case 'u': /* Integer */
			uint_ptr = va_arg(ap, unsigned int*);
			if (get_int(&ival, reply, s->doc, value, f) < 0) goto error;
			*uint_ptr = (unsigned int)ival;
			break;
			
		case 'f': /* double */
			double_ptr = va_arg(ap, double*);
			if (get_double(double_ptr, reply, s->doc, value, f) < 0)
				goto error;
			break;

		case 's': /* zero terminated string */
			char_ptr = va_arg(ap, char**);
			if (get_string(char_ptr, reply, s->doc, value, f) < 0) goto error;
			break;

		case 'S': /* str structure */
			str_ptr = va_arg(ap, str*);
			if (get_string(&str_ptr->s, reply, s->doc, value, f) < 0)
				goto error;
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


/** Returns the RPC capabilities supported by the xmlrpc driver.
 */
static rpc_capabilities_t rpc_capabilities(rpc_ctx_t* ctx)
{
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
static struct rpc_delayed_ctx* rpc_delayed_ctx_new(rpc_ctx_t* ctx)
{
	struct rpc_delayed_ctx* ret;
	int size;
	rpc_ctx_t* r_ctx;
	struct sip_msg* shm_msg;
	int len;
	
	ret=0;
	shm_msg=0;
	
	if (ctx->reply_sent)
		return 0; /* no delayed reply if already replied */
	/* clone the sip msg */
	shm_msg=sip_msg_shm_clone(ctx->msg, &len, 1);
	if (shm_msg==0)
		goto error;
	
	/* alloc into one block */
	size=ROUND_POINTER(sizeof(*ret))+sizeof(rpc_ctx_t);
	if ((ret=shm_malloc(size))==0)
		goto error;
	memset(ret, 0, size);
	ret->rpc=func_param;
	ret->reply_ctx=(char*)ret+ROUND_POINTER(sizeof(*ret));
	r_ctx=ret->reply_ctx;
	r_ctx->flags=ctx->flags | XMLRPC_DELAYED_CTX_F;
	ctx->flags |= XMLRPC_DELAYED_REPLY_F;
	r_ctx->msg=shm_msg;
	r_ctx->msg_shm_block_size=len;
	
	return ret;
error:
	if (shm_msg)
		shm_free(shm_msg);
	if (ret)
		shm_free(ret);
	return 0;
}



/** Closes a "delayed reply" context and sends the reply.
 * If no reply has been sent the reply will be built and sent automatically.
 * See the notes from rpc_new_delayed_ctx()
 */
static void rpc_delayed_ctx_close(struct rpc_delayed_ctx* dctx)
{
	rpc_ctx_t* r_ctx;
	struct hdr_field* hdr;
	
	r_ctx=dctx->reply_ctx;
	if (unlikely(!(r_ctx->flags & XMLRPC_DELAYED_CTX_F))){
		BUG("reply ctx not marked as async/delayed\n");
		goto error;
	}
	if (fix_delayed_reply_ctx(r_ctx)<0)
		goto error;
	if (!r_ctx->reply_sent){
		rpc_send(r_ctx);
	}
error:
	clean_context(r_ctx);
	/* collect possible garbage (e.g. generated by structures) */
	collect_garbage();
	/* free added lumps (rpc_send adds a body lump) */
	del_nonshm_lump( &(r_ctx->msg->add_rm) );
	del_nonshm_lump( &(r_ctx->msg->body_lumps) );
	del_nonshm_lump_rpl( &(r_ctx->msg->reply_lump) );
	/* free header's parsed structures that were added by failure handlers */
	for( hdr=r_ctx->msg->headers ; hdr ; hdr=hdr->next ) {
		if ( hdr->parsed && hdr_allocs_parse(hdr) &&
		(hdr->parsed<(void*)r_ctx->msg ||
		hdr->parsed>=(void*)(r_ctx->msg+r_ctx->msg_shm_block_size))) {
			/* header parsed filed doesn't point inside uas.request memory
			 * chunck -> it was added by failure funcs.-> free it as pkg */
			DBG("DBG:free_faked_req: removing hdr->parsed %d\n",
					hdr->type);
			clean_hdr_field(hdr);
			hdr->parsed = 0;
		}
	}
	shm_free(r_ctx->msg);
	r_ctx->msg=0;
	dctx->reply_ctx=0;
	shm_free(dctx);
}


/** Starts parsing XML-RPC document, get the name of the method to be called
 * and position the cursor at the first parameter in the document.
 */
static int open_doc(rpc_ctx_t* ctx, sip_msg_t* msg)
{
	str doc = {NULL,0};
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

static int init_context(rpc_ctx_t* ctx, sip_msg_t* msg)
{
	ctx->msg = msg;
	ctx->msg_shm_block_size=0;
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



/** Creates a SIP message (in "buffer" form) from a HTTP XML-RPC request).
 * 
 * NOTE: the result must be mxr_free()'ed when not needed anymore.  
 * @return 0 on error, buffer allocated using mxr_malloc on success.
 */
static char* http_xmlrpc2sip(sip_msg_t* msg, int* new_msg_len)
{
	unsigned int len, via_len;
	char* via, *new_msg, *p;
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
	if (via == 0) {
		DEBUG("failed to build via\n");
		return 0;
	}
	len = msg->first_line.u.request.method.len + 1 /* space */ + 
		XMLRPC_URI_LEN + 1 /* space */ + 
		msg->first_line.u.request.version.len + CRLF_LEN + via_len + 
		(msg->len-msg->first_line.len);
	p = new_msg = mxr_malloc(len + 1);
	if (new_msg == 0) {
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
	p += msg->first_line.u.request.method.len;
	*p = ' ';
	p++;
	memcpy(p, XMLRPC_URI, XMLRPC_URI_LEN);
	p += XMLRPC_URI_LEN;
	*p = ' ';
	p++;
	memcpy(p, msg->first_line.u.request.version.s,
		   msg->first_line.u.request.version.len);
	p += msg->first_line.u.request.version.len;
	memcpy(p, CRLF, CRLF_LEN);
	p += CRLF_LEN;
	memcpy(p, via, via_len);
	p += via_len;
	memcpy(p,  SIP_MSG_START(msg) + msg->first_line.len, 
		   msg->len - msg->first_line.len);
	new_msg[len] = 0; /* null terminate, required by receive_msg() */
	pkg_free(via);
	*new_msg_len = len;
	return new_msg;
}



/** Emulate receive_msg for an XML-RPC request .
 */
static int em_receive_request(sip_msg_t* orig_msg, 
							  char* new_buf, unsigned int new_len)
{
	int ret;
	sip_msg_t tmp_msg, *msg;
	struct run_act_ctx ra_ctx;
	
	ret=0;
	if (new_buf && new_len) {
		memset(&tmp_msg, 0, sizeof(sip_msg_t));
		tmp_msg.buf = new_buf;
		tmp_msg.len = new_len;
		tmp_msg.rcv = orig_msg->rcv;
		tmp_msg.id = orig_msg->id;
		tmp_msg.set_global_address = orig_msg->set_global_address;
		tmp_msg.set_global_port = orig_msg->set_global_port;
		if (parse_msg(new_buf, new_len, &tmp_msg) != 0) {
			ERR("xmlrpc: parse_msg failed\n");
			goto error;
		}
		msg = &tmp_msg;
	} else {
		msg = orig_msg;
	}
	
	/* not needed, performed by the "real" receive_msg()
	   clear_branches();
	   reset_static_buffer();
	*/
	if ((msg->first_line.type != SIP_REQUEST) || (msg->via1 == 0) ||
		(msg->via1->error != PARSE_OK)) {
		BUG("xmlrpc: strange message: %.*s\n", msg->len, msg->buf);
		goto error;
	}
	if (exec_pre_script_cb(msg, REQUEST_CB_TYPE) == 0) {
		goto end; /* drop request */
	}
	/* exec routing script */
	init_run_actions_ctx(&ra_ctx);
	if (run_actions(&ra_ctx, main_rt.rlist[xmlrpc_route_no], msg) < 0) {
		ret=-1;
		DBG("xmlrpc: error while trying script\n");
		goto end;
	}
 end:
	exec_post_script_cb(msg, REQUEST_CB_TYPE); /* needed for example if tm is used */
	/* reset_avps(); non needed, performed by the real receive_msg */
	if (msg != orig_msg) { /* avoid double free (freed from receive_msg
							  too) */
		free_sip_msg(msg);
	}
	return ret;
 error:
	return -1;
}


/** The main handler that will be called when SER core receives a non-SIP
 * request (i.e. HTTP request carrying XML-RPC document in the body).
 */
static int process_xmlrpc(sip_msg_t* msg)
{
	int ret;
	char* fake_msg;
	int fake_msg_len;
	unsigned char* method;
	unsigned int method_len, n_method;
	regmatch_t pmatch;
	char c;
	
	ret=NONSIP_MSG_DROP;
	if (!IS_HTTP(msg))
		return NONSIP_MSG_PASS;

	if(xmlrpc_url_skip!=NULL || xmlrpc_url_match!=NULL)
	{
		c = msg->first_line.u.request.uri.s[msg->first_line.u.request.uri.len];
		msg->first_line.u.request.uri.s[msg->first_line.u.request.uri.len]
			= '\0';
		if (xmlrpc_url_skip!=NULL &&
			regexec(&xmlrpc_url_skip_regexp, msg->first_line.u.request.uri.s,
					1, &pmatch, 0)==0)
		{
			LM_DBG("URL matched skip re\n");
			msg->first_line.u.request.uri.s[msg->first_line.u.request.uri.len]
				= c;
			return NONSIP_MSG_PASS;
		}
		if (xmlrpc_url_match!=NULL &&
			regexec(&xmlrpc_url_match_regexp, msg->first_line.u.request.uri.s,
					1, &pmatch, 0)!=0)
		{
			LM_DBG("URL not matched\n");
			msg->first_line.u.request.uri.s[msg->first_line.u.request.uri.len]
				= c;
			return NONSIP_MSG_PASS;
		}
		msg->first_line.u.request.uri.s[msg->first_line.u.request.uri.len] = c;
	}

	method = (unsigned char*)msg->first_line.u.request.method.s;
	method_len = msg->first_line.u.request.method.len;
	/* first line is always > 4, so it's always safe to try to read the
	 * 1st 4 bytes from method, even if method is shorter*/
	n_method = method[0] + (method[1] << 8) + (method[2] << 16) +
			(method[3] << 24);
	n_method |= 0x20202020;
	n_method &= ((method_len < 4) * (1U << method_len * 8) - 1);
	/* accept only GET or POST */
	if ((n_method == N_HTTP_GET) ||
			((n_method == N_HTTP_POST) && (method_len == HTTP_POST_LEN))) {
		if (msg->via1 == 0){
			/* create a fake sip message */
			fake_msg = http_xmlrpc2sip(msg, &fake_msg_len);
			if (fake_msg == 0) {
				ERR("xmlrpc: out of memory\n");
				ret=NONSIP_MSG_ERROR;
			} else {
			/* send it */
				DBG("new fake xml msg created (%d bytes):\n<%.*s>\n",
					fake_msg_len, fake_msg_len, fake_msg);
				if (em_receive_request(msg, fake_msg, fake_msg_len)<0)
					ret=NONSIP_MSG_ERROR;
				mxr_free(fake_msg);
			}
			return ret; /* we "ate" the message, stop processing */
		} else { /* the message has a via */
			DBG("http xml msg unchanged (%d bytes):\n<%.*s>\n",
				msg->len, msg->len, msg->buf);
			if (em_receive_request(msg, 0, 0)<0)
				ret=NONSIP_MSG_ERROR;
			return ret;
		}
	} else {
		ERR("xmlrpc: bad HTTP request method: \"%.*s\"\n",
			msg->first_line.u.request.method.len,
			msg->first_line.u.request.method.s);
		/* the message was for us, but it is an error */
		return NONSIP_MSG_ERROR;
	}
	return NONSIP_MSG_PASS; /* message not for us, maybe somebody 
								   else needs it */
}


/** The main processing function of xmlrpc module.
 *
 * This is the main function of this module. It extracts the name
 * of the method to be called from XML-RPC request and then it
 * searches through the list of all available management function,
 * when a function with matching name is found then it will be
 * executed.
 */
static int dispatch_rpc(sip_msg_t* msg, char* s1, char* s2)
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
	if (exp->flags & RET_ARRAY && 
		add_xmlrpc_reply(&ctx.reply, &array_prefix) < 0) goto skip;
	exp->function(&func_param, &ctx);

 skip:
	     /* The function may have sent the reply itself */
	if (!ctx.reply_sent && !(ctx.flags&XMLRPC_DELAYED_REPLY_F)) {
		ret = rpc_send(&ctx);
	}
	clean_context(&ctx);
	collect_garbage();
	if (ret < 0) return -1;
	else return 1;
}


/** This function can be called from SER scripts to generate
 * an XML-RPC reply.
 */
static int xmlrpc_reply(sip_msg_t* msg, char* p1, char* p2)
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


/** Implementation of \@xmlrpc.method select that can be used in
 * SER scripts to retrieve the method string from XML-RPC documents
 */
static int select_method(str* res, struct select* s, sip_msg_t* msg)
{
	static char buf[1024];
	str doc = {NULL,0};
	xmlDocPtr xmldoc;
	xmlNodePtr cur;
	char* method;

	xmldoc = 0;
	method = 0;

	if (get_rpc_document(&doc, msg) < 0) goto err;
	xmldoc = xmlReadMemory(doc.s, doc.len, 0, 0, 
						   XML_PARSE_NOBLANKS | 
						   XML_PARSE_NONET | 
						   XML_PARSE_NOCDATA);
	
	if (!xmldoc) goto err;
	cur = xmlDocGetRootElement(xmldoc);
	if (!cur) goto err;
	if (xmlStrcmp(cur->name, (const xmlChar*)"methodCall")) goto err;
	cur = cur->xmlChildrenNode;
	while(cur) {
		if (!xmlStrcmp(cur->name, (const xmlChar*)"methodName")) {
			method = (char*)xmlNodeListGetString(xmldoc, cur->xmlChildrenNode,
												 1);
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

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	memset(&func_param, 0, sizeof(func_param));
	func_param.send = (rpc_send_f)rpc_send;
	func_param.fault = (rpc_fault_f)rpc_fault;
	func_param.add = (rpc_add_f)rpc_add;
	func_param.scan = (rpc_scan_f)rpc_scan;
	func_param.rpl_printf = (rpc_rpl_printf_f)rpc_rpl_printf;
	func_param.struct_add = (rpc_struct_add_f)rpc_struct_add;
	func_param.array_add = (rpc_array_add_f)rpc_array_add;
	func_param.struct_scan = (rpc_struct_scan_f)rpc_struct_scan;
	func_param.struct_printf = (rpc_struct_printf_f)rpc_struct_printf;
	func_param.capabilities = (rpc_capabilities_f)rpc_capabilities;
	func_param.delayed_ctx_new = (rpc_delayed_ctx_new_f)rpc_delayed_ctx_new;
	func_param.delayed_ctx_close =
		(rpc_delayed_ctx_close_f)rpc_delayed_ctx_close;
	register_select_table(xmlrpc_sel);
	
	/* register non-sip hooks */
	if(xmlrpc_mode==0)
	{
		memset(&nsh, 0, sizeof(nsh));
		nsh.name="xmlrpc";
		nsh.destroy=0;
		nsh.on_nonsip_req=process_xmlrpc;
		if (register_nonsip_msg_hook(&nsh)<0){
			ERR("Failed to register non sip msg hooks\n");
			return -1;
		}
	}
	if(xmlrpc_url_match!=NULL)
	{
		memset(&xmlrpc_url_match_regexp, 0, sizeof(regex_t));
		if (regcomp(&xmlrpc_url_match_regexp, xmlrpc_url_match, REG_EXTENDED)!=0) {
			LM_ERR("bad match re %s\n", xmlrpc_url_match);
			return E_BAD_RE;
		}
	}
	if(xmlrpc_url_skip!=NULL)
	{
		memset(&xmlrpc_url_skip_regexp, 0, sizeof(regex_t));
		if (regcomp(&xmlrpc_url_skip_regexp, xmlrpc_url_skip, REG_EXTENDED)!=0) {
			LM_ERR("bad skip re %s\n", xmlrpc_url_skip);
			return E_BAD_RE;
		}
	}

	return 0;
}


/**
 * advertise that sip workers handle rpc commands
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	set_child_sip_rpc_mode();
	return 0;
}


static int fixup_xmlrpc_reply(void** param, int param_no)
{
	int ret;

	if (param_no == 1) {
		ret = fix_param(FPARAM_AVP, param);
		if (ret <= 0) return ret;		
	    if (fix_param(FPARAM_INT, param) != 0) return -1;
	} else if (param_no == 2) {
	        return fixup_var_str_12(param, 2);
	}
	return 0;
}

/** @} */
