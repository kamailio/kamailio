/*
 * $Id$
 */


#ifndef receive_h
#define receive_h

#include "ip_addr.h"

int receive_msg(char* buf, unsigned int len, union sockaddr_union *src_su);


#endif
