/*
 * XMPP Module
 * This file is part of Kamailio, a free SIP server.
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 * Author: Andreea Spirea
 *
 */
/*! \file
 *  \brief Kamailio XMPP module
 *  \ingroup xmpp
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "../../sr_module.h"

int net_listen(char *server, int port)
{
	int fd;
	struct sockaddr_in sin;
	int on = 1;
	
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	
	if (!inet_aton(server, &sin.sin_addr)) {
		struct hostent *host;
		
		LM_DBG("resolving %s...\n", server);
		
		if (!(host = gethostbyname(server))) {
			LM_ERR("resolving %s failed (%s).\n", server,
					hstrerror(h_errno));
			return -1;
		}
		memcpy(&sin.sin_addr, host->h_addr_list[0], host->h_length);
	}
	
	if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		LM_ERR("socket() failed: %s\n", strerror(errno));
		return -1;
	}
	
	LM_DBG("listening on %s:%d\n", inet_ntoa(sin.sin_addr), port);
	
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		LM_WARN("setsockopt(SO_REUSEADDR) failed: %s\n",strerror(errno));
	}

	if (bind(fd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in)) < 0) {
		LM_ERR("bind() failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	if (listen(fd, 1) < 0) {
		LM_ERR("listen() failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	return fd;
}

int net_connect(char *server, int port)
{
	int fd;
	struct sockaddr_in sin;
	
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	
	if (!inet_aton(server, &sin.sin_addr)) {
		struct hostent *host;
		
		LM_DBG("resolving %s...\n", server);
		
		if (!(host = gethostbyname(server))) {
			LM_ERR("resolving %s failed (%s).\n", server,
					hstrerror(h_errno));
			return -1;
		}
		memcpy(&sin.sin_addr, host->h_addr_list[0], host->h_length);
	}
	
	if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		LM_ERR("socket() failed: %s\n", strerror(errno));
		return -1;
	}
	
	LM_DBG("connecting to %s:%d...\n", inet_ntoa(sin.sin_addr), port);
	
	if (connect(fd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in)) < 0) {
		LM_ERR("connect() failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	LM_DBG("connected to %s:%d...\n", inet_ntoa(sin.sin_addr), port);
	return fd;
}

int net_send(int fd, const char *buf, int len)
{
	const char *p = buf;
	int res;

	do {
		res = send(fd, p, len, 0);
		if (res <= 0)
			return res;
		len -= res;
		p += res;
	} while (len);

	return (p - buf);
}

int net_printf(int fd, char *format, ...)
{
	va_list args;
	char buf[4096];
	
	va_start(args, format);
	vsnprintf(buf, sizeof(buf) - 1, format, args);
	va_end(args);

	LM_DBG("net_printf: [%s]\n", buf);
	
	return net_send(fd, buf, strlen(buf));
}

char *net_read_static(int fd)
{
	static char buf[4096];
	int res;

	res = recv(fd, buf, sizeof(buf) - 1, 0);
	if (res < 0) {
		LM_ERR("recv() failed: %s\n", strerror(errno));
		return NULL;
	}
	if (!res)
		return NULL;
	buf[res] = 0;
	return buf;
}
