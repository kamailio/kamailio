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

extern int udp_sock;

int udp_init(struct ip_addr* ip, unsigned short port);
int udp_send(char *buf, unsigned len, union sockaddr_union*  to,
				unsigned tolen);
int udp_rcv_loop();


#endif
