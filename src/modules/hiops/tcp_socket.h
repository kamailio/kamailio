/*
 * Copyright (C) 2019 Mojtaba Esfandiari.S, Nasim-Telecom
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
 *
 */


#ifndef _TCP_SOCKET_H
#define _TCP_SOCKET_H

#include "../../core/mod_fix.h"

#define BUFSIZE 1024
typedef struct sockaddr_in sockaddr_in_t;

extern char buffer[BUFSIZE];

int socket_open(char *poroto, char *ipaddress, unsigned int port, unsigned int *sock);
int socket_close(unsigned int *sock);
int socket_listen(int sock);
int socket_send(int sock, char *buf);
int socket_recv(int sock);

#endif //TCP_SOCKET_H
