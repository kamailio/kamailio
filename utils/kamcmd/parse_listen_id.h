/*
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/* History:
 * --------
 *  2006-02-20  created by andrei
 */


#ifndef parse_listen_id_h
#define parse_listen_id_h

enum payload_proto	{ P_BINRPC , P_FIFO };

enum socket_protos	{	UNKNOWN_SOCK=0, UDP_SOCK, TCP_SOCK, 
						UNIXS_SOCK, UNIXD_SOCK
#ifdef USE_FIFO
							, FIFO_SOCK
#endif
};



struct id_list{
	char* name;
	enum socket_protos proto;
	enum payload_proto data_proto;
	int port;
	char* buf; /* name points somewhere here */
	struct id_list* next;
};


struct id_list* parse_listen_id(char* l, int len, enum socket_protos def);

#endif
