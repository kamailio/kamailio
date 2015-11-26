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
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>



static char *id="$Id$";
static char *version="udp_test_proxy 0.1";
static char* help_msg="\
Usage: udp_test_proxy  -l address -s port -d address -p port [-n no] [-v]\n\
Options:\n\
    -l address    listen address\n\
    -s port       listen(source) port\n\
    -d address    destination address\n\
    -p port       destination port\n\
    -n no         number of processes\n\
    -2            use a different socket for sending\n\
    -v            increase verbosity level\n\
    -V            version number\n\
    -h            this help message\n\
";
#define BUF_SIZE 65535
static char buf[BUF_SIZE];

int main(int argc, char** argv)
{
	int sock;
	int s_sock;
	pid_t pid;
	struct sockaddr_in addr;
	struct sockaddr_in to;
	int r, n, len;
	char c;
	struct hostent* he;
	int verbose;
	int sport, dport;
	char *dst;
	char *src;
	char* tmp;
	int use_diff_ssock;


	/* init */
	verbose=0;
	dst=0;
	sport=dport=0;
	src=dst=0;
	n=0;
	use_diff_ssock=0;
	
	opterr=0;
	while ((c=getopt(argc,argv, "l:p:d:s:n:2vhV"))!=-1){
		switch(c){
			case '2':
				use_diff_ssock=1;
				break;
			case 'v':
				verbose++;
				break;
			case 'd':
				dst=optarg;
				break;
			case 'l':
				src=optarg;
				break;
			case 'p':
				dport=strtol(optarg, &tmp, 10);
				if ((tmp==0)||(*tmp)){
					fprintf(stderr, "bad port number: -p %s\n", optarg);
					goto error;
				}
				break;
			case 's':
				sport=strtol(optarg, &tmp, 10);
				if ((tmp==0)||(*tmp)){
					fprintf(stderr, "bad port number: -s %s\n", optarg);
					goto error;
				}
				break;
			case 'n':
				n=strtol(optarg, &tmp, 10);
				if ((tmp==0)||(*tmp)){
					fprintf(stderr, "bad process number: -n %s\n", optarg);
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
	if (dst==0){
		fprintf(stderr, "Missing destination (-d ...)\n");
		exit(-1);
	}
	if (src==0){
		fprintf(stderr, "Missing listen address (-l ...)\n");
		exit(-1);
	}
	if(sport==0){
		fprintf(stderr, "Missing source port number (-s port)\n");
		exit(-1);
	}else if(sport<0){
		fprintf(stderr, "Invalid source port number (-s %d)\n", sport);
		exit(-1);
	}
	if(dport==0){
		fprintf(stderr, "Missing destination port number (-p port)\n");
		exit(-1);
	}else if(dport<0){
		fprintf(stderr, "Invalid destination port number (-p %d)\n", dport);
		exit(-1);
	}
	if(n<0){
		fprintf(stderr, "Invalid process no (-n %d)\n", n);
		exit(-1);
	}


	/* resolve destination */
	he=gethostbyname(dst);
	if (he==0){
		fprintf(stderr, "ERROR: could not resolve %s\n", dst);
		goto error;
	}

	/* set to*/
	to.sin_family=he->h_addrtype;
	to.sin_port=htons(dport);
	memcpy(&to.sin_addr.s_addr, he->h_addr_list[0], he->h_length);

	/* resolve source/listen */
	he=gethostbyname(src);
	if (he==0){
		fprintf(stderr, "ERROR: could not resolve %s\n", dst);
		goto error;
	}
	/* open socket*/
	addr.sin_family=he->h_addrtype;
	addr.sin_port=htons(sport);
	memcpy(&addr.sin_addr.s_addr, he->h_addr_list[0], he->h_length);

	s_sock=-1;
	sock = socket(he->h_addrtype, SOCK_DGRAM, 0);
	if (use_diff_ssock && (sock!=-1))
		s_sock = socket(he->h_addrtype, SOCK_DGRAM, 0);
	else
		s_sock=sock;
	
	if ((sock==-1)||(s_sock==-1)){
		fprintf(stderr, "ERROR: socket: %s\n", strerror(errno));
		goto error;
	}
	if (bind(sock, (struct sockaddr*) &addr, sizeof(struct sockaddr_in))==-1){
		fprintf(stderr, "ERROR: bind: %s\n", strerror(errno));
		goto error;
	}
	if (use_diff_ssock){
		if (connect(s_sock, (struct sockaddr*) &to,
					sizeof(struct sockaddr_in))==-1){
			fprintf(stderr, "ERROR: connect: %s\n", strerror(errno));
			goto error;
		}
	}

	for(r=1; r<n; r++){
		if ((pid=fork())==-1){
			fprintf(stderr, "ERROR: fork: %s\n", strerror(errno));
			goto error;
		}
		if (pid==0) break; /* child, skip */
	}

	if (verbose>3) printf("process starting\n");
	for(;;){
		len=read(sock, buf, BUF_SIZE);
		if (len==-1){
			fprintf(stderr, "ERROR: read: %s\n", strerror(errno));
			continue;
		}
		if (verbose>2) putchar('r');
		/* send it back*/
		if (use_diff_ssock)
			len=send(s_sock, buf, len, 0);
		else
			len=sendto(s_sock, buf, len, 0,
					(struct sockaddr*) &to, sizeof(struct sockaddr_in));
		if (len==-1){
			fprintf(stderr, "ERROR: send: %s\n", strerror(errno));
			continue;
		}
		if (verbose>1) putchar('.');
	}
error:
	exit(-1);
}
