/*
 * $Id$
 *
 *
 * ip address & address family related functions
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * History:
 * --------
 *  2003-03-19  replaced all mallocs/frees w/ pkg_malloc/pkg_free
 */


#include <stdlib.h>
#include <stdio.h>

#include "ip_addr.h"
#include "dprint.h"
#include "mem/mem.h"


struct net* mk_net(struct ip_addr* ip, struct ip_addr* mask)
{
	struct net* n;
	
	if ((ip->af != mask->af) || (ip->len != mask->len)){
		LOG(L_CRIT, "ERROR: mk_net: trying to use a different mask family"
				" (eg. ipv4/ipv6mask or ipv6/ipv4mask)\n");
		goto error;
	}
	n=(struct net*)pkg_malloc(sizeof(struct net));
	if (n==0){ 
		LOG(L_CRIT, "ERROR: mk_net: memory allocation failure\n");
		goto error;
	}
	n->ip=*ip;
	n->mask=*mask;
	return n;
error:
	return 0;
}



struct net* mk_net_bitlen(struct ip_addr* ip, unsigned int bitlen)
{
	struct net* n;
	int r;
	
	if (bitlen>ip->len*8){
		LOG(L_CRIT, "ERROR: mk_net_bitlen: bad bitlen number %d\n", bitlen);
		goto error;
	}
	n=(struct net*)pkg_malloc(sizeof(struct net));
	if (n==0){
		LOG(L_CRIT, "ERROR: mk_net_bitlen: memory allocation failure\n"); 
		goto error;
	}
	memset(n,0, sizeof(struct net));
	n->ip=*ip;
	for (r=0;r<bitlen/8;r++) n->mask.u.addr[r]=0xff;
	if (bitlen%8) n->mask.u.addr[r]=  ~((1<<(8-(bitlen%8)))-1);
	n->mask.af=ip->af;
	n->mask.len=ip->len;
	
	return n;
error:
	return 0;
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
		LOG(L_WARN, "ERROR: print net: null pointer\n");
		return;
	}
	print_ip("", &net->ip, "/"); print_ip("", &net->mask, "");
}


#ifdef USE_MCAST

/* Returns 1 if the given address is a multicast address */
int is_mcast(struct ip_addr* ip)
{
	if (!ip){
		LOG(L_ERR, "ERROR: is_mcast: Invalid parameter value\n");
		return -1;
	}

	if (ip->af==AF_INET){
		return IN_MULTICAST(htonl(ip->u.addr32[0]));
#ifdef USE_IPV6
	} else if (ip->af==AF_INET6){
		return IN6_IS_ADDR_MULTICAST((struct in6_addr *)ip->u.addr);
#endif /* USE_IPV6 */
	} else {
		LOG(L_ERR, "ERROR: is_mcast: Unsupported protocol family\n");
		return -1;
	}
}

#endif /* USE_MCAST */
