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
/*
 * History:
 * -------
 *  2001-??-?? created by andrei
 *  ????-??-?? lots of changes by a lot of people
 *  2003-02-11 added inline msg_send (andrei)
 *  2003-04-07 changed all ports to host byte order (andrei)
 *  2003-04-12  FORCE_RPORT_T added (andrei)
 *  2003-04-15  added tcp_disable support (andrei)
 */



#ifndef forward_h
#define forward_h

#include "globals.h"
#include "parser/msg_parser.h"
#include "route.h"
#include "proxy.h"
#include "ip_addr.h"

#include "stats.h"
#include "udp_server.h"
#ifdef USE_TCP
#include "tcp_server.h"
#endif


struct socket_info* get_send_socket(union sockaddr_union* su, int proto);
struct socket_info* get_out_socket(union sockaddr_union* to, int proto);
int check_self(str* host, unsigned short port);
int forward_request( struct sip_msg* msg,  struct proxy_l* p, int proto);
int update_sock_struct_from_via( union sockaddr_union* to,
								 struct sip_msg* msg,
								 struct via_body* via );

/* use src_ip, port=src_port if rport, via port if via port, 5060 otherwise */
#define update_sock_struct_from_ip(  to, msg ) \
	init_su((to), &(msg)->rcv.src_ip, \
			(((msg)->via1->rport)||((msg)->msg_flags&&FL_FORCE_RPORT))? \
							(msg)->rcv.src_port: \
							((msg)->via1->port)?(msg)->via1->port: SIP_PORT )

int forward_reply( struct sip_msg* msg);



/* params:
 *  send_sock= 0 if already known (e.g. for udp in some cases), non-0 otherwise
 *  proto=TCP|UDP
 *  to = destination,
 *  id - only used on tcp, it will force sending on connection "id" if id!=0 
 *       and the connection exists, else it will send to "to" 
 *       (usefull for sending replies on  the same connection as the request
 *       that generated them; use 0 if you don't want this)
 * returns: 0 if ok, -1 on error*/
static inline int msg_send(	struct socket_info* send_sock, int proto,
							union sockaddr_union* to, int id,
							char* buf, int len)
{
	
	if (proto==PROTO_UDP){
		if (send_sock==0) send_sock=get_send_socket(to, proto);
		if (send_sock==0){
			LOG(L_ERR, "msg_send: ERROR: no sending socket found\n");
			goto error;
		}
		if (udp_send(send_sock, buf, len, to)==-1){
			STATS_TX_DROPS;
			LOG(L_ERR, "msg_send: ERROR: udp_send failed\n");
			goto error;
		}
	}
#ifdef USE_TCP
	else if (proto==PROTO_TCP){
		if (tcp_disable){
			STATS_TX_DROPS;
			LOG(L_WARN, "msg_send: WARNING: attempt to send on tcp and tcp"
					" support is disabled\n");
			goto error;
		}else{
			if (tcp_send(proto, buf, len, to, id)<0){
				STATS_TX_DROPS;
				LOG(L_ERR, "msg_send: ERROR: tcp_send failed\n");
				goto error;
			}
		}
	}
#ifdef USE_TLS
	else if (proto==PROTO_TLS){
		if (tls_disable){
			STATS_TX_DROPS;
			LOG(L_WARN, "msg_send: WARNING: attempt to send on tls and tls"
					" support is disabled\n");
			goto error;
		}else{
			if (tcp_send(proto, buf, len, to, id)<0){
				STATS_TX_DROPS;
				LOG(L_ERR, "msg_send: ERROR: tcp_send failed\n");
				goto error;
			}
		}
	}
#endif /* USE_TLS */
#endif /* USE_TCP */
	else{
			LOG(L_CRIT, "BUG: msg_send: unknown proto %d\n", proto);
			goto error;
	}
	return 0;
error:
	return -1;
}

#endif
