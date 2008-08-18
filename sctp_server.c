/* 
 * $Id$
 * 
 * Copyright (C) 2008 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/* 
 * sctp one to many 
 */
/*
 * History:
 * --------
 *  2008-08-07  initial version (andrei)
 */

#ifdef USE_SCTP

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/sctp.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>


#include "sctp_server.h"
#include "sctp_options.h"
#include "globals.h"
#include "config.h"
#include "dprint.h"
#include "receive.h"
#include "mem/mem.h"
#include "ip_addr.h"
#include "cfg/cfg_struct.h"



int sctp_init_sock(struct socket_info* sock_info)
{
	union sockaddr_union* addr;
	int optval;
	socklen_t optlen;
	struct addr_info* ai;
	
	addr=&sock_info->su;
	sock_info->proto=PROTO_SCTP;
	if (init_su(addr, &sock_info->address, sock_info->port_no)<0){
		LOG(L_ERR, "ERROR: sctp_init_sock: could not init sockaddr_union for"
					"primary sctp address %.*s:%d\n",
					sock_info->address_str.len, sock_info->address_str.s,
					sock_info->port_no );
		goto error;
	}
	for (ai=sock_info->addr_info_lst; ai; ai=ai->next)
		if (init_su(&ai->su, &ai->address, sock_info->port_no)<0){
			LOG(L_ERR, "ERROR: sctp_init_sock: could not init"
					"backup sctp sockaddr_union for %.*s:%d\n",
					ai->address_str.len, ai->address_str.s,
					sock_info->port_no );
			goto error;
		}
	sock_info->socket = socket(AF2PF(addr->s.sa_family), SOCK_SEQPACKET, 
								IPPROTO_SCTP);
	if (sock_info->socket==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock: socket: %s\n", strerror(errno));
		goto error;
	}
	INFO("sctp: socket %d initialized (%p)\n", sock_info->socket, sock_info);
	/* make socket non-blocking */
#if 0
	/* MSG_WAITALL doesn't work for recvmsg, so use blocking sockets
	 * and send with MSG_DONTWAIT */
	optval=fcntl(sock_info->socket, F_GETFL);
	if (optval==-1){
		LOG(L_ERR, "ERROR: init_sctp: fnctl failed: (%d) %s\n",
				errno, strerror(errno));
		goto error;
	}
	if (fcntl(sock_info->socket, F_SETFL, optval|O_NONBLOCK)==-1){
		LOG(L_ERR, "ERROR: init_sctp: fcntl: set non-blocking failed:"
				" (%d) %s\n", errno, strerror(errno));
		goto error;
	}
#endif

	/* set sock opts */
	/* set receive buffer: SO_RCVBUF*/
	if (sctp_options.sctp_so_rcvbuf){
		optval=sctp_options.sctp_so_rcvbuf;
		if (setsockopt(sock_info->socket, SOL_SCTP, SO_RCVBUF,
					(void*)&optval, sizeof(optval)) ==-1){
			LOG(L_ERR, "ERROR: sctp_init_sock: setsockopt: SO_RCVBUF (%d):"
						" %s\n", optval, strerror(errno));
			/* continue, non-critical */
		}
	}
	
	/* set send buffer: SO_SNDBUF */
	if (sctp_options.sctp_so_sndbuf){
		optval=sctp_options.sctp_so_sndbuf;
		if (setsockopt(sock_info->socket, SOL_SCTP, SO_SNDBUF,
					(void*)&optval, sizeof(optval)) ==-1){
			LOG(L_ERR, "ERROR: sctp_init_sock: setsockopt: SO_SNDBUF (%d):"
						" %s\n", optval, strerror(errno));
			/* continue, non-critical */
		}
	}
	
	/* disable fragments interleave (SCTP_FRAGMENT_INTERLEAVE) --
	 * we don't want partial delivery, so fragment interleave must be off too
	 */
	optval=0;
	if (setsockopt(sock_info->socket, SOL_SCTP, SCTP_FRAGMENT_INTERLEAVE ,
					(void*)&optval, sizeof(optval)) ==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock: setsockopt: %s\n",
						strerror(errno));
		goto error;
	}
	
	/* turn off partial delivery: on linux setting SCTP_PARTIAL_DELIVERY_POINT
	 * to 0 or a very large number seems to be enough, however the portable
	 * way to do it is to set it to the socket receive buffer size
	 * (this is the maximum value allowed in the sctp api draft) */
	optlen=sizeof(optval);
	if (getsockopt(sock_info->socket, SOL_SCTP, SO_RCVBUF,
					(void*)&optval, &optlen) ==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock: getsockopt: %s\n",
						strerror(errno));
		goto error;
	}
	if (setsockopt(sock_info->socket, SOL_SCTP, SCTP_PARTIAL_DELIVERY_POINT,
					(void*)&optval, sizeof(optval)) ==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock: setsockopt: %s\n",
						strerror(errno));
		goto error;
	}
	
	/* nagle / no delay */
	optval=1;
	if (setsockopt(sock_info->socket, SOL_SCTP, SCTP_NODELAY,
					(void*)&optval, sizeof(optval)) ==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock: setsockopt: %s\n",
						strerror(errno));
		/* non critical, try to continue */
	}
	
	/* enable message fragmentation (SCTP_DISABLE_FRAGMENTS)  (on send) */
	optval=0;
	if (setsockopt(sock_info->socket, SOL_SCTP, SCTP_DISABLE_FRAGMENTS,
					(void*)&optval, sizeof(optval)) ==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock: setsockopt: %s\n",
						strerror(errno));
		/* non critical, try to continue */
	}
	
	/* set autoclose */
	optval=sctp_options.sctp_autoclose;
	if (setsockopt(sock_info->socket, SOL_SCTP, SCTP_DISABLE_FRAGMENTS,
					(void*)&optval, sizeof(optval)) ==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock: setsockopt: %s\n",
						strerror(errno));
		/* non critical, try to continue */
	}
	
	/* SCTP_EVENTS for SCTP_SNDRCV (sctp_data_io_event) -> per message
	 *  information in sctp_sndrcvinfo */
	
	/* SCTP_EVENTS for send dried out -> present in the draft not yet
	 * present in linux (might help to detect when we could send again to
	 * some peer, kind of poor's man poll on write, based on received
	 * SCTP_SENDER_DRY_EVENTs */
	
	
	/* bind the addresses  (TODO multiple addresses support)*/
	if (bind(sock_info->socket,  &addr->s, sockaddru_len(*addr))==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock: bind(%x, %p, %d) on %s: %s\n",
				sock_info->socket, &addr->s, 
				(unsigned)sockaddru_len(*addr),
				sock_info->address_str.s,
				strerror(errno));
	#ifdef USE_IPV6
		if (addr->s.sa_family==AF_INET6)
			LOG(L_ERR, "ERROR: sctp_init_sock: might be caused by using a "
							"link local address, try site local or global\n");
	#endif
		goto error;
	}
	for (ai=sock_info->addr_info_lst; ai; ai=ai->next)
		if (sctp_bindx(sock_info->socket, &ai->su.s, 1, SCTP_BINDX_ADD_ADDR)
					==-1){
			LOG(L_ERR, "ERROR: sctp_init_sock: sctp_bindx(%x, %.*s:%d, 1, ...)"
						" on %s:%d : [%d] %s (trying to continue)\n",
						sock_info->socket,
						ai->address_str.len, ai->address_str.s, 
						sock_info->port_no,
						sock_info->address_str.s, sock_info->port_no,
						errno, strerror(errno));
		#ifdef USE_IPV6
			if (ai->su.s.sa_family==AF_INET6)
				LOG(L_ERR, "ERROR: sctp_init_sock: might be caused by using a "
							"link local address, try site local or global\n");
		#endif
			/* try to continue, a secondary address bind failure is not 
			 * critical */
		}
	if (listen(sock_info->socket, 1)<0){
		LOG(L_ERR, "ERROR: sctp_init_sock: listen(%x, 1) on %s: %s\n",
					sock_info->socket, sock_info->address_str.s,
					strerror(errno));
		goto error;
	}
	return 0;
error:
	return -1;
}



int sctp_rcv_loop()
{
	unsigned len;
	static char buf [BUF_SIZE+1];
	char *tmp;
	struct receive_info ri;
	struct sctp_sndrcvinfo* sinfo;
	struct msghdr msg;
	struct iovec iov[1];
	/*struct cmsghdr* cmsg; */
	char cbuf[CMSG_SPACE(sizeof(*sinfo))];

	
	ri.bind_address=bind_address; /* this will not change */
	ri.dst_port=bind_address->port_no;
	ri.dst_ip=bind_address->address;
	ri.proto=PROTO_SCTP;
	ri.proto_reserved1=ri.proto_reserved2=0;
	
	iov[0].iov_base=buf;
	iov[0].iov_len=BUF_SIZE;
	msg.msg_iov=iov;
	msg.msg_iovlen=1;
	msg.msg_control=cbuf;
	msg.msg_controllen=sizeof(cbuf);
	msg.msg_flags=0;
	

	/* initialize the config framework */
	if (cfg_child_init()) goto error;
	
	for(;;){
		/* recv
		 * recvmsg must be used because the socket is non-blocking
		 * and we want MSG_WAITALL */
		msg.msg_name=&ri.src_su.s;
		msg.msg_namelen=sockaddru_len(bind_address->su);

		len=recvmsg(bind_address->socket, &msg, MSG_WAITALL);
		/* len=sctp_recvmsg(bind_address->socket, buf, BUF_SIZE, &ri.src_su.s,
							&msg.msg_namelen, &sinfo, &msg.msg_flags); */
		if (len==-1){
			if (errno==EAGAIN){
				DBG("sctp_rcv_loop: EAGAIN on sctp socket\n");
				continue;
			}
			LOG(L_ERR, "ERROR: sctp_rcv_loop: sctp_recvmsg on %d (%p):"
						"[%d] %s\n", bind_address->socket, bind_address,
						errno, strerror(errno));
			if ((errno==EINTR)||(errno==EWOULDBLOCK)|| (errno==ECONNREFUSED))
				continue; /* goto skip;*/
			else goto error;
		}
		if (unlikely(msg.msg_flags & MSG_NOTIFICATION)){
			/* intercept usefull notifications: TODO */
			DBG("sctp_rcv_loop: MSG_NOTIFICATION\n");
			/* notification in CMSG data */
			continue;
		}else if (unlikely(!(msg.msg_flags & MSG_EOR))){
			LOG(L_ERR, "ERROR: sctp_rcv_loop: partial delivery not"
						"supported\n");
			continue;
		}
		/* we  0-term the messages for debugging */
		buf[len]=0; /* no need to save the previous char */
		su2ip_addr(&ri.src_ip, &ri.src_su);
		ri.src_port=su_getport(&ri.src_su);

		/* sanity checks */
		if (len<MIN_SCTP_PACKET) {
			tmp=ip_addr2a(&ri.src_ip);
			DBG("sctp_rcv_loop: probing packet received from %s %d\n",
					tmp, htons(ri.src_port));
			continue;
		}
		if (ri.src_port==0){
			tmp=ip_addr2a(&ri.src_ip);
			LOG(L_INFO, "sctp_rcv_loop: dropping 0 port packet from %s\n",
						tmp);
			continue;
		}
	
		/* update the local config */
		cfg_update();
		receive_msg(buf, len, &ri);
	}
error:
	return -1;
}


/* send buf:len over udp to dst (uses only the to and send_sock dst members)
 * returns the numbers of bytes sent on success (>=0) and -1 on error
 */
int sctp_msg_send(struct dest_info* dst, char* buf, unsigned len)
{
	int n;
	int tolen;
	struct ip_addr ip; /* used only on error, for debugging */
	struct msghdr msg;
	struct iovec iov[1];
	
	tolen=sockaddru_len(dst->to);
	iov[0].iov_base=buf;
	iov[0].iov_len=len;
	msg.msg_iov=iov;
	msg.msg_iovlen=1;
	msg.msg_name=&dst->to.s;
	msg.msg_namelen=tolen;
	msg.msg_control=0;
	msg.msg_controllen=0;
	msg.msg_flags=SCTP_UNORDERED;
again:
	n=sendmsg(dst->send_sock->socket, &msg, MSG_DONTWAIT);
#if 0
	n=sctp_sendmsg(dst->send_sock->socket, buf, len, &dst->to.s, tolen,
					0 /* ppid */, SCTP_UNORDERED /* | SCTP_EOR */ /* flags */,
					0 /* stream */, sctp_options.sctp_send_ttl /* ttl */,
					0 /* context */);
#endif
	if (n==-1){
		su2ip_addr(&ip, &dst->to);
		LOG(L_ERR, "ERROR: sctp_msg_send: sendmsg(sock,%p,%d,0,%s:%d,%d):"
				" %s(%d)\n", buf, len, ip_addr2a(&ip), su_getport(&dst->to),
				tolen, strerror(errno),errno);
		if (errno==EINTR) goto again;
		if (errno==EINVAL) {
			LOG(L_CRIT,"CRITICAL: invalid sendmsg parameters\n"
			"one possible reason is the server is bound to localhost and\n"
			"attempts to send to the net\n");
		}else if (errno==EAGAIN || errno==EWOULDBLOCK){
			LOG(L_ERR, "ERROR: sctp_msg_send: failed to send, send buffers"
						" full\n");
			/* TODO: fix blocking writes */
		}
	}
	return n;
}



void destroy_sctp()
{
}

#endif /* USE_SCTP */
