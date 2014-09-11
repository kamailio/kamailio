/*
 * $Id$
 *
 * UNIX Domain Socket Server
 *
 * Copyright (C) 2001-2004 FhG Fokus
 * Copyright (C) 2005 iptelorg GmbH
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

#ifndef _UNIXSOCK_SERVER_H
#define _UNIXSOCK_SERVER_H


#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include "../../str.h"

extern char* unixsock_name;
extern char* unixsock_user;
extern char* unixsock_mode;
extern char* unixsock_group;
extern unsigned int unixsock_children;
extern unsigned int unixsock_tx_timeout;

/*
 * Initialize Unix domain socket server
 */
int init_unixsock_socket(void);


/*
 * Initialize Unix domain socket server
 */
int init_unixsock_children(void);


/*
 * Clean up
 */
void close_unixsock_server(void);


/*
 * Send the reply
 */
ssize_t unixsock_reply_send(void);


/*
 * Send the reply to the given destination
 */
ssize_t unixsock_reply_sendto(struct sockaddr_un* to);

/*
 * Return the address of the sender
 */
struct sockaddr_un* unixsock_sender_addr(void);

void unixsock_reply_reset(void);

#endif /* _UNIXSOCK_SERVER_H */
