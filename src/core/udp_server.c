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
 *
 */


/** Kamailio core :: udp send and loop-receive functions.
 * @file udp_server.c
 * @ingroup core
 * Module: @ref core
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <errno.h>
#include <arpa/inet.h>
#ifdef __linux__
	#include <linux/types.h>
	#include <linux/errqueue.h>
#endif


#include "udp_server.h"
#include "compiler_opt.h"
#include "globals.h"
#include "config.h"
#include "dprint.h"
#include "receive.h"
#include "mem/mem.h"
#include "ip_addr.h"
#include "socket_info.h"
#include "cfg/cfg_struct.h"
#include "events.h"
#include "stun.h"
#ifdef USE_RAW_SOCKS
#include "raw_sock.h"
#endif /* USE_RAW_SOCKS */
#ifdef USE_MCAST
#include <net/if.h>
#endif /* USE_MCAST */


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


	/* is there a zero character in there ? */
	if (memchr(buf, 0, len)) {
		LM_CRIT("message with 0 in it\n");
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
								LM_CRIT("too many spaces\n");
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
		LM_ERR("fd: %d getsockopt: %s\n", udp_sock, strerror(errno));
		return -1;
	}
	if ( ioptval==0 )
	{
		LM_DBG("SO_RCVBUF initially set to 0 for fd %d; resetting to %d\n",
				udp_sock, BUFFER_INCREMENT);
		ioptval=BUFFER_INCREMENT;
	} else LM_INFO("SO_RCVBUF is initially %d for fd %d\n", ioptval, udp_sock);
	for (optval=ioptval; ;  ) {
		/* increase size; double in initial phase, add linearly later */
		if (phase==0) optval <<= 1; else optval+=BUFFER_INCREMENT;
		if (optval > maxbuffer){
			if (phase==1) break;
			else { phase=1; optval >>=1; continue; }
		}
		if(ksr_verbose_startup)
			LM_DBG("trying SO_RCVBUF: %d on fd: %d\n", optval, udp_sock);
		if (setsockopt( udp_sock, SOL_SOCKET, SO_RCVBUF,
			(void*)&optval, sizeof(optval)) ==-1){
			/* Solaris returns -1 if asked size too big; Linux ignores */
			LM_DBG("SOL_SOCKET failed for val %d on fd %d, phase %d: %s\n",
					optval, udp_sock, phase, strerror(errno));
			/* if setting buffer size failed and still in the aggressive
			   phase, try less aggressively; otherwise give up
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
			LM_ERR("fd: %d getsockopt: %s\n", udp_sock, strerror(errno));
			return -1;
		} else {
			if(ksr_verbose_startup)
				LM_DBG("setting SO_RCVBUF on fd %d; val=%d, verify=%d\n",
						udp_sock, optval, voptval);
			if (voptval<optval) {
				LM_DBG("setting SO_RCVBUF on fd %d has no effect\n", udp_sock);
				/* if setting buffer size failed and still in the aggressive
				phase, try less aggressively; otherwise give up
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
		LM_ERR("fd: %d getsockopt: %s\n", udp_sock, strerror(errno));
		return -1;
	}
	LM_INFO("SO_RCVBUF is finally %d on fd %d\n", foptval, udp_sock);

	return 0;

	/* EoJKU */
}


#ifdef USE_MCAST

/*
 * Setup multicast receiver
 */
static int setup_mcast_rcvr(int sock, union sockaddr_union* addr, char* interface)
{
#ifdef HAVE_IP_MREQN
	struct ip_mreqn mreq;
#else
	struct ip_mreq mreq;
#endif
	struct ipv6_mreq mreq6;

	if (addr->s.sa_family==AF_INET){
		memcpy(&mreq.imr_multiaddr, &addr->sin.sin_addr,
		       sizeof(struct in_addr));
#ifdef HAVE_IP_MREQN
		if (interface!=0) {
			mreq.imr_ifindex = if_nametoindex(interface);
		} else {
			mreq.imr_ifindex = 0;
		}
		mreq.imr_address.s_addr = htonl(INADDR_ANY);
#else
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);
#endif

		if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,&mreq,
			       sizeof(mreq))==-1){
			LM_ERR("setsockopt: %s\n", strerror(errno));
			return -1;
		}

	} else if (addr->s.sa_family==AF_INET6){
		memcpy(&mreq6.ipv6mr_multiaddr, &addr->sin6.sin6_addr,
		       sizeof(struct in6_addr));
		if (interface!=0) {
			mreq6.ipv6mr_interface = if_nametoindex(interface);
		} else {
			mreq6.ipv6mr_interface = 0;
		}
#ifdef __OS_linux
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq6,
#else
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq6,
#endif
			       sizeof(mreq6))==-1){
			LM_ERR("setsockopt:%s\n", strerror(errno));
			return -1;
		}

	} else {
		LM_ERR("setup_mcast_rcvr: Unsupported protocol family\n");
		return -1;
	}
	return 0;
}

#endif /* USE_MCAST */


int udp_init(struct socket_info* sock_info)
{
	union sockaddr_union* addr;
	int optval;
#ifdef USE_MCAST
	unsigned char m_ttl, m_loop;
#endif
	addr=&sock_info->su;
/*
	addr=(union sockaddr_union*)pkg_malloc(sizeof(union sockaddr_union));
	if (addr==0){
		PKG_MEM_ERROR;
		goto error;
	}
*/
	sock_info->proto=PROTO_UDP;
	if (init_su(addr, &sock_info->address, sock_info->port_no)<0){
		LM_ERR("could not init sockaddr_union\n");
		goto error;
	}

	sock_info->socket = socket(AF2PF(addr->s.sa_family), SOCK_DGRAM, 0);
	if (sock_info->socket==-1){
		LM_ERR("socket: %s\n", strerror(errno));
		goto error;
	}
	/* set sock opts? */
	optval=1;
	if (setsockopt(sock_info->socket, SOL_SOCKET, SO_REUSEADDR ,
					(void*)&optval, sizeof(optval)) ==-1){
		LM_ERR("setsockopt: %s\n", strerror(errno));
		goto error;
	}
	/* tos */
	optval = tos;
	if (addr->s.sa_family==AF_INET){
		if (setsockopt(sock_info->socket, IPPROTO_IP, IP_TOS, (void*)&optval,
				sizeof(optval)) ==-1){
			LM_WARN("setsockopt tos: %s\n", strerror(errno));
			/* continue since this is not critical */
		}
	} else if (addr->s.sa_family==AF_INET6){
		if (setsockopt(sock_info->socket, IPPROTO_IPV6, IPV6_TCLASS,
					(void*)&optval, sizeof(optval)) ==-1) {
			LM_WARN("setsockopt v6 tos: %s\n", strerror(errno));
			/* continue since this is not critical */
		}
		if(sr_bind_ipv6_link_local!=0) {
			LM_INFO("setting scope of %s\n", sock_info->address_str.s);
			addr->sin6.sin6_scope_id =
					ipv6_get_netif_scope(sock_info->address_str.s);
		}
	}

#if defined (__OS_linux) && defined(UDP_ERRORS)
	optval=1;
	/* enable error receiving on unconnected sockets */
	if(setsockopt(sock_info->socket, SOL_IP, IP_RECVERR,
					(void*)&optval, sizeof(optval)) ==-1){
		LM_ERR("setsockopt: %s\n", strerror(errno));
		goto error;
	}
#endif
#if defined (__OS_linux)
	if (addr->s.sa_family==AF_INET){
		/* If pmtu_discovery=1 then set DF bit and do Path MTU discovery
		 * disabled by default.  Specific to IPv4. */
		optval= (pmtu_discovery) ? IP_PMTUDISC_DO : IP_PMTUDISC_DONT;
		if(setsockopt(sock_info->socket, IPPROTO_IP, IP_MTU_DISCOVER,
				(void*)&optval, sizeof(optval)) ==-1){
			LM_ERR("setsockopt: %s\n", strerror(errno));
			goto error;
		}
	} else if (addr->s.sa_family==AF_INET6){
		/* Since IPv6 never fragments but sends ICMPv6 Packet too Big,
		 * we always want to benefit from kernel Path MTU knowledge;
		 * especially because our sockets are unconnected and cannot
		 * learn this for themselves.  Details in Issue #3119.  */
		optval= IPV6_PMTUDISC_WANT;
		if(setsockopt(sock_info->socket, IPPROTO_IPV6,
				IPV6_MTU_DISCOVER,
				(void*)&optval, sizeof(optval)) ==-1){
			LM_ERR("setsockopt: %s\n", strerror(errno));
			goto error;
		}
	}
#endif

#if defined(IP_FREEBIND)
	/* allow bind to non local address.
	 * useful when daemon started before network initialized */
	optval = 1;
	if (_sr_ip_free_bind && setsockopt(sock_info->socket, IPPROTO_IP,
				IP_FREEBIND, (void*)&optval, sizeof(optval)) ==-1) {
		LM_WARN("setsockopt freebind failed: %s\n", strerror(errno));
		/* continue since this is not critical */
	}
#endif
#ifdef USE_MCAST
	if ((sock_info->flags & SI_IS_MCAST)
	    && (setup_mcast_rcvr(sock_info->socket, addr, sock_info->mcast.s)<0)){
			goto error;
	}
	/* set the multicast options */
	if (addr->s.sa_family==AF_INET){
		m_loop=mcast_loopback;
		if (setsockopt(sock_info->socket, IPPROTO_IP, IP_MULTICAST_LOOP,
						&m_loop, sizeof(m_loop))==-1){
			LM_WARN("setsockopt(IP_MULTICAST_LOOP): %s\n", strerror(errno));
			/* it's only a warning because we might get this error if the
			  network interface doesn't support multicasting -- andrei */
		}
		if (mcast_ttl>=0){
			m_ttl=mcast_ttl;
			if (setsockopt(sock_info->socket, IPPROTO_IP, IP_MULTICAST_TTL,
						&m_ttl, sizeof(m_ttl))==-1){
				LM_WARN("setsockopt (IP_MULTICAST_TTL): %s\n", strerror(errno));
			}
		}
	} else if (addr->s.sa_family==AF_INET6){
		if (setsockopt(sock_info->socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
						&mcast_loopback, sizeof(mcast_loopback))==-1){
			LM_WARN("setsockopt (IPV6_MULTICAST_LOOP): %s\n", strerror(errno));
		}
		if (mcast_ttl>=0){
			if (setsockopt(sock_info->socket, IPPROTO_IP, IPV6_MULTICAST_HOPS,
							&mcast_ttl, sizeof(mcast_ttl))==-1){
				LM_WARN("setssckopt (IPV6_MULTICAST_HOPS): %s\n", strerror(errno));
			}
		}
	} else {
		LM_ERR("Unsupported protocol family %d\n", addr->s.sa_family);
		goto error;
	}
#endif /* USE_MCAST */

	if ( probe_max_receive_buffer(sock_info->socket)==-1) goto error;

	if (bind(sock_info->socket,  &addr->s, sockaddru_len(*addr))==-1){
		LM_ERR("bind(%x, %p, %d) on %s: %s\n",
				sock_info->socket, &addr->s,
				(unsigned)sockaddru_len(*addr),
				sock_info->address_str.s,
				strerror(errno));
		if (addr->s.sa_family==AF_INET6) {
			LM_ERR("might be caused by using a link local address, is 'bind_ipv6_link_local' set?\n");
		}
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
	static char buf [BUF_SIZE+1];
	char *tmp;
	union sockaddr_union* fromaddr;
	unsigned int fromaddrlen;
	receive_info_t rcvi;
	sr_event_param_t evp = {0};
#define UDP_RCV_PRINTBUF_SIZE 512
#define UDP_RCV_PRINT_LEN 100
	char printbuf[UDP_RCV_PRINTBUF_SIZE];
	int i;
	int j;
	int l;


	fromaddr=(union sockaddr_union*) pkg_malloc(sizeof(union sockaddr_union));
	if (fromaddr==0){
		PKG_MEM_ERROR;
		goto error;
	}
	memset(fromaddr, 0,sizeof(union sockaddr_union));
	memset(&rcvi, 0, sizeof(receive_info_t));
	/* these do not change, set only once*/
	rcvi.bind_address=bind_address;
	rcvi.dst_port=bind_address->port_no;
	rcvi.dst_ip=bind_address->address;
	rcvi.proto=PROTO_UDP;

	/* initialize the config framework */
	if (cfg_child_init()) goto error;

	for(;;){
		fromaddrlen=sizeof(union sockaddr_union);
		len=recvfrom(bind_address->socket, buf, BUF_SIZE, 0,
				(struct sockaddr*)fromaddr, &fromaddrlen);
		if (len==-1){
			if (errno==EAGAIN){
				LM_DBG("packet with bad checksum received\n");
				continue;
			}
			LM_ERR("recvfrom:[%d] %s\n", errno, strerror(errno));
			if ((errno==EINTR)||(errno==EWOULDBLOCK)|| (errno==ECONNREFUSED))
				continue; /* goto skip;*/
			else goto error;
		}
		if(fromaddrlen != (unsigned int)sockaddru_len(bind_address->su)) {
			LM_ERR("ignoring data - unexpected from addr len: %u != %u\n",
					fromaddrlen, (unsigned int)sockaddru_len(bind_address->su));
			continue;
		}
		/* we must 0-term the messages, receive_msg expects it */
		buf[len]=0; /* no need to save the previous char */

		if(is_printable(L_DBG) && len>10) {
			j = 0;
			for(i=0; i<len && i<UDP_RCV_PRINT_LEN
						&& j+8<UDP_RCV_PRINTBUF_SIZE; i++) {
				if(isprint(buf[i])) {
					printbuf[j++] = buf[i];
				} else {
					l = snprintf(printbuf+j, 6, " %02X ", (unsigned char)buf[i]);
					if(l<0 || l>=6) {
						LM_ERR("print buffer building failed (%d/%d/%d)\n",
								l, j, i);
						continue; /* skip it */
					}
					j += l;
				}
			}
			LM_DBG("received on udp socket: (%d/%d/%d) [[%.*s]]\n",
					j, i, len, j, printbuf);
		}
		rcvi.src_su=*fromaddr;
		su2ip_addr(&rcvi.src_ip, fromaddr);
		rcvi.src_port=su_getport(fromaddr);

		if(unlikely(sr_event_enabled(SREV_NET_DGRAM_IN)))
		{
			void *sredp[3];
			sredp[0] = (void*)buf;
			sredp[1] = (void*)(&len);
			sredp[2] = (void*)(&rcvi);
			evp.data = (void*)sredp;
			if(sr_event_exec(SREV_NET_DGRAM_IN, &evp)<0) {
				/* data handled by callback - continue to next packet */
				continue;
			}
		}
#ifndef NO_ZERO_CHECKS
		if (!unlikely(sr_event_enabled(SREV_STUN_IN)) || (unsigned char)*buf != 0x00) {
			if (len<MIN_UDP_PACKET) {
				tmp=ip_addr2a(&rcvi.src_ip);
				LM_DBG("probing packet received from %s %d\n", tmp, htons(rcvi.src_port));
				continue;
			}
		}
#endif
#ifdef DBG_MSG_QA
		if (!dbg_msg_qa(buf, len)) {
			LM_WARN("an incoming message didn't pass test,"
						"  drop it: %.*s\n", len, buf );
			continue;
		}
#endif
		if (rcvi.src_port==0){
			tmp=ip_addr2a(&rcvi.src_ip);
			LM_INFO("dropping 0 port packet from %s\n", tmp);
			continue;
		}

		/* update the local config */
		cfg_update();
		if (unlikely(sr_event_enabled(SREV_STUN_IN)) && (unsigned char)*buf == 0x00) {
			/* stun_process_msg releases buf memory if necessary */
			if ((stun_process_msg(buf, len, &rcvi)) != 0) {
				continue; /* some error occurred */
			}
		} else {
			/* receive_msg must free buf too!*/
			receive_msg(buf, len, &rcvi);
		}

	/* skip: do other stuff */

	}
	/*
	if (fromaddr) pkg_free(fromaddr);
	return 0;
	*/

error:
	if (fromaddr) pkg_free(fromaddr);
	return -1;
}




/* send buf:len over udp to dst (uses only the to and send_sock dst members)
 * returns the numbers of bytes sent on success (>=0) and -1 on error
 */
int udp_send(struct dest_info* dst, char *buf, unsigned len)
{

	int n;
	int tolen;
	struct ip_addr ip; /* used only on error, for debugging */
#ifdef USE_RAW_SOCKS
	int mtu;
#endif /* USE_RAW_SOCKS */

#ifdef DBG_MSG_QA
	/* aborts on error, does nothing otherwise */
	if (!dbg_msg_qa( buf, len )) {
		LM_ERR("dbg_msg_qa failed\n");
		abort();
	}
#endif
#ifdef USE_RAW_SOCKS
	if (likely( ! (raw_udp4_send_sock >= 0 &&
					cfg_get(core, core_cfg, udp4_raw) &&
					dst->send_sock->address.af == AF_INET) )) {
#endif /* USE_RAW_SOCKS */
		/* normal send over udp socket */
		tolen=sockaddru_len(dst->to);
again:
		n=sendto(dst->send_sock->socket, buf, len, 0, &dst->to.s, tolen);
#ifdef XL_DEBUG
		LM_INFO("send status: %d\n", n);
#endif
		if (unlikely(n==-1)){
			su2ip_addr(&ip, &dst->to);
			LM_ERR("sendto(sock, buf: %p, len: %u, 0, dst: (%s:%d), tolen: %d)"
					" - err: %s (%d)\n",
					buf,len, ip_addr2a(&ip),
					su_getport(&dst->to), tolen, strerror(errno), errno);
			if (errno==EINTR) goto again;
			if (errno==EINVAL) {
				LM_CRIT("invalid sendtoparameters\n"
				"one possible reason is the server is bound to localhost and\n"
				"attempts to send to the net\n");
			}
		}
#ifdef USE_RAW_SOCKS
	} else {
		/* send over a raw socket */
		mtu = cfg_get(core, core_cfg, udp4_raw_mtu);
raw_again:
		n=raw_iphdr_udp4_send(raw_udp4_send_sock, buf, len,
								&dst->send_sock->su,
								&dst->to,
								mtu);
		if (unlikely(n==-1)){
			su2ip_addr(&ip, &dst->to);
			LM_ERR("raw_iphdr_udp4_send(%d,%p,%u,...,%s:%d,%d): %s(%d)\n",
					raw_udp4_send_sock, buf,len, ip_addr2a(&ip),
					su_getport(&dst->to), mtu, strerror(errno), errno);
			if (errno==EINTR) goto raw_again;
		}
	}
#endif /* USE_RAW_SOCKS */
	return n;
}
