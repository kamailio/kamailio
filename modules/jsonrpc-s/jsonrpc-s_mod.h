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

#include "../../str.h"
#include "../../rpc_lookup.h"
#include "../../parser/msg_parser.h"
#include "../../lib/srutils/srjson.h"


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
	char* method;          /**< Name of the management function to be called */
	unsigned int flags;    /**< Various flags, such as return value type */
	srjson_doc_t *jreq;    /**< JSON request document */
	srjson_t *req_node;    /**< Pointer to crt node in json req parameter list */
	srjson_doc_t *jrpl;    /**< JSON reply document */
	srjson_t *rpl_node;    /**< Pointer to crt node in json reply doc */
	int reply_sent;        /**< Flag set if the json reply was sent */
	int error_code;        /**< Json error code */
	int http_code;         /**< http reply code */
	str http_text;         /**< http reply reason text */
} jsonrpc_ctx_t;


#endif

