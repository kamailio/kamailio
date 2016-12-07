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

#ifndef _io_listener_h
#define _io_listener_h
#include "ctrl_socks.h"

#define MAX_IO_READ_CONNECTIONS		128 /* FIXME: make it a config var */

enum sock_con_type { S_CONNECTED, S_DISCONNECTED
#ifdef USE_FIFO
						, S_FIFO
#endif
};



struct send_handle{
	int fd;
	int type;
	union sockaddr_u from;
	unsigned int from_len;
};



int sock_send_v(void *h, struct iovec* v, size_t count);

void io_listen_loop(int fd_no, struct ctrl_socket* cs_lst);


#endif
