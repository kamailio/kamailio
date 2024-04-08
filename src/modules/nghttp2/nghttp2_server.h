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

typedef ptrdiff_t nghttp2_ssize;

typedef struct ksr_nghttp2_ctx
{
	//struct MHD_Connection *connection;
	void *connection;
	str method;
	str url;
	str httpversion;
	str data;
	//const union MHD_ConnectionInfo *cinfo;
	void *cinfo;
	char srcipbuf[IP_ADDR_MAX_STR_SIZE];
	str srcip;
} ksr_nghttp2_ctx_t;

extern str _nghttp2_listen_port;
extern str _nghttp2_listen_addr;
extern str _nghttp2_tls_public_key;
extern str _nghttp2_tls_private_key;
extern int _nghttp2_server_pid;

int nghttp2_server_run(void);

#endif
