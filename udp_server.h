/*
 * $Id$
 */

#ifndef udp_server_h
#define udp_server_h



extern int udp_sock;

int udp_init(unsigned long ip, unsigned short port);
int udp_send(char *buf, unsigned len, struct sockaddr*  to, unsigned tolen);
int udp_rcv_loop();


#endif
