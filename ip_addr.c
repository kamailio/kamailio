/*
 * ip address & address family related functions
 *
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

/** Kamailio core :: internal ip addresses representation functions.
 * @file ip_addr.c
 * @ingroup core
 * Module: @ref core
 */


#include <stdlib.h>
#include <stdio.h>

#include "ip_addr.h"
#include "dprint.h"
#include "mem/mem.h"
#include "resolve.h"
#include "trim.h"


struct net* mk_new_net(struct ip_addr* ip, struct ip_addr* mask)
{
	struct net* n;
	int warning;
	int r;
	
	warning=0;
	if ((ip->af != mask->af) || (ip->len != mask->len)){
		LM_CRIT("trying to use a different mask family"
				" (eg. ipv4/ipv6mask or ipv6/ipv4mask)\n");
		goto error;
	}
	n=(struct net*)pkg_malloc(sizeof(struct net));
	if (n==0){ 
		LM_CRIT("memory allocation failure\n");
		goto error;
	}
	n->ip=*ip;
	n->mask=*mask;
	for (r=0; r<n->ip.len/4; r++) { /*ipv4 & ipv6 addresses are multiple of 4*/
		n->ip.u.addr32[r] &= n->mask.u.addr32[r];
		if (n->ip.u.addr32[r]!=ip->u.addr32[r]) warning=1;
	};
	if (warning){
		LM_WARN("invalid network address/netmask "
					"combination fixed...\n");
		print_ip("original network address:", ip, "/");
		print_ip("", mask, "\n");
		print_ip("fixed    network address:", &(n->ip), "/");
		print_ip("", &(n->mask), "\n");
	};
	return n;
error:
	return 0;
}



struct net* mk_new_net_bitlen(struct ip_addr* ip, unsigned int bitlen)
{
	struct ip_addr mask;
	int r;
	
	if (bitlen>ip->len*8){
		LM_CRIT("bad bitlen number %d\n", bitlen);
		goto error;
	}
	memset(&mask,0, sizeof(mask));
	for (r=0;r<bitlen/8;r++) mask.u.addr[r]=0xff;
	if (bitlen%8) mask.u.addr[r]=  ~((1<<(8-(bitlen%8)))-1);
	mask.af=ip->af;
	mask.len=ip->len;
	
	return mk_new_net(ip, &mask);
error:
	return 0;
}



/** fills a net structure from an ip and a mask.
 *
 * This function will not print any error messages or allocate
 * memory (as opposed to mk_new_net() above).
 *
 * @param n - destination net structure
 * @param ip
 * @param mask
 * @return -1 on error (af mismatch), 0 on success
 */
int mk_net(struct net* n, struct ip_addr* ip, struct ip_addr* mask)
{
	int r;
	
	if (unlikely((ip->af != mask->af) || (ip->len != mask->len))) {
		return -1;
	}
	n->ip=*ip;
	n->mask=*mask;
	/* fix the network part of the mask */
	for (r=0; r<n->ip.len/4; r++) { /*ipv4 & ipv6 addresses are multiple of 4*/
		n->ip.u.addr32[r] &= n->mask.u.addr32[r];
	};
	return 0;
}



/** fills a net structure from an ip and a bitlen.
 *
 * This function will not print any error messages or allocate
 * memory (as opposed to mk_new_net_bitlen() above).
 *
 * @param n - destination net structure
 * @param ip
 * @param bitlen
 * @return -1 on error (af mismatch), 0 on success
 */
int mk_net_bitlen(struct net* n, struct ip_addr* ip, unsigned int bitlen)
{
	struct ip_addr mask;
	int r;
	
	if (unlikely(bitlen>ip->len*8))
		/* bitlen too big */
		return -1;
	memset(&mask,0, sizeof(mask));
	for (r=0;r<bitlen/8;r++) mask.u.addr[r]=0xff;
	if (bitlen%8) mask.u.addr[r]=  ~((1<<(8-(bitlen%8)))-1);
	mask.af=ip->af;
	mask.len=ip->len;
	
	return mk_net(n, ip, &mask);
}



/** initializes a net structure from a string.
 * @param dst - net structure that will be filled
 * @param s - string of the form "ip", "ip/mask_len" or "ip/ip_mak".
 * @return -1 on error, 0 on succes
 */
int mk_net_str(struct net* dst, str* s)
{
	struct ip_addr* t;
	char* p;
	struct ip_addr ip;
	str addr;
	str mask;
	unsigned int bitlen;
	
	/* test for ip only */
	t = str2ip(s);
	if (unlikely(t == 0))
		t = str2ip6(s);
	if (likely(t))
		return mk_net_bitlen(dst, t, t->len*8);
	/* not a simple ip, maybe an ip/netmask pair */
	p = q_memchr(s->s, '/', s->len);
	if (likely(p)) {
		addr.s = s->s;
		addr.len = (int)(long)(p - s->s);
		mask.s = p + 1;
		mask.len = s->len - (addr.len + 1);
		/* allow '/' enclosed by whitespace */
		trim_trailing(&addr);
		trim_leading(&mask);
		t = str2ip(&addr);
		if (likely(t)) {
			/* it can be a number */
			if (str2int(&mask, &bitlen) == 0)
				return mk_net_bitlen(dst, t, bitlen);
			ip = *t;
			t = str2ip(&mask);
			if (likely(t))
				return mk_net(dst, &ip, t);
			/* error */
			return -1;
		}
		else {
			t = str2ip6(&addr);
			if (likely(t)) {
				/* it can be a number */
				if (str2int(&mask, &bitlen) == 0)
					return mk_net_bitlen(dst, t, bitlen);
				ip = *t;
				t = str2ip6(&mask);
				if (likely(t))
					return mk_net(dst, &ip, t);
				/* error */
				return -1;
			}
		}
	}
	return -1;
}



void print_ip(char* p, struct ip_addr* ip, char *s)
{
	switch(ip->af){
		case AF_INET:
			DBG("%s%d.%d.%d.%d%s", (p)?p:"",
								ip->u.addr[0],
								ip->u.addr[1],
								ip->u.addr[2],
								ip->u.addr[3],
								(s)?s:""
								);
			break;
		case AF_INET6:
			DBG("%s%x:%x:%x:%x:%x:%x:%x:%x%s", (p)?p:"",
											htons(ip->u.addr16[0]),
											htons(ip->u.addr16[1]),
											htons(ip->u.addr16[2]),
											htons(ip->u.addr16[3]),
											htons(ip->u.addr16[4]),
											htons(ip->u.addr16[5]),
											htons(ip->u.addr16[6]),
											htons(ip->u.addr16[7]),
											(s)?s:""
				);
			break;
		default:
			DBG("print_ip: warning unknown address family %d\n", ip->af);
	}
}



void stdout_print_ip(struct ip_addr* ip)
{
	switch(ip->af){
		case AF_INET:
			printf("%d.%d.%d.%d",	ip->u.addr[0],
								ip->u.addr[1],
								ip->u.addr[2],
								ip->u.addr[3]);
			break;
		case AF_INET6:
			printf("%x:%x:%x:%x:%x:%x:%x:%x",	htons(ip->u.addr16[0]),
											htons(ip->u.addr16[1]),
											htons(ip->u.addr16[2]),
											htons(ip->u.addr16[3]),
											htons(ip->u.addr16[4]),
											htons(ip->u.addr16[5]),
											htons(ip->u.addr16[6]),
											htons(ip->u.addr16[7])
				);
			break;
		default:
			DBG("print_ip: warning unknown address family %d\n", ip->af);
	}
}



void print_net(struct net* net)
{
	if (net==0){
		LM_WARN("null pointer\n");
		return;
	}
	print_ip("", &net->ip, "/"); print_ip("", &net->mask, "");
}


#ifdef USE_MCAST

/* Returns 1 if the given address is a multicast address */
int is_mcast(struct ip_addr* ip)
{
	if (!ip){
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	if (ip->af==AF_INET){
		return IN_MULTICAST(htonl(ip->u.addr32[0]));
	} else if (ip->af==AF_INET6){
		return IN6_IS_ADDR_MULTICAST((struct in6_addr*)ip->u.addr32);
	} else {
		LM_ERR("Unsupported protocol family\n");
		return -1;
	}
}

#endif /* USE_MCAST */

/** get string for known protocols.
 * @param iproto - protocol number
 * @param utype  - 1 if result is used for URI, or 0
 * @param vtype  - 1 if result is wanted uppercase, or 0 for lowercase
 * @param sproto - the string for the proto
 * @return  0 if it is a valid and supported protocol, negative otherwise
 */
int get_valid_proto_string(unsigned int iproto, int utype, int vtype,
		str *sproto)
{
	switch(iproto){
		case PROTO_NONE:
			return -1;
		case PROTO_UDP:
			sproto->len = 3;
			sproto->s = (vtype)?"UDP":"udp";
			return 0;
		case PROTO_TCP:
			sproto->len = 3;
			sproto->s = (vtype)?"TCP":"tcp";
			return 0;
		case PROTO_TLS:
			sproto->len = 3;
			sproto->s = (vtype)?"TLS":"tls";
			return 0;
		case PROTO_SCTP:
			sproto->len = 4;
			sproto->s = (vtype)?"SCTP":"sctp";
			return 0;
		case PROTO_WS:
		case PROTO_WSS:
			if(iproto==PROTO_WS || utype) {
				/* ws-only in SIP URI */
				sproto->len = 2;
				sproto->s = (vtype)?"WS":"ws";
			} else {
				sproto->len = 3;
				sproto->s = (vtype)?"WSS":"wss";
			}
			return 0;
		default:
			return -2;
	}
}

/** get protocol name (asciiz).
 * @param proto - protocol number
 * @return  string with the protocol name or "unknown".
 */
char* get_proto_name(unsigned int proto)
{
	str sproto;
	switch(proto){
		case PROTO_NONE:
			return "*";
		default:
			if(get_valid_proto_string(proto, 1, 0, &sproto)<0)
				return "unknown";
			return sproto.s;
	}
}


/**
 * match ip address with net address and bitmask
 * - return 0 on match, -1 otherwise
 */
int ip_addr_match_net(ip_addr_t *iaddr, ip_addr_t *naddr,
		int mask)
{
	unsigned char ci;
	unsigned char cn;
	int i;
	int mbytes;
	int mbits;

	if(mask==0)
		return 0;
	if(iaddr==NULL || naddr==NULL || mask<0)
		return -1;
	if(iaddr->af != naddr->af)
		return -1;

	if(iaddr->af == AF_INET)
	{
		if(mask>32)
			return -1;
		if(mask==32)
		{
			if(ip_addr_cmp(iaddr, naddr))
				return 0;
			return -1;
		}
	} else if(iaddr->af ==  AF_INET6) {
		if(mask>128)
			return -1;

		if(mask==128)
		{
			if(ip_addr_cmp(iaddr, naddr))
				return 0;
			return -1;
		}
	}

	mbytes = mask / 8;
	for(i=0; i<mbytes; i++)
	{
		if(iaddr->u.addr[i] != naddr->u.addr[i])
			return -1;
	}
	mbits = mask % 8;
	if(mbits==0)
		return 0;
	ci = iaddr->u.addr[i] & (~((1 << (8 - mbits)) - 1));
	cn = naddr->u.addr[i] & (~((1 << (8 - mbits)) - 1));
	if(ci == cn)
		return 0;
	return -1;
}

int si_get_signaling_data(struct socket_info *si, str **addr, str **port)
{
	if(si==NULL)
		return -1;
	if(addr) {
		if(si->useinfo.name.len>0) {
			*addr = &si->useinfo.name;
		} else {
			*addr = &si->address_str;
		}
	}
	if(port) {
		if(si->useinfo.port_no>0) {
			*port = &si->useinfo.port_no_str;
		} else {
			*port = &si->port_no_str;
		}
	}
	return 0;
}
