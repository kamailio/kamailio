/*
 * $Id: ctrl_socks.h,v 1.1 2006/02/23 19:57:31 andrei Exp $
 *
 * Copyright (C) 2006 iptelorg GmbH
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
 *  2007        ported to libbinrpc (bpintea)
 */

#ifndef _ctrl_socks_h
#define _ctrl_socks_h
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h> /* iovec */
#include <unistd.h> /* pid_t */
#include "../../ip_addr.h"
#include "../../rpc.h"

#include <binrpc.h>

#include "init_socks.h"

enum payload_proto {
	P_BINRPC, /* transport BINRPC protocol bytes */
#ifdef USE_FIFO
	P_FIFO, /* transport FIFO protocol bytes */
#endif
	P_FDPASS /* transport descriptors */
};

enum SOCK_LIST_TYPES {
	SOCKLIST_NONE	= 0,
	SOCKLIST_STREAM	= 1 << 0,
	SOCKLIST_DGRAM	= 1 << 1,
#ifdef USE_FIFO
	SOCKLIST_FIFO	= 1 << 2,
#endif
	SOCKLIST_FDPASS	= 1 << 3,
};


/* list of control sockets */
struct ctrl_socket {
	int fd;
#ifdef USE_FIFO
	int write_fd; /* used only by fifo */
#endif
	enum payload_proto p_proto;
	brpc_addr_t addr; /* addresses where the "control" sockets listen */
	union {
		char* name;
		pid_t child; /* == 0 for the children */
	};
	struct ctrl_socket* next;
	void *data; /* extra data, socket dependent */
};

extern struct ctrl_socket* ctrl_sock_lst;

int init_ctrl_socket(struct ctrl_socket* ctrl_sock, int perm, 
		int uid, int gid);
int add_binrpc_listener(char *s);
struct ctrl_socket *add_fdpass_socket(int sockfd, pid_t pid);
#ifdef USE_FIFO
int add_fifo_socket(char *s);
#endif
void del_binrpc_listeners(int keep);
void close_binrpc_listeners(int keep);
char **ctrl_sock_paths();
char* payload_proto_name(enum payload_proto p);

#define CTRLSOCK_URI(_cs_) \
	((_cs_)->p_proto == P_FDPASS) ? \
			"(FDPASS)" :  \
			brpc_print_addr(&(_cs_)->addr)

extern const char *ctl_listen_ls_doc[];
void  ctrl_listen_ls_rpc(rpc_t* rpc, void* ctx);


#endif
