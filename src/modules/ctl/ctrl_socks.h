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

#ifndef _ctrl_socks_h
#define _ctrl_socks_h
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h> /* iovec */
#include "../../ip_addr.h"
#include "init_socks.h"



enum payload_proto	{ P_BINRPC , P_FIFO };

struct id_list{
	char* name;
	enum socket_protos proto;
	enum payload_proto data_proto;
	int port;
	char* buf; /* name points somewhere here */
	struct id_list* next;
};

union sockaddr_u{
	union sockaddr_union sa_in;
	struct sockaddr_un sa_un;
};



/* list of control sockets */
struct ctrl_socket{
	int fd;
	int write_fd; /* used only by fifo */
	enum socket_protos transport;
	enum payload_proto p_proto;
	char* name;
	int port;
	struct ctrl_socket* next;
	union sockaddr_u u;
	void *data; /* extra data, socket dependent */
};



struct id_list* parse_listen_id(char*, int, enum socket_protos);

int init_ctrl_sockets(struct ctrl_socket** c_lst, struct id_list* lst,
						int def_port, int perm, int uid, int gid);
void free_id_list(struct id_list*);
void free_ctrl_socket_list(struct ctrl_socket* l);


inline static char* payload_proto_name(enum payload_proto p)
{
	switch(p){
		case P_BINRPC:
			return "binrpc";
		case P_FIFO:
			return "fifo";
		default:
			;
	}
	return "<unknown>";
}

#endif
