/*
 * $Id$
 */

#ifndef udp_server_h
#define udp_server_h

#include <sys/types.h>
#include <sys/socket.h>
#include "ip_addr.h"

#define MAX_RECV_BUFFER_SIZE	256*1024
#define BUFFER_INCREMENT	2048


int udp_init(struct socket_info* si);
int udp_send(struct socket_info* source,char *buf, unsigned len,
				union sockaddr_union*  to);
int udp_rcv_loop();


#endif
