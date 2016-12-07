/* $Id$ */
/*
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
/*
 * History:
 *  2005-09-09  basic tcp support added (andrei)
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
#include <netinet/tcp.h>
#ifdef USE_SCTP
#include <netinet/sctp.h>
#endif /* USE_SCTP */
#include <arpa/inet.h>
#include <signal.h>


static char *id="$Id$";
static char *version="protoshoot 0.4";
static char* help_msg="\
Usage: protoshoot -f file -d address -p port -c count [-v]\n\
Options:\n\
    -f file       file with the content of the udp packet (max 65k)\n\
    -d address    destination address\n\
    -p port       destination port\n\
    -c count      number of packets to be sent\n\
    -s usec       microseconds to sleep before sending \"throttle\" packets\n\
    -t throttle   number of packets to send before sleeping\n\
    -r            sleep randomly up to -s usec packets (see -s) \n\
    -T            use tcp instead of udp \n\
    -S            use sctp instead of udp \n\
    -1            use sctp in one to one mode \n\
    -n no         tcp connection number \n\
    -R            close the tcp connections with RST (SO_LINGER) \n\
    -v            increase verbosity level\n\
    -V            version number\n\
    -h            this help message\n\
";

#define BUF_SIZE 65535


enum protos { PROTO_NONE, PROTO_UDP, PROTO_TCP, PROTO_SCTP };

int main (int argc, char** argv)
{
	int fd;
	int sock;
	char c;
	int n,r;
	char* tmp;
	char buf[BUF_SIZE];
	struct hostent* he;
	struct sockaddr_in addr;
	
	int count;
	int verbose;
	char *fname;
	char *dst;
	int port;
	unsigned long usec;
	int throttle;
	int random_sleep;
	enum protos proto;
	int sctp_o2o;
	int tcp_rst;
	int con_no;
	int t;
	struct linger t_linger;
	int k;
	int err;
	
	/* init */
	count=1;
	verbose=0;
	fname=0;
	dst="127.0.0.1";
	port=5060;
	usec=0;
	throttle=0;
	random_sleep=0;
	proto=PROTO_UDP;
	tcp_rst=0;
	con_no=1;
	sctp_o2o=0;
	err=0;

	opterr=0;
	while ((c=getopt(argc,argv, "f:c:d:p:s:t:n:rTS1RvhV"))!=-1){
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
			case 's':
				usec=strtol(optarg, &tmp, 10);
				if ((tmp==0)||(*tmp)){
					fprintf(stderr, "bad count: -c %s\n", optarg);
					goto error;
				}
				break;
			case 't':
				throttle=strtol(optarg, &tmp, 10);
				if ((tmp==0)||(*tmp)){
					fprintf(stderr, "bad count: -c %s\n", optarg);
					goto error;
				}
				break;
			case 'n':
				con_no=strtol(optarg, &tmp, 10);
				if ((tmp==0)||(*tmp)||(con_no<1)){
					fprintf(stderr, "bad count: -c %s\n", optarg);
					goto error;
				}
				break;
			case 'r':
				random_sleep=1;
				break;
			case 'T':
				proto=PROTO_TCP;
				break;
			case 'S':
#ifdef USE_SCTP
				proto=PROTO_SCTP;
#else
				fprintf(stderr, "sctp not supported (recompile with "
								"-DUSE_SCTP)\n");
				goto error;
#endif /* USE_SCTP */
				break;
			case '1':
				sctp_o2o=1;
				break;
			case 'R':
				tcp_rst=1;
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
					fprintf(stderr, "Unknown option '-%c'\n", optopt);
				else
					fprintf(stderr, "Unknown character '\\x%x'\n", optopt);
				goto error;
			case ':':
				fprintf(stderr, "Option '-%c' requires an argument.\n",
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
	if (proto==PROTO_UDP || (proto==PROTO_SCTP && !sctp_o2o)) con_no=1;
	
	/* ignore sigpipe */
	if (signal(SIGPIPE, SIG_IGN)==SIG_ERR){
		fprintf(stderr, "failed to ignore SIGPIPE: %s\n", strerror(errno));
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
#ifdef HAVE_SOCKADDR_SA_LEN
	addr.sin_len=sizeof(struct sockaddr_in);
#endif
	memcpy(&addr.sin_addr.s_addr, he->h_addr_list[0], he->h_length);
	
	for (k=0; k<con_no; k++){
		switch(proto){
			case PROTO_UDP:
				sock = socket(he->h_addrtype, SOCK_DGRAM, 0);
				break;
			case PROTO_TCP:
				sock = socket(he->h_addrtype, SOCK_STREAM, 0);
				break;
#ifdef USE_SCTP
			case PROTO_SCTP:
				sock = socket(he->h_addrtype, 
								sctp_o2o?SOCK_STREAM:SOCK_SEQPACKET,
								IPPROTO_SCTP);
				break;
#endif /* USE_SCTP */
			default:
				fprintf(stderr, "BUG: unkown proto %d\n", proto);
				goto error;
		}
		if (sock==-1){
			fprintf(stderr, "ERROR: socket: %s\n", strerror(errno));
			goto error;
		}
		if (proto==PROTO_TCP){
			t=1;
			if (setsockopt(sock, IPPROTO_TCP , TCP_NODELAY, &t, sizeof(t))<0){
				fprintf(stderr, "ERROR: could not disable Nagle: %s\n",
								strerror(errno));
			}
			if (tcp_rst){
				t_linger.l_onoff=1;
				t_linger.l_linger=0;
				if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &t_linger,
									sizeof(t_linger))<0){
					fprintf(stderr, "ERROR: could not set SO_LINGER: %s\n",
									strerror(errno));
				}
			}
		}
#ifdef USE_SCTP
		else if (proto==PROTO_SCTP){
			t=1;
			if (setsockopt(sock, IPPROTO_SCTP, SCTP_NODELAY, &t, sizeof(t))<0){
				fprintf(stderr, "ERROR: could not disable Nagle: %s\n",
								strerror(errno));
			}
		}
#endif /* USE_SCTP */

		if (
#ifdef USE_SCTP
			(proto!=PROTO_SCTP || sctp_o2o) &&
#endif /* USE_SCTP */
			(connect(sock, (struct sockaddr*) &addr,
					sizeof(struct sockaddr))!=0)){
			fprintf(stderr, "ERROR: connect: %s\n", strerror(errno));
			goto error;
		}
		
		
		/* flood loop */
		t=throttle;
		for (r=0; r<count; r++){
			if ((verbose>1)&&((r%1000)==999)){  putchar('.'); fflush(stdout); }
#ifdef USE_SCTP
			if (proto==PROTO_SCTP && !sctp_o2o){
				if (sctp_sendmsg(sock, buf, n,  (struct sockaddr*) &addr,
									sizeof(struct sockaddr), 0, SCTP_UNORDERED,
									0, 0, 0)==-1){
					fprintf(stderr, "Error(%d): send: %s\n", err,
							strerror(errno));
					err++;;
				}
			}else
#endif /* USE_SCTP */
			{
				if (send(sock, buf, n, 0)==-1) {
					fprintf(stderr, "Error(%d): send: %s\n", err,
							strerror(errno));
					err++;;
				}
			}
			if (usec){
				t--;
				if (t==0){
					usleep(random_sleep?
								(unsigned long)((double)usec*rand()/RAND_MAX):usec);
					t=throttle;
				}
			}
		}
		
		close(sock);
		if ((verbose) && (k%1000==999)) { putchar('#'); fflush(stdout); }
	}
	if (proto==PROTO_TCP || proto==PROTO_SCTP){
		printf("\n%d packets sent on %d %s connections (%d on each of them),"
				" %d bytes each => total %d bytes\n",
				count*con_no-err, con_no, (proto==PROTO_TCP)?"tcp":"sctp",
				count, n,
				(con_no*count-err)*n);
	}else{
		printf("\n%d packets sent, %d bytes each => total %d bytes\n",
				count-err, n, n*(count-err));
	}
	if (err) printf("%d errors\n", err);
	exit(0);

error:
	fprintf(stderr, "exiting due to error (%s)\n", strerror(errno));
	exit(-1);
}
