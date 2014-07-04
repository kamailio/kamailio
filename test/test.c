/* 
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>


#define LOOP_COUNT 100
#define PORT 5060
#define SEND_PORT 5090
#define SEND_ADDR 0x0a000022 /*10.0.0.34*/   /*   0x7f000001 127.0.0.1*/
#define BUF_SIZE 65535
static char buf[BUF_SIZE];

int main(char** argv, int argn)
{
	int sock;
	struct sockaddr_in addr;
	struct sockaddr_in to;
	int r, len;

	printf("starting\n");
	
	addr.sin_family=AF_INET;
	addr.sin_port=htons(PORT);
	addr.sin_addr.s_addr=INADDR_ANY;
	to.sin_family=AF_INET;
	to.sin_port=htons(SEND_PORT);
	to.sin_addr.s_addr=htonl(SEND_ADDR);
		

	sock=socket(PF_INET, SOCK_DGRAM,0);
	if (bind(sock, (struct sockaddr*) &addr, sizeof(struct sockaddr_in))==-1){
		fprintf(stderr, "ERROR: udp_init: bind: %s\n", strerror(errno));
		exit(1);
	}

	//if (fork())
		if (fork()){
			close(sock);
			for(;;) sleep(100);
			exit(1);
		}
	/*children*/
	printf("child\n");
	for(;;){
		len=read(sock, buf, BUF_SIZE);
		/*for (r=0;r < LOOP_COUNT; r++);*/
		/* send it back*/
		sendto(sock, buf, len, 0, (struct sockaddr*) &to,
				sizeof(struct sockaddr_in));
	}
}
