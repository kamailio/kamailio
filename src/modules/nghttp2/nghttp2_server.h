/**
 * Copyright (C) 2024 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef NGHTTP2_SERVER_H_
#define NGHTTP2_SERVER_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <netinet/in.h>

#include <string.h>
#include <errno.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/conf.h>

#include <event.h>
#include <event2/event.h>
#include <event2/bufferevent_ssl.h>
#include <event2/listener.h>

#define NGHTTP2_NO_SSIZE_T
#include <nghttp2/nghttp2.h>

#include "../../core/str.h"
#include "../../core/ip_addr.h"


struct app_context;
typedef struct app_context app_context;

typedef struct http2_msghdr
{
	str name;
	str value;
	struct http2_msghdr *next;
} http2_msghdr_t;

typedef struct http2_stream_data
{
	struct http2_stream_data *prev, *next;
	char *request_path;
	char *request_pathfull;
	char *request_method;
	http2_msghdr_t *hdrlist;
	str request_data;
	int32_t stream_id;
	int fd;
} http2_stream_data;

typedef struct http2_session_data
{
	struct http2_stream_data root;
	struct bufferevent *bev;
	app_context *app_ctx;
	nghttp2_session *session;
	char *client_addr;
} http2_session_data;

struct app_context
{
	SSL_CTX *ssl_ctx;
	struct event_base *evbase;
};

#define KSR_NGHTTP2_RPLHDRS_SIZE 16
typedef struct ksr_nghttp2_ctx
{
	nghttp2_session *session;
	http2_session_data *session_data;
	http2_stream_data *stream_data;
	nghttp2_nv rplhdrs_v[KSR_NGHTTP2_RPLHDRS_SIZE];
	int rplhdrs_n;
	str method;
	str path;
	str pathfull;
	str httpversion;
	str data;
	char srcipbuf[IP_ADDR_MAX_STR_SIZE];
	str srcip;
} ksr_nghttp2_ctx_t;

extern str _nghttp2_listen_port;
extern str _nghttp2_listen_addr;
extern str _nghttp2_tls_public_key;
extern str _nghttp2_tls_private_key;
extern int _nghttp2_server_pid;
extern ksr_nghttp2_ctx_t _ksr_nghttp2_ctx;

int nghttp2_server_run(void);
void ksr_event_route(void);
int ksr_nghttp2_send_response(nghttp2_session *session, int32_t stream_id,
		nghttp2_nv *nva, size_t nvlen, int fd);

#endif
