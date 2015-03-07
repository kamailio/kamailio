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



#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef __linux__
#include <linux/types.h>
#include <linux/errqueue.h>
#endif


static char *id="$Id$";
static char *version="udp_flood_disc 0.1";
static char* help_msg="\
Usage: udp_flood -f file -d address -p port -c count [-v]\n\
Options:\n\
    -f file       file with the content of the udp packet (max 65k)\n\
    -d address    destination address\n\
    -p port       destination port\n\
    -c count      number of packets to be sent\n\
    -v            increase verbosity level\n\
    -V            version number\n\
    -h            this help message\n\
";

#define BUF_SIZE 65535


int main (int argc, char** argv)
{
	int fd;
	int sock;
	char c;
	int n,r;
	char* tmp;
	char* buf[BUF_SIZE];
	struct hostent* he;
	struct sockaddr_in addr;
	
	int count;
	int verbose;
	char *fname;
	char *dst;
	int port;
#ifdef __linux__
	int optval;
#endif
	
	/* init */
	count=0;
	verbose=0;
	fname=0;
	dst=0;
	port=0;

	opterr=0;
	while ((c=getopt(argc,argv, "f:c:d:p:vhV"))!=-1){
		switch(c){
			case 'f':
				fname=optarg;
				break;
			case 'v':
				verbose++;
				break;
			case 'd':
				dst=optarg;
				break;
			case 'p':
				port=strtol(optarg, &tmp, 10);
				if ((tmp==0)||(*tmp)){
					fprintf(stderr, "bad port number: -p %s\n", optarg);
					goto error;
				}
				break;
			case 'c':
				count=strtol(optarg, &tmp, 10);
				if ((tmp==0)||(*tmp)){
					fprintf(stderr, "bad count: -c %s\n", optarg);
					goto error;
				}
				break;
			case 'V':
				printf("version: %s\n", version);
				printf("%s\n",id);
				exit(0);
				break;
			case 'h':
				printf("version: %s\n", version);
				printf("%s", help_msg);
				exit(0);
				break;
			case '?':
				if (isprint(optopt))
					fprintf(stderr, "Unknown option `-%c´\n", optopt);
				else
					fprintf(stderr, "Unknown character `\\x%x´\n", optopt);
				goto error;
			case ':':
				fprintf(stderr, "Option `-%c´ requires an argument.\n",
						optopt);
				goto error;
				break;
			default:
					abort();
		}
	}
	
	/* check if all the required params are present */
	if (fname==0){
		fprintf(stderr, "Missing -f file\n");
		exit(-1);
	}
	if (dst==0){
		fprintf(stderr, "Missing destination (-d ...)\n");
		exit(-1);
	}
	if(port==0){
		fprintf(stderr, "Missing port number (-p port)\n");
		exit(-1);
	}else if(port<0){
		fprintf(stderr, "Invalid port number (-p %d)\n", port);
		exit(-1);
	}
	if(count==0){
		fprintf(stderr, "Missing packet count (-c number)\n");
		exit(-1);
	}else if(count<0){
		fprintf(stderr, "Invalid packet count (-c %d)\n", count);
		exit(-1);
	}
	
	/* open packet file */
	fd=open(fname, O_RDONLY);
	if (fd<0){
		fprintf(stderr, "ERROR: loading packet-file(%s): %s\n", fname,
				strerror(errno));
		goto error;
	}
	n=read(fd, buf, BUF_SIZE);
	if (n<0){
		fprintf(stderr, "ERROR: reading file(%s): %s\n", fname,
				strerror(errno));
		goto error;
	}
	if (verbose) printf("read %d bytes from file %s\n", n, fname);
	close(fd);

	/* resolve destination */
	he=gethostbyname(dst);
	if (he==0){
		fprintf(stderr, "ERROR: could not resolve %s\n", dst);
		goto error;
	}
	/* open socket*/
	addr.sin_family=he->h_addrtype;
	addr.sin_port=htons(port);
	memcpy(&addr.sin_addr.s_addr, he->h_addr_list[0], he->h_length);
	
	sock = socket(he->h_addrtype, SOCK_DGRAM, 0);
	if (sock==-1){
		fprintf(stderr, "ERROR: socket: %s\n", strerror(errno));
		goto error;
	}
#ifdef __linux__
	/* enable error receiving on unconnected sockets*/
	optval=1;
	if(setsockopt(sock,SOL_IP,IP_RECVERR,(void*)&optval,sizeof(optval))==-1){
		fprintf(stderr, "Error: setsockopt: %s\n", strerror(errno));
		exit(1);
	}
#endif


	/* flood loop */
	for (r=0; r<count; r++){
		if ((verbose>1)&&(r%1000))  putchar('.');
		if (sendto(sock,buf, n, 0, (struct sockaddr*) &addr, sizeof(addr))==-1)
		{
			fprintf(stderr, "Error: send: %s\n",  strerror(errno));
			exit(1);
		}
	}
	printf("\n%d packets sent, %d bytes each => total %d bytes\n",
			count, n, n*count);

	close(sock);
	exit(0);

error:
	exit(-1);
}
