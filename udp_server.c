/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History
 * --------
 *  2003-01-28  packet zero-termination moved to receive_msg (jiri)
 *  2003-02-10  undoed the above changes (andrei)
 *  2003-03-19  replaced all the mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 */


#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#ifdef __linux__
	#include <linux/types.h>
	#include <linux/errqueue.h>
#endif


#include "udp_server.h"
#include "globals.h"
#include "config.h"
#include "dprint.h"
#include "receive.h"
#include "mem/mem.h"
#include "ip_addr.h"


#ifdef DBG_MSG_QA
/* message quality assurance -- frequently, bugs in ser have
   been indicated by zero characters or long whitespaces
   in generated messages; this debugging option aborts if
   any such message is sighted
*/
static int dbg_msg_qa(char *buf, int len)
{
#define _DBG_WS_LEN 3
#define _DBG_WS "   "

	char *scan;
	int my_len;
	int space_cnt;
	enum { QA_ANY, QA_SPACE, QA_EOL1 } state;


	/* is there a zero character inthere ? */	
	if (memchr(buf, 0, len)) {
		LOG(L_CRIT, "BUG: message with 0 in it\n");
		return 0;
	}

	my_len=len;
	scan=buf;
	state=QA_ANY;
	space_cnt=0;

	while(my_len) {
		switch(*scan) {
			case ' ':	if (state==QA_SPACE) {
							space_cnt++;
							if (space_cnt==4) {
								LOG(L_CRIT, "BUG(propably): DBG_MSG_QA: "
									"too many spaces\n");
								return 0;
							}
						} else space_cnt=0;
						state=QA_SPACE; 
						break;

			case '\r':	/* ignore */
						space_cnt=0;
						break;

			case '\n': /* don't proceed to body on EoH */
						if (state==QA_EOL1) goto qa_passed;
						space_cnt=0;
						state=QA_EOL1;
						break;

			default:	space_cnt=0;
						state=QA_ANY;
						break;
		}
		scan++;
		my_len--;
	}


qa_passed:
	return 1;
}

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

	addr=&sock_info->su;
/*
	addr=(union sockaddr_union*)pkg_malloc(sizeof(union sockaddr_union));
	if (addr==0){
		LOG(L_ERR, "ERROR: udp_init: out of memory\n");
		goto error;
	}
*/
	sock_info->proto=PROTO_UDP;
	if (init_su(addr, &sock_info->address, sock_info->port_no)<0){
		LOG(L_ERR, "ERROR: udp_init: could not init sockaddr_union\n");
		goto error;
	}
	
	sock_info->socket = socket(AF2PF(addr->s.sa_family), SOCK_DGRAM, 0);
	if (sock_info->socket==-1){
		LOG(L_ERR, "ERROR: udp_init: socket: %s\n", strerror(errno));
		goto error;
	}
	/* set sock opts? */
	optval=1;
	if (setsockopt(sock_info->socket, SOL_SOCKET, SO_REUSEADDR ,
					(void*)&optval, sizeof(optval)) ==-1){
		LOG(L_ERR, "ERROR: udp_init: setsockopt: %s\n", strerror(errno));
		goto error;
	}
#if defined (__linux__) && defined(UDP_ERRORS)
	optval=1;
	/* enable error receiving on unconnected sockets */
	if(setsockopt(sock_info->socket, SOL_IP, IP_RECVERR,
					(void*)&optval, sizeof(optval)) ==-1){
		LOG(L_ERR, "ERROR: udp_init: setsockopt: %s\n", strerror(errno));
		goto error;
	}
#endif


	if ( probe_max_receive_buffer(sock_info->socket)==-1) goto error;
	
	if (bind(sock_info->socket,  &addr->s, sockaddru_len(*addr))==-1){
		LOG(L_ERR, "ERROR: udp_init: bind(%x, %p, %d) on %s: %s\n",
				sock_info->socket, &addr->s, 
				sockaddru_len(*addr),
				sock_info->address_str.s,
				strerror(errno));
	#ifdef USE_IPV6
		if (addr->s.sa_family==AF_INET6)
			LOG(L_ERR, "ERROR: udp_init: might be caused by using a link "
					" local address, try site local or global\n");
	#endif
		goto error;
	}

/*	pkg_free(addr);*/
	return 0;

error:
/*	if (addr) pkg_free(addr);*/
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
	struct receive_info ri;


	from=(union sockaddr_union*) pkg_malloc(sizeof(union sockaddr_union));
	if (from==0){
		LOG(L_ERR, "ERROR: udp_rcv_loop: out of memory\n");
		goto error;
	}
	memset(from, 0 , sizeof(union sockaddr_union));
	ri.bind_address=bind_address; /* this will not change, we do it only once*/
	ri.dst_port=bind_address->port_no;
	ri.dst_ip=bind_address->address;
	ri.proto=PROTO_UDP;
	ri.proto_reserved1=ri.proto_reserved2=0;
	for(;;){
#ifdef DYN_BUF
		buf=pkg_malloc(BUF_SIZE+1);
		if (buf==0){
			LOG(L_ERR, "ERROR: udp_rcv_loop: could not allocate receive"
					 " buffer\n");
			goto error;
		}
#endif
		fromlen=sockaddru_len(bind_address->su);
		len=recvfrom(bind_address->socket, buf, BUF_SIZE, 0, &from->s,
											&fromlen);
		if (len==-1){
			LOG(L_ERR, "ERROR: udp_rcv_loop:recvfrom:[%d] %s\n",
						errno, strerror(errno));
			if ((errno==EINTR)||(errno==EAGAIN)||(errno==EWOULDBLOCK)||
					(errno==ECONNREFUSED))
				continue; /* goto skip;*/
			else goto error;
		}
		/* we must 0-term the messages, receive_msg expects it */
		buf[len]=0; /* no need to save the previous char */

#ifndef NO_ZERO_CHECKS
		if (len==0) {
			LOG(L_WARN, "WARNING: empty packet received\n");
			continue;
		}
		if (buf[len-1]==0) {
			LOG(L_WARN, "WARNING: upstream bug - 0-terminated packet\n");
			len--;
		}
#endif
#ifdef DBG_MSG_QA
		if (!dbg_msg_qa(buf, len)) {
			LOG(L_WARN, "WARNING: an incoming message didn't pass test,"
						"  drop it: %.*s\n", len, buf );
			continue;
		}
#endif
		ri.src_su=*from;
		su2ip_addr(&ri.src_ip, from);
		ri.src_port=su_getport(from);
		
		
		/* receive_msg must free buf too!*/
		receive_msg(buf, len, &ri);
		
	/* skip: do other stuff */
		
	}
	/*
	if (from) pkg_free(from);
	return 0;
	*/
	
error:
	if (from) pkg_free(from);
	return -1;
}




/* which socket to use? main socket or new one? */
int udp_send(struct socket_info *source, char *buf, unsigned len,
										union sockaddr_union*  to)
{

	int n;
	int tolen;

#ifdef DBG_MSG_QA
	/* aborts on error, does nothing otherwise */
	if (!dbg_msg_qa( buf, len )) {
		LOG(L_ERR, "ERROR: udp_send: dbg_msg_qa failed\n");
		abort();
	}
#endif

	tolen=sockaddru_len(*to);
again:
	n=sendto(source->socket, buf, len, 0, &to->s, tolen);
#ifdef XL_DEBUG
	LOG(L_INFO, "INFO: send status: %d\n", n);
#endif
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
