/*
 *  $Id$
 */


#ifndef forward_h
#define forward_h

#include "msg_parser.h"
#include "route.h"
#include "proxy.h"


int forward_request( struct sip_msg* msg,  struct proxy_l* p);

int forward_reply( struct sip_msg* msg);

#endif
