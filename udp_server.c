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
#include "globals.h"
#include "config.h"
#include "dprint.h"
#include "receive.h"
#include "mem/mem.h"
#include "ip_addr.h"

#ifdef DEBUG_DMALLOC
#include <mem/dmalloc.h>
#endif


int probe_max_receive_buffer( int udp_sock )
{
	int optval;
	int ioptval;
	unsigned int ioptvallen;
	int foptval;
	unsigned int foptvallen;
	int voptval;
	unsigned int voptvallen;
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
		if (optval > maxbuffer){
			if (phase==1) break; 
			else { phase=1; optval >>=1; continue; }
		}
		LOG(L_DBG, "DEBUG: udp_init: trying SO_RCVBUF: %d\n", optval );
		if (setsockopt( udp_sock, SOL_SOCKET, SO_RCVBUF,
			(void*)&optval, sizeof(optval)) ==-1){
			/* Solaris returns -1 if asked size too big; Linux ignores */
			LOG(L_DBG, "DEBUG: udp_init: SOL_SOCKET failed"
					" for %d, phase %d: %s\n", optval, phase, strerror(errno));
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

int udp_init(struct socket_info* sock_info)
{
	union sockaddr_union* addr;
	int optval;


	addr=(union sockaddr_union*)malloc(sizeof(union sockaddr_union));
	if (addr==0){
		LOG(L_ERR, "ERROR: udp_init: out of memory\n");
		goto error;
	}
	
	if (init_su(addr, &sock_info->address, htons(sock_info->port_no))<0){
		LOG(L_ERR, "ERROR: udp_init: could not init sockaddr_union\n");
		goto error;
	}
	/*
	addr->sin_family=AF_INET;
	addr->sin_port=htons(port);
	addr->sin_addr.s_addr=ip;
	*/

	
	sock_info->socket = socket(AF2PF(addr->s.sa_family), SOCK_DGRAM, 0);
	if (sock_info->socket==-1){
		LOG(L_ERR, "ERROR: udp_init: socket: %s\n", strerror(errno));
		goto error;
	}
	/* set sock opts? */
	optval=1;
	if (setsockopt(sock_info->socket, SOL_SOCKET, SO_REUSEADDR ,
					(void*)&optval, sizeof(optval)) ==-1)
	{
		LOG(L_ERR, "ERROR: udp_init: setsockopt: %s\n", strerror(errno));
		goto error;
	}

	if ( probe_max_receive_buffer(sock_info->socket)==-1) goto error;

	if (bind(sock_info->socket,  &addr->s, sizeof(union sockaddr_union))==-1){
		LOG(L_ERR, "ERROR: udp_init: bind(%x, %p, %d) on %s: %s\n",
				sock_info->socket, &addr->s, 
				sizeof(union sockaddr_union),
				sock_info->address_str.s,
				strerror(errno));
	#ifdef USE_IPV6
		if (addr->s.sa_family==AF_INET6)
			LOG(L_ERR, "ERROR: udp_init: might be caused by using a link "
					" local address, try site local or global\n");
	#endif
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

	union sockaddr_union* from;
	unsigned int fromlen;


	from=(union sockaddr_union*) malloc(sizeof(union sockaddr_union));
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
		fromlen=sizeof(union sockaddr_union);
		len=recvfrom(bind_address->socket, buf, BUF_SIZE, 0, &from->s,
											&fromlen);
		if (len==-1){
			LOG(L_ERR, "ERROR: udp_rcv_loop:recvfrom:[%d] %s\n",
						errno, strerror(errno));
			if ((errno==EINTR)||(errno==EAGAIN)||(errno==EWOULDBLOCK))
				continue; /* goto skip;*/
			else goto error;
		}
		/*debugging, make print* msg work */
		buf[len+1]=0;
		
		/* receive_msg must free buf too!*/
		receive_msg(buf, len, from);
		
	/* skip: do other stuff */
		
	}
	/*
	if (from) free(from);
	return 0;
	*/
	
error:
	if (from) free(from);
	return -1;
}



/* which socket to use? main socket or new one? */
int udp_send(struct socket_info *source, char *buf, unsigned len,
				union sockaddr_union*  to, unsigned tolen)
{

	int n;

again:
	n=sendto(source->socket, buf, len, 0, &to->s, tolen);
	if (n==-1){
		LOG(L_ERR, "ERROR: udp_send: sendto(sock,%p,%d,0,%p,%d): %s(%d)\n",
				buf,len,to,tolen,
				strerror(errno),errno);
		if (errno==EINTR) goto again;
		if (errno==EINVAL) {
			LOG(L_CRIT,"CRITICAL: invalid sendtoparameters\n"
			"one possible reason is the server is bound to localhost and\n"
			"attempts to send to the net\n");
		}
	}
	return n;
}
