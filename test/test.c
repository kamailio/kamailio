/* 
 * $Id$
 *
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
