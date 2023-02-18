/**
 *
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


#ifndef _JSONRPC_S_H_
#define _JSONRPC_S_H_

#include "../../core/str.h"
#include "../../core/rpc_lookup.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/utils/srjson.h"

#define JSONRPC_ID_SIZE	64

/** The context of the jsonrpc request being processed.
 *
 * This is the data structure that contains all data related to the xhttp_rpc
 * request being processed, such as the reply code and reason, data to be sent
 * to the client in the reply, and so on.
 *
 * There is always one context per jsonrpc request.
 */
typedef struct jsonrpc_ctx {
	sip_msg_t* msg;        /**< The SIP/HTTP received message. */
	int msg_shm_block_size; /**< non-zero for delayed reply contexts with
							  shm cloned msgs */
	char* method;          /**< Name of the management function to be called */
	unsigned int flags;    /**< Various flags, such as return value type */
	srjson_doc_t *jreq;    /**< JSON request document */
	srjson_t *req_node;    /**< Pointer to crt node in json req parameter list */
	srjson_doc_t *jrpl;    /**< JSON reply document */
	srjson_t *rpl_node;    /**< Pointer to crt node in json reply doc */
	int reply_sent;        /**< Flag set if the json reply was sent */
	int error_code;        /**< Json error code */
	str error_text;        /**< Json error text */
	int http_code;         /**< http reply code */
	str http_text;         /**< http reply reason text */
	int transport;         /**< RPC transport */
	int jsrid_type;        /**< type for Json RPC id value */
	char jsrid_val[JSONRPC_ID_SIZE]; /**< value for Json RPC id */
} jsonrpc_ctx_t;

/* extra rpc_ctx_t flags */
/* first 8 bits reserved for rpc flags (e.g. RET_ARRAY) */
#define JSONRPC_DELAYED_CTX_F	256
#define JSONRPC_DELAYED_REPLY_F	512

#define JSONRPC_TRANS_NONE	0
#define JSONRPC_TRANS_HTTP	1
#define JSONRPC_TRANS_FIFO	2
#define JSONRPC_TRANS_DGRAM	3

typedef struct jsonrpc_plain_reply {
	int rcode;         /**< reply code */
	str rtext;         /**< reply reason text */
	str rbody;         /**< reply body */
} jsonrpc_plain_reply_t;

jsonrpc_plain_reply_t* jsonrpc_plain_reply_get(void);

int jsonrpc_exec_ex(str *cmd, str *rpath, str *spath);
char *jsonrpcs_stored_id_get(void);

#define JSONRPC_RESPONSE_STORING_DONE "{\n\"jsonrpc\": \"2.0\",\n\t\"result\": \"Stored\",\n\t\"id\": %s\n}"
#define JSONRPC_RESPONSE_STORING_FAILED "{\n\"jsonrpc\": \"2.0\",\n\t\"error\": {\n\t\t\"code\": 500,\n\t\t\"message\": \"Storing failed\"\n\t},\n\t\"id\": %s\n}"
#define JSONRPC_RESPONSE_STORING_BUFSIZE (sizeof(JSONRPC_RESPONSE_STORING_FAILED)+64)

#endif

