/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _pass_fd_h
#define _pass_fd_h

#ifdef __OS_cygwin
/* check if MSG_WAITALL is defined */
#include <sys/types.h>
#include <sys/socket.h>

#ifndef MSG_WAITALL
#define NO_MSG_WAITALL
#define MSG_WAITALL 0x80000000
#endif /* MSG_WAITALL */

#ifndef MSG_DONTWAIT
#define NO_MSG_DONTWAIT
#endif /* MSG_DONT_WAIT */

#endif /* __OS_cygwin */

int send_fd(int unix_socket, void* data, int data_len, int fd);
int receive_fd(int unix_socket, void* data, int data_len, int* fd, int flags);

int recv_all(int socket, void* data, int data_len, int flags);
int send_all(int socket, void* data, int data_len);


#endif
