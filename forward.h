/*
 *  $Id$
 */


#ifndef forward_h
#define forward_h

#include "parser/msg_parser.h"
#include "route.h"
#include "proxy.h"


int forward_request( struct sip_msg* msg,  struct proxy_l* p);
int update_sock_struct_from_via( struct sockaddr_in* to,  struct via_body* via );
int forward_reply( struct sip_msg* msg);

#endif
