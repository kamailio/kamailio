/*
 * $Id$
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

	if (connect(sock, (struct sockaddr *) &addr, sizeof(addr))==-1) {
		fprintf( stderr, "connect failed\n");
		exit(1);
	}

	for (i=0; i<1024*1024*16; i++) 
		write( sock, &buffer, 1 );
/*		sendto(sock, &buffer, 1, 0, (struct sockaddr *) &addr, sizeof(addr)); */

}

main() {

	udp_send();
}
