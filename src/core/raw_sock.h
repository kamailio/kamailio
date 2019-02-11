/*
 * Copyright (C) 2010 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/** Kamailio core :: raw socket functions.
 *  @file raw_sock.c
 *  @ingroup core
 *  @author andrei
 *  Module: @ref core
 */

#ifndef _raw_sock_h
#define _raw_sock_h

#include "ip_addr.h"

/** filter for limiting packets received on raw sockets. */
struct raw_filter{
	struct net   dst;
	unsigned short port1;
	unsigned short port2;
	char proto;
};

extern int raw_ipip;

int raw_socket(int proto, struct ip_addr* ip, str* iface, int iphdr_incl);
int raw_udp4_socket(struct ip_addr* ip, str* iface, int iphdr_incl);
int recvpkt4(int sock, char* buf, int len, union sockaddr_union* from,
					union sockaddr_union* to);
int raw_udp4_recv(int rsock, char** buf, int len, union sockaddr_union* from,
					union sockaddr_union* to, struct raw_filter* rf);
int raw_udp4_send(int rsock, char* buf, unsigned int len,
					union sockaddr_union* from,
					union sockaddr_union* to);
int raw_iphdr_udp4_send(int rsock, char* buf, unsigned int len,
						union sockaddr_union* from,
						union sockaddr_union* to, unsigned short mtu);

#endif /* _raw_sock_h */
