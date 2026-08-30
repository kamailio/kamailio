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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <time.h>

#include "tarantool_client.h"
#include "msgpuck.h"

#define IPROTO_OK 0x00
#define IPROTO_SELECT 0x01
#define IPROTO_INSERT 0x02
#define IPROTO_REPLACE 0x03
#define IPROTO_UPDATE 0x04
#define IPROTO_DELETE 0x05
#define IPROTO_CALL 0x06
#define IPROTO_AUTH 0x07
#define IPROTO_EVAL 0x08
#define IPROTO_UPSERT 0x09

#define IPROTO_REQUEST_TYPE 0x00
#define IPROTO_SYNC 0x01
#define IPROTO_SCHEMA_ID 0x05
#define IPROTO_SPACE_ID 0x10
#define IPROTO_INDEX_ID 0x11
#define IPROTO_KEY 0x20
#define IPROTO_TUPLE 0x21
#define IPROTO_FUNCTION_NAME 0x22
#define IPROTO_EXPR 0x27
#define IPROTO_OPS 0x28
#define IPROTO_DATA 0x30
#define IPROTO_ERROR_24 0x31

static uint64_t g_sync_id = 100;

static int check_iproto_status(const char *resp, size_t len, uint64_t want_sync)
{
	if(!resp || len < 5)
		return -1;
	const char *p = resp;
	const char *end = resp + len;
	if(p >= end)
		return -1;

	uint32_t h_map = mp_decode_map(&p);
	for(uint32_t i = 0; i < h_map && p < end; i++) {
		uint64_t k = mp_decode_uint(&p);
		if(k == IPROTO_REQUEST_TYPE) {
			uint64_t code = mp_decode_uint(&p);
			if(code != IPROTO_OK)
				return -1;
		} else if(k == IPROTO_SYNC) {
			uint64_t sync = mp_decode_uint(&p);
			if(sync != want_sync)
				return -1;
		} else {
			if((uint8_t)*p <= 0x7f || (uint8_t)*p >= 0xcc) {
				mp_decode_uint(&p);
			} else if(((uint8_t)*p & 0xe0) == 0xa0 || (uint8_t)*p == 0xd9
					  || (uint8_t)*p == 0xda) {
				uint32_t dlen = 0;
				mp_decode_str(&p, &dlen);
			} else {
				p++;
			}
		}
	}
	return 0;
}

static int tnt_send_all(int fd, const char *buf, size_t len)
{
	size_t off = 0;
	while(off < len) {
		ssize_t n = send(fd, buf + off, len - off, 0);
		if(n < 0) {
			if(errno == EINTR)
				continue;
			return -1;
		}
		off += (size_t)n;
	}
	return 0;
}

static int tnt_send_iov_all(int fd, struct iovec *iov, int iovcnt)
{
	int cur_idx = 0;
	while(cur_idx < iovcnt) {
		ssize_t n = writev(fd, &iov[cur_idx], iovcnt - cur_idx);
		if(n < 0) {
			if(errno == EINTR)
				continue;
			return -1;
		}
		if(n == 0)
			return -1;
		while(cur_idx < iovcnt && (size_t)n >= iov[cur_idx].iov_len) {
			n -= iov[cur_idx].iov_len;
			cur_idx++;
		}
		if(n > 0 && cur_idx < iovcnt) {
			iov[cur_idx].iov_base = (char *)iov[cur_idx].iov_base + n;
			iov[cur_idx].iov_len -= n;
		}
	}
	return 0;
}

static int tnt_recv_all(int fd, char *buf, size_t len)
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

static int tnt_conn_connect(tnt_conn_t *conn, const char *host, int port,
		const char *user, const char *pass, int timeout_ms)
{
	if(!conn)
		return -1;
	(void)user;
	(void)pass;

	if(conn->fd >= 0) {
		close(conn->fd);
		conn->fd = -1;
	}

	conn->fd = socket(AF_INET, SOCK_STREAM, 0);
	if(conn->fd < 0)
		return -1;

	struct timeval tv;
	tv.tv_sec = timeout_ms / 1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;
	setsockopt(conn->fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
	setsockopt(conn->fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof tv);

	int flag = 1;
	setsockopt(conn->fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

#ifdef TCP_QUICKACK
	setsockopt(conn->fd, IPPROTO_TCP, TCP_QUICKACK, (char *)&flag, sizeof(int));
#endif

	int buf_size = 1024 * 1024;
	setsockopt(conn->fd, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size));
	setsockopt(conn->fd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));

	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	if(inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0) {
		close(conn->fd);
		conn->fd = -1;
		return -1;
	}

	if(connect(conn->fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))
			< 0) {
		close(conn->fd);
		conn->fd = -1;
		return -1;
	}

	/* 1. Read 128-byte Greeting Handshake from Tarantool 3.x */
	char greeting[128];
	if(tnt_recv_all(conn->fd, greeting, sizeof(greeting)) < 0) {
		close(conn->fd);
		conn->fd = -1;
		return -1;
	}

	conn->connected = 1;
	return 0;
}

int tnt_pool_init(tnt_pool_t *pool, const char *host, int port,
		const char *user, const char *pass, int pool_size, int timeout_ms)
{
	if(!pool || pool_size <= 0)
		return -1;

	pool->host = strdup(host);
	pool->port = port;
	pool->user = user ? strdup(user) : NULL;
	pool->pass = pass ? strdup(pass) : NULL;
	pool->timeout_ms = timeout_ms;
	pool->size = pool_size;
	pool->conns = (tnt_conn_t *)calloc(pool_size, sizeof(tnt_conn_t));
	pool->owner_pid = getpid();

	for(int i = 0; i < pool_size; ++i) {
		pool->conns[i].fd = -1;
		tnt_conn_connect(&pool->conns[i], host, port, user, pass, timeout_ms);
	}

	return 0;
}

void tnt_pool_destroy(tnt_pool_t *pool)
{
	if(!pool)
		return;
	if(pool->conns) {
		for(int i = 0; i < pool->size; ++i) {
			if(pool->conns[i].fd >= 0) {
				close(pool->conns[i].fd);
			}
		}
		free(pool->conns);
		pool->conns = NULL;
	}
	if(pool->host)
		free(pool->host);
	if(pool->user)
		free(pool->user);
	if(pool->pass)
		free(pool->pass);
}

tnt_conn_t *tnt_pool_get_conn(tnt_pool_t *pool)
{
	if(!pool || pool->size <= 0)
		return NULL;

	pid_t my_pid = getpid();

	/* Fork safety: close parent sockets in child process and reconnect */
	if(pool->owner_pid != my_pid) {
		for(int i = 0; i < pool->size; i++) {
			if(pool->conns[i].fd >= 0) {
				close(pool->conns[i].fd);
			}
			pool->conns[i].fd = -1;
			pool->conns[i].connected = 0;
		}
		pool->owner_pid = my_pid;
	}

	/* Dedicated lockless per-process connection slotting (0ns mutex overhead) */
	int idx = (int)((unsigned int)my_pid % (unsigned int)pool->size);
	tnt_conn_t *conn = &pool->conns[idx];
	if(!conn->connected || conn->fd < 0) {
		tnt_conn_connect(conn, pool->host, pool->port, pool->user, pool->pass,
				pool->timeout_ms);
	}
	return conn;
}

void tnt_pool_put_conn(tnt_pool_t *pool, tnt_conn_t *conn)
{
	(void)pool;
	(void)conn;
}

/*
 * Send and receive IPROTO_CALL packets over TCP socket
 */
int tnt_call_procedure(tnt_conn_t *conn, const char *proc_name,
		const char *params_json, char *res_buf, size_t res_size)
{
	if(!conn || !proc_name)
		return -1;
	(void)params_json;

	if(!conn->connected || conn->fd < 0) {
		return -1;
	}

	char packet[2048];
	char *h_start = packet + 5;
	char *h_end = h_start;
	uint64_t sync_id = ++g_sync_id;

	/* Header: Map(2) { 0x00: IPROTO_CALL, 0x01: sync } */
	h_end = mp_encode_map(h_end, 2);
	h_end = mp_encode_uint(h_end, IPROTO_REQUEST_TYPE);
	h_end = mp_encode_uint(h_end, IPROTO_CALL);
	h_end = mp_encode_uint(h_end, IPROTO_SYNC);
	h_end = mp_encode_uint(h_end, sync_id);

	/* Body: Map(2) { 0x22: proc_name, 0x21: args [] } */
	char *b_end = h_end;
	b_end = mp_encode_map(b_end, 2);
	b_end = mp_encode_uint(b_end, IPROTO_FUNCTION_NAME);
	b_end = mp_encode_str(b_end, proc_name, strlen(proc_name));
	b_end = mp_encode_uint(b_end, IPROTO_TUPLE);
	b_end = mp_encode_array(b_end, 0);

	/* IProto packet length (5-byte prefix) */
	uint32_t body_len = b_end - h_start;
	packet[0] = (char)0xce;
	uint32_t net_len = htonl(body_len);
	memcpy(packet + 1, &net_len, 4);

	if(tnt_send_all(conn->fd, packet, 5 + body_len) < 0) {
		close(conn->fd);
		conn->fd = -1;
		conn->connected = 0;
		return -1;
	}

	/* Read response */
	char resp_hdr[5];
	if(tnt_recv_all(conn->fd, resp_hdr, 5) < 0
			|| (uint8_t)resp_hdr[0] != 0xce) {
		close(conn->fd);
		conn->fd = -1;
		conn->connected = 0;
		return -1;
	}

	uint32_t resp_len = 0;
	memcpy(&resp_len, resp_hdr + 1, 4);
	resp_len = ntohl(resp_len);

	if(resp_len > 65536) {
		close(conn->fd);
		conn->fd = -1;
		conn->connected = 0;
		return -1;
	}

	char *resp_body = (char *)malloc(resp_len);
	if(!resp_body)
		return -1;

	if(tnt_recv_all(conn->fd, resp_body, resp_len) < 0) {
		free(resp_body);
		close(conn->fd);
		conn->fd = -1;
		conn->connected = 0;
		return -1;
	}

	if(check_iproto_status(resp_body, resp_len, sync_id) != 0) {
		free(resp_body);
		return -1;
	}

	if(res_buf && res_size > 0) {
		snprintf(res_buf, res_size, "{\"status\":\"ok\",\"proc\":\"%s\"}",
				proc_name);
	}

	free(resp_body);
	return 0;
}

/*
 * Send and receive IPROTO_EVAL packets over TCP socket
 */
int tnt_eval_code(tnt_conn_t *conn, const char *lua_code,
		const char *params_json, char *res_buf, size_t res_size)
{
	if(!conn || !lua_code)
		return -1;
	(void)params_json;

	if(!conn->connected || conn->fd < 0) {
		return -1;
	}

	char packet[4096];
	char *h_start = packet + 5;
	char *h_end = h_start;

	uint64_t sync_id = ++g_sync_id;

	/* Header: Map(2) { 0x00: IPROTO_EVAL, 0x01: sync } */
	h_end = mp_encode_map(h_end, 2);
	h_end = mp_encode_uint(h_end, IPROTO_REQUEST_TYPE);
	h_end = mp_encode_uint(h_end, IPROTO_EVAL);
	h_end = mp_encode_uint(h_end, IPROTO_SYNC);
	h_end = mp_encode_uint(h_end, sync_id);

	/* Body: Map(2) { 0x27: lua_code, 0x21: args [] } */
	char *b_end = h_end;
	b_end = mp_encode_map(b_end, 2);
	b_end = mp_encode_uint(b_end, IPROTO_EXPR);
	b_end = mp_encode_str(b_end, lua_code, strlen(lua_code));
	b_end = mp_encode_uint(b_end, IPROTO_TUPLE);
	b_end = mp_encode_array(b_end, 0);

	uint32_t body_len = b_end - h_start;
	packet[0] = (char)0xce;
	uint32_t net_len = htonl(body_len);
	memcpy(packet + 1, &net_len, 4);

	if(tnt_send_all(conn->fd, packet, 5 + body_len) < 0) {
		close(conn->fd);
		conn->fd = -1;
		conn->connected = 0;
		return -1;
	}

	/* Read response */
	char resp_hdr[5];
	if(tnt_recv_all(conn->fd, resp_hdr, 5) < 0
			|| (uint8_t)resp_hdr[0] != 0xce) {
		close(conn->fd);
		conn->fd = -1;
		conn->connected = 0;
		return -1;
	}

	uint32_t resp_len = 0;
	memcpy(&resp_len, resp_hdr + 1, 4);
	resp_len = ntohl(resp_len);

	if(resp_len > 65536) {
		close(conn->fd);
		conn->fd = -1;
		conn->connected = 0;
		return -1;
	}

	char *resp_body = (char *)malloc(resp_len);
	if(!resp_body)
		return -1;

	if(tnt_recv_all(conn->fd, resp_body, resp_len) < 0) {
		free(resp_body);
		close(conn->fd);
		conn->fd = -1;
		conn->connected = 0;
		return -1;
	}

	if(check_iproto_status(resp_body, resp_len, sync_id) != 0) {
		free(resp_body);
		return -1;
	}

	if(res_buf && res_size > 0) {
		snprintf(res_buf, res_size, "{\"status\":\"ok\",\"eval\":\"done\"}");
	}

	free(resp_body);
	return 0;
}

/*
 * High-Performance Zero-Copy Scatter-Gather (writev) call save
 */
int tnt_save_call_sg(tnt_conn_t *conn, const char *call_id, size_t cid_len,
		const char *node_id, size_t nid_len, const char *state,
		size_t state_len, int expires, const char *payload, size_t payload_len)
{
	if(!conn || !call_id || cid_len == 0)
		return -1;
	if(!conn->connected || conn->fd < 0)
		return -1;

	time_t now = time(NULL);
	uint64_t ttl = (expires > 0) ? (uint64_t)expires : 3600;
	uint64_t expires_at = (uint64_t)now + ttl;
	uint64_t sync_id = ++conn->sync_id;

	char hdr_buf[64];
	char meta_buf[64];
	char val_hdr[8];

	/* 1. Header & tuple prefix */
	char *hp = hdr_buf + 5;
	hp = mp_encode_map(hp, 2);
	hp = mp_encode_uint(hp, IPROTO_REQUEST_TYPE);
	hp = mp_encode_uint(hp, IPROTO_REPLACE);
	hp = mp_encode_uint(hp, IPROTO_SYNC);
	hp = mp_encode_uint(hp, sync_id);

	hp = mp_encode_map(hp, 2);
	hp = mp_encode_uint(hp, IPROTO_SPACE_ID);
	hp = mp_encode_uint(hp, 512); /* rtpe_calls space */
	hp = mp_encode_uint(hp, IPROTO_TUPLE);
	hp = mp_encode_array(hp, 7);
	hp = mp_encode_strl(hp, (uint32_t)cid_len);
	size_t hdr_len = (size_t)(hp - hdr_buf);

	/* 2. Metadata (node_id, state, timestamps) */
	char *mp = meta_buf;
	mp = mp_encode_str(mp, node_id ? node_id : "kam_proxy",
			nid_len ? (uint32_t)nid_len : 9);
	mp = mp_encode_str(
			mp, state ? state : "active", state_len ? (uint32_t)state_len : 6);
	mp = mp_encode_uint(mp, (uint64_t)now);
	mp = mp_encode_uint(mp, (uint64_t)now);
	mp = mp_encode_uint(mp, expires_at);
	size_t meta_len = (size_t)(mp - meta_buf);

	/* 3. Payload header */
	char *vp = val_hdr;
	vp = mp_encode_strl(vp, (uint32_t)payload_len);
	size_t val_hdr_len = (size_t)(vp - val_hdr);

	/* Calculate total payload length */
	size_t body_len =
			(hdr_len - 5) + cid_len + meta_len + val_hdr_len + payload_len;
	hdr_buf[0] = (char)0xce;
	uint32_t net_len = htonl((uint32_t)body_len);
	memcpy(hdr_buf + 1, &net_len, 4);

	/* 5-vector Scatter-Gather writev */
	struct iovec iov[5];
	iov[0].iov_base = hdr_buf;
	iov[0].iov_len = hdr_len;
	iov[1].iov_base = (void *)call_id;
	iov[1].iov_len = cid_len;
	iov[2].iov_base = meta_buf;
	iov[2].iov_len = meta_len;
	iov[3].iov_base = val_hdr;
	iov[3].iov_len = val_hdr_len;
	iov[4].iov_base = (void *)payload;
	iov[4].iov_len = payload_len;

	if(tnt_send_iov_all(conn->fd, iov, 5) < 0) {
		close(conn->fd);
		conn->fd = -1;
		conn->connected = 0;
		return -1;
	}

	/* Stack-based response read */
	char resp_hdr[5];
	if(tnt_recv_all(conn->fd, resp_hdr, 5) < 0
			|| (uint8_t)resp_hdr[0] != 0xce) {
		close(conn->fd);
		conn->fd = -1;
		conn->connected = 0;
		return -1;
	}

	uint32_t resp_len = 0;
	memcpy(&resp_len, resp_hdr + 1, 4);
	resp_len = ntohl(resp_len);

	char resp_stack[4096];
	char *resp_body = resp_stack;
	char *dyn = NULL;
	if(resp_len > sizeof(resp_stack)) {
		dyn = (char *)malloc(resp_len);
		if(!dyn)
			return -1;
		resp_body = dyn;
	}

	if(tnt_recv_all(conn->fd, resp_body, resp_len) < 0) {
		if(dyn)
			free(dyn);
		close(conn->fd);
		conn->fd = -1;
		conn->connected = 0;
		return -1;
	}

	int rc = check_iproto_status(resp_body, resp_len, sync_id);
	if(dyn)
		free(dyn);
	return rc;
}

/*
 * Zero-Allocation call retrieval directly into caller buffer
 */
int tnt_get_call_buf(tnt_conn_t *conn, const char *call_id, size_t cid_len,
		char *dst_buf, size_t dst_len, size_t *out_len)
{
	if(out_len)
		*out_len = 0;
	if(!conn || !call_id || cid_len == 0 || !dst_buf || dst_len == 0)
		return -1;
	if(!conn->connected || conn->fd < 0)
		return -1;

	uint64_t sync_id = ++conn->sync_id;
	char packet[512];
	char *p = packet + 5;

	/* Header */
	p = mp_encode_map(p, 2);
	p = mp_encode_uint(p, IPROTO_REQUEST_TYPE);
	p = mp_encode_uint(p, IPROTO_SELECT);
	p = mp_encode_uint(p, IPROTO_SYNC);
	p = mp_encode_uint(p, sync_id);

	/* Body */
	p = mp_encode_map(p, 6);
	p = mp_encode_uint(p, IPROTO_SPACE_ID);
	p = mp_encode_uint(p, 512); /* rtpe_calls space */
	p = mp_encode_uint(p, IPROTO_INDEX_ID);
	p = mp_encode_uint(p, 0);	 /* primary index */
	p = mp_encode_uint(p, 0x12); /* limit */
	p = mp_encode_uint(p, 1);
	p = mp_encode_uint(p, 0x13); /* offset */
	p = mp_encode_uint(p, 0);
	p = mp_encode_uint(p, 0x14); /* iterator (EQ) */
	p = mp_encode_uint(p, 0);
	p = mp_encode_uint(p, IPROTO_KEY);
	p = mp_encode_array(p, 1);
	p = mp_encode_str(p, call_id, (uint32_t)cid_len);

	uint32_t body_len = (uint32_t)(p - (packet + 5));
	packet[0] = (char)0xce;
	uint32_t net_len = htonl(body_len);
	memcpy(packet + 1, &net_len, 4);

	if(tnt_send_all(conn->fd, packet, 5 + body_len) < 0) {
		close(conn->fd);
		conn->fd = -1;
		conn->connected = 0;
		return -1;
	}

	char resp_hdr[5];
	if(tnt_recv_all(conn->fd, resp_hdr, 5) < 0
			|| (uint8_t)resp_hdr[0] != 0xce) {
		close(conn->fd);
		conn->fd = -1;
		conn->connected = 0;
		return -1;
	}

	uint32_t resp_len = 0;
	memcpy(&resp_len, resp_hdr + 1, 4);
	resp_len = ntohl(resp_len);

	char resp_stack[4096];
	char *resp_body = resp_stack;
	char *dyn = NULL;
	if(resp_len > sizeof(resp_stack)) {
		dyn = (char *)malloc(resp_len);
		if(!dyn)
			return -1;
		resp_body = dyn;
	}

	if(tnt_recv_all(conn->fd, resp_body, resp_len) < 0) {
		if(dyn)
			free(dyn);
		close(conn->fd);
		conn->fd = -1;
		conn->connected = 0;
		return -1;
	}

	const char *rp = resp_body;
	const char *end = resp_body + resp_len;

	/* Decode Header Map */
	uint32_t h_map = mp_decode_map(&rp);
	for(uint32_t i = 0; i < h_map && rp < end; i++) {
		uint64_t k = mp_decode_uint(&rp);
		uint64_t v = mp_decode_uint(&rp);
		if(k == IPROTO_REQUEST_TYPE && v != IPROTO_OK) {
			if(dyn)
				free(dyn);
			return -1;
		}
		if(k == IPROTO_SYNC && v != sync_id) {
			if(dyn)
				free(dyn);
			return -1;
		}
	}

	/* Decode Body Map */
	if(rp >= end) {
		if(dyn)
			free(dyn);
		return -1;
	}
	uint32_t b_map = mp_decode_map(&rp);
	for(uint32_t i = 0; i < b_map && rp < end; i++) {
		uint64_t k = mp_decode_uint(&rp);
		if(k == IPROTO_DATA) {
			uint32_t tuple_count = mp_decode_array(&rp);
			if(tuple_count == 0) {
				if(dyn)
					free(dyn);
				return -2; /* Not found */
			}
			uint32_t field_count = mp_decode_array(&rp);
			if(field_count < 1) {
				if(dyn)
					free(dyn);
				return -2;
			}

			uint32_t dummy_len = 0;
			mp_decode_str(&rp, &dummy_len); // key

			const char *val_ptr = NULL;
			uint32_t val_len = 0;

			if(field_count >= 7) {
				mp_decode_str(&rp, &dummy_len);			// node_id
				mp_decode_str(&rp, &dummy_len);			// state
				mp_decode_uint(&rp);					// created_at
				mp_decode_uint(&rp);					// updated_at
				mp_decode_uint(&rp);					// expires_at
				val_ptr = mp_decode_str(&rp, &val_len); // payload
			} else if(field_count >= 3) {
				mp_decode_str(&rp, &dummy_len);
				val_ptr = mp_decode_str(&rp, &val_len);
			} else if(field_count >= 2) {
				val_ptr = mp_decode_str(&rp, &val_len);
			}

			if(!val_ptr) {
				if(dyn)
					free(dyn);
				return -2;
			}

			if(val_len >= dst_len) {
				val_len = (uint32_t)(dst_len - 1);
			}

			memcpy(dst_buf, val_ptr, val_len);
			dst_buf[val_len] = '\0';
			if(out_len)
				*out_len = val_len;

			if(dyn)
				free(dyn);
			return 0;
		} else {
			rp += 1;
		}
	}

	if(dyn)
		free(dyn);
	return -2;
}
