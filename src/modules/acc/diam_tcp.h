/*
 * $Id$
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*! \file
 * \ingroup acc
 * \brief Acc:: Diameter TCP connections
 *
 * - Module: \ref acc
 */
#ifdef DIAM_ACC

#ifndef ACC_TCP
#define ACC_TCP

#include "../../str.h"
#include "../../parser/msg_parser.h"

/* information needed for reading messages from tcp connection */
typedef struct rd_buf
{
	unsigned int first_4bytes;
	unsigned int buf_len;
	unsigned char *buf;
} rd_buf_t;


#define AAA_ERROR			-1
#define AAA_CONN_CLOSED		-2
#define AAA_TIMEOUT			-3
#define ACC_SUCCESS			0
#define ACC_FAILURE			1

#define AAA_NO_CONNECTION 	-1

#define MAX_WAIT_SEC	2
#define MAX_WAIT_USEC	0

#define MAX_AAA_MSG_SIZE  65536

#define CONN_SUCCESS	 1 
#define CONN_ERROR		-1
#define CONN_CLOSED		-2

#define MAX_TRIES		10

int sockfd;

int do_read( int socket, rd_buf_t *p);
void reset_read_buffer(rd_buf_t *rb);

/* it initializes the TCP connection */ 
int init_mytcp(char* host, int port);
/* send a message over an already opened TCP connection */
int tcp_send_recv(int sockfd, char* buf, int len, rd_buf_t* rb, 
					unsigned int waited_id);
void close_tcp_connection(int sfd);

int get_uri(struct sip_msg* m, str** uri);
#endif

#endif
