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

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#include <msgpack.h>

#include "../../core/basex.h"
#include "../../core/crypto/shautils.h"
#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/trim.h"
#include "../../core/ut.h"

#include "tarantool_client.h"

/* IProto Protocol Constants (Tarantool 1.10+, 2.x, 3.x) */
#define TNT_IPROTO_OK 0x00
#define TNT_IPROTO_SELECT 0x01
#define TNT_IPROTO_INSERT 0x02
#define TNT_IPROTO_REPLACE 0x03
#define TNT_IPROTO_UPDATE 0x04
#define TNT_IPROTO_DELETE 0x05
#define TNT_IPROTO_AUTH 0x07
#define TNT_IPROTO_EVAL 0x08
#define TNT_IPROTO_UPSERT 0x09
#define TNT_IPROTO_CALL 0x0a

/* IProto Map Keys */
#define TNT_IPROTO_REQUEST_TYPE 0x00
#define TNT_IPROTO_SYNC 0x01
#define TNT_IPROTO_SPACE_ID 0x10
#define TNT_IPROTO_INDEX_ID 0x11
#define TNT_IPROTO_LIMIT 0x12
#define TNT_IPROTO_OFFSET 0x13
#define TNT_IPROTO_ITERATOR 0x14
#define TNT_IPROTO_KEY 0x20
#define TNT_IPROTO_TUPLE 0x21
#define TNT_IPROTO_FUNCTION_NAME 0x22
#define TNT_IPROTO_USER_NAME 0x23
#define TNT_IPROTO_EXPR 0x27
#define TNT_IPROTO_DATA 0x30
#define TNT_IPROTO_ERROR_24 0x31
#define TNT_IPROTO_ERROR 0x52

#define TNT_GREETING_SIZE 128
#define TNT_SHA1_DIGEST_SIZE 20

/* Global server configurations list in SHM memory */
static tnt_server_t *tnt_srv_list = NULL;

extern int init_without_tarantool;
extern int tnt_connect_timeout_param;
extern int tnt_cmd_timeout_param;
extern int tnt_disable_time_param;
extern int tnt_allowed_timeouts_param;

/**
 * tnt_strdup_pkg - Allocate and copy string into PKG memory
 * @src: source string
 * @dst: destination str struct
 *
 * Returns 0 on success, -1 on failure.
 */
static int tnt_strdup_pkg(const char *src, str *dst)
{
	int len;

	if(!src || !dst)
		return -1;

	len = (int)strlen(src);
	dst->s = (char *)pkg_malloc(len + 1);
	if(!dst->s) {
		LM_ERR("pkg_malloc failed for string: %s\n", src);
		dst->len = 0;
		return -1;
	}
	memcpy(dst->s, src, len + 1);
	dst->len = len;
	return 0;
}

/**
 * tnt_add_server - Parse server specification string and append to list
 * @srv_spec: parameter string e.g. "name=srv1;addr=127.0.0.1;port=3301;user=rtpe;pass=secret"
 *
 * Returns 0 on success, -1 on error.
 */
int tnt_add_server(const char *srv_spec)
{
	tnt_server_t *srv = NULL;
	char *spec_copy = NULL;
	char *token = NULL;
	char *saveptr = NULL;
	int spec_len;

	if(!srv_spec) {
		LM_ERR("null server specification\n");
		return -1;
	}

	spec_len = (int)strlen(srv_spec);
	spec_copy = (char *)pkg_malloc(spec_len + 1);
	if(!spec_copy) {
		LM_ERR("pkg_malloc failed for server spec parser\n");
		return -1;
	}
	memcpy(spec_copy, srv_spec, spec_len + 1);

	srv = (tnt_server_t *)pkg_malloc(sizeof(tnt_server_t));
	if(!srv) {
		LM_ERR("pkg_malloc failed for tnt_server_t\n");
		pkg_free(spec_copy);
		return -1;
	}
	memset(srv, 0, sizeof(tnt_server_t));

	/* Apply module defaults */
	srv->port = TNT_DEFAULT_PORT;
	srv->connect_timeout = tnt_connect_timeout_param;
	srv->cmd_timeout = tnt_cmd_timeout_param;
	srv->disable_time = tnt_disable_time_param;
	srv->allowed_timeouts = tnt_allowed_timeouts_param;
	srv->fd = -1;

	/* Default alias */
	tnt_strdup_pkg("default", &srv->sname);
	tnt_strdup_pkg(TNT_DEFAULT_HOST, &srv->addr);

	token = strtok_r(spec_copy, ";", &saveptr);
	while(token) {
		char *eq = strchr(token, '=');
		if(eq) {
			*eq = '\0';
			const char *key = token;
			const char *val = eq + 1;

			/* Trim whitespace */
			while(*key == ' ' || *key == '\t')
				key++;
			while(*val == ' ' || *val == '\t')
				val++;

			if(strcmp(key, "name") == 0 || strcmp(key, "srv") == 0) {
				if(srv->sname.s)
					pkg_free(srv->sname.s);
				tnt_strdup_pkg(val, &srv->sname);
			} else if(strcmp(key, "addr") == 0 || strcmp(key, "host") == 0) {
				if(srv->addr.s)
					pkg_free(srv->addr.s);
				tnt_strdup_pkg(val, &srv->addr);
			} else if(strcmp(key, "port") == 0) {
				srv->port = (int)strtol(val, NULL, 10);
			} else if(strcmp(key, "user") == 0) {
				if(srv->user.s)
					pkg_free(srv->user.s);
				tnt_strdup_pkg(val, &srv->user);
			} else if(strcmp(key, "pass") == 0
					  || strcmp(key, "password") == 0) {
				if(srv->pass.s)
					pkg_free(srv->pass.s);
				tnt_strdup_pkg(val, &srv->pass);
			} else if(strcmp(key, "connect_timeout") == 0) {
				srv->connect_timeout = (int)strtol(val, NULL, 10);
			} else if(strcmp(key, "cmd_timeout") == 0) {
				srv->cmd_timeout = (int)strtol(val, NULL, 10);
			} else if(strcmp(key, "disable_time") == 0) {
				srv->disable_time = (int)strtol(val, NULL, 10);
			} else if(strcmp(key, "allowed_timeouts") == 0) {
				srv->allowed_timeouts = (int)strtol(val, NULL, 10);
			}
		}
		token = strtok_r(NULL, ";", &saveptr);
	}
	pkg_free(spec_copy);

	if(srv->port <= 0 || srv->port > 65535) {
		LM_ERR("invalid tarantool port: %d\n", srv->port);
		if(srv->sname.s)
			pkg_free(srv->sname.s);
		if(srv->addr.s)
			pkg_free(srv->addr.s);
		if(srv->user.s)
			pkg_free(srv->user.s);
		if(srv->pass.s)
			pkg_free(srv->pass.s);
		pkg_free(srv);
		return -1;
	}

	/* Append to linked list */
	srv->next = tnt_srv_list;
	tnt_srv_list = srv;

	LM_INFO("registered tarantool server [%.*s] -> %.*s:%d\n", srv->sname.len,
			srv->sname.s, srv->addr.len, srv->addr.s, srv->port);
	return 0;
}

/**
 * tnt_get_server - Find server by name or return default
 * @name: optional server alias string
 */
tnt_server_t *tnt_get_server(const str *name)
{
	tnt_server_t *it = NULL;
	if(!tnt_srv_list)
		return NULL;
	if(!name || !name->s || name->len == 0)
		return tnt_srv_list;

	for(it = tnt_srv_list; it; it = it->next) {
		if(it->sname.len == name->len
				&& strncmp(it->sname.s, name->s, name->len) == 0) {
			return it;
		}
	}
	return NULL;
}

/**
 * tnt_socket_send_all - Reliable full buffer send loop
 */
static int tnt_socket_send_all(int fd, const char *buf, size_t len)
{
	size_t off = 0;

	while(off < len) {
		ssize_t n = send(fd, buf + off, len - off, 0);
		if(n < 0) {
			if(errno == EINTR)
				continue;
			return -1;
		}
		if(n == 0)
			return -1;
		off += (size_t)n;
	}
	return 0;
}

/**
 * tnt_socket_recv_all - Reliable full buffer receive loop
 */
static int tnt_socket_recv_all(int fd, char *buf, size_t len)
{
	size_t off = 0;

	while(off < len) {
		ssize_t n = recv(fd, buf + off, len - off, 0);
		if(n < 0) {
			if(errno == EINTR)
				continue;
			return -1;
		}
		if(n == 0)
			return -1;
		off += (size_t)n;
	}
	return 0;
}

/**
 * tnt_auth_scramble - Execute SHA-1 scramble authentication against Tarantool
 * greeting salt
 */
static int tnt_auth_scramble(
		tnt_server_t *srv, const char *salt_b64, int salt_b64_len)
{
	unsigned char raw_salt[64];
	int raw_salt_len = 0;
	unsigned char h1[TNT_SHA1_DIGEST_SIZE];
	unsigned char h2[TNT_SHA1_DIGEST_SIZE];
	unsigned char step3_in[TNT_SHA1_DIGEST_SIZE * 2];
	unsigned char h3[TNT_SHA1_DIGEST_SIZE];
	unsigned char scramble[TNT_SHA1_DIGEST_SIZE];
	uint32_t i;
	int j;
	uint64_t sync_id;
	msgpack_sbuffer sbuf;
	msgpack_packer pk;
	uint32_t body_len;
	char len_hdr[5];
	uint32_t net_len;
	char resp_hdr[5];
	uint32_t resp_len;
	char *resp_body = NULL;
	msgpack_unpacked msg;
	size_t off = 0;
	int auth_ok = 0;
	int rc = -1;

	if(!srv->user.s || srv->user.len == 0) {
		return 0; /* No authentication configured */
	}

	/* 1. Base64 decode salt */
	raw_salt_len = base64_dec((unsigned char *)salt_b64, salt_b64_len, raw_salt,
			(int)sizeof(raw_salt));
	if(raw_salt_len < TNT_SHA1_DIGEST_SIZE) {
		LM_ERR("failed to decode tarantool greeting salt (len=%d)\n",
				raw_salt_len);
		return -1;
	}

	/* 2. Compute SHA-1 scramble:
   * h1 = SHA1(password)
   * h2 = SHA1(h1)
   * h3 = SHA1(salt + h2)
   * scramble = h1 XOR h3
   */
	compute_sha1_raw(
			h1, (u_int8_t *)(srv->pass.s ? srv->pass.s : ""), srv->pass.len);
	compute_sha1_raw(h2, (u_int8_t *)h1, TNT_SHA1_DIGEST_SIZE);

	memcpy(step3_in, raw_salt, TNT_SHA1_DIGEST_SIZE);
	memcpy(step3_in + TNT_SHA1_DIGEST_SIZE, h2, TNT_SHA1_DIGEST_SIZE);
	compute_sha1_raw(h3, (u_int8_t *)step3_in, TNT_SHA1_DIGEST_SIZE * 2);

	for(j = 0; j < TNT_SHA1_DIGEST_SIZE; j++) {
		scramble[j] = h1[j] ^ h3[j];
	}

	/* 3. Build IPROTO_AUTH packet using msgpack */
	sync_id = ++srv->sync_id;
	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	/* Header Map(2): { 0x00: IPROTO_AUTH, 0x01: sync_id } */
	msgpack_pack_map(&pk, 2);
	msgpack_pack_uint8(&pk, TNT_IPROTO_REQUEST_TYPE);
	msgpack_pack_uint8(&pk, TNT_IPROTO_AUTH);
	msgpack_pack_uint8(&pk, TNT_IPROTO_SYNC);
	msgpack_pack_uint64(&pk, sync_id);

	/* Body Map(2): { 0x23 (USER): username, 0x21 (TUPLE): ["chap-sha1", bin(20,
   * scramble)] } */
	msgpack_pack_map(&pk, 2);
	msgpack_pack_uint8(&pk, TNT_IPROTO_USER_NAME);
	msgpack_pack_str(&pk, srv->user.len);
	msgpack_pack_str_body(&pk, srv->user.s, srv->user.len);

	msgpack_pack_uint8(&pk, TNT_IPROTO_TUPLE);
	msgpack_pack_array(&pk, 2);
	msgpack_pack_str(&pk, 9);
	msgpack_pack_str_body(&pk, "chap-sha1", 9);
	msgpack_pack_bin(&pk, TNT_SHA1_DIGEST_SIZE);
	msgpack_pack_bin_body(&pk, scramble, TNT_SHA1_DIGEST_SIZE);

	/* Send 5-byte IProto length prefix + msgpack payload */
	body_len = (uint32_t)sbuf.size;
	len_hdr[0] = (char)0xce;
	net_len = htonl(body_len);
	memcpy(len_hdr + 1, &net_len, 4);

	if(tnt_socket_send_all(srv->fd, len_hdr, 5) < 0
			|| tnt_socket_send_all(srv->fd, sbuf.data, body_len) < 0) {
		LM_ERR("failed to send IPROTO_AUTH to %.*s:%d: %s\n", srv->addr.len,
				srv->addr.s, srv->port, strerror(errno));
		msgpack_sbuffer_destroy(&sbuf);
		return -1;
	}
	msgpack_sbuffer_destroy(&sbuf);

	/* 4. Receive AUTH response */
	if(tnt_socket_recv_all(srv->fd, resp_hdr, 5) < 0
			|| (uint8_t)resp_hdr[0] != 0xce) {
		LM_ERR("failed to receive IPROTO_AUTH response header from %.*s:%d: "
			   "%s\n",
				srv->addr.len, srv->addr.s, srv->port, strerror(errno));
		return -1;
	}
	memcpy(&resp_len, resp_hdr + 1, 4);
	resp_len = ntohl(resp_len);
	if(resp_len == 0 || resp_len > 65536) {
		LM_ERR("invalid IPROTO_AUTH response length %u from %.*s:%d\n",
				resp_len, srv->addr.len, srv->addr.s, srv->port);
		return -1;
	}

	resp_body = (char *)pkg_malloc(resp_len);
	if(!resp_body) {
		LM_ERR("pkg_malloc failed for auth response (%u bytes)\n", resp_len);
		return -1;
	}

	if(tnt_socket_recv_all(srv->fd, resp_body, resp_len) < 0) {
		LM_ERR("failed to receive IPROTO_AUTH response body from %.*s:%d: %s\n",
				srv->addr.len, srv->addr.s, srv->port, strerror(errno));
		goto out_free;
	}

	/* Validate IProto status code */
	msgpack_unpacked_init(&msg);
	off = 0;
	if(msgpack_unpack_next(&msg, resp_body, resp_len, &off)
			== MSGPACK_UNPACK_SUCCESS) {
		if(msg.data.type == MSGPACK_OBJECT_MAP) {
			for(i = 0; i < msg.data.via.map.size; i++) {
				if(msg.data.via.map.ptr[i].key.type
								== MSGPACK_OBJECT_POSITIVE_INTEGER
						&& msg.data.via.map.ptr[i].key.via.u64
								   == TNT_IPROTO_REQUEST_TYPE) {
					if(msg.data.via.map.ptr[i].val.type
									== MSGPACK_OBJECT_POSITIVE_INTEGER
							&& msg.data.via.map.ptr[i].val.via.u64
									   == TNT_IPROTO_OK) {
						auth_ok = 1;
					}
				}
			}
		}
	}
	msgpack_unpacked_destroy(&msg);

	if(!auth_ok) {
		LM_ERR("authentication failed for user '%.*s' on %.*s:%d\n",
				srv->user.len, srv->user.s, srv->addr.len, srv->addr.s,
				srv->port);
		goto out_free;
	}

	LM_INFO("authenticated successfully as '%.*s' on %.*s:%d\n", srv->user.len,
			srv->user.s, srv->addr.len, srv->addr.s, srv->port);
	rc = 0;

out_free:
	pkg_free(resp_body);
	return rc;
}

/**
 * tnt_conn_close - Close socket and reset connection state
 */
static void tnt_conn_close(tnt_server_t *srv)
{
	if(!srv)
		return;
	if(srv->fd >= 0) {
		close(srv->fd);
		srv->fd = -1;
	}
	srv->connected = 0;
}

/**
 * tnt_conn_fail - Register error, close socket and trigger cooldown if
 * threshold reached
 */
static void tnt_conn_fail(tnt_server_t *srv)
{
	if(!srv)
		return;
	tnt_conn_close(srv);
	srv->consecutive_errors++;
	if(srv->consecutive_errors >= srv->allowed_timeouts) {
		srv->disabled = 1;
		srv->restore_tick = time(NULL) + srv->disable_time;
		LM_WARN("tarantool server %.*s:%d marked disabled for %d seconds\n",
				srv->addr.len, srv->addr.s, srv->port, srv->disable_time);
	}
}

/**
 * tnt_conn_connect - Connect and perform greeting + authentication handshake
 */
static int tnt_conn_connect(tnt_server_t *srv)
{
	struct sockaddr_in serv_addr;
	struct timeval tv;
	int flag = 1;
	int buf_size = 1024 * 1024;
	char greeting[TNT_GREETING_SIZE];

	if(!srv)
		return -1;
	tnt_conn_close(srv);

	/* Check if server is in disable cooldown */
	if(srv->disabled) {
		if(time(NULL) < srv->restore_tick) {
			LM_DBG("tarantool server %.*s:%d is temporarily disabled\n",
					srv->addr.len, srv->addr.s, srv->port);
			return -1;
		}
		LM_INFO("tarantool server %.*s:%d cooldown expired, retrying "
				"connection...\n",
				srv->addr.len, srv->addr.s, srv->port);
		srv->disabled = 0;
		srv->consecutive_errors = 0;
	}

	srv->fd = socket(AF_INET, SOCK_STREAM, 0);
	if(srv->fd < 0) {
		LM_ERR("socket() failed: %s\n", strerror(errno));
		return -1;
	}

	/* Socket options: timeouts, nodelay, buffers */
	tv.tv_sec = srv->connect_timeout / 1000;
	tv.tv_usec = (suseconds_t)(srv->connect_timeout % 1000) * 1000;
	setsockopt(srv->fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
	setsockopt(srv->fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
	setsockopt(srv->fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
	setsockopt(srv->fd, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size));
	setsockopt(srv->fd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(srv->port);
	if(inet_pton(AF_INET, srv->addr.s, &serv_addr.sin_addr) <= 0) {
		LM_ERR("invalid IPv4 address: %.*s\n", srv->addr.len, srv->addr.s);
		tnt_conn_close(srv);
		return -1;
	}

	if(connect(srv->fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		LM_ERR("connect() to %.*s:%d failed: %s\n", srv->addr.len, srv->addr.s,
				srv->port, strerror(errno));
		tnt_conn_fail(srv);
		return -1;
	}

	/* Read 128-byte Greeting Handshake */
	if(tnt_socket_recv_all(srv->fd, greeting, sizeof(greeting)) < 0) {
		LM_ERR("failed to read greeting from %.*s:%d: %s\n", srv->addr.len,
				srv->addr.s, srv->port, strerror(errno));
		tnt_conn_fail(srv);
		return -1;
	}

	/* Apply command timeout for subsequent transactions */
	tv.tv_sec = srv->cmd_timeout / 1000;
	tv.tv_usec = (suseconds_t)(srv->cmd_timeout % 1000) * 1000;
	setsockopt(srv->fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
	setsockopt(srv->fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));

	/* Authenticate if user is configured (greeting salt is at bytes 64..107) */
	if(srv->user.s && srv->user.len > 0) {
		if(tnt_auth_scramble(srv, greeting + 64, 44) < 0) {
			tnt_conn_fail(srv);
			return -1;
		}
	}

	srv->connected = 1;
	srv->consecutive_errors = 0;
	LM_DBG("connected successfully to Tarantool at %.*s:%d\n", srv->addr.len,
			srv->addr.s, srv->port);
	return 0;
}

/**
 * tnt_child_init - Initialize Tarantool client connections for worker process
 */
int tnt_child_init(int rank)
{
	tnt_server_t *srv = NULL;

	init_basex();

	if(!tnt_srv_list) {
		if(init_without_tarantool) {
			LM_INFO("no tarantool servers configured, continuing "
					"(init_without_tarantool enabled)\n");
			return 0;
		}
		LM_ERR("no tarantool servers configured\n");
		return -1;
	}

	/* Establish socket connection for this worker process */
	for(srv = tnt_srv_list; srv; srv = srv->next) {
		srv->fd = -1;
		srv->connected = 0;
		srv->sync_id = (uint64_t)rank * 1000000;
		if(tnt_conn_connect(srv) < 0) {
			if(init_without_tarantool) {
				LM_WARN("failed to connect to Tarantool %.*s:%d on child init "
						"(init_without_tarantool is on)\n",
						srv->addr.len, srv->addr.s, srv->port);
				continue;
			}
			LM_ERR("failed to connect to Tarantool %.*s:%d on child init\n",
					srv->addr.len, srv->addr.s, srv->port);
			return -1;
		}
	}
	return 0;
}

/**
 * tnt_child_destroy - Clean up process socket connections
 */
void tnt_child_destroy(void)
{
	tnt_server_t *srv = NULL;
	for(srv = tnt_srv_list; srv; srv = srv->next) {
		tnt_conn_close(srv);
	}
}

/**
 * tnt_destroy_all - Destroy all server configurations and free PKG memory
 */
void tnt_destroy_all(void)
{
	tnt_server_t *srv = tnt_srv_list;
	tnt_server_t *next = NULL;

	while(srv) {
		next = srv->next;
		tnt_conn_close(srv);
		if(srv->sname.s)
			pkg_free(srv->sname.s);
		if(srv->addr.s)
			pkg_free(srv->addr.s);
		if(srv->user.s)
			pkg_free(srv->user.s);
		if(srv->pass.s)
			pkg_free(srv->pass.s);
		pkg_free(srv);
		srv = next;
	}
	tnt_srv_list = NULL;
}

/**
 * tnt_mp_to_json_str - Convert MessagePack Object to JSON string in pkg memory
 */
static int tnt_mp_to_json_str(const msgpack_object *obj, str *dst)
{
	char buf[8192];
	int len;

	if(!obj || !dst)
		return -1;

	len = msgpack_object_print_buffer(buf, sizeof(buf) - 1, *obj);
	if(len > 0) {
		buf[len] = '\0';
		dst->s = (char *)pkg_malloc(len + 1);
		if(!dst->s)
			return -1;
		memcpy(dst->s, buf, len + 1);
		dst->len = len;
	} else {
		dst->s = (char *)pkg_malloc(3);
		if(dst->s) {
			memcpy(dst->s, "[]", 3);
			dst->len = 2;
		}
	}
	return 0;
}

/**
 * tnt_exec_call - Execute IPROTO_CALL on Tarantool instance
 */
int tnt_exec_call(tnt_server_t *srv, const str *proc_name,
		const str *params_json, str *res_dst)
{
	uint64_t sync_id;
	msgpack_sbuffer sbuf;
	msgpack_packer pk;
	uint32_t body_len;
	char len_hdr[5];
	uint32_t net_len;
	char resp_hdr[5];
	uint32_t resp_len;
	char *resp_body = NULL;
	msgpack_unpacked msg;
	size_t off = 0;
	uint64_t resp_type = 0xff;
	uint32_t i;
	const msgpack_object *data_obj = NULL;
	int rc = -1;

	if(!srv) {
		srv = tnt_srv_list;
		if(!srv) {
			LM_ERR("no tarantool servers configured\n");
			return -1;
		}
	}

	if(!srv->connected || srv->fd < 0) {
		if(tnt_conn_connect(srv) < 0) {
			LM_ERR("cannot execute call: connection to %.*s:%d is down\n",
					srv->addr.len, srv->addr.s, srv->port);
			return -1;
		}
	}

	sync_id = ++srv->sync_id;
	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	/* Header Map(2): { 0x00: IPROTO_CALL, 0x01: sync_id } */
	msgpack_pack_map(&pk, 2);
	msgpack_pack_uint8(&pk, TNT_IPROTO_REQUEST_TYPE);
	msgpack_pack_uint8(&pk, TNT_IPROTO_CALL);
	msgpack_pack_uint8(&pk, TNT_IPROTO_SYNC);
	msgpack_pack_uint64(&pk, sync_id);

	/* Body Map(2): { 0x22: proc_name, 0x21: args [] } */
	msgpack_pack_map(&pk, 2);
	msgpack_pack_uint8(&pk, TNT_IPROTO_FUNCTION_NAME);
	msgpack_pack_str(&pk, proc_name->len);
	msgpack_pack_str_body(&pk, proc_name->s, proc_name->len);

	msgpack_pack_uint8(&pk, TNT_IPROTO_TUPLE);
	if(params_json && params_json->len > 0 && params_json->s[0] != '[') {
		msgpack_pack_array(&pk, 1);
		msgpack_pack_str(&pk, params_json->len);
		msgpack_pack_str_body(&pk, params_json->s, params_json->len);
	} else {
		msgpack_pack_array(&pk, 0);
	}

	body_len = (uint32_t)sbuf.size;
	len_hdr[0] = (char)0xce;
	net_len = htonl(body_len);
	memcpy(len_hdr + 1, &net_len, 4);

	if(tnt_socket_send_all(srv->fd, len_hdr, 5) < 0
			|| tnt_socket_send_all(srv->fd, sbuf.data, body_len) < 0) {
		LM_ERR("failed to send IPROTO_CALL (proc=%.*s) to %.*s:%d: %s\n",
				proc_name->len, proc_name->s, srv->addr.len, srv->addr.s,
				srv->port, strerror(errno));
		msgpack_sbuffer_destroy(&sbuf);
		tnt_conn_fail(srv);
		return -1;
	}
	msgpack_sbuffer_destroy(&sbuf);

	/* Read response header */
	if(tnt_socket_recv_all(srv->fd, resp_hdr, 5) < 0
			|| (uint8_t)resp_hdr[0] != 0xce) {
		LM_ERR("failed to receive IPROTO_CALL response header from %.*s:%d: "
			   "%s\n",
				srv->addr.len, srv->addr.s, srv->port, strerror(errno));
		tnt_conn_fail(srv);
		return -1;
	}

	memcpy(&resp_len, resp_hdr + 1, 4);
	resp_len = ntohl(resp_len);
	if(resp_len > 1048576) {
		LM_ERR("IPROTO_CALL response too large (%u bytes) from %.*s:%d\n",
				resp_len, srv->addr.len, srv->addr.s, srv->port);
		tnt_conn_fail(srv);
		return -1;
	}

	resp_body = (char *)pkg_malloc(resp_len);
	if(!resp_body) {
		LM_ERR("pkg_malloc failed for IPROTO_CALL response body (%u bytes)\n",
				resp_len);
		return -1;
	}

	if(tnt_socket_recv_all(srv->fd, resp_body, resp_len) < 0) {
		LM_ERR("failed to receive IPROTO_CALL response body from %.*s:%d: %s\n",
				srv->addr.len, srv->addr.s, srv->port, strerror(errno));
		tnt_conn_fail(srv);
		goto out_free;
	}

	/* Unpack header map and body map */
	msgpack_unpacked_init(&msg);
	off = 0;

	/* 1. Header */
	if(msgpack_unpack_next(&msg, resp_body, resp_len, &off)
			== MSGPACK_UNPACK_SUCCESS) {
		if(msg.data.type == MSGPACK_OBJECT_MAP) {
			for(i = 0; i < msg.data.via.map.size; i++) {
				if(msg.data.via.map.ptr[i].key.type
								== MSGPACK_OBJECT_POSITIVE_INTEGER
						&& msg.data.via.map.ptr[i].key.via.u64
								   == TNT_IPROTO_REQUEST_TYPE) {
					resp_type = msg.data.via.map.ptr[i].val.via.u64;
				}
			}
		}
	}

	if(resp_type != TNT_IPROTO_OK) {
		LM_ERR("Tarantool call '%.*s' returned error 0x%lx\n", proc_name->len,
				proc_name->s, (unsigned long)resp_type);
		goto out_unpack;
	}

	/* 2. Body */
	if(msgpack_unpack_next(&msg, resp_body, resp_len, &off)
			== MSGPACK_UNPACK_SUCCESS) {
		if(msg.data.type == MSGPACK_OBJECT_MAP) {
			for(i = 0; i < msg.data.via.map.size; i++) {
				if(msg.data.via.map.ptr[i].key.type
								== MSGPACK_OBJECT_POSITIVE_INTEGER
						&& msg.data.via.map.ptr[i].key.via.u64
								   == TNT_IPROTO_DATA) {
					data_obj = &msg.data.via.map.ptr[i].val;
				}
			}
		}
	}

	if(res_dst && data_obj) {
		tnt_mp_to_json_str(data_obj, res_dst);
	}

	srv->consecutive_errors = 0;
	rc = 1;

out_unpack:
	msgpack_unpacked_destroy(&msg);
out_free:
	pkg_free(resp_body);
	return rc;
}

/**
 * tnt_exec_eval - Execute IPROTO_EVAL on Tarantool instance
 */
int tnt_exec_eval(tnt_server_t *srv, const str *expr,
		const str *params_json, str *res_dst)
{
	uint64_t sync_id;
	msgpack_sbuffer sbuf;
	msgpack_packer pk;
	uint32_t body_len;
	char len_hdr[5];
	uint32_t net_len;
	char resp_hdr[5];
	uint32_t resp_len;
	char *resp_body = NULL;
	msgpack_unpacked msg;
	size_t off = 0;
	uint64_t resp_type = 0xff;
	uint32_t i;
	const msgpack_object *data_obj = NULL;
	int rc = -1;

	if(!srv) {
		srv = tnt_srv_list;
		if(!srv) {
			LM_ERR("no tarantool servers configured\n");
			return -1;
		}
	}

	if(!srv->connected || srv->fd < 0) {
		if(tnt_conn_connect(srv) < 0) {
			LM_ERR("cannot execute eval: connection to %.*s:%d is down\n",
					srv->addr.len, srv->addr.s, srv->port);
			return -1;
		}
	}

	sync_id = ++srv->sync_id;
	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	/* Header Map(2): { 0x00: IPROTO_EVAL, 0x01: sync_id } */
	msgpack_pack_map(&pk, 2);
	msgpack_pack_uint8(&pk, TNT_IPROTO_REQUEST_TYPE);
	msgpack_pack_uint8(&pk, TNT_IPROTO_EVAL);
	msgpack_pack_uint8(&pk, TNT_IPROTO_SYNC);
	msgpack_pack_uint64(&pk, sync_id);

	/* Body Map(2): { 0x27: expr, 0x21: args [] } */
	msgpack_pack_map(&pk, 2);
	msgpack_pack_uint8(&pk, TNT_IPROTO_EXPR);
	msgpack_pack_str(&pk, expr->len);
	msgpack_pack_str_body(&pk, expr->s, expr->len);

	msgpack_pack_uint8(&pk, TNT_IPROTO_TUPLE);
	if(params_json && params_json->len > 0 && params_json->s[0] != '[') {
		msgpack_pack_array(&pk, 1);
		msgpack_pack_str(&pk, params_json->len);
		msgpack_pack_str_body(&pk, params_json->s, params_json->len);
	} else {
		msgpack_pack_array(&pk, 0);
	}

	body_len = (uint32_t)sbuf.size;
	len_hdr[0] = (char)0xce;
	net_len = htonl(body_len);
	memcpy(len_hdr + 1, &net_len, 4);

	if(tnt_socket_send_all(srv->fd, len_hdr, 5) < 0
			|| tnt_socket_send_all(srv->fd, sbuf.data, body_len) < 0) {
		LM_ERR("failed to send IPROTO_EVAL to %.*s:%d: %s\n", srv->addr.len,
				srv->addr.s, srv->port, strerror(errno));
		msgpack_sbuffer_destroy(&sbuf);
		tnt_conn_fail(srv);
		return -1;
	}
	msgpack_sbuffer_destroy(&sbuf);

	/* Read response header */
	if(tnt_socket_recv_all(srv->fd, resp_hdr, 5) < 0
			|| (uint8_t)resp_hdr[0] != 0xce) {
		LM_ERR("failed to receive IPROTO_EVAL response header from %.*s:%d: "
			   "%s\n",
				srv->addr.len, srv->addr.s, srv->port, strerror(errno));
		tnt_conn_fail(srv);
		return -1;
	}

	memcpy(&resp_len, resp_hdr + 1, 4);
	resp_len = ntohl(resp_len);
	if(resp_len > 1048576) {
		LM_ERR("IPROTO_EVAL response too large (%u bytes) from %.*s:%d\n",
				resp_len, srv->addr.len, srv->addr.s, srv->port);
		tnt_conn_fail(srv);
		return -1;
	}

	resp_body = (char *)pkg_malloc(resp_len);
	if(!resp_body) {
		LM_ERR("pkg_malloc failed for IPROTO_EVAL response body (%u bytes)\n",
				resp_len);
		return -1;
	}

	if(tnt_socket_recv_all(srv->fd, resp_body, resp_len) < 0) {
		LM_ERR("failed to receive IPROTO_EVAL response body from %.*s:%d: %s\n",
				srv->addr.len, srv->addr.s, srv->port, strerror(errno));
		tnt_conn_fail(srv);
		goto out_free;
	}

	msgpack_unpacked_init(&msg);
	off = 0;

	if(msgpack_unpack_next(&msg, resp_body, resp_len, &off)
			== MSGPACK_UNPACK_SUCCESS) {
		if(msg.data.type == MSGPACK_OBJECT_MAP) {
			for(i = 0; i < msg.data.via.map.size; i++) {
				if(msg.data.via.map.ptr[i].key.type
								== MSGPACK_OBJECT_POSITIVE_INTEGER
						&& msg.data.via.map.ptr[i].key.via.u64
								   == TNT_IPROTO_REQUEST_TYPE) {
					resp_type = msg.data.via.map.ptr[i].val.via.u64;
				}
			}
		}
	}

	if(resp_type != TNT_IPROTO_OK) {
		LM_ERR("Tarantool eval returned error 0x%lx\n",
				(unsigned long)resp_type);
		goto out_unpack;
	}

	if(msgpack_unpack_next(&msg, resp_body, resp_len, &off)
			== MSGPACK_UNPACK_SUCCESS) {
		if(msg.data.type == MSGPACK_OBJECT_MAP) {
			for(i = 0; i < msg.data.via.map.size; i++) {
				if(msg.data.via.map.ptr[i].key.type
								== MSGPACK_OBJECT_POSITIVE_INTEGER
						&& msg.data.via.map.ptr[i].key.via.u64
								   == TNT_IPROTO_DATA) {
					data_obj = &msg.data.via.map.ptr[i].val;
				}
			}
		}
	}

	if(res_dst && data_obj) {
		tnt_mp_to_json_str(data_obj, res_dst);
	}

	srv->consecutive_errors = 0;
	rc = 1;

out_unpack:
	msgpack_unpacked_destroy(&msg);
out_free:
	pkg_free(resp_body);
	return rc;
}

/**
 * tnt_save_call_sg - Zero-Copy Scatter-Gather call state save (writev)
 */
int tnt_save_call_sg(tnt_server_t *srv, const char *call_id, size_t cid_len,
		const char *node_id, size_t nid_len, const char *state,
		size_t state_len, int expires, const char *payload, size_t payload_len)
{
	str proc = str_init("rtpe_call_upsert");
	msgpack_sbuffer sbuf;
	msgpack_packer pk;
	str param_str;
	int rc;

	if(!srv)
		srv = tnt_srv_list;
	if(!srv || !call_id)
		return -1;

	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	/* Pack tuple: [call_id, node_id, state, expires, payload] */
	msgpack_pack_array(&pk, 5);
	msgpack_pack_str(&pk, cid_len);
	msgpack_pack_str_body(&pk, call_id, cid_len);
	msgpack_pack_str(&pk, nid_len);
	msgpack_pack_str_body(&pk, node_id ? node_id : "", nid_len);
	msgpack_pack_str(&pk, state_len);
	msgpack_pack_str_body(&pk, state ? state : "", state_len);
	msgpack_pack_int(&pk, expires);
	msgpack_pack_str(&pk, payload_len);
	msgpack_pack_str_body(&pk, payload ? payload : "", payload_len);

	param_str.s = sbuf.data;
	param_str.len = (int)sbuf.size;

	rc = tnt_exec_call(srv, &proc, &param_str, NULL);
	msgpack_sbuffer_destroy(&sbuf);
	return rc > 0 ? 0 : -1;
}

/**
 * tnt_get_call_buf - Zero-Allocation call retrieval directly into caller buffer
 */
int tnt_get_call_buf(tnt_server_t *srv, const char *call_id, size_t cid_len,
		char *dst_buf, size_t dst_len, size_t *out_len)
{
	str proc = str_init("rtpe_call_get");
	str cid_str;
	str res_dst = {0, 0};
	int rc;

	if(!srv)
		srv = tnt_srv_list;
	if(!srv || !call_id || !dst_buf)
		return -1;

	cid_str.s = (char *)call_id;
	cid_str.len = (int)cid_len;

	rc = tnt_exec_call(srv, &proc, &cid_str, &res_dst);
	if(rc > 0 && res_dst.s) {
		size_t to_copy = (size_t)res_dst.len < dst_len - 1 ? (size_t)res_dst.len
														   : dst_len - 1;
		memcpy(dst_buf, res_dst.s, to_copy);
		dst_buf[to_copy] = '\0';
		if(out_len)
			*out_len = to_copy;
		pkg_free(res_dst.s);
		return 0;
	}

	if(res_dst.s)
		pkg_free(res_dst.s);
	return -1;
}
