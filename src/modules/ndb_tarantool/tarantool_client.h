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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef NDB_TARANTOOL_CLIENT_H
#define NDB_TARANTOOL_CLIENT_H

#include "../../core/sr_module.h"
#include "../../core/str.h"
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define TNT_DEFAULT_HOST "127.0.0.1"
#define TNT_DEFAULT_PORT 3301
#define TNT_DEFAULT_CONNECT_TIMEOUT 1000
#define TNT_DEFAULT_CMD_TIMEOUT 1000
#define TNT_DEFAULT_DISABLE_TIME 10
#define TNT_DEFAULT_ALLOWED_TIMEOUTS 3

/**
 * @brief Tarantool Server instance descriptor
 */
typedef struct tnt_server {
  str sname;              /**< Server identifier/alias */
  str addr;               /**< Hostname or IP address */
  int port;               /**< TCP port (default 3301) */
  str user;               /**< Authentication username */
  str pass;               /**< Authentication password */
  int connect_timeout;    /**< Connect timeout in ms */
  int cmd_timeout;        /**< Command read/write timeout in ms */
  int disable_time;       /**< Failover cooldown in seconds */
  int allowed_timeouts;   /**< Allowed consecutive errors before cooldown */
  int disabled;           /**< 1 if currently disabled/in cooldown */
  int consecutive_errors; /**< Consecutive timeout/error count */
  time_t restore_tick;    /**< Unix timestamp when server can be retried */

  /* Worker process private runtime fields (initialized in child_init) */
  int fd;           /**< Active TCP socket file descriptor */
  int connected;    /**< 1 if connection is authenticated and ready */
  uint64_t sync_id; /**< Monotonic request sync counter */

  struct tnt_server *next;
} tnt_server_t;

/**
 * @brief Add a server definition parsed from modparam string
 * @param spec Server specification string:
 * "name=srv1;addr=127.0.0.1;port=3301;user=u;pass=p"
 * @return 0 on success, -1 on failure
 */
int tnt_add_server(const char *srv_spec);

/**
 * @brief Initialize Tarantool client connections for a Kamailio worker process
 * @param rank Process rank (PROC_SIPINIT, worker rank, etc.)
 * @return 0 on success, -1 on failure
 */
int tnt_child_init(int rank);

/**
 * @brief Close worker sockets on child process termination
 */
void tnt_child_destroy(void);

/**
 * @brief Free all shared server configurations on module shutdown
 */
void tnt_destroy_all(void);

/**
 * @brief Look up a server by name, or return default first server if name is
 * NULL/empty
 */
tnt_server_t *tnt_get_server(const str *name);

/**
 * @brief Execute a stored procedure (IPROTO_CALL) on Tarantool instance
 * @param srv Target server descriptor (or NULL for default)
 * @param proc_name Stored procedure name
 * @param params_json JSON or string parameters array
 * @param res_dst Destination str buffer (allocated in pkg memory on success)
 * @return 1 on success, -1 on failure
 */
int tnt_exec_call(tnt_server_t *srv, const str *proc_name,
		const str *params_json, str *res_dst);

/**
 * @brief Execute expression (IPROTO_EVAL) on Tarantool instance
 * @param srv Target server descriptor (or NULL for default)
 * @param expr Expression to evaluate
 * @param params_json JSON or string parameters array
 * @param res_dst Destination str buffer (allocated in pkg memory on success)
 * @return 1 on success, -1 on failure
 */
int tnt_exec_eval(tnt_server_t *srv, const str *expr,
		const str *params_json, str *res_dst);

/**
 * @brief High-performance Zero-Copy Scatter-Gather call state save (writev)
 */
int tnt_save_call_sg(tnt_server_t *srv, const char *call_id, size_t cid_len,
		const char *node_id, size_t nid_len, const char *state,
		size_t state_len, int expires, const char *payload,
		size_t payload_len);

/**
 * @brief Zero-Allocation call retrieval directly into caller-provided buffer
 */
int tnt_get_call_buf(tnt_server_t *srv, const char *call_id, size_t cid_len,
		char *dst_buf, size_t dst_len, size_t *out_len);

#endif /* NDB_TARANTOOL_CLIENT_H */
