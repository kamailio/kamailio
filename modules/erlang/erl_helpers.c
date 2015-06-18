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

#include "erl_helpers.h"
#include "mod_erlang.h"

#include "../../resolve.h"
#include "../../ip_addr.h"
#include "../../str.h"

#include <netinet/ip.h> /*IPTOS_LOWDELAY*/

/* fall back if Kamailio host name is not given */
char thishostname[512] = {0};
struct sockaddr_in *thisaddr = NULL;

int erl_set_nonblock(int sockfd)
{
	int flags;

	flags = fcntl(sockfd, F_GETFD);

	if (flags == -1) {
		LM_ERR("socket %d read settings error: %s\n",sockfd,strerror(errno));
	} else if (fcntl(sockfd, F_SETFD, flags|O_NONBLOCK) == -1) {
		LM_ERR("socket %d set O_NONBLOCK failed: %s\n",sockfd,strerror(errno));
	} else {
		return 0;
	}

	return -1;
}

void erl_close_socket(int sockfd)
{
	if (sockfd > 0) {
		shutdown(sockfd, SHUT_RDWR);
		close(sockfd);
	}
}

/** \brief allocate & bind a server socket using TCP.
 *
 * 	Allocate & bind a server socket using TCP.
 *
 */
int erl_passive_socket(const char *hostname, int qlen,
		struct addrinfo **ai_ret)
{
	int sockfd; /* socket descriptor and socket type	*/
	int on = 1;
	int error_num = 0;
	int port;
	struct addrinfo *ai;
	struct addrinfo hints;
	struct ip_addr ip;
	socklen_t addrlen = sizeof(struct sockaddr);

	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_UNSPEC;
	/* = AF_INET;  IPv4 address family */
	/* = AF_UNSPEC; Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_V4MAPPED;
	hints.ai_protocol = IPPROTO_TCP;

	if ((error_num = getaddrinfo(hostname, 0 /* unused */, &hints, &ai)))
	{
		LM_CRIT("failed to resolve %s: %s\n", hostname, gai_strerror(error_num));
		return -1;
	}

	/* Allocate a socket */
	sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (sockfd < 0)
	{
		LM_CRIT("failed to create socket. %s.\n", strerror(errno));
		freeaddrinfo(ai);
		return -1;
	}

	/* initialize TCP */
#if  !defined(TCP_DONT_REUSEADDR)
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int))) {
		LM_ERR("failed to enable SO_REUSEADDR for socket on %s %s, %s\n",
				hostname, ip_addr2strz(&ip), strerror(errno));
	}
#endif
	/* tos */
	on=IPTOS_LOWDELAY;
	if (setsockopt(sockfd, IPPROTO_IP, IP_TOS, (void*)&on,sizeof(on)) ==-1) {
			LM_WARN("setsockopt tos: %s\n", strerror(errno));
			/* continue since this is not critical */
	}

	/* Bind the socket */
	if (bind(sockfd, ai->ai_addr, ai->ai_addrlen) < 0)
	{
		port=sockaddr_port(ai->ai_addr);
		LM_CRIT("failed to bind socket on %s %s:%u. %s.\n", hostname,
				ip_addr2strz(&ip), port, strerror(errno));

		erl_close_socket(sockfd);
		freeaddrinfo(ai);
		return -1;
	}

	if (ai->ai_socktype == SOCK_STREAM && listen(sockfd, qlen) < 0)
	{
		LM_CRIT("failed to listen socket on %s, %s. %s.\n", hostname,
				ip_addr2strz(&ip), strerror(errno));

		erl_close_socket(sockfd);
		freeaddrinfo(ai);

		return -1;
	}

	/* get addr on socket */
	if (getsockname(sockfd, ai->ai_addr, &addrlen)){
		LM_ERR("getsockname failed: %s\n",strerror(errno));
	}

	if (ai_ret && *ai_ret == NULL)
	{
		*ai_ret = ai;
	}
	else if (ai_ret)
	{
		freeaddrinfo(*ai_ret);
		*ai_ret = ai;
	}
	else
	{
		freeaddrinfo(ai);
	}

	return sockfd;
}

/** @brief allocate active socket using TCP.
 *
 * 	Allocate active client socket using TCP.
 *
 */
int erl_active_socket(const char *hostname, int qlen, struct addrinfo **ai_ret)
{
	int error_num = 0;
	struct addrinfo *ai;
	struct addrinfo hints;

	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_UNSPEC;
	/* = AF_INET;  IPv4 address family */
	/* = AF_UNSPEC; Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_V4MAPPED;
	hints.ai_protocol = IPPROTO_TCP;

	if ((error_num = getaddrinfo(hostname, 0 /* unused */, &hints, &ai)))
	{
		LM_CRIT("failed to resolve %s: %s\n", hostname, gai_strerror(error_num));
		return -1;
	}

	if (ai_ret && *ai_ret == NULL)
	{
		*ai_ret = ai;
	}
	else if (ai_ret)
	{
		freeaddrinfo(*ai_ret);
		*ai_ret = ai;
	}
	else
	{
		freeaddrinfo(ai);
	}

	return 0;
}

/*
 * Call before any erl_interface library function.
 */
void erl_init_common()
{
	/* distribution trace level */
	if (trace_level)
	{
		ei_set_tracelevel(trace_level);
	}
}

/*
 * Free allocated memory for common patterns.
 */
void erl_free_common()
{
}

/**
 * @brief Initialize Erlang connection
 */
int erl_init_ec(ei_cnode *ec, const str *alivename, const str *hostname, const str *cookie)
{
	/* identifies a specific instance of a C node. */
	short creation = getpid();
	struct addrinfo *ai = NULL;
	struct sockaddr *addr = NULL;
	ip_addr_t ip;

	char nodename[MAXNODELEN];

	int result;
	int port;

	/* copy the nodename into something we can modify */
	if (snprintf(nodename, MAXNODELEN, "%.*s@%.*s", STR_FMT(alivename), STR_FMT(hostname)) >= MAXNODELEN) {
		LM_CRIT("the node name %.*s@%.*s is too large max length allowed is %d\n", STR_FMT(alivename), STR_FMT(hostname), MAXNODELEN-1);
		return -1;
	}

	if (erl_active_socket(hostname->s, 128, &ai)) {
		return -1;
	}

	addr = (struct sockaddr*) ai->ai_addr;
	sockaddr2ip_addr(&ip,addr);

	if ((result = ei_connect_xinit(ec, hostname->s, alivename->s, nodename, (Erl_IpAddr) &(((struct sockaddr_in*)ai->ai_addr)->sin_addr), cookie->s, creation)) < 0) {

		LM_CRIT("failed to initialize self as cnode name %s\n", nodename);
		return -1;
	}

	port = sockaddr_port(addr);

	LM_DBG("initialized ec for cnode '%s' on %.*s[%s] creation %d.\n", nodename, STR_FMT(hostname), ip_addr2strz(&ip), creation);

	freeaddrinfo(ai);
	return 0;
}
/**
 * \brief Initialize C node server and returns listen socket.
 */
int erl_init_node(ei_cnode *ec, const str *alivename, const str *hostname, const str *cookie)
{
	/* identifies a specific instance of a C node. */
	short creation = getpid();
	struct addrinfo *ai = NULL;
	struct sockaddr *addr = NULL;
	ip_addr_t ip;

	char nodename[MAXNODELEN];

	int result;
	int listen_fd;
	int port;

	unsigned timeout_ms = CONNECT_TIMEOUT;
	int epmdfd;

	/* copy the nodename into something we can modify */
	if (snprintf(nodename, MAXNODELEN, "%.*s@%.*s", STR_FMT(alivename), STR_FMT(hostname)) >= MAXNODELEN) {
		LM_CRIT("the node name %.*s@%.*s is too large max length allowed is %d\n", STR_FMT(alivename), STR_FMT(hostname), MAXNODELEN-1);
		return -1;
	}

	if ((listen_fd = erl_passive_socket(hostname->s, 128, &ai)) == -1) {
		return -1;
	}

	/* use first ip address only, it's internal Erlang connection to empd */
	addr = (struct sockaddr*) ai->ai_addr;

	if ((result = ei_connect_xinit(ec, hostname->s, alivename->s, nodename, (Erl_IpAddr) &(((struct sockaddr_in*)ai->ai_addr)->sin_addr), cookie->s, creation)) < 0) {

		LM_CRIT("failed to initialize self as node %s\n", nodename);
		erl_close_socket(listen_fd);
		return -1;
	}

	port = sockaddr_port(addr);
	sockaddr2ip_addr(&ip, addr);

	/* publish */
	if ((epmdfd = ei_publish_tmo(ec, port, timeout_ms)) == -1) {

		LM_ERR("Failed to publish port %u to epmd, check is epmd started\n", port);

		erl_close_socket(listen_fd);
		return -1;
	} else {
		LM_DBG("listen on %s:%u[%u]/[%d] as %s\n",ip_addr2strz(&ip),port,listen_fd,epmdfd,nodename);
	}

	freeaddrinfo(ai);
	return listen_fd;
}

void io_handler_ins(handler_common_t* phandler)
{
	if (io_handlers) {
		io_handlers->prev = phandler;
		phandler->next = io_handlers;
	} else {
		phandler->next = NULL;
	}

	phandler->prev = NULL;
	io_handlers = phandler;
}

void io_handler_del(handler_common_t* phandler)
{
	handler_common_t* p = phandler;

	if (p == io_handlers) {
		io_handlers = phandler->next;
	} else {
		phandler->prev->next = phandler->next;
	}

	if(phandler->destroy_f) phandler->destroy_f(phandler);

	pkg_free((void*)phandler);
}

void io_handlers_delete()
{
	handler_common_t* p;

	while(io_handlers){
		p = io_handlers;
		io_handlers = io_handlers->next;
		pkg_free((void*)p);
	}
}

/**
 * Decode string/binary into destination buffer.
 *
 */
int ei_decode_strorbin(char *buf, int *index, int maxlen, char *dst)
{
	int type, size, res;
	long len;

	ei_get_type(buf, index, &type, &size);

	if (type == ERL_NIL_EXT || size == 0)
	{
		dst[0] = '\0';
		return 0;
	}

	if (type != ERL_STRING_EXT && type != ERL_BINARY_EXT)
	{
		return -1;
	}
	else if (size > maxlen)
	{
		LM_ERR("buffer size %d too small for %s with size %d\n",
				maxlen, type == ERL_BINARY_EXT ? "binary" : "string", size);
		return -1;
	}
	else if (type == ERL_BINARY_EXT)
	{
		res = ei_decode_binary(buf, index, dst, &len);
		dst[len] = '\0';
	}
	else
	{
		res = ei_decode_string(buf, index, dst);
	}

	return res;
}
