/*
 * Copyright (C) 2006 iptelorg GmbH
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
 */

#ifndef _init_socks_h
#define _init_socks_h
#include <sys/types.h>
#include <sys/un.h>
#include "../../ip_addr.h"

enum socket_protos	{	UNKNOWN_SOCK=0, UDP_SOCK, TCP_SOCK, 
						UNIXS_SOCK, UNIXD_SOCK
#ifdef USE_FIFO
							, FIFO_SOCK
#endif
};

int init_unix_sock(struct sockaddr_un* su, char* name, int type,
					int perm, int uid, int gid);
int init_tcpudp_sock(union sockaddr_union* su, char* address, int port,
					enum socket_protos type);
int init_sock_opt(int s, enum socket_protos type);

inline static char* socket_proto_name(enum socket_protos p)
{
	switch(p){
		case UDP_SOCK:
			return "udp";
		case TCP_SOCK:
			return "tcp";
		case UNIXS_SOCK:
			return "unix_stream";
		case UNIXD_SOCK:
			return "unix_dgram";
#ifdef USE_FIFO
		case FIFO_SOCK:
			return "fifo";
#endif
		default:
			;
	}
	return "<unknown>";
}
#endif
