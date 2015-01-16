/*
 * find & manage listen addresses 
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * along with this program; if not, write to" the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*! \file
 * \brief Kamailio core :: listen address
 * This file contains code that initializes and handles Kamailio listen addresses
 * lists (struct socket_info). It is used mainly on startup.
 * 
 */


#ifndef socket_info_h
#define socket_info_h

#include "ip_addr.h" 
#include "dprint.h"
#include "globals.h"

/* This macro evaluates to the maximum length of string buffer needed to print
 * the text description of any socket, not counting the terminating zero added
 * by socket2str */
#define MAX_SOCKET_STR (sizeof("unknown") - 1 + IP_ADDR_MAX_STR_SIZE + \
	INT2STR_MAX_LEN + 2 + 2)

int socket2str(char* s, int* len, struct socket_info* si);
int socketinfo2str(char* s, int* len, struct socket_info* si, int mode);


/* struct socket_info is defined in ip_addr.h */

extern struct socket_info* udp_listen;
#ifdef USE_TCP
extern struct socket_info* tcp_listen;
#endif
#ifdef USE_TLS
extern struct socket_info* tls_listen;
#endif
#ifdef USE_SCTP
extern struct socket_info* sctp_listen;
#endif

extern enum sip_protos nxt_proto[PROTO_LAST+1];



/* flags for finding out the address types */
#define SOCKET_T_IPV4  1
#define SOCKET_T_IPV6  2
#define SOCKET_T_UDP   4
#define SOCKET_T_TCP   8
#define SOCKET_T_TLS  16
#define SOCKET_T_SCTP 32

extern int socket_types;

void init_proto_order(void);

int add_listen_iface(char* name, struct name_lst* nlst,
						unsigned short port, unsigned short proto,
						enum si_flags flags);
int add_listen_advertise_iface(char* name, struct name_lst* nlst,
						unsigned short port, unsigned short proto,
						char *useaddr, unsigned short useport,
						enum si_flags flags);
int fix_all_socket_lists(void);
void print_all_socket_lists(void);
void print_aliases(void);

struct socket_info* grep_sock_info(str* host, unsigned short port,
										unsigned short proto);
struct socket_info* grep_sock_info_by_port(unsigned short port,
											unsigned short proto);
struct socket_info* find_si(struct ip_addr* ip, unsigned short port,
												unsigned short proto);

struct socket_info** get_sock_info_list(unsigned short proto);

int parse_phostport(char* s, char** host, int* hlen,
								 int* port, int* proto);

int parse_proto(unsigned char* s, long len, int* proto);

char* get_valid_proto_name(unsigned short proto);

/* helper function:
 * returns next protocol, if the last one is reached return 0
 * useful for cycling on the supported protocols
 * order: udp, tcp, tls, sctp */
static inline int next_proto(unsigned short proto)
{
	if (proto>PROTO_LAST)
		LM_ERR("unknown proto %d\n", proto);
	else
		return nxt_proto[proto];
	return 0;
}



/* gets first non-null socket_info structure
 * (useful if for. e.g we are not listening on any udp sockets )
 */
inline static struct socket_info* get_first_socket(void)
{
	if (udp_listen) return udp_listen;
#ifdef USE_TCP
	else if (tcp_listen) return tcp_listen;
#endif
#ifdef USE_SCTP
	else if (sctp_listen) return sctp_listen;
#endif
#ifdef USE_TCP
#ifdef USE_TLS
	else if (tls_listen) return tls_listen;
#endif
#endif
	return 0;
}

/* structure to break down 'proto:host:port' */
typedef struct _sr_phostp {
	int proto;
	str host;
	int port;
} sr_phostp_t;

struct socket_info* lookup_local_socket(str *phostp);

#endif
