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


#ifndef _XHTTP_RPC_H
#define _XHTTP_RPC_H

#include "../../str.h"
#include "../../rpc_lookup.h"
#include "../../parser/msg_parser.h"


#define ERROR_REASON_BUF_LEN 1024
#define PRINT_VALUE_BUF_LEN 256



struct rpc_data_struct {
	struct rpc_ctx* ctx;
	struct rpc_data_struct* next;
};


/** Representation of the xhttp_rpc reply being constructed.
 *
 * This data structure describes the xhttp_rpc reply that is being constructed
 * and will be sent to the client.
 */
struct xhttp_rpc_reply {
	int code;	/**< Reply code which indicates the type of the reply */
	str reason;	/**< Reason phrase text which provides human-readable
			 * description that augments the reply code */
	str body;	/**< The xhttp_rpc http body built so far */
	str buf;	/**< The memory buffer allocated for the reply, this is
			 * where the body attribute of the structure points to */
};


/** The context of the xhttp_rpc request being processed.
 *
 * This is the data structure that contains all data related to the xhttp_rpc
 * request being processed, such as the reply code and reason, data to be sent
 * to the client in the reply, and so on.
 *
 * There is always one context per xhttp_rpc request.
 */
typedef struct rpc_ctx {
	sip_msg_t* msg;			/**< The SIP/HTTP received message. */
	struct xhttp_rpc_reply reply;	/**< xhttp_rpc reply to be sent to the client */
	int reply_sent;
	int mod;			/**< Module being processed */
	int cmd;			/**< RPC command being processed */
	int arg_received;		/**< RPC argument flag */
	str arg;			/**< RPC command argument */
	str arg2scan;			/**< RPC command args to be parsed */
	struct rpc_struct *structs;
	struct rpc_data_struct *data_structs;
	unsigned int struc_depth;
} rpc_ctx_t;


/* An RPC module representation.
 *
 * The module is the first substring of the RPC commands (delimited by '.'.
 */
typedef struct xhttp_rpc_mod_cmds_ {
	int rpc_e_index;	/**< Index to the first module RPC rec in rpc_sarray */
	str mod;		/**< Module name */
	int size;		/**< Number of commands provided by the above module */
} xhttp_rpc_mod_cmds_t;

#endif

