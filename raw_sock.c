/* 
 * $Id$
 *
 * Copyright (C) 2010 iptelorg GmbH
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
/** raw socket functions.
 *  @file raw_sock.c
 *  @ingroup core
 *  Module: @ref core
 */
/* 
 * History:
 * --------
 *  2010-06-07  initial version (from older code) andrei
 */
/*
 * FIXME: IP_PKTINFO & IP_HDRINCL - linux specific
 * FIXME: linux specific iphdr and udphdr
 * FIXME: send support for IP_HDRINCL
 */

#ifdef USE_RAW_SOCKS

#include "compiler_opt.h"
#include "ip_addr.h"
#include "dprint.h"
#include "str.h"
#include "ut.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

#include "raw_sock.h"


/** create and return a raw socket.
 * @param proto - protocol used (e.g. IPPROTO_UDP, IPPROTO_RAW)
 * @param ip - if not null the socket will be bound on this ip.
 * @param iface - if not null the socket will be bound to this interface
 *                (SO_BINDTODEVICE).
 * @param iphdr_incl - set to 1 if packets send on this socket include
 *                     a pre-built ip header (some fields, like the checksum
 *                     will still be filled by the kernel, OTOH packet
 *                     fragmentation has to be done in user space).
 * @return socket on success, -1 on error
 */
int raw_socket(int proto, struct ip_addr* ip, str* iface, int iphdr_incl)
{
	int sock;
	int t;
	union sockaddr_union su;
	char short_ifname[sizeof(int)];
	int ifname_len;
	char* ifname;

	sock = socket(PF_INET, SOCK_RAW, proto);
	if (sock==-1){
		ERR("raw_socket: socket() failed: %s [%d]\n",
				strerror(errno), errno);
		goto error;
	}
	/* set socket options */
	if (iphdr_incl) {
		t=1;
		if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &t, sizeof(t))<0){
			ERR("raw_socket: setsockopt(IP_HDRINCL) failed: %s [%d]\n",
					strerror(errno), errno);
			goto error;
		}
	} else {
		/* IP_PKTINFO makes no sense if the ip header is included */
		/* using IP_PKTINFO */
		t=1;
		if (setsockopt(sock, IPPROTO_IP, IP_PKTINFO, &t, sizeof(t))<0){
			ERR("raw_socket: setsockopt(IP_PKTINFO) failed: %s [%d]\n",
					strerror(errno), errno);
			goto error;
		}
	}
	t=IP_PMTUDISC_DONT;
	if(setsockopt(sock, IPPROTO_IP, IP_MTU_DISCOVER, &t, sizeof(t)) ==-1){
		LOG(L_ERR, "raw_socket: setsockopt(IP_MTU_DISCOVER): %s\n",
				strerror(errno));
		goto error;
	}
	if (iface && iface->s){
		/* workaround for linux bug: arg to setsockopt must have at least
		 * sizeof(int) size or EINVAL would be returned */
		if (iface->len<sizeof(int)){
			memcpy(short_ifname, iface->s, iface->len);
			short_ifname[iface->len]=0; /* make sure it's zero term */
			ifname_len=sizeof(short_ifname);
			ifname=short_ifname;
		}else{
			ifname_len=iface->len;
			ifname=iface->s;
		}
		if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, ifname, ifname_len)
						<0){
				ERR("raw_socket: could not bind to %.*s: %s [%d]\n",
							iface->len, ZSW(iface->s), strerror(errno), errno);
				goto error;
		}
	}
	/* FIXME: probe_max_receive_buffer(sock) missing */
	if (ip){
		init_su(&su, ip, 0);
		if (bind(sock, &su.s, sockaddru_len(su))==-1){
			ERR("raw_socket: bind(%s) failed: %s [%d]\n",
				ip_addr2a(ip), strerror(errno), errno);
			goto error;
		}
	}
	return sock;
error:
	if (sock!=-1) close(sock);
	return -1;
}



/** create and return an udp over ipv4  raw socket.
 * @param ip - if not null the socket will be bound on this ip.
 * @param iface - if not null the socket will be bound to this interface
 *                (SO_BINDTODEVICE).
 * @param iphdr_incl - set to 1 if packets send on this socket include
 *                     a pre-built ip header (some fields, like the checksum
 *                     will still be filled by the kernel, OTOH packet
 *                     fragmentation has to be done in user space).
 * @return socket on success, -1 on error
 */
int raw_udp4_socket(struct ip_addr* ip, str* iface, int iphdr_incl)
{
	return raw_socket(IPPROTO_UDP, ip, iface, iphdr_incl);
}



/** receives an ipv4 packet suing a raw socket.
 * An ipv4 packet is received in buf, using IP_PKTINFO.
 * from and to are filled (only the ip part the ports are 0 since this
 * function doesn't try to look beyond the IP level).
 * @param sock - raw socket
 * @param buf - detination buffer.
 * @param len - buffer len (should be enough for receiving a packet +
 *               IP header).
 * @param from - result parameter, the IP address part of it will be filled
 *                with the source address and the port with 0.
 * @param to - result parameter, the IP address part of it will be filled
 *                with the destination (local) address and the port with 0.
 * @return packet len or <0 on error: -1 (check errno),
 *        -2 no IP_PKTINFO found or AF mismatch
 */
int recvpkt4(int sock, char* buf, int len, union sockaddr_union* from,
					union sockaddr_union* to)
{
	struct iovec iov[1];
	struct msghdr rcv_msg;
	struct cmsghdr* cmsg;
	struct in_pktinfo* rcv_pktinfo;
	int n, ret;
	char msg_ctrl_buf[1024];

	iov[0].iov_base=buf;
	iov[0].iov_len=len;
	rcv_msg.msg_name=from;
	rcv_msg.msg_namelen=sockaddru_len(*from);
	rcv_msg.msg_control=msg_ctrl_buf;
	rcv_msg.msg_controllen=sizeof(msg_ctrl_buf);
	rcv_msg.msg_iov=&iov[0];
	rcv_msg.msg_iovlen=1;
	ret=-2; /* no PKT_INFO or AF mismatch */
retry:
	n=recvmsg(sock, &rcv_msg, MSG_WAITALL);
	if (unlikely(n==-1)){
		if (errno==EINTR)
			goto retry;
		ret=n;
		goto end;
	}
	/* find the pkt info */
	rcv_pktinfo=0;
	for (cmsg=CMSG_FIRSTHDR(&rcv_msg); cmsg; cmsg=CMSG_NXTHDR(&rcv_msg, cmsg)){
		if (likely((cmsg->cmsg_level==IPPROTO_IP) &&
					(cmsg->cmsg_type==IP_PKTINFO))) {
			rcv_pktinfo=(struct in_pktinfo*)CMSG_DATA(cmsg);
			to->sin.sin_family=AF_INET;
			memcpy(&to->sin.sin_addr, &rcv_pktinfo->ipi_spec_dst.s_addr, 
									sizeof(to->sin.sin_addr));
			to->sin.sin_port=0; /* not known */
			/* interface no. in ipi_ifindex */
			ret=n; /* success */
			break;
		}
	}
end:
	return ret;
}



/* receive an ipv4 udp packet over a raw socket.
 * The packet is copied in *buf and *buf is advanced to point to the
 * payload.  Fills from and to.
 * @param rsock - raw socket
 * @param buf - the packet will be written to where *buf points intially and
 *              then *buf will be advanced to point to the udp payload.
 * @param len - buffer length (should be enough to hold at least the
 *               ip and udp headers + 1 byte).
 * @param from - result parameter, filled with source address and port of the
 *               packet.
 * @param from - result parameter, filled with destination (local) address and
 *               port of the packet.
 * @param rf   - filter used to decide whether or not the packet is
 *                accepted/processed. If null, all the packets are accepted.
 * @return packet len or  <0 on error (-1 and -2 on recv error @see recvpkt4,
 *         -3 if the headers are invalid and -4 if the packet doesn't
 *         match the  filter).
 */
int raw_udp4_recv(int rsock, char** buf, int len, union sockaddr_union* from,
					union sockaddr_union* to, struct raw_filter* rf)
{
	int n;
	unsigned short dst_port;
	unsigned short src_port;
	struct ip_addr dst_ip;
	char* end;
	char* udph_start;
	char* udp_payload;
	struct iphdr iph;
	struct udphdr udph;
	unsigned short udp_len;

	n=recvpkt4(rsock, *buf, len, from, to);
	if (unlikely(n<0)) goto error;
	
	end=*buf+n;
	if (unlikely(n<(sizeof(struct iphdr)+sizeof(struct udphdr)))) {
		n=-3;
		goto error;
	}
	/* FIXME: if initial buffer is aligned, one could skip the memcpy
	   and directly cast iphdr and udphdr pointer to the memory */
	memcpy(&iph, *buf, sizeof(struct iphdr));
	udph_start=*buf+iph.ihl*4;
	udp_payload=udph_start+sizeof(struct udphdr);
	if (unlikely(udp_payload>end)){
		n=-3;
		goto error;
	}
	memcpy(&udph, udph_start, sizeof(struct udphdr));
	udp_len=ntohs(udph.len);
	if (unlikely((udph_start+udp_len)!=end)){
		if ((udph_start+udp_len)>end){
			n=-3;
			goto error;
		}else{
			LOG(L_ERR, "udp length too small: %d/%d\n",
					(int)udp_len, (int)(end-udph_start));
			n=-3;
			goto error;
		}
	}
	/* advance buf */
	*buf=udp_payload;
	n=(int)(end-*buf);
	/* fill ip from the packet (needed if no PKT_INFO is used) */
	dst_ip.af=AF_INET;
	dst_ip.len=4;
	dst_ip.u.addr32[0]=iph.daddr;
	/* fill dst_port */
	dst_port=ntohs(udph.dest);
	ip_addr2su(to, &dst_ip, port);
	/* fill src_port */
	src_port=ntohs(udph.source);
	su_setport(from, src_port);
	if (likely(rf)) {
		su2ip_addr(&dst_ip, to);
		if ( (dst_port && rf->port1 && ((dst_port<rf->port1) ||
										(dst_port>rf->port2)) ) ||
			(matchnet(&dst_ip, &rf->dst)!=1) ){
			/* no match */
			n=-4;
			goto error;
		}
	}
	
error:
	return n;
}



/** udp checksum helper: compute the pseudo-header 16-bit "sum".
 * Computes the partial checksum (no complement) of the pseudo-header.
 * It is meant to be used by udpv4_chksum().
 * @param uh - filled udp header
 * @param src - source ip address in network byte order.
 * @param dst - destination ip address in network byte order.
 * @param length - payload lenght (not including the udp header),
 *                 in _host_ order.
 * @return the partial checksum in host order
 */
inline unsigned short udpv4_vhdr_sum(	struct udphdr* uh,
										struct in_addr* src,
										struct in_addr* dst,
										unsigned short length)
{
	unsigned sum;
	
	/* pseudo header */
	sum=(src->s_addr>>16)+(src->s_addr&0xffff)+
		(dst->s_addr>>16)+(dst->s_addr&0xffff)+
		htons(IPPROTO_UDP)+(uh->len);
	/* udp header */
	sum+=(uh->dest)+(uh->source)+(uh->len) + 0 /*chksum*/; 
	/* fold it */
	sum=(sum>>16)+(sum&0xffff);
	sum+=(sum>>16);
	/* no complement */
	return ntohs((unsigned short) sum);
}



/** compute the udp over ipv4 checksum.
 * @param u - filled udp header (except checksum).
 * @param src - source ip v4 address, in _network_ byte order.
 * @param dst - destination ip v4 address, int _network_ byte order.
 * @param data - pointer to the udp payload.
 * @param length - payload length, not including the udp header and in
 *                 _host_ order. The length mist be <= 0xffff - 8
 *                 (to allow space for the udp header).
 * @return the checksum in _host_ order */
inline static unsigned short udpv4_chksum(struct udphdr* u,
							struct in_addr* src, struct in_addr* dst,
							unsigned char* data, unsigned short length)
{
	unsigned sum;
	unsigned char* end;
	sum=udpv4_vhdr_sum(u, src, dst, length);
	end=data+(length&(~0x1)); /* make sure it's even */
	/* TODO: 16 & 32 bit aligned version */
		/* not aligned */
		for(;data<end;data+=2){
			sum+=((data[0]<<8)+data[1]);
		}
		if (length&0x1)
			sum+=((*data)<<8);
	
	/* fold it */
	sum=(sum>>16)+(sum&0xffff);
	sum+=(sum>>16);
	return (unsigned short)~sum;
}



/** fill in an udp header.
 * @param u - udp header that will be filled.
 * @param from - source ip v4 address and port.
 * @param to -   destination ip v4 address and port.
 * @param buf - pointer to the payload.
 * @param len - payload length (not including the udp header).
 * @param do_chk - if set the udp checksum will be computed, else it will
 *                 be set to 0.
 * @return 0 on success, < 0 on error.
 */
inline static int mk_udp_hdr(struct udphdr* u, struct sockaddr_in* from, 
				struct sockaddr_in* to, unsigned char* buf, int len,
					int do_chk)
{
	u->len=htons((unsigned short)len+sizeof(struct udphdr));
	u->source=from->sin_port;
	u->dest=to->sin_port;
	if (do_chk)
		u->check=htons(
				udpv4_chksum(u, &from->sin_addr, &to->sin_addr,  buf, len));
	else
		u->check=0; /* no checksum */
	return 0;
}



/** send an udp packet over a raw socket.
 * @param rsock - raw socket
 * @param buf - data
 * @param len - data len
 * @param from - source address:port (_must_ be non-null, but the ip address
 *                can be 0, in which case it will be filled by the kernel).
 * @param to - destination address:port
 * @return  <0 on error (errno set too), number of bytes sent on success
 *          (including the udp header => on success len + udpheader size).
 */
int raw_udp4_send(int rsock, char* buf, int len, union sockaddr_union* from,
					union sockaddr_union* to)
{
	struct msghdr snd_msg;
	struct cmsghdr* cmsg;
	struct in_pktinfo* snd_pktinfo;
	struct iovec iov[2];
	struct udphdr udp_hdr;
	char msg_ctrl_snd_buf[1024];
	int ret;

	memset(&snd_msg, 0, sizeof(snd_msg));
	snd_msg.msg_name=&to->sin;
	snd_msg.msg_namelen=sockaddru_len(*to);
	snd_msg.msg_iov=&iov[0];
	/* prepare udp header */
	mk_udp_hdr(&udp_hdr, &from->sin, &to->sin, (unsigned char*)buf, len, 1);
	iov[0].iov_base=(char*)&udp_hdr;
	iov[0].iov_len=sizeof(udp_hdr);
	iov[1].iov_base=buf;
	iov[1].iov_len=len;
	snd_msg.msg_iovlen=2;
	snd_msg.msg_control=msg_ctrl_snd_buf;
	snd_msg.msg_controllen=sizeof(msg_ctrl_snd_buf);
	/* init pktinfo cmsg */
	cmsg=CMSG_FIRSTHDR(&snd_msg);
	cmsg->cmsg_level=IPPROTO_IP;
	cmsg->cmsg_type=IP_PKTINFO;
	cmsg->cmsg_len=CMSG_LEN(sizeof(struct in_pktinfo));
	snd_pktinfo=(struct in_pktinfo*)CMSG_DATA(cmsg);
	snd_pktinfo->ipi_ifindex=0;
	snd_pktinfo->ipi_spec_dst.s_addr=from->sin.sin_addr.s_addr;
	snd_msg.msg_controllen=cmsg->cmsg_len;
	snd_msg.msg_flags=0;
	ret=sendmsg(rsock, &snd_msg, 0);
	return ret;
}



#endif /* USE_RAW_SOCKS */
