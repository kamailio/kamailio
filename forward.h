/*
 *  $Id$
 */


#ifndef forward_h
#define forward_h

#include "msg_parser.h"
#include "route.h"
#include "proxy.h"


int check_address(unsigned long ip, char *name, int resolver);

int forward_request( struct sip_msg* msg,  struct proxy_l* p);

int forward_reply( struct sip_msg* msg);

#endif
