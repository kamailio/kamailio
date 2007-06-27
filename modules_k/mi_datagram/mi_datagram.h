/*
 * $Id: mi_datagram.h 1133 2007-04-02 17:31:13Z ancuta_onofrei $
 *
 * Copyright (C) 2007 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2007-06-25  first version (ancuta)
 */

#ifndef _MI_DATAGRAM_H_
#define _MI_DATAGRAM_H_

#include <sys/socket.h>

#define DEFAULT_MI_REPLY_IDENT "\t"
#define MI_CMD_SEPARATOR       ':'

/* the 2-chars separator between name and value */
#define MI_ATTR_VAL_SEP1 ':'
#define MI_ATTR_VAL_SEP2 ':'


/* maximum size for the socket reply name */
#define MAX_MI_FILENAME 128

/* size of buffer used by parser to read and build the MI tree */
#define MI_CHILD_NO	    1 

#include <sys/un.h>
#include "../../ip_addr.h"

typedef union{
	union sockaddr_union udp_addr;
	struct sockaddr_un   unix_addr;
}sockaddr_dtgram;
/*union because we support 3 types of sockaddr : 
	UNIX, IPv4 and IPv6*/

typedef union{
		struct sockaddr_un unix_deb;
		struct sockaddr_in inet_v4;
		struct sockaddr_in6 inet_v6;
}my_sock_address;

typedef struct{
	my_sock_address address;
		unsigned int domain;
	int address_len;
}my_socket_address;


#endif /* _MI_DATAGRAM */


