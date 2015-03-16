/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
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

/* results:


real    0m23.332s
user    0m0.410s
sys     0m15.710s

*/




/* which socket to use? main socket or new one? */
int udp_send()
{

	register int i;
	 int n;
	char buffer;
	int sock;
	struct sockaddr_in addr;

	if ((sock=socket(PF_INET, SOCK_DGRAM, 0))<0) {
		fprintf( stderr, "socket error\n");
		exit(1);
	}

	memset( &addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family=AF_INET;
	addr.sin_port=htons(9);
	addr.sin_addr.s_addr= inet_addr("127.0.0.1");

	for (i=0; i<1024*1024*16; i++) 
		sendto(sock, &buffer, 1, 0, (struct sockaddr *) &addr, sizeof(addr));

}

main() {

	udp_send();
}
