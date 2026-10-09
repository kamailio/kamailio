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
#include <ctype.h>
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
extern int tnt_max_response_size_param;
extern int tnt_max_response_percent_param;
extern int tnt_probe_interval_param;

static uint32_t tnt_calc_max_response_size(void)
{
	if(tnt_max_response_size_param > 0) {
		return (uint32_t)tnt_max_response_size_param;
	}
	uint32_t pct = (tnt_max_response_percent_param > 0
						   && tnt_max_response_percent_param <= 100)
						   ? (uint32_t)tnt_max_response_percent_param
						   : 25;
	uint32_t auto_limit = (32 * 1024 * 1024 / 100) * pct;
	if(auto_limit < 1048576)
		auto_limit = 1048576;
	if(auto_limit > 33554432)
		auto_limit = 33554432;
	return auto_limit;
}

int tnt_resolve_server(tnt_server_t *srv)
{
	char proto = PROTO_TCP;
	if(!srv)
		return -1;
	if(sip_hostport2su(
			   &srv->addr_su, &srv->addr, (unsigned short)srv->port, &proto)
			< 0) {
		srv->su_valid = 0;
		return -1;
	}
	srv->su_valid = 1;
	return 0;
}

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
	srv->probe_interval = tnt_probe_interval_param;
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
			} else if(strcmp(key, "probe_interval") == 0) {
				srv->probe_interval = (int)strtol(val, NULL, 10);
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

	{
		struct in_addr in4_tmp;
		struct in6_addr in6_tmp;
		char addr_buf[256];
		int alen = srv->addr.len < 255 ? srv->addr.len : 255;
		memcpy(addr_buf, srv->addr.s, alen);
		addr_buf[alen] = '\0';
		if(inet_pton(AF_INET, addr_buf, &in4_tmp) > 0
				|| inet_pton(AF_INET6, addr_buf, &in6_tmp) > 0) {
			srv->is_ip_literal = 1;
		} else {
			srv->is_ip_literal = 0;
		}
	}

	if(tnt_resolve_server(srv) < 0) {
		if(!init_without_tarantool) {
			LM_ERR("failed to resolve tarantool server address %.*s:%d\n",
					srv->addr.len, srv->addr.s, srv->port);
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
		LM_WARN("failed to resolve tarantool server address %.*s:%d "
				"(init_without_tarantool enabled)\n",
				srv->addr.len, srv->addr.s, srv->port);
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
	if(!name || !name->s || name->len == 0) {
		for(it = tnt_srv_list; it; it = it->next) {
			if(it->sname.len == 7 && strncmp(it->sname.s, "default", 7) == 0) {
				return it;
			}
		}
		return tnt_srv_list;
	}

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
		if(n == 0) {
			errno = ECONNRESET;
			return -1;
		}
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
		srv->last_probe_time = time(NULL);
		LM_WARN("tarantool server %.*s:%d marked disabled for %d seconds\n",
				srv->addr.len, srv->addr.s, srv->port, srv->disable_time);
	}
}

/**
 * tnt_conn_connect - Connect and perform greeting + authentication handshake
 */
static int tnt_conn_connect(tnt_server_t *srv)
{
	struct timeval tv;
	int flag = 1;
	int buf_size = 1024 * 1024;
	char greeting[TNT_GREETING_SIZE];
	int af;

	if(!srv)
		return -1;
	tnt_conn_close(srv);

	/* Check if server is in disable cooldown */
	if(srv->disabled) {
		time_t now = time(NULL);
		if(now < srv->restore_tick) {
			if(srv->probe_interval <= 0
					|| (now - srv->last_probe_time < srv->probe_interval)) {
				LM_DBG("tarantool server %.*s:%d is temporarily disabled\n",
						srv->addr.len, srv->addr.s, srv->port);
				return -1;
			}
			/* Allow single probe attempt */
			srv->last_probe_time = now;
			LM_INFO("probing disabled tarantool server %.*s:%d...\n",
					srv->addr.len, srv->addr.s, srv->port);
		} else {
			LM_INFO("tarantool server %.*s:%d cooldown expired, retrying "
					"connection...\n",
					srv->addr.len, srv->addr.s, srv->port);
			srv->disabled = 0;
		}
	}

	/* Resolve server address if not yet valid or if hostname could have changed */
	if(!srv->su_valid || !srv->is_ip_literal) {
		if(tnt_resolve_server(srv) < 0) {
			LM_ERR("failed to resolve tarantool server %.*s:%d: %s\n",
					srv->addr.len, srv->addr.s, srv->port, strerror(errno));
			tnt_conn_fail(srv);
			return -1;
		}
	}

	af = srv->addr_su.s.sa_family ? srv->addr_su.s.sa_family : AF_INET;
	srv->fd = socket(af, SOCK_STREAM, 0);
	if(srv->fd < 0) {
		LM_ERR("socket() failed: %s\n", strerror(errno));
		tnt_conn_fail(srv);
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

	if(connect(srv->fd, &srv->addr_su.s, sockaddru_len(srv->addr_su)) < 0) {
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
	if(srv->disabled) {
		LM_INFO("tarantool server %.*s:%d recovered successfully via active "
				"probe\n",
				srv->addr.len, srv->addr.s, srv->port);
		srv->disabled = 0;
	}
	/* srv->consecutive_errors is reset upon successful query execution */
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

typedef struct
{
	char *s;
	size_t len;
	size_t cap;
} tnt_buf_t;

static int tnt_buf_append(tnt_buf_t *b, const char *data, size_t len)
{
	if(b->len + len + 1 > b->cap) {
		size_t new_cap = b->cap ? b->cap * 2 : 512;
		while(new_cap < b->len + len + 1)
			new_cap *= 2;
		char *p = (char *)pkg_realloc(b->s, new_cap);
		if(!p)
			return -1;
		b->s = p;
		b->cap = new_cap;
	}
	memcpy(b->s + b->len, data, len);
	b->len += len;
	b->s[b->len] = '\0';
	return 0;
}

static int tnt_buf_append_str_escaped(tnt_buf_t *b, const char *s, size_t len)
{
	size_t i;
	if(tnt_buf_append(b, "\"", 1) < 0)
		return -1;
	for(i = 0; i < len; i++) {
		unsigned char c = (unsigned char)s[i];
		switch(c) {
			case '\"':
				if(tnt_buf_append(b, "\\\"", 2) < 0)
					return -1;
				break;
			case '\\':
				if(tnt_buf_append(b, "\\\\", 2) < 0)
					return -1;
				break;
			case '\b':
				if(tnt_buf_append(b, "\\b", 2) < 0)
					return -1;
				break;
			case '\f':
				if(tnt_buf_append(b, "\\f", 2) < 0)
					return -1;
				break;
			case '\n':
				if(tnt_buf_append(b, "\\n", 2) < 0)
					return -1;
				break;
			case '\r':
				if(tnt_buf_append(b, "\\r", 2) < 0)
					return -1;
				break;
			case '\t':
				if(tnt_buf_append(b, "\\t", 2) < 0)
					return -1;
				break;
			default:
				if(c < 0x20) {
					char ubuf[8];
					int ulen = snprintf(ubuf, sizeof(ubuf), "\\u%04x", c);
					if(tnt_buf_append(b, ubuf, ulen) < 0)
						return -1;
				} else {
					if(tnt_buf_append(b, (const char *)&c, 1) < 0)
						return -1;
				}
				break;
		}
	}
	return tnt_buf_append(b, "\"", 1);
}

static int tnt_mp_to_json_rec(const msgpack_object *obj, tnt_buf_t *b)
{
	char numbuf[64];
	int nlen;
	uint32_t i;

	switch(obj->type) {
		case MSGPACK_OBJECT_NIL:
			return tnt_buf_append(b, "null", 4);
		case MSGPACK_OBJECT_BOOLEAN:
			return obj->via.boolean ? tnt_buf_append(b, "true", 4)
									: tnt_buf_append(b, "false", 5);
		case MSGPACK_OBJECT_POSITIVE_INTEGER:
			nlen = snprintf(numbuf, sizeof(numbuf), "%llu",
					(unsigned long long)obj->via.u64);
			return tnt_buf_append(b, numbuf, nlen);
		case MSGPACK_OBJECT_NEGATIVE_INTEGER:
			nlen = snprintf(
					numbuf, sizeof(numbuf), "%lld", (long long)obj->via.i64);
			return tnt_buf_append(b, numbuf, nlen);
		case MSGPACK_OBJECT_FLOAT32:
		case MSGPACK_OBJECT_FLOAT64:
			nlen = snprintf(numbuf, sizeof(numbuf), "%.15g", obj->via.f64);
			return tnt_buf_append(b, numbuf, nlen);
		case MSGPACK_OBJECT_STR:
			return tnt_buf_append_str_escaped(
					b, obj->via.str.ptr, obj->via.str.size);
		case MSGPACK_OBJECT_BIN:
			return tnt_buf_append_str_escaped(
					b, obj->via.bin.ptr, obj->via.bin.size);
		case MSGPACK_OBJECT_ARRAY:
			if(tnt_buf_append(b, "[", 1) < 0)
				return -1;
			for(i = 0; i < obj->via.array.size; i++) {
				if(i > 0 && tnt_buf_append(b, ", ", 2) < 0)
					return -1;
				if(tnt_mp_to_json_rec(&obj->via.array.ptr[i], b) < 0)
					return -1;
			}
			return tnt_buf_append(b, "]", 1);
		case MSGPACK_OBJECT_MAP:
			if(tnt_buf_append(b, "{", 1) < 0)
				return -1;
			for(i = 0; i < obj->via.map.size; i++) {
				if(i > 0 && tnt_buf_append(b, ", ", 2) < 0)
					return -1;
				if(obj->via.map.ptr[i].key.type == MSGPACK_OBJECT_STR) {
					if(tnt_buf_append_str_escaped(b,
							   obj->via.map.ptr[i].key.via.str.ptr,
							   obj->via.map.ptr[i].key.via.str.size)
							< 0)
						return -1;
				} else {
					if(tnt_mp_to_json_rec(&obj->via.map.ptr[i].key, b) < 0)
						return -1;
				}
				if(tnt_buf_append(b, ": ", 2) < 0)
					return -1;
				if(tnt_mp_to_json_rec(&obj->via.map.ptr[i].val, b) < 0)
					return -1;
			}
			return tnt_buf_append(b, "}", 1);
		default:
			return tnt_buf_append(b, "null", 4);
	}
}

/**
 * tnt_mp_to_json_str - Convert MessagePack Object to proper JSON string in pkg memory
 */
static int tnt_mp_to_json_str(const msgpack_object *obj, str *dst)
{
	tnt_buf_t b;

	if(!obj || !dst)
		return -1;

	memset(&b, 0, sizeof(tnt_buf_t));
	if(tnt_mp_to_json_rec(obj, &b) < 0 || !b.s) {
		if(b.s)
			pkg_free(b.s);
		LM_ERR("failed to convert MsgPack object to JSON: memory allocation "
			   "failed\n");
		return -1;
	}

	dst->s = b.s;
	dst->len = (int)b.len;
	return 0;
}

static const char *tnt_skip_ws(const char *p, const char *end)
{
	while(p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
		p++;
	return p;
}

static int tnt_count_json_elements(
		const char *p, const char *end, char close_char)
{
	int count = 0;
	int depth = 0;
	int in_str = 0;
	int escape = 0;

	p = tnt_skip_ws(p, end);
	if(p >= end)
		return -1;
	if(*p == close_char)
		return 0;

	count = 1;
	while(p < end) {
		char c = *p;
		if(in_str) {
			if(escape) {
				escape = 0;
			} else if(c == '\\') {
				escape = 1;
			} else if(c == '\"') {
				in_str = 0;
			}
		} else {
			if(c == '\"') {
				in_str = 1;
			} else if(c == '[' || c == '{') {
				depth++;
			} else if(c == ']' || c == '}') {
				if(depth == 0) {
					if(c == close_char)
						return count;
					return -1;
				}
				depth--;
			} else if(c == ',' && depth == 0) {
				count++;
			}
		}
		p++;
	}
	return -1;
}

static int tnt_pack_json_value(
		msgpack_packer *pk, const char **cur, const char *end)
{
	const char *p = tnt_skip_ws(*cur, end);
	if(p >= end)
		return -1;

	if(*p == '\"') {
		p++;
		tnt_buf_t sbuf;
		memset(&sbuf, 0, sizeof(tnt_buf_t));
		while(p < end && *p != '\"') {
			if(*p == '\\' && p + 1 < end) {
				p++;
				switch(*p) {
					case '\"':
						tnt_buf_append(&sbuf, "\"", 1);
						break;
					case '\\':
						tnt_buf_append(&sbuf, "\\", 1);
						break;
					case '/':
						tnt_buf_append(&sbuf, "/", 1);
						break;
					case 'b':
						tnt_buf_append(&sbuf, "\b", 1);
						break;
					case 'f':
						tnt_buf_append(&sbuf, "\f", 1);
						break;
					case 'n':
						tnt_buf_append(&sbuf, "\n", 1);
						break;
					case 'r':
						tnt_buf_append(&sbuf, "\r", 1);
						break;
					case 't':
						tnt_buf_append(&sbuf, "\t", 1);
						break;
					case 'u': {
						if(p + 4 < end) {
							char hex[5] = {p[1], p[2], p[3], p[4], '\0'};
							char *endptr = NULL;
							long cp = strtol(hex, &endptr, 16);
							if(endptr == hex + 4 && cp >= 0) {
								if(cp < 0x80) {
									char ascii_c = (char)cp;
									tnt_buf_append(&sbuf, &ascii_c, 1);
								} else if(cp < 0x800) {
									char utf8[2];
									utf8[0] = (char)(0xC0 | (cp >> 6));
									utf8[1] = (char)(0x80 | (cp & 0x3F));
									tnt_buf_append(&sbuf, utf8, 2);
								} else {
									char utf8[3];
									utf8[0] = (char)(0xE0 | (cp >> 12));
									utf8[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
									utf8[2] = (char)(0x80 | (cp & 0x3F));
									tnt_buf_append(&sbuf, utf8, 3);
								}
							}
							p += 4;
						}
						break;
					}
					default:
						tnt_buf_append(&sbuf, p, 1);
						break;
				}
			} else {
				tnt_buf_append(&sbuf, p, 1);
			}
			p++;
		}
		if(p >= end || *p != '\"') {
			if(sbuf.s)
				pkg_free(sbuf.s);
			return -1;
		}
		p++;
		*cur = p;
		msgpack_pack_str(pk, (uint32_t)sbuf.len);
		if(sbuf.len > 0)
			msgpack_pack_str_body(pk, sbuf.s, (uint32_t)sbuf.len);
		if(sbuf.s)
			pkg_free(sbuf.s);
		return 0;
	}

	if(*p == '[') {
		p++;
		int count = tnt_count_json_elements(p, end, ']');
		if(count < 0)
			return -1;
		msgpack_pack_array(pk, (uint32_t)count);
		p = tnt_skip_ws(p, end);
		if(p < end && *p == ']') {
			*cur = p + 1;
			return 0;
		}
		for(int i = 0; i < count; i++) {
			if(tnt_pack_json_value(pk, &p, end) < 0)
				return -1;
			p = tnt_skip_ws(p, end);
			if(i < count - 1) {
				if(p >= end || *p != ',')
					return -1;
				p++;
			}
		}
		p = tnt_skip_ws(p, end);
		if(p >= end || *p != ']')
			return -1;
		p++;
		*cur = p;
		return 0;
	}

	if(*p == '{') {
		p++;
		int count = tnt_count_json_elements(p, end, '}');
		if(count < 0)
			return -1;
		msgpack_pack_map(pk, (uint32_t)count);
		p = tnt_skip_ws(p, end);
		if(p < end && *p == '}') {
			*cur = p + 1;
			return 0;
		}
		for(int i = 0; i < count; i++) {
			if(tnt_pack_json_value(pk, &p, end) < 0)
				return -1;
			p = tnt_skip_ws(p, end);
			if(p >= end || *p != ':')
				return -1;
			p++;
			if(tnt_pack_json_value(pk, &p, end) < 0)
				return -1;
			p = tnt_skip_ws(p, end);
			if(i < count - 1) {
				if(p >= end || *p != ',')
					return -1;
				p++;
			}
		}
		p = tnt_skip_ws(p, end);
		if(p >= end || *p != '}')
			return -1;
		p++;
		*cur = p;
		return 0;
	}

	if(end - p >= 4 && strncmp(p, "true", 4) == 0) {
		const char *after = p + 4;
		if(after == end || isspace((unsigned char)*after) || *after == ','
				|| *after == ']' || *after == '}') {
			msgpack_pack_true(pk);
			*cur = after;
			return 0;
		}
		return -1;
	}

	if(end - p >= 5 && strncmp(p, "false", 5) == 0) {
		const char *after = p + 5;
		if(after == end || isspace((unsigned char)*after) || *after == ','
				|| *after == ']' || *after == '}') {
			msgpack_pack_false(pk);
			*cur = after;
			return 0;
		}
		return -1;
	}

	if(end - p >= 4 && strncmp(p, "null", 4) == 0) {
		const char *after = p + 4;
		if(after == end || isspace((unsigned char)*after) || *after == ','
				|| *after == ']' || *after == '}') {
			msgpack_pack_nil(pk);
			*cur = after;
			return 0;
		}
		return -1;
	}

	if(*p == '-' || isdigit((unsigned char)*p)) {
		char *next = NULL;
		int is_float = 0;
		const char *scan = p;
		if(*scan == '-')
			scan++;
		if(scan >= end || !isdigit((unsigned char)*scan))
			return -1;
		while(scan < end && isdigit((unsigned char)*scan))
			scan++;
		if(scan < end && *scan == '.') {
			is_float = 1;
			scan++;
			if(scan >= end || !isdigit((unsigned char)*scan))
				return -1;
			while(scan < end && isdigit((unsigned char)*scan))
				scan++;
		}
		if(scan < end && (*scan == 'e' || *scan == 'E')) {
			is_float = 1;
			scan++;
			if(scan < end && (*scan == '+' || *scan == '-'))
				scan++;
			if(scan >= end || !isdigit((unsigned char)*scan))
				return -1;
			while(scan < end && isdigit((unsigned char)*scan))
				scan++;
		}
		if(scan < end && *scan != ',' && *scan != ']' && *scan != '}'
				&& !isspace((unsigned char)*scan)) {
			return -1;
		}

		if(is_float) {
			double d = strtod(p, &next);
			if(next != scan)
				return -1;
			msgpack_pack_double(pk, d);
		} else {
			long long val = strtoll(p, &next, 10);
			if(next != scan)
				return -1;
			msgpack_pack_int64(pk, val);
		}
		*cur = scan;
		return 0;
	}

	return -1;
}

static int tnt_pack_params_tuple(msgpack_packer *pk, const str *params)
{
	if(!params || !params->s || params->len <= 0) {
		return msgpack_pack_array(pk, 0);
	}

	const char *cur = tnt_skip_ws(params->s, params->s + params->len);
	const char *end = params->s + params->len;

	if(cur >= end) {
		return msgpack_pack_array(pk, 0);
	}

	/* If params starts with '[', it must be a strict JSON array */
	if(*cur == '[') {
		cur++;
		cur = tnt_skip_ws(cur, end);
		if(cur < end && *cur == ']') {
			cur++;
			cur = tnt_skip_ws(cur, end);
			if(cur != end)
				return -1;
			return msgpack_pack_array(pk, 0);
		}

		int count = tnt_count_json_elements(cur, end, ']');
		if(count <= 0)
			return -1;

		if(msgpack_pack_array(pk, (uint32_t)count) < 0)
			return -1;

		for(int i = 0; i < count; i++) {
			if(tnt_pack_json_value(pk, &cur, end) < 0)
				return -1;
			cur = tnt_skip_ws(cur, end);
			if(i < count - 1) {
				if(cur >= end || *cur != ',')
					return -1;
				cur++;
				cur = tnt_skip_ws(cur, end);
			}
		}
		cur = tnt_skip_ws(cur, end);
		if(cur >= end || *cur != ']')
			return -1;
		cur++;
		cur = tnt_skip_ws(cur, end);
		if(cur != end)
			return -1;
		return 0;
	}

	/* Bare string or single parameter: pack as exactly 1 string argument */
	if(msgpack_pack_array(pk, 1) < 0)
		return -1;
	if(msgpack_pack_str(pk, (uint32_t)params->len) < 0)
		return -1;
	if(params->len > 0)
		return msgpack_pack_str_body(pk, params->s, (uint32_t)params->len);
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
	uint32_t max_resp;
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
	if(tnt_pack_params_tuple(&pk, params_json) < 0) {
		LM_ERR("failed to pack procedure parameters for proc=%.*s (syntax "
			   "error or malformed JSON)\n",
				proc_name->len, proc_name->s);
		msgpack_sbuffer_destroy(&sbuf);
		return -1;
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
		int err = errno;
		LM_ERR("failed to receive IPROTO_CALL response header from %.*s:%d: "
			   "%s\n",
				srv->addr.len, srv->addr.s, srv->port, strerror(err));
		tnt_conn_fail(srv);
		if(err == EAGAIN || err == EWOULDBLOCK || err == ETIMEDOUT) {
			return -3;
		}
		return -1;
	}

	memcpy(&resp_len, resp_hdr + 1, 4);
	resp_len = ntohl(resp_len);
	max_resp = tnt_calc_max_response_size();
	if(resp_len > max_resp) {
		LM_ERR("IPROTO_CALL response too large (%u bytes, exceeds "
			   "max_response_size limit %u) from %.*s:%d\n",
				resp_len, max_resp, srv->addr.len, srv->addr.s, srv->port);
		tnt_conn_fail(srv);
		return -1;
	}

	resp_body = (char *)pkg_malloc(resp_len);
	if(!resp_body) {
		LM_ERR("pkg_malloc failed for IPROTO_CALL response body (%u bytes)\n",
				resp_len);
		tnt_conn_fail(srv);
		return -1;
	}

	if(tnt_socket_recv_all(srv->fd, resp_body, resp_len) < 0) {
		int err = errno;
		LM_ERR("failed to receive IPROTO_CALL response body from %.*s:%d: %s\n",
				srv->addr.len, srv->addr.s, srv->port, strerror(err));
		tnt_conn_fail(srv);
		rc = (err == EAGAIN || err == EWOULDBLOCK || err == ETIMEDOUT) ? -3
																	   : -1;
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
		str err_msg = str_init("unknown error");
		if(msgpack_unpack_next(&msg, resp_body, resp_len, &off)
				== MSGPACK_UNPACK_SUCCESS) {
			if(msg.data.type == MSGPACK_OBJECT_MAP) {
				for(i = 0; i < msg.data.via.map.size; i++) {
					uint64_t k = msg.data.via.map.ptr[i].key.via.u64;
					if(k == TNT_IPROTO_ERROR || k == TNT_IPROTO_ERROR_24) {
						if(msg.data.via.map.ptr[i].val.type
								== MSGPACK_OBJECT_STR) {
							err_msg.s = (char *)msg.data.via.map.ptr[i]
												.val.via.str.ptr;
							err_msg.len =
									msg.data.via.map.ptr[i].val.via.str.size;
						}
					}
				}
			}
		}
		LM_ERR("Tarantool call '%.*s' error 0x%lx: %.*s\n", proc_name->len,
				proc_name->s, (unsigned long)resp_type, err_msg.len, err_msg.s);
		rc = -2;
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
		if(tnt_mp_to_json_str(data_obj, res_dst) < 0) {
			LM_ERR("failed to serialize procedure result to JSON\n");
			rc = -1;
			goto out_unpack;
		}
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
int tnt_exec_eval(tnt_server_t *srv, const str *expr, const str *params_json,
		str *res_dst)
{
	uint64_t sync_id;
	msgpack_sbuffer sbuf;
	msgpack_packer pk;
	uint32_t body_len;
	char len_hdr[5];
	uint32_t net_len;
	char resp_hdr[5];
	uint32_t resp_len;
	uint32_t max_resp;
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
	if(tnt_pack_params_tuple(&pk, params_json) < 0) {
		LM_ERR("failed to pack expression parameters for expr=%.*s (syntax "
			   "error or malformed JSON)\n",
				expr->len, expr->s);
		msgpack_sbuffer_destroy(&sbuf);
		return -1;
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
		int err = errno;
		LM_ERR("failed to receive IPROTO_EVAL response header from %.*s:%d: "
			   "%s\n",
				srv->addr.len, srv->addr.s, srv->port, strerror(err));
		tnt_conn_fail(srv);
		if(err == EAGAIN || err == EWOULDBLOCK || err == ETIMEDOUT) {
			return -3;
		}
		return -1;
	}

	memcpy(&resp_len, resp_hdr + 1, 4);
	resp_len = ntohl(resp_len);
	max_resp = tnt_calc_max_response_size();
	if(resp_len > max_resp) {
		LM_ERR("IPROTO_EVAL response too large (%u bytes, exceeds "
			   "max_response_size limit %u) from %.*s:%d\n",
				resp_len, max_resp, srv->addr.len, srv->addr.s, srv->port);
		tnt_conn_fail(srv);
		return -1;
	}

	resp_body = (char *)pkg_malloc(resp_len);
	if(!resp_body) {
		LM_ERR("pkg_malloc failed for IPROTO_EVAL response body (%u bytes)\n",
				resp_len);
		tnt_conn_fail(srv);
		return -1;
	}

	if(tnt_socket_recv_all(srv->fd, resp_body, resp_len) < 0) {
		int err = errno;
		LM_ERR("failed to receive IPROTO_EVAL response body from %.*s:%d: %s\n",
				srv->addr.len, srv->addr.s, srv->port, strerror(err));
		tnt_conn_fail(srv);
		rc = (err == EAGAIN || err == EWOULDBLOCK || err == ETIMEDOUT) ? -3
																	   : -1;
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
		str err_msg = str_init("unknown error");
		if(msgpack_unpack_next(&msg, resp_body, resp_len, &off)
				== MSGPACK_UNPACK_SUCCESS) {
			if(msg.data.type == MSGPACK_OBJECT_MAP) {
				for(i = 0; i < msg.data.via.map.size; i++) {
					uint64_t k = msg.data.via.map.ptr[i].key.via.u64;
					if(k == TNT_IPROTO_ERROR || k == TNT_IPROTO_ERROR_24) {
						if(msg.data.via.map.ptr[i].val.type
								== MSGPACK_OBJECT_STR) {
							err_msg.s = (char *)msg.data.via.map.ptr[i]
												.val.via.str.ptr;
							err_msg.len =
									msg.data.via.map.ptr[i].val.via.str.size;
						}
					}
				}
			}
		}
		LM_ERR("Tarantool eval error 0x%lx: %.*s\n", (unsigned long)resp_type,
				err_msg.len, err_msg.s);
		rc = -2;
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
		if(tnt_mp_to_json_str(data_obj, res_dst) < 0) {
			LM_ERR("failed to serialize eval result to JSON\n");
			rc = -1;
			goto out_unpack;
		}
	}

	srv->consecutive_errors = 0;
	rc = 1;

out_unpack:
	msgpack_unpacked_destroy(&msg);
out_free:
	pkg_free(resp_body);
	return rc;
}
