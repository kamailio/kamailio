/*
 * Copyright (c) 2010 IPTEGO GmbH.
 * All rights reserved.
 *
 * This file is part of the BinRPC Library (libbinrpc).
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY IPTEGO GmbH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL IPTEGO GmbH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Bogdan Pintea.
 */

#include <sys/types.h> 
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <limits.h>
#ifdef HAVE_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdio.h>

#include "tls.h"
#include "mem.h"
#include "errnr.h"
#include "log.h"
#include "config.h"
#include "misc.h"
#include "net.h"


#ifndef AF_LOCAL
#define AF_LOCAL	AF_UNIX
#endif
#ifndef PF_LOCAL
#define PF_LOCAL	PF_UNIX
#endif
#ifndef SUN_LEN
# define SUN_LEN(ptr) ((size_t) (((struct sockaddr_un *) 0)->sun_path)	      \
		      + strlen ((ptr)->sun_path))
#endif /* SUN_LEN */


#define BRPC_SCHEME_PREF	"brpc"
#define BRPC_SCHEME_POSTF	"://"


__LOCAL bool resolve_inet(char *name, brpc_addr_t *addr)
{
	struct addrinfo hint, *result;
	int res;

	memset((char *)&hint, 0, sizeof(struct addrinfo));
	if (! name) /* TODO: possible? */
		hint.ai_flags |= AI_PASSIVE;
#ifdef HAVE_IPV4_MAPPED
	if (addr->domain == AF_INET6)
		hint.ai_flags |= AI_V4MAPPED;
#endif
#if defined(HAVE_GAI_ADDRCONFIG) && defined(WITH_GAI_ADDRCONFIG)
	hint.ai_flags |= AI_ADDRCONFIG;
#endif
	hint.ai_family = addr->domain;
	hint.ai_socktype = addr->socktype;

	if ((res = getaddrinfo(name, NULL, &hint, &result)) != 0) {
		WERRNO(ERESLV);
		ERR("failed to resolve name `%s': %s.\n", name, gai_strerror(res));
		return false;
	}
	assert(result->ai_socktype == addr->socktype);
	memcpy((char *)&addr->sockaddr, result->ai_addr, result->ai_addrlen);
	addr->domain = result->ai_family;
	addr->addrlen = result->ai_addrlen;

	freeaddrinfo(result);
	return true;
}

/**
 * brpcns://asta.la.vista.baby
 * brpcnd://1.2.3.4:5555
 * brpcns://[1.2.3.4]:5555
 * brpcls:///var/run/binrpc.usock
 * brpcld://~bpi/test.ldom
 */
brpc_addr_t *brpc_parse_uri(const char *_uri)
{
	static __brpc_tls brpc_addr_t addr;
	long port;
	char *uri;
	char *pos, *resume;
	size_t addrlen;
	char pres[INET6_ADDRSTRLEN];
	void *addrp;

	DBG("parsing `%s' [%zd] @%p\n", _uri, strlen(_uri), _uri);
	/* check against min-len. sane URI */
	if ((! _uri) || (strlen(_uri) < sizeof(BRPC_SCHEME_PREF) - 1
			+ /* addr type discr. */1 + /* sock type discr. */1
			+ sizeof(BRPC_SCHEME_POSTF) - 1
			+ /* at least one char as address */1)) {
		WERRNO(EINVAL);
		ERR("invalid URI `%s'.\n", _uri);
		return NULL;
	}

	uri = strdup(_uri);
	if (! uri) {
		WERRNO(ENOMEM);
		return NULL;
	}

	memset((char *)&addr, 0, sizeof(brpc_addr_t));

	pos = uri;
	if (strncasecmp(pos, BRPC_SCHEME_PREF, 
			sizeof(BRPC_SCHEME_PREF) - 1) != 0) {
		WERRNO(EINVAL);
		ERR("invalid scheme in URI `%s'.\n", _uri);
		goto error;
	}
	pos += sizeof(BRPC_SCHEME_PREF) - 1;

	/* get address family */
	switch (*pos | 0x20) {
		case 'n': addr.domain = AF_UNSPEC; break;
		case '4': addr.domain = PF_INET; break;
		case '6': addr.domain = PF_INET6; break;
		case 'l': addr.domain = PF_LOCAL; break;
		default:
			WERRNO(EINVAL);
			ERR("invalid address type selector '%c' in URI `%s'.\n", 
					*pos, _uri);
			goto error;
	}
	pos ++;

	/* get socket type */
	switch (*pos | 0x20) {
		case 's': addr.socktype = SOCK_STREAM; break;
		case 'd': addr.socktype = SOCK_DGRAM; break;
		default:
			WERRNO(EINVAL);
			ERR("invalid socket type selector '%c' in URI `%s'.\n", 
					*pos, _uri);
			goto error;
	}
	pos ++;
	pos += sizeof(BRPC_SCHEME_POSTF) - 1;

	if (addr.domain == PF_LOCAL) {
		addr.sockaddr.un.sun_family = AF_LOCAL;
		addrlen = strlen(pos) + /*0-term*/1;
		if (sizeof(addr.sockaddr.un.sun_path) < addrlen) {
			WERRNO(E2BIG);
			ERR("local domain address `%s' too long (maximum: %zd) in URI "
					"`%s'.\n", pos, sizeof(addr.sockaddr.un.sun_path), _uri);
			goto error;
		}
		memcpy(addr.sockaddr.un.sun_path, pos, addrlen);
		addr.addrlen = SUN_LEN(&addr.sockaddr.un);
		INFO("path in URI `%s' resolved to `%s'.\n", _uri, pos);
	} else {
		if (*pos == '[') {
			pos ++;
			if (! (resume = strchr(pos, ']'))) {
				WERRNO(EINVAL);
				ERR("invalid address in URI `%s' (missing ']').\n", _uri);
				goto error;
			} else {
				*resume = 0;
				resume ++; /* now pointing either to : or \0 */
				if (*resume) {
					if (*resume != ':') {
						WERRNO(EINVAL);
						ERR("invalid URI `%s' (unexpected char at `%s').\n",
								_uri, resume);
					}
					*resume = 0;
					resume ++;
				}
			}
		} else if ((resume = strchr(pos, ':'))) {
				if (strchr(resume + 1, ':')) /*IPv6 hex*/
					resume = NULL;
				else {
					*resume = 0; /* delimit hostname */
					resume ++;
				}
		}

		if (! resolve_inet(pos, &addr)) {
			WERRNO(EADDRNOTAVAIL);
			ERR("failed to resolve address `%s' in URI `%s'.\n", pos, _uri);
			goto error;
		}

		pos = resume;
		if (pos) {
			errno = 0;
			if (((port = strtol(pos, NULL, 10)) == 0) && errno) {
				WERRNO(EINVAL);
				ERR("failed to read port `%s' as integer in URI `%s'.\n", 
						pos, _uri);
				goto error;
			}
			DBG("string `%s' decoded as integer %ld.\n", pos, port);
			if ((1 << (8 * sizeof(short))) <= port) {
				WERRNO(EINVAL);
				ERR("invalid port number %u (to high) in URI `%s'.\n", 
						port, _uri);
				goto error;
			}
		} else {
			DBG("using default port %u.\n", BINRPC_PORT);
			port = BINRPC_PORT;
		}

		if (addr.domain == PF_INET) {
			addr.sockaddr.in4.sin_port = htons(port);
			addrp = (void *)&addr.sockaddr.in4.sin_addr;
		} else {
			addr.sockaddr.in6.sin6_port = htons(port);
			addrp = (void *)&addr.sockaddr.in6.sin6_addr;
		}

		if (! inet_ntop(addr.domain, addrp, pres, sizeof(pres)))
			WARN("failed to get presentation for address in URI `%s'.\n",
					_uri);
		else
			INFO("address in URI `%s' resolved to `%s'; port: %ld.\n", 
					_uri, pres, port);
	}

	free(uri);
	return &addr;
error:
	free(uri);
	return NULL;
}


char *brpc_print_addr(const brpc_addr_t *addr)
{
/* assume sun_path's larger than INET6_ADDRSTRLEN */
#define BUFF_SIZE	/* brpcXY:// */9 + /*[]*/2 + \
	sizeof(((struct sockaddr_un *)0)->sun_path) + /*:*/1 + \
	/*port presentation*/5 + /*0-term*/1
#define PROTO_POS	4
#define STYPE_POS	5
#define ADDR_OFFT	9
	static __brpc_tls char buff[BUFF_SIZE] = BRPC_URI_PREFIX;
	char *pos;

	switch (addr->domain) {
		case PF_LOCAL:
			buff[PROTO_POS] = 'l';
			memcpy(buff + ADDR_OFFT, addr->sockaddr.un.sun_path, 
					strlen(addr->sockaddr.un.sun_path) + /*0-term*/1);
			break;
		case PF_INET:
			buff[PROTO_POS] = '4';
			pos = buff + ADDR_OFFT;
			if (! inet_ntop(addr->domain, &addr->sockaddr.in4.sin_addr, pos,
					sizeof(buff) - ADDR_OFFT)) {
				WSYSERRNO;
				return NULL;
			}
			while (*pos)
				pos ++;
			snprintf(pos, /*:*/1 + /*short*/5 + /*0-term*/1, ":%d", 
					ntohs(addr->sockaddr.in4.sin_port));
			break;
		case PF_INET6:
			buff[PROTO_POS] = '6';
			pos = buff + ADDR_OFFT;
			*pos = '['; pos ++;
			if (! inet_ntop(addr->domain, &addr->sockaddr.in6.sin6_addr, pos, 
					sizeof(buff) - ADDR_OFFT)) {
				WSYSERRNO;
				return NULL;
			}
			while (*pos)
				pos ++;
			snprintf(pos, /*]*/1 + /*:*/1 +/*short*/5 + /*0-term*/1, "]:%d", 
					ntohs(addr->sockaddr.in6.sin6_port));
			break;

		default:
			BUG("unsupported value as protocol specifier: %d.\n", 
					addr->domain);
			WERRNO(EINVAL);
			return NULL;
	}
	switch (addr->socktype) {
		case SOCK_STREAM: buff[STYPE_POS] = 's'; break;
		case SOCK_DGRAM: buff[STYPE_POS] = 'd'; break;
		default:
			BUG("unsupported value as socket type specifier: %d.\n",
					addr->socktype);
			WERRNO(EINVAL);
			return NULL;
	}

	return buff;

#undef BUFF_SIZE
#undef PROTO_POS
#undef STYPE_POS
#undef ADDR_OFFT
}

bool brpc_addr_eq(const brpc_addr_t *a, const brpc_addr_t *b)
{
	if (a->domain != b->domain)
		return false;
	if (a->socktype != b->socktype)
		return false;
	if (memcmp((char *)&a->sockaddr, (char *)&b->sockaddr, 
			sizeof(a->sockaddr)))
		return false;
	return true;
}

__LOCAL bool setsockmode(int sockfd, bool blocking, bool *_prev)
{
	int optval;
	bool prev;

	if ((optval = fcntl(sockfd, F_GETFL, 0)) < 0) {
		WSYSERRNO;
		return false;
	}
	prev = ! (optval & O_NONBLOCK);
	if (blocking == prev)
		/* nothing to do: mode already as desired */
		goto done;
	if (! blocking)
		optval |= O_NONBLOCK;
	else
		optval &= ~O_NONBLOCK;
	if (fcntl(sockfd, F_SETFL, optval) == -1) {
		WSYSERRNO;
		return false;
	}
done:
	if (_prev)
		*_prev = prev;
	return true;
}

__LOCAL bool setsocktos(int sockfd)
{
	int optval;
	/* TODO: does it matter _IP/_IP6? */
	optval = IPTOS_LOWDELAY|IPTOS_THROUGHPUT|IPTOS_RELIABILITY;
	if (setsockopt(sockfd, IPPROTO_IP, IP_TOS, &optval, sizeof(optval)) == -1){
		WSYSERRNO;
		return false;
	}
	return true;
}

__LOCAL bool disable_nagle(int sockfd)
{
	int optval = 1;
	if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, 
			&optval, sizeof(optval)) == -1) {
		WSYSERRNO;
		return false;
	}
	return true;
}

int brpc_socket(brpc_addr_t *addr, bool blocking, bool named)
{
	int sockfd;
	int optval;

	sockfd = socket(addr->domain, addr->socktype, /*proto: kern choice */0);
	if (sockfd < 0) {
		WSYSERRNO;
		return sockfd;
	}

	if (! setsockmode(sockfd, blocking, NULL))
		goto error;


	optval = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, 
			sizeof(optval)) != 0) {
		WSYSERRNO;
		goto error;
	}
	
	switch (addr->domain) {
		case AF_INET:
		case AF_INET6:
			if (! setsocktos(sockfd))
				WARN("failed to set TOS.\n");
	}

	if (named) {
		/* remove any previous socket file */
		if (addr->domain == PF_LOCAL) {
			if(unlink(addr->sockaddr.un.sun_path) < 0) {
				switch (errno) {
					case ENOENT:
					case ENOTDIR: /* allow failure at right place (bind) */
						break;
					default:
						WSYSERRNO;
						goto error;
				}
			}
		}

		if (bind(sockfd, (struct sockaddr *)&addr->sockaddr, 
				addr->addrlen) != 0) {
			WSYSERRNO;
			goto error;
		}
	}

	return sockfd;
error:
	close(sockfd);
	return -1;
}

bool brpc_connect(brpc_addr_t *addr, int *_sockfd, brpc_tv_t tout)
{
	int sockfd;
	fd_set rset, wset;
	struct timeval tv;
	int res, err;
	socklen_t elen;
	bool was_blocking;

	if ((*_sockfd < 0) && ((*_sockfd = brpc_socket(addr, false, false)) < 0))
		return false;
	sockfd = *_sockfd;

	/* set nonblocking so that it can timeout */
	if (! setsockmode(sockfd, false, &was_blocking))
		goto error;
	switch (addr->domain) {
		case AF_INET:
		case AF_INET6:
			if (! setsocktos(sockfd))
				WARN("failed to set TOS.\n");
	}
#ifndef DISABLE_NAGLE
	/* NAGLE's good in this case: save one packet (ACK), locally, and read
	 * more in one syscall, remotely */
	if ((addr->domain == PF_INET) && (addr->socktype == SOCK_STREAM))
		if (disable_nagle(sockfd))
			WARN("failed to disable Nagle for socket [%d:%d] (%s).\n",
					addr->domain, addr->socktype, brpc_strerror());
#endif

	errno = 0;
	if (connect(sockfd, (struct sockaddr *)&addr->sockaddr, 
			addr->addrlen) == 0)
		return sockfd;
	else if (errno != EINPROGRESS) {
		WSYSERRNO;
		goto error;
	}

	FD_ZERO(&rset);
	FD_SET(sockfd, &rset);
	wset = rset;

	if (tout) {
		tv.tv_sec = tout / 1000000LU;
		tv.tv_usec = tout - (tv.tv_sec * 1000000LU);
	}

	if ((res = select(sockfd + 1, &rset, &wset, NULL, 
			tout ? &tv : NULL)) < 0) {
		WSYSERRNO;
		goto error;
	} else if (res == 0) {
		WERRNO(ETIMEDOUT);
		goto error;
	}

	if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset)) {
		elen = sizeof(err);
		if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &elen) < 0)
			goto error;

		if (err) {
			WERRNO(err);
			goto error;
		}
	} else {
		BUG("select returned %d for one descriptor, timeout not signaled,"
				" but no file descriptor set; socket: %d.\n", res, sockfd);
		abort();
	}
	
	/* reinstate previous blocking mode */
	if (! setsockmode(sockfd, was_blocking, NULL))
		goto error;
	
	return true;
error:
	close(sockfd);
	*_sockfd = -1;
	return false;
}

/**
 * Error codes:
 * 	EINPROGRESS : timeout occured while something had been sent.
 * 	ETIMEDOUT : timeout occured but nothing sent.
 * 	EMSGSIZE : message to large, but nothing sent.
 * 	rest are 'socket-fatal'.
 */
bool brpc_sendto(int sockfd, brpc_addr_t *dest, brpc_t *msg, brpc_tv_t tout)
{
	uint8_t *pos;
	ssize_t offt;
	size_t still;
	const brpc_bin_t *buff;
	fd_set wset;
	brpc_tv_t prev, now;
	struct timeval tv;
	ssize_t sent;
	struct sockaddr *saddr;
	socklen_t saddrlen;

	if (dest) {
		DBG("sending to %s.\n", brpc_print_addr(dest));
		saddr = (struct sockaddr *)&dest->sockaddr;
		saddrlen = dest->addrlen;
	} else {
		saddr = NULL;
		saddrlen = 0;
	}

	if (! (buff = brpc_serialize(msg)))
		return false;

	pos = buff->val;
	still = buff->len;
	offt = 0;

	prev = tout ? brpc_now() : 0;
	do {
		DBG("sending through FD#%d; to send: %zd.\n", sockfd, still);
		if ((sent = sendto(sockfd, pos, still, MSG_DONTWAIT
#ifdef HAVE_MSG_NOSIGNAL
				|MSG_NOSIGNAL
#endif
				, saddr, saddrlen)) <= 0) {
			switch (errno) {
				case 0:
					/* should never happen (don't send empty datagrams) */
					BUG("sendto() returned %zd, but no error signaled.\n", 
							sent);
					assert(still);
#ifndef NDEBUG
					abort();
#endif
						
					return false;

				/* (potentially) transient errors */
				case EINTR: /* signal delivered */
				case ENOBUFS:
				case ENOMEM:
				case ENETDOWN:
				case ENETUNREACH:
					continue;

				case EMSGSIZE:
					if (offt)
						/* should never happen */
						WERRNO(EINPROGRESS);
					else
						WSYSERRNO;
					return false;

				default:
					WSYSERRNO;
					return false;

				case EAGAIN:
#if EAGAIN != EWOULDBLOCK
				case EWOULDBLOCK:
#endif
					FD_ZERO(&wset);
					FD_SET(sockfd, &wset);
					if (prev) {
						now = brpc_now();
						tout -= now - prev;
						prev = now;
						tv.tv_sec = tout / 1000000LU;
						tv.tv_usec = tout - (tv.tv_sec * 1000000LU);
					}
#ifdef NET_DEBUG
					DBG("select timer armed (tout: %lu).\n", tout);
#endif
					switch (select(sockfd + 1, NULL, &wset, NULL, 
							prev ? &tv : NULL)) {
						case 0:
							if (offt)
								WERRNO(EINPROGRESS);
							else
								WERRNO(ETIMEDOUT);
							return false;
						case 1:
							break;
						default:
							switch (errno) {
								case EINTR: /* signal delivered */
									continue;
								default:
									/* EBADF, EINVAL */
									WSYSERRNO;
									return false;
							}
					}
				break;
			}
		} else {
			pos += sent;
			still -= sent;
		}
	} while (still);

	DBG("full message buffer sent.\n");
	return true;
}


/**
 * Error codes:
 * 	EMSGSIZE: something was read (header) but buffer would not accomodate 
 * 	message.
 */
brpc_t *brpc_recvfrom(int sockfd, brpc_addr_t *src, brpc_tv_t tout)
{
	/* can not use larger static as it could be called by threads */
	/* TODO: if no multithreaded => static, larger */
	uint8_t buff[BINRPC_MAX_PKT_LEN], *pos;
	ssize_t rcvd;
	brpc_tv_t now, prev;
	ssize_t offt;
	size_t msglen, buff_need;
	ssize_t have_len /*flag*/;
	struct timeval tv;
	fd_set rset;
	struct sockaddr *saddr;
	socklen_t *addrlen;
	int socktype;
	socklen_t optlen;
	brpc_t *msg;

	if (src) {
		saddr = (struct sockaddr *)&src->sockaddr;
		addrlen = &src->addrlen;
		socktype = BRPC_ADDR_TYPE(src);
		switch (socktype) {
			case PF_LOCAL: *addrlen = sizeof(struct sockaddr_un);
			case PF_INET: *addrlen = sizeof(struct sockaddr_in);
			case PF_INET6: *addrlen = sizeof(struct sockaddr_in6);
		}
	} else {
		saddr = NULL;
		addrlen = NULL;
		optlen = sizeof(socktype);
		if (getsockopt(sockfd, SOL_SOCKET, SO_TYPE, &socktype, 
				&optlen) < 0) {
			WSYSERRNO;
			return NULL;
		}
	}

	/* on streams we can only read up to current message, while we have to
	 * engulf the whole datagram from one shot */
	msglen = (socktype == SOCK_DGRAM) ? BINRPC_MAX_PKT_LEN : MIN_PKT_LEN;
	offt = 0;
	have_len = -1;
	pos = buff;
	prev = tout ? brpc_now() : 0;
	do {
#ifdef NET_DEBUG
		DBG("receiving through FD#%d; rcvd: %zd; room for: %zd.\n", sockfd, 
				offt, msglen - offt);
#endif
		/* first do receive and only then, if needed, select(); this ensures
		 * that if data to read exist, no un-necessary syscalls are invoked  */
		if (0 < (rcvd = recvfrom(sockfd, pos, msglen - offt, MSG_DONTWAIT
#ifdef HAVE_MSG_NOSIGNAL
				|MSG_NOSIGNAL
#endif
				, saddr, addrlen))) {
			pos += rcvd;
			offt += rcvd;

			if (have_len < 0) {
				have_len = brpc_pkt_len(buff, offt);
				if (have_len < 0) { /* need to read more from header */
					if (socktype == SOCK_DGRAM)
						goto errdgram;
					buff_need = offt + (-have_len);
					if (msglen < buff_need)
						msglen = buff_need;
				} else if (BINRPC_MAX_PKT_LEN < have_len) {
					/* TODO: try to read all the packet if still have time,
					 * so that the socket remains usable */
					WERRNO(EMSGSIZE);
					return NULL;
				} else {
					msglen = have_len;
					if ((socktype == SOCK_DGRAM) && (msglen != rcvd))
						goto errdgram;
				}
			}
		} else {
			switch (errno) {
				case 0:
					if (rcvd == 0) { /* conn closed */
						WERRNO(ECONNRESET);
						return NULL;
					}
					/* should never happen */
					BUG("recvfrom() returned %zd, but no error signaled.\n", 
							rcvd);
#ifndef NDEBUG
					abort();
#endif
					return NULL;
				case ENOBUFS:
				case ENOMEM: /* TODO: does this realy make sense? */
				case EINTR:
					break;
				default:
					WSYSERRNO;
					return NULL;

				case EAGAIN:
#if EAGAIN != EWOULDBLOCK
				case EWOULDBLOCK:
#endif
eagain:
					FD_ZERO(&rset);
					FD_SET(sockfd, &rset);
					if (prev) {
						now = brpc_now();
						tout -= now - prev;
						prev = now;
						tv.tv_sec = tout / 1000000LU;
						tv.tv_usec = tout - (tv.tv_sec * 1000000LU);
					}
#ifdef NET_DEBUG
					DBG("select timer armed (tout: %lu).\n", tout);
#endif
					switch (select(sockfd + 1, &rset, NULL, NULL, 
							prev ? &tv : NULL)) {
						case 0:
							if (offt)
								WERRNO(EINPROGRESS);
							else
								WERRNO(ETIMEDOUT);
							return NULL;
						case 1:
							assert(FD_ISSET(sockfd, &rset));
							break;
						default:
							switch (errno) {
								case EINTR:
									goto eagain;
								default:
									WSYSERRNO;
									return NULL;
							}
					}
					break;
			}

			errno = 0;
			continue; /* the condition below will fail for sure */
		}
	} while (offt < msglen);

	pos = (uint8_t *)brpc_malloc(msglen * sizeof(uint8_t));
	if (! pos) {
		WERRNO(ENOMEM);
		return NULL;
	}
	memcpy(pos, buff, msglen);
	msg = brpc_raw(pos, msglen);
	if (! msg) {
		brpc_free(pos);
		return NULL;
	}
	return msg;

errdgram:
	ERR("failed to read complete message in datagram.\n");
	WERRNO(EMSGSIZE);
	return NULL;
}

/**
 * Read available data from peer.
 * @param state BINRPC read state.
 * @return State of operation: true - something was read; system error, if
 * error occurs.
 */
bool brpc_strd_read(brpc_strd_t *state)
{
	ssize_t rcvd;

restart:
	rcvd = read(state->fd, state->buff + state->offset, 
			sizeof(state->buff) - state->offset);
	DBG("r_reading: offset: %zd, room for: %zd; read: %zd.\n", state->offset, 
			sizeof(state->buff) - state->offset, rcvd);
	
	if (0 < rcvd) {
		state->offset += rcvd;
		return true;
	}
	
	if (rcvd == 0) {
		DBG("connection shut down while reading descriptor %d.\n", state->fd);
	} else {
		if (errno == EINTR)
			goto restart;
		WSYSERRNO;
		DBG("read (transient?) error (%d: %s) while reading descriptor %d.\n", 
				brpc_errno, brpc_strerror(), state->fd);
	}
	return false;
}

/**
 * Get buffer holding a BINRPC packet.
 * @param state BINRPC read state
 * @param len Output parameter: lenght of packet
 * @return Packet buffer; NULL on error; EMSGSIZE may be set if packet to
 * large.
 */
uint8_t *brpc_strd_wirepkt(brpc_strd_t *state, size_t *len)
{
	uint8_t *pktbuf;

	if (state->pkt_len < 0) {
		state->pkt_len = brpc_pkt_len(state->buff, state->offset);
		if (state->pkt_len < 0)
			return NULL; /* complete header not yet ready */
		else if (BINRPC_MAX_PKT_LEN < state->pkt_len) {
			/* TODO: try recover (=skip pkt_len bytes)?? */
			ERR("packet too large: %zd while max supported is %zd.\n",
					state->pkt_len, BINRPC_MAX_PKT_LEN);
			WERRNO(EMSGSIZE);
			return NULL;
		}
	}
	if (state->offset < state->pkt_len)
		return NULL; /* complete packet not yet read */
	
	if (! (pktbuf = (uint8_t *)brpc_malloc(state->pkt_len * sizeof(uint8_t)))){
		WERRNO(ENOMEM);
		return NULL;
	}
	*len = state->pkt_len;
	memcpy(pktbuf, state->buff, state->pkt_len);
	/* fix state: recycle buffer space, fix offset & pkt length */
	state->offset -= state->pkt_len;
	memcpy(state->buff, state->buff + state->pkt_len, state->offset);
	state->pkt_len = -MIN_PKT_LEN;
	return pktbuf;
}
