/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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



#ifndef _tcp_conn_h
#define _tcp_conn_h



#define TCP_BUF_SIZE 65535
enum {TCP_REQ_INIT, TCP_REQ_OK, TCP_READ_ERROR, TCP_REQ_OVERRUN };
enum {H_PARSING, H_LF, H_LFCR,  H_BODY };

struct tcp_req{
	struct tcp_req* next;
	int fd;
	/* sockaddr ? */
	char buf[TCP_BUF_SIZE]; /* bytes read so far*/
	char* pos; /* current position in buf */
	char* parsed; /* last parsed position */
	char* body; /* body position */
	int error;
	int state;
};


#define init_tcp_req( r, f) \
	do{ \
		memset( (r), 0, sizeof(struct tcp_req)); \
		(r)->fd=(f); \
		(r)->parsed=(r)->pos=(r)->buf; \
		(r)->error=TCP_REQ_OK;\
		(r)->state=H_PARSING; \
	}while(0)









#endif


