/*
 * $Id: init_socks.h,v 1.1 2006/02/23 19:57:31 andrei Exp $
 *
 * Copyright (C) 2006 iptelorg GmbH
 * Copyright (C) 2007 iptego GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/* History:
 * --------
 *  2006-02-14  created by andrei
 */

#ifndef _init_socks_h
#define _init_socks_h
#include <sys/un.h>
#include "../../ip_addr.h"

#include <binrpc.h>

#if 0
enum socket_protos	{	UNKNOWN_SOCK=0, UDP_SOCK, TCP_SOCK, 
						UNIXS_SOCK, UNIXD_SOCK
#ifdef USE_FIFO
							, FIFO_SOCK
#endif
};
#endif

#ifndef PF_MAX
#define PF_MAX	0xF1F0
#endif

#define PF_FIFO	(PF_MAX + 1)


int init_unix_sock(brpc_addr_t* addr, int perm, int uid, int gid);
int init_tcpudp_sock(brpc_addr_t *addr);
int init_sock_opt(int s, sa_family_t domain, int socktype);


inline static char* socket_proto_name(brpc_addr_t *addr)
{
	switch (addr->domain) {
		case PF_LOCAL:
			return (addr->socktype == SOCK_STREAM) ? "unix_stream" : 
					"unix_datagram";
		case PF_INET:
			return (addr->socktype == SOCK_STREAM) ? "IPv4_TCP" : "IPv4_UDP";
		case PF_INET6:
			return (addr->socktype == SOCK_STREAM) ? "IPv6_TCP" : "IPv6_UDP";
#ifdef USE_FIFO
		case PF_FIFO:
			return "fifo";
#endif
	}
	BUG("unknown address type %d:%d.\n", addr->domain, addr->socktype);
	return "<unknown>";
}

#endif
