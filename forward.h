/*
 *  $Id$
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



#ifndef forward_h
#define forward_h

#include "parser/msg_parser.h"
#include "route.h"
#include "proxy.h"
#include "ip_addr.h"


struct socket_info* get_send_socket(union sockaddr_union* su);
int check_self(str* host);
int forward_request( struct sip_msg* msg,  struct proxy_l* p);
int update_sock_struct_from_via( union sockaddr_union* to,
								struct via_body* via );
int update_sock_struct_from_ip( union sockaddr_union* to,
    struct sip_msg *msg );
int forward_reply( struct sip_msg* msg);

#endif
