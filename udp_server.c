/*
 * $Id$
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>


#include "udp_server.h"
#include "config.h"
#include "dprint.h"
#include "receive.h"
#include "mem/mem.h"

#ifdef DEBUG_DMALLOC
#include <mem/dmalloc.h>
#endif

int udp_sock;

int probe_max_receive_buffer( int udp_sock )
{
	int optval, optvallen;
	int ioptval, ioptvallen;
	int foptval, foptvallen;
	int voptval, voptvallen;
	int i;
	int phase=0;

	/* jku: try to increase buffer size as much as we can */
	ioptvallen=sizeof(ioptval);
	if (getsockopt( udp_sock, SOL_SOCKET, SO_RCVBUF, (void*) &ioptval,
		    &ioptvallen) == -1 )
	{
		LOG(L_ERR, "ERROR: udp_init: getsockopt: %s\n", strerror(errno));
		return -1;
	}
	if ( ioptval==0 ) 
	{
		LOG(L_DBG, "DEBUG: udp_init: SO_RCVBUF initialy set to 0; resetting to %d\n",
			BUFFER_INCREMENT );
		ioptval=BUFFER_INCREMENT;
	} else LOG(L_INFO, "INFO: udp_init: SO_RCVBUF is initially %d\n", ioptval );
	for (optval=ioptval; ;  ) {
		/* increase size; double in initial phase, add linearly later */
		if (phase==0) optval <<= 1; else optval+=BUFFER_INCREMENT;
		if (optval > maxbuffer) if (phase==1) break; else { phase=1; optval >>=1; continue; }
		LOG(L_DBG, "DEBUG: udp_init: trying SO_RCVBUF: %d\n", optval );
        	if (setsockopt( udp_sock, SOL_SOCKET, SO_RCVBUF,
                             (void*)&optval, sizeof(optval)) ==-1)
        	{
			/* Solaris returns -1 if asked size too big; Linux ignores */
			LOG(L_DBG, "DEBUG: udp_init: SOL_SOCKET failed for %d, phase %d: %s\n",
			    optval,  phase, strerror(errno) );
			/* if setting buffer size failed and still in the aggressive
			   phase, try less agressively; otherwise give up 
			*/
			if (phase==0) { phase=1; optval >>=1 ; continue; } 
			else break;
        	} 
		/* verify if change has taken effect */
		/* Linux note -- otherwise I would never know that; funny thing: Linux
		   doubles size for which we asked in setsockopt
		*/
		voptvallen=sizeof(voptval);
		if (getsockopt( udp_sock, SOL_SOCKET, SO_RCVBUF, (void*) &voptval,
		    &voptvallen) == -1 )
		{
			LOG(L_ERR, "ERROR: udp_init: getsockopt: %s\n", strerror(errno));
			return -1;
		} else {
			LOG(L_DBG, "DEBUG: setting SO_RCVBUF; set=%d,verify=%d\n", 
				optval, voptval);
			if (voptval<optval) {
				LOG(L_DBG, "DEBUG: setting SO_RCVBUF has no effect\n");
				/* if setting buffer size failed and still in the aggressive
			   	phase, try less agressively; otherwise give up 
				*/
                        	if (phase==0) { phase=1; optval >>=1 ; continue; } 
                        	else break;
			} 
		}

	} /* for ... */
	foptvallen=sizeof(foptval);
	if (getsockopt( udp_sock, SOL_SOCKET, SO_RCVBUF, (void*) &foptval,
		    &foptvallen) == -1 )
	{
		LOG(L_ERR, "ERROR: udp_init: getsockopt: %s\n", strerror(errno));
		return -1;
	}
 	LOG(L_INFO, "INFO: udp_init: SO_RCVBUF is finally %d\n", foptval );

	return 0;

	/* EoJKU */
}

int udp_init(unsigned long ip, unsigned short port)
{
	struct sockaddr_in* addr;
	int optval, optvallen;


	addr=(struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	if (addr==0){
		LOG(L_ERR, "ERROR: udp_init: out of memory\n");
		goto error;
	}
	addr->sin_family=AF_INET;
	addr->sin_port=htons(port);
	addr->sin_addr.s_addr=ip;

	udp_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (udp_sock==-1){
		LOG(L_ERR, "ERROR: udp_init: socket: %s\n", strerror(errno));
		goto error;
	}
	/* set sock opts? */
	optval=1;
	if (setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR ,
					(void*)&optval, sizeof(optval)) ==-1)
	{
		LOG(L_ERR, "ERROR: udp_init: setsockopt: %s\n", strerror(errno));
		goto error;
	}

	if ( probe_max_receive_buffer(udp_sock)==-1) goto error;


	if (bind(udp_sock, (struct sockaddr*) addr, sizeof(struct sockaddr))==-1){
		LOG(L_ERR, "ERROR: udp_init: bind: %s\n", strerror(errno));
		goto error;
	}

	free(addr);
	return 0;

error:
	if (addr) free(addr);
	return -1;
}



int udp_rcv_loop()
{
	unsigned len;
#ifdef DYN_BUF
	char* buf;
#else
	static char buf [BUF_SIZE+1];
#endif

	struct sockaddr* from;
	int fromlen;


	from=(struct sockaddr*) malloc(sizeof(struct sockaddr));
	if (from==0){
		LOG(L_ERR, "ERROR: udp_rcv_loop: out of memory\n");
		goto error;
	}

	for(;;){
#ifdef DYN_BUF
		buf=pkg_malloc(BUF_SIZE+1);
		if (buf==0){
			LOG(L_ERR, "ERROR: udp_rcv_loop: could not allocate receive"
					 " buffer\n");
			goto error;
		}
#endif
		fromlen=sizeof(struct sockaddr);
		len=recvfrom(udp_sock, buf, BUF_SIZE, 0, from, &fromlen);
		if (len==-1){
			LOG(L_ERR, "ERROR: udp_rcv_loop:recvfrom: %s\n",
						strerror(errno));
			if (errno==EINTR)	continue; /* goto skip;*/
			else goto error;
		}
		/*debugging, make print* msg work */
		buf[len+1]=0;
		
		/* receive_msg must free buf too!*/
		receive_msg(buf, len, ((struct sockaddr_in*)from)->sin_addr.s_addr);
		
	/* skip: do other stuff */
		
	}
	
	if (from) free(from);
	return 0;
	
error:
	if (from) free(from);
	return -1;
}



/* which socket to use? main socket or new one? */
int udp_send(char *buf, unsigned len, struct sockaddr*  to, unsigned tolen)
{

	int n;

/*	struct sockaddr_in a2;*/
#ifndef NO_DEBUG
#define MAX_IP_LENGTH 18
	char ip_txt[MAX_IP_LENGTH];
	char *c;
	struct sockaddr_in* a;
	unsigned short p;

	a=(struct sockaddr_in*) to;
	memset(ip_txt, 0, MAX_IP_LENGTH);
	c=inet_ntoa(a->sin_addr);
	strncpy( ip_txt, c, MAX_IP_LENGTH - 1 );
	p=ntohs(a->sin_port);

	if (tolen < sizeof(struct sockaddr_in))
		DBG("DEBUG: udp_send: tolen small\n");
	if (a->sin_family && a->sin_family != AF_INET)
		DBG("DEBUG: udp_send: to not INET\n");
	if (a->sin_port == 0)
		DBG("DEBUG: udp_send: no port\n");

#ifdef EXTRA_DEBUG
	if ( tolen < sizeof(struct sockaddr_in) ||
	a->sin_family && a->sin_family != AF_INET || a->sin_port == 0 )
		abort();
	/* every message must be terminated by CRLF */
	if (memcmp(buf+len-CRLF_LEN, CRLF, CRLF_LEN)!=0) {
		LOG(L_CRIT, "ERROR: this is ugly -- we are sending a packet"
			" not terminated by CRLF\n");
		abort();
	}
#endif

	DBG("DEBUG: udp_send destination: IP=%s, port=%u;\n", ip_txt, p);
#endif
/*
	memset(&a2, 0, sizeof(struct sockaddr_in));
	a2.sin_family = a->sin_family;
	a2.sin_port = a->sin_port;
	a2.sin_addr.s_addr = a->sin_addr.s_addr;
*/

again:
	n=sendto(udp_sock, buf, len, 0, to, tolen);
/*	n=sendto(udp_sock, buf, len, 0, &a2, sizeof(struct sockaddr_in) );*/
	if (n==-1){
		LOG(L_ERR, "ERROR: udp_send: sendto(sock,%x,%d,0,%x,%d): %s(%d)\n",
				buf,len,to,tolen,
				strerror(errno),errno);
		if (errno==EINTR) goto again;
		if (errno==EINVAL) {
			LOG(L_CRIT,"CRITICAL: invalid sendtoparameters\n"
			"one possible reason is the server is bound to localhost and\n"
			"attempts to send to the net\n");
#			ifdef EXTRA_DEBUG
			abort();
#			endif
		}
	}
	return n;
}
