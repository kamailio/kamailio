/* 
 * Kamailio Remote Procedure Call Interface
 *
 * Copyright (C) 2005 iptelorg GmbH
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
 */

/*!
* \file
* \brief Kamailio core :: RPC, Remote procedure call interface
* \ingroup core
* Module: \ref core
*/

#ifndef _RPC_H
#define _RPC_H

/*
 * TODO: Add the possibility to add printf-like formatted string to fault
 */

enum rpc_flags {
	RET_ARRAY = (1 << 0),
	RET_VALUE = (1 << 1)
};

typedef enum rpc_capabilities {
	RPC_DELAYED_REPLY = (1 <<0)  /* delayed reply support */
} rpc_capabilities_t;

struct rpc_delayed_ctx;


/* Send the result to the caller */
typedef int (*rpc_send_f)(void* ctx);                                      /*!< Send the reply to the client */
typedef void (*rpc_fault_f)(void* ctx, int code, char* fmt, ...);          /*!< Signal a failure to the client */
typedef int (*rpc_add_f)(void* ctx, char* fmt, ...);                       /*!< Add a new piece of data to the result */
typedef int (*rpc_scan_f)(void* ctx, char* fmt, ...);                      /*!< Retrieve request parameters */
typedef int (*rpc_rpl_printf_f)(void* ctx, char* fmt, ...);                /*!< Add printf-like formated data to the result set */
typedef int (*rpc_struct_add_f)(void* ctx, char* fmt, ...);                /*!< Add fields in a structure */
typedef int (*rpc_array_add_f)(void* ctx, char* fmt, ...);                 /*!< Add values in an array */
typedef int (*rpc_struct_scan_f)(void* ctx, char* fmt, ...);               /*!< Scan attributes of a structure */
typedef int (*rpc_struct_printf_f)(void* ctx, char* name, char* fmt, ...); /*!< Struct version of rpc_printf */

/* returns the supported capabilities */
typedef rpc_capabilities_t (*rpc_capabilities_f)(void* ctx);
/* create a special "context" for delayed replies */
typedef struct rpc_delayed_ctx* (*rpc_delayed_ctx_new_f)(void* ctx);
/* close the special "context" for delayed replies */
typedef void (*rpc_delayed_ctx_close_f)(struct rpc_delayed_ctx* dctx);

/*
 * RPC context, this is what RPC functions get as a parameter and use
 * it to obtain the value of the parameters of the call and reference
 * to the result structure that will be returned to the caller
 */
typedef struct rpc {
	rpc_fault_f fault;
	rpc_send_f send;
	rpc_add_f add;
	rpc_scan_f scan;
	rpc_rpl_printf_f rpl_printf;
	rpc_struct_add_f struct_add;
	rpc_array_add_f array_add;
	rpc_struct_scan_f struct_scan;
	rpc_struct_printf_f struct_printf;
	rpc_capabilities_f capabilities;
	rpc_delayed_ctx_new_f delayed_ctx_new;
	rpc_delayed_ctx_close_f delayed_ctx_close;
} rpc_t;


typedef struct rpc_delayed_ctx{
	rpc_t rpc;
	void* reply_ctx;
	/* more private data might follow */
} rpc_delayed_ctx_t;


/**
 * RPC Function Prototype
 */
typedef void (*rpc_function_t)(rpc_t* rpc, void* ctx);

/**
 * RPC callback context.
 *
 * Defines a convenient way of packing an rpc callback
 * (rpc_function_t) parameters and it's not used/needed
 * by the rpc api/interface.
 */
typedef struct rpc_cb_ctx {
	rpc_t *rpc;
	void *c;
} rpc_cb_ctx_t;


/**
 * Remote Procedure Call Export
 */
typedef struct rpc_export {
	const char* name;        /*!< Name of the RPC function (null terminated) */
	rpc_function_t function; /*!< Pointer to the function */
	const char** doc_str;  /*!< Documentation strings, method signature and description */
	unsigned int flags;      /*!< Various flags, reserved for future use */
} rpc_export_t;


#endif /* _RPC_H */
