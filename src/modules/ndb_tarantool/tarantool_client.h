/*
 * Copyright (C) 2026 Andrei Lashchinskii <koorwork+kamailio@gmail.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _TARANTOOL_CLIENT_H_
#define _TARANTOOL_CLIENT_H_

#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

#define TNT_DEFAULT_HOST "127.0.0.1"
#define TNT_DEFAULT_PORT 3301
#define TNT_DEFAULT_TIMEOUT_MS 500
#define TNT_DEFAULT_POOL_SIZE 4

/**
 * @brief Single Tarantool IProto TCP connection descriptor.
 */
typedef struct tnt_conn
{
	int fd; /**< TCP socket file descriptor (with TCP_NODELAY enabled) */
	char host[128];	   /**< Remote host IPv4/IPv6 or hostname */
	int port;		   /**< Remote IProto port (default: 3301) */
	char user[64];	   /**< Authentication username (optional) */
	char password[64]; /**< Authentication password (optional) */
	int connected; /**< 1 if connection is active and greeting validated, 0 otherwise */
	uint64_t
			sync_id; /**< Monotonically increasing IProto request sync identifier */
} tnt_conn_t;

/**
 * @brief Per-process connection pool managing Tarantool connections.
 */
typedef struct tnt_pool
{
	tnt_conn_t *conns; /**< Array of active connections */
	char *host;		   /**< Target host string */
	int port;		   /**< Target port */
	char *user;		   /**< Target username */
	char *pass;		   /**< Target password */
	int size;		   /**< Number of connections in the pool */
	int cur_idx;	   /**< Round-robin index for connection acquisition */
	int timeout_ms;	   /**< Socket read/write timeout in milliseconds */
	pid_t owner_pid;   /**< PID of the process owning this pool */
} tnt_pool_t;

/**
 * @brief Initialize a connection pool and pre-establish IProto connections.
 */
int tnt_pool_init(tnt_pool_t *pool, const char *host, int port,
		const char *user, const char *pass, int size, int timeout_ms);

/**
 * @brief Destroy connection pool and close all underlying sockets.
 */
void tnt_pool_destroy(tnt_pool_t *pool);

/**
 * @brief Acquire a connection from the pool (thread/process safe).
 */
tnt_conn_t *tnt_pool_get_conn(tnt_pool_t *pool);

/**
 * @brief Release a connection back to the pool.
 */
void tnt_pool_put_conn(tnt_pool_t *pool, tnt_conn_t *conn);

/**
 * @brief Execute a stored procedure (IPROTO_CALL) on Tarantool instance.
 */
int tnt_call_procedure(tnt_conn_t *conn, const char *proc_name,
		const char *params_json, char *res_buf, size_t res_size);

/**
 * @brief Execute an arbitrary Lua expression (IPROTO_EVAL) on Tarantool instance.
 */
int tnt_eval_code(tnt_conn_t *conn, const char *lua_code,
		const char *params_json, char *res_buf, size_t res_size);

/**
 * @brief High-Performance Zero-Copy Scatter-Gather (writev) call persistence.
 * Uses a 5-vector iovec array to serialize IProto headers, metadata, and payload
 * directly without intermediate buffer allocations.
 *
 * @param conn Active connection descriptor
 * @param call_id SIP Call-ID string
 * @param cid_len Length of call_id in bytes
 * @param node_id Media node identifier (e.g., 'rtpe-node-01')
 * @param nid_len Length of node_id in bytes
 * @param state Session state ('active', 'closing', etc.)
 * @param state_len Length of state string
 * @param expires TTL duration in seconds
 * @param payload Raw SDP / session JSON or binary payload
 * @param payload_len Length of payload in bytes
 * @return 0 on success, -1 on failure
 */
int tnt_save_call_sg(tnt_conn_t *conn, const char *call_id, size_t cid_len,
		const char *node_id, size_t nid_len, const char *state,
		size_t state_len, int expires, const char *payload, size_t payload_len);

/**
 * @brief Zero-Allocation call retrieval directly into caller buffer.
 * Performs single-pass in-place MessagePack decoding without dynamic heap allocations.
 *
 * @param conn Active connection descriptor
 * @param call_id SIP Call-ID to query
 * @param cid_len Length of call_id
 * @param dst_buf Destination buffer provided by caller
 * @param dst_len Maximum capacity of destination buffer
 * @param out_len Pointer to store actual payload size copied
 * @return 0 on success, -1 on failure
 */
int tnt_get_call_buf(tnt_conn_t *conn, const char *call_id, size_t cid_len,
		char *dst_buf, size_t dst_len, size_t *out_len);

#endif /* _TARANTOOL_CLIENT_H_ */
