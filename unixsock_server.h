/*
 * $Id$
 *
 * UNIX Domain Socket Server
 *
 * Copyright (C) 2001-2004 Fhg Fokus
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

#ifndef _UNIXSOCK_SERVER_H
#define _UNIXSOCK_SERVER_H


#include <sys/un.h>
#include <unistd.h>
#include "str.h"


typedef int (unixsock_f)(str* msg);


struct unixsock_cmd {
	str name;                   /* The name of the function */
	unixsock_f* f;              /* Function to be called */
	struct unixsock_cmd* next;  /* Next element in the linked list */
};


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
 * Register a new command
 */
int unixsock_register_cmd(char* name, unixsock_f* f);


/*
 * Reset the reply buffer -- start to write
 * at the beginning
 */
void unixsock_reply_reset(void);


/*
 * Add ASCIIZ to the reply buffer
 */
int unixsock_reply_asciiz(char* str);


/*
 * Add a string represented by str structure
 * to the reply buffer
 */
int unixsock_reply_str(str* s);


/*
 * Printf-like reply function
 */
int unixsock_reply_printf(char* fmt, ...);


/*
 * Send the reply
 */
ssize_t unixsock_reply_send(void);


/*
 * Send the reply to the given destination
 */
ssize_t unixsock_reply_sendto(struct sockaddr_un* to);


/*
 * Read a line, the result will be stored in line
 * parameter, the data is not copied, it's just
 * a pointer to an existing buffer
 */
int unixsock_read_line(str* line, str* source);


/*
 * Read body until the closing .CRLF, no CRLF recovery
 * is done so no additional buffer is necessary, body will
 * point to an existing buffer
 */
int unixsock_read_body(str* body, str* source);


/*
 * Read a set of lines, the functions performs CRLF recovery,
 * therefore lineset must point to an additional buffer
 * to which the data will be copied. Initial lineset->len contains
 * the size of the buffer
 */
int unixsock_read_lineset(str* lineset, str* source);


/*
 * Return the address of the sender
 */
struct sockaddr_un* unixsock_sender_addr(void);


#endif /* _UNIXSOCK_SERVER_H */
