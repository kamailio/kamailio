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

/** Kamailio core :: raw socket udp listen functions.
 *  @file raw_listener.c
 *  @ingroup core
 *  Module: @ref core
 */

#ifdef USE_RAW_SOCKS


#include "raw_listener.h"
#include "raw_sock.h"
#include "receive.h"

#include <errno.h>
#include <string.h>

struct socket_info* raw_udp_sendipv4=0;

/** creates a raw socket based on a socket_info structure.
 * Side-effects: sets raw_udp_sendipv4 if not already set.
 * @param si - pointer to partially filled socket_info structure (su must
 *              be set).
 * @param iface - pointer to network interface to bind on (str). Can be null.
 * @param iphdr_incl - 1 if send on these socket will include the IP header.
 * @return <0 on error, socket on success.
 */
int raw_listener_init(struct socket_info* si, str* iface, int iphdr_incl)
{
	int sock;
	struct ip_addr ip;
	
	su2ip_addr(&ip, &si->su);
	sock=raw_udp4_socket(&ip, iface, iphdr_incl);
	if (sock>=0){
		if (raw_udp_sendipv4==0 || iface==0 || iface->s==0)
			raw_udp_sendipv4=si;
	}
	return sock;
}



/** receive sip udp ipv4 packets over a raw socket in a loop.
 * It should be called by a "raw socket receiver" process
 * (since the function never exits unless it encounters a
 *  critical error).
 * @param rsock - initialized raw socket.
 * @param port1 - start of port range.
 * @param port2 - end of port range. If 0 it's equivalent to listening only
 *                on port1.
 * @return <0 on error, never returns on success.
 */
int raw_udp4_rcv_loop(int rsock, int port1, int port2)
{
	static char buf[BUF_SIZE+1];
	char* p;
	char* tmp;
	union sockaddr_union from;
	union sockaddr_union to;
	struct receive_info ri;
	struct raw_filter rf;
	int len;
	
	/* this will not change */
	from.sin.sin_family=AF_INET;
	ri.bind_address=0;
	ri.proto=PROTO_UDP;
	ri.proto_reserved1=0;
	ri.proto_reserved2=0;
	/* set filter to match any address but with the specified port range */
	memset(&rf, 0, sizeof(rf));
	rf.dst.ip.af=AF_INET;
	rf.dst.ip.len=4;
	rf.dst.mask.af=AF_INET;
	rf.dst.mask.len=4;
	rf.proto=PROTO_UDP;
	rf.port1=port1;
	rf.port2=port2?port2:port1;
	for(;;){
		p=buf;
		len=raw_udp4_recv(rsock, &p, BUF_SIZE, &from, &to, &rf);
		if (len<0){
			if (len==-1){
				LM_ERR("%s [%d]\n", strerror(errno), errno);
				if ((errno==EINTR)||(errno==EWOULDBLOCK))
					continue;
				else
					goto error;
			}else{
				LM_DBG("error len: %d\n", len);
				continue;
			}
		}
		/* we must 0-term the message */
		p[len]=0;
		ri.src_su=from;
		su2ip_addr(&ri.src_ip, &from);
		ri.src_port=su_getport(&from);
		su2ip_addr(&ri.dst_ip, &to);
		ri.dst_port=su_getport(&to);
		/* sanity checks */
		if (len<MIN_UDP_PACKET){
			tmp=ip_addr2a(&ri.src_ip);
			LM_DBG("probing packet received from %s %d\n",
					tmp, htons(ri.src_port));
			continue;
		}
		if (ri.src_port==0){
			tmp=ip_addr2a(&ri.src_ip);
			LM_INFO("dropping 0 port packet from %s\n", tmp);
			continue;
		}
		tmp=ip_addr2a(&ri.src_ip);
		LM_DBG("received from %s:\n[%.*s]\n", tmp, len, p);
		receive_msg(p, len, &ri);
	}
error:
	return -1;
}


#endif /* USE_RAW_SOCKS */
