/**
 * Copyright (C) 2015 Bicom Systems Ltd, (bicomsystems.com)
 *
 * Author: Seudin Kasumovic (seudin.kasumovic@gmail.com)
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

#include "../../rpc.h"
#include "erl_helpers.h"
#include "cnode.h"

#ifndef HANDLE_RPC_H_
#define HANDLE_RPC_H_

#define FAULT_BUF_LEN 1024

/*
 * RPC structure to store reply before marshaling.
 */
typedef struct erl_rpc_param_s {
	int type;
	union {
		int n;
		double d;
		str S;
		void* handler;
	} value;
	char *member_name;
	struct erl_rpc_param_s *next;
} erl_rpc_param_t;

typedef struct erl_rpc_ctx {
	cnode_handler_t *phandler;
	erlang_ref_ex_t *ref;
	erlang_pid *pid;
	ei_x_buff *request;
	int request_index;
	ei_x_buff *response;
	int response_sent;
	int response_index;
	erl_rpc_param_t *reply_params; /* encoded into reply as {ok,[<reply_params>]} */
	erl_rpc_param_t *tail;
	erl_rpc_param_t *fault; /* is set has precedence on reply_params
	 	 	 	 	 	 	   encoded as {error, {code, <fault message>}} */
	erl_rpc_param_t **fault_p;
	int no_params; /* number of encoding params */
	int optional; /* are params optional */
	int size; /* size of decoding structure or main list */
} erl_rpc_ctx_t;

int erl_rpc_send(erl_rpc_ctx_t* ctx, int depth);					/* Send the reply to the Erlang Node */
void erl_rpc_fault(erl_rpc_ctx_t* ctx, int code, char* fmt, ...);	/* Signal a failure to the client */
int erl_rpc_add(erl_rpc_ctx_t* ctx, char* fmt, ...);				/* Add a new piece of data to the result */
int erl_rpc_scan(erl_rpc_ctx_t* ctx, char* fmt, ...);				/* Retrieve request parameters */
int erl_rpc_printf(erl_rpc_ctx_t* ctx, char* fmt, ...);				/* Add printf-like formated data to the result set */
int erl_rpc_struct_add(erl_rpc_ctx_t* ctx, char* fmt, ...);			/* Add a new structure into structure */
int erl_rpc_array_add(erl_rpc_ctx_t* ctx, char* fmt, ...);			/* Add a new structure into array */
int erl_rpc_struct_scan(erl_rpc_ctx_t* ctx, char* fmt, ...);		/* Scan attributes of a structure */
int erl_rpc_struct_printf(erl_rpc_ctx_t* ctx, char* name, char* fmt, ...); /* Struct version of rpc_printf */
int erl_rpc_capabilities(erl_rpc_ctx_t* ctx);	/* capabilities */

void init_rpc_handlers();
void empty_recycle_bin(void);

/**
 * Garbage collection data structure.
 */
struct erl_rpc_garbage {
        enum {
                JUNK_EI_X_BUFF,   /**< This type indicates that the memory block was
								   * allocated for the erlang EI interface, this
								   * type needs to be freed differently
								   */
                JUNK_PKGCHAR      /** This type indicates a pkg_malloc'ed string */
        } type;               /**< Type of the memory block */
        void* ptr;            /**< Pointer to the memory block obtained from pkg_malloc */
        struct erl_rpc_garbage* next; /**< The linked list of all allocated memory blocks */
};


/** Pointers to the functions that implement the RPC interface
 * of Erlang module
 */
extern rpc_t erl_rpc_func_param;

#endif /* HANDLE_RPC_H_ */
