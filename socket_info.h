/* $Id$
 *
 * find & manage listen addresses 
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * along with this program; if not, write to" the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * This file contains code that initializes and handles ser listen addresses
 * lists (struct socket_info). It is used mainly on startup.
 * 
 * History:
 * --------
 *  2003-10-22  created by andrei
 */


#ifndef socket_info_h
#define socket_info_h

#include "ip_addr.h" 
#include "dprint.h"
/* struct socket_info is defined in ip_addr.h */

struct socket_info* udp_listen;
#ifdef USE_TCP
struct socket_info* tcp_listen;
#endif
#ifdef USE_TLS
struct socket_info* tls_listen;
#endif


int add_listen_iface(char* name, unsigned short port, unsigned short proto,
							enum si_flags flags);
int fix_all_socket_lists();
void print_all_socket_lists();
void print_aliases();


/* helper function:
 * returns next protocol, if the last one is reached return 0
 * usefull for cycling on the supported protocols */
static inline int next_proto(unsigned short proto)
{
	switch(proto){
		case PROTO_NONE:
			return PROTO_UDP;
		case PROTO_UDP:
#ifdef	USE_TCP
			return PROTO_TCP;
#else
			return 0;
#endif
#ifdef USE_TCP
		case PROTO_TCP:
#ifdef USE_TLS
			return PROTO_TLS;
#else
			return 0;
#endif
#endif
#ifdef USE_TLS
		case PROTO_TLS:
			return 0;
#endif
		default:
			LOG(L_ERR, "ERROR: next_proto: unknown proto %d\n", proto);
	}
	return 0;
}



/* gets first non-null socket_info structure
 * (usefull if for. e.g we are not listening on any udp sockets )
 */
inline static struct socket_info* get_first_socket()
{
	if (udp_listen) return udp_listen;
#ifdef USE_TCP
	else if (tcp_listen) return tcp_listen;
#ifdef USE_TLS
	else if (tls_listen) return tls_listen;
#endif
#endif
	return 0;
}



#endif
