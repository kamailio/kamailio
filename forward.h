/*
 *  $Id$
 */


#ifndef forward_h
#define forward_h

#include "parser/msg_parser.h"
#include "route.h"
#include "proxy.h"
#include "ip_addr.h"


struct socket_info* get_send_socket(union sockaddr_union* su);
int forward_request( struct sip_msg* msg,  struct proxy_l* p);
int update_sock_struct_from_via( union sockaddr_union* to,
								struct via_body* via );
int update_sock_struct_from_ip( union sockaddr_union* to,
    struct sip_msg *msg );
int forward_reply( struct sip_msg* msg);

#endif
