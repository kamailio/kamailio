/* $Id$
 *
 * ip address family realted structures
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
 */
/*
 * History:
 * --------
 *  2003-02-13  added struct dest_info (andrei)
 *  2003-04-06  all ports are stored/passed in host byte order now (andrei)
 */

#ifndef ip_addr_h
#define ip_addr_h

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "str.h"


#include "dprint.h"

enum sip_protos { PROTO_NONE, PROTO_UDP, PROTO_TCP, PROTO_TLS, PROTO_SCTP };


struct ip_addr{
	unsigned int af; /* address family: AF_INET6 or AF_INET */
	unsigned int len;    /* address len, 16 or 4 */
	
	/* 64 bits alligned address */
	union {
		unsigned long  addrl[16/sizeof(long)]; /* long format*/
		unsigned int   addr32[4];
		unsigned short addr16[8];
		unsigned char  addr[16];
	}u;
};



struct net{
	struct ip_addr ip;
	struct ip_addr mask;
};

union sockaddr_union{
		struct sockaddr     s;
		struct sockaddr_in  sin;
	#ifdef USE_IPV6
		struct sockaddr_in6 sin6;
	#endif
};



enum si_flags { SI_NONE=0, SI_IS_IP=1, SI_IS_LO=2 };

struct socket_info{
	int socket;
	str name; /* name - eg.: foo.bar or 10.0.0.1 */
	struct ip_addr address; /* ip address */
	str address_str;        /* ip address converted to string -- optimization*/
	unsigned short port_no;  /* port number */
	str port_no_str; /* port number converted to string -- optimization*/
	enum si_flags flags; /* SI_IS_IP | SI_IS_LO */
	union sockaddr_union su; 
	int proto; /* tcp or udp*/
	struct socket_info* next;
	struct socket_info* prev;
};


struct receive_info{
	struct ip_addr src_ip;
	struct ip_addr dst_ip;
	unsigned short src_port; /* host byte order */
	unsigned short dst_port; /* host byte order */
	int proto;
	int proto_reserved1; /* tcp stores the connection id here */
	int proto_reserved2;
	union sockaddr_union src_su; /* usefull for replies*/
	struct socket_info* bind_address; /* sock_info structure on which 
									  the msg was received*/
	/* no need for dst_su yet */
};


struct dest_info{
	int proto;
	int proto_reserved1; /* tcp stores the connection id here */ 
	union sockaddr_union to;
	struct socket_info* send_sock;
};



/* len of the sockaddr */
#ifdef HAVE_SOCKADDR_SA_LEN
#define sockaddru_len(su)	((su).s.sa_len)
#else
#ifdef USE_IPV6
#define sockaddru_len(su)	\
			(((su).s.sa_family==AF_INET6)?sizeof(struct sockaddr_in6):\
					sizeof(struct sockaddr_in))
#else
#define sockaddru_len(su)	sizeof(struct sockaddr_in)
#endif /*USE_IPV6*/
#endif /* HAVE_SOCKADDR_SA_LEN*/
	
/* inits an ip_addr with the addr. info from a hostent structure
 * ip = struct ip_addr*
 * he= struct hostent*
 */
#define hostent2ip_addr(ip, he, addr_no) \
	do{ \
		(ip)->af=(he)->h_addrtype; \
		(ip)->len=(he)->h_length;  \
		memcpy((ip)->u.addr, (he)->h_addr_list[(addr_no)], (ip)->len); \
	}while(0)
	



/* gets the protocol family corresponding to a specific address family
 * ( PF_INET - AF_INET, PF_INET6 - AF_INET6, af for others)
 */
#ifdef USE_IPV6
#define AF2PF(af)   (((af)==AF_INET)?PF_INET:((af)==AF_INET6)?PF_INET6:(af))
#else
#define AF2PF(af)   (((af)==AF_INET)?PF_INET:(af))
#endif




struct net* mk_net(struct ip_addr* ip, struct ip_addr* mask);
struct net* mk_net_bitlen(struct ip_addr* ip, unsigned int bitlen);

void print_ip(char* prefix, struct ip_addr* ip, char* suffix);
void stdout_print_ip(struct ip_addr* ip);
void print_net(struct net* net);




/* returns 1 if ip & net.mask == net.ip ; 0 otherwise & -1 on error 
	[ diff. adress fams ]) */
inline static int matchnet(struct ip_addr* ip, struct net* net)
{
	unsigned int r;
	int ret;
	
	ret=-1;
	if (ip->af == net->ip.af){
		for(r=0; r<ip->len/4; r++){ /* ipv4 & ipv6 addresses are
									   all multiple of 4*/
			if ((ip->u.addr32[r]&net->mask.u.addr32[r])!=
														 net->ip.u.addr32[r]){
				return 0;
			}
		}
		return 1;
	};
	return -1;
}




/* inits an ip_addr pointer from a sockaddr structure*/
static inline void sockaddr2ip_addr(struct ip_addr* ip, struct sockaddr* sa)
{
	switch(sa->sa_family){
	case AF_INET:
			ip->af=AF_INET;
			ip->len=4;
			memcpy(ip->u.addr, &((struct sockaddr_in*)sa)->sin_addr, 4);
			break;
#ifdef USE_IPV6
	case AF_INET6:
			ip->af=AF_INET6;
			ip->len=16;
			memcpy(ip->u.addr, &((struct sockaddr_in6*)sa)->sin6_addr, 16);
			break;
#endif
	default:
			LOG(L_CRIT, "sockaddr2ip_addr: BUG: unknown address family %d\n",
					sa->sa_family);
	}
}



/* compare 2 ip_addrs (both args are pointers)*/
#define ip_addr_cmp(ip1, ip2) \
	(((ip1)->af==(ip2)->af)&& \
	 	(memcmp((ip1)->u.addr, (ip2)->u.addr, (ip1)->len)==0))



/* compare 2 sockaddr_unions */
static inline int su_cmp(union sockaddr_union* s1, union sockaddr_union* s2)
{
	if (s1->s.sa_family!=s2->s.sa_family) return 0;
	switch(s1->s.sa_family){
		case AF_INET:
			return (s1->sin.sin_port==s2->sin.sin_port)&&
					(memcmp(&s1->sin.sin_addr, &s2->sin.sin_addr, 4)==0);
#ifdef USE_IPV6
		case AF_INET6:
			return (s1->sin6.sin6_port==s2->sin6.sin6_port)&&
					(memcmp(&s1->sin6.sin6_addr, &s2->sin6.sin6_addr, 16)==0);
#endif
		default:
			LOG(L_CRIT,"su_cmp: BUG: unknown address family %d\n",
						s1->s.sa_family);
			return 0;
	}
}



/* gets the port number (host byte order) */
static inline short su_getport(union sockaddr_union* su)
{
	switch(su->s.sa_family){
		case AF_INET:
			return ntohs(su->sin.sin_port);
#ifdef USE_IPV6
		case AF_INET6:
			return ntohs(su->sin6.sin6_port);
#endif
		default:
			LOG(L_CRIT,"su_get_port: BUG: unknown address family %d\n",
						su->s.sa_family);
			return 0;
	}
}



/* sets the port number (host byte order) */
static inline void su_setport(union sockaddr_union* su, unsigned short port)
{
	switch(su->s.sa_family){
		case AF_INET:
			su->sin.sin_port=htons(port);
			break;
#ifdef USE_IPV6
		case AF_INET6:
			 su->sin6.sin6_port=htons(port);
			 break;
#endif
		default:
			LOG(L_CRIT,"su_set_port: BUG: unknown address family %d\n",
						su->s.sa_family);
	}
}



/* inits an ip_addr pointer from a sockaddr_union ip address */
static inline void su2ip_addr(struct ip_addr* ip, union sockaddr_union* su)
{
	switch(su->s.sa_family){
	case AF_INET: 
					ip->af=AF_INET;
					ip->len=4;
					memcpy(ip->u.addr, &su->sin.sin_addr, 4);
					break;
#ifdef USE_IPV6
	case AF_INET6:
					ip->af=AF_INET6;
					ip->len=16;
					memcpy(ip->u.addr, &su->sin6.sin6_addr, 16);
					break;
#endif
	default:
					LOG(L_CRIT,"su2ip_addr: BUG: unknown address family %d\n",
							su->s.sa_family);
	}
}


/* ip_addr2su -> the same as init_su*/
#define ip_addr2su init_su

/* inits a struct sockaddr_union from a struct ip_addr and a port no 
 * returns 0 if ok, -1 on error (unknown address family)
 * the port number is in host byte order */
static inline int init_su( union sockaddr_union* su,
							struct ip_addr* ip,
							unsigned short   port ) 
{
	memset(su, 0, sizeof(union sockaddr_union));/*needed on freebsd*/
	su->s.sa_family=ip->af;
	switch(ip->af){
#ifdef USE_IPV6
	case	AF_INET6:
		memcpy(&su->sin6.sin6_addr, ip->u.addr, ip->len); 
		#ifdef HAVE_SOCKADDR_SA_LEN
			su->sin6.sin6_len=sizeof(struct sockaddr_in6);
		#endif
		su->sin6.sin6_port=htons(port);
		break;
#endif
	case AF_INET:
		memcpy(&su->sin.sin_addr, ip->u.addr, ip->len);
		#ifdef HAVE_SOCKADDR_SA_LEN
			su->sin.sin_len=sizeof(struct sockaddr_in);
		#endif
		su->sin.sin_port=htons(port);
		break;
	default:
		LOG(L_CRIT, "init_ss: BUG: unknown address family %d\n", ip->af);
		return -1;
	}
	return 0;
}



/* inits a struct sockaddr_union from a struct hostent, an address index in
 * the hostent structure and a port no. (host byte order)
 * WARNING: no index overflow  checks!
 * returns 0 if ok, -1 on error (unknown address family) */
static inline int hostent2su( union sockaddr_union* su,
								struct hostent* he,
								unsigned int idx,
								unsigned short   port ) 
{
	memset(su, 0, sizeof(union sockaddr_union)); /*needed on freebsd*/
	su->s.sa_family=he->h_addrtype;
	switch(he->h_addrtype){
#ifdef USE_IPV6
	case	AF_INET6:
		memcpy(&su->sin6.sin6_addr, he->h_addr_list[idx], he->h_length);
		#ifdef HAVE_SOCKADDR_SA_LEN
			su->sin6.sin6_len=sizeof(struct sockaddr_in6);
		#endif
		su->sin6.sin6_port=htons(port);
		break;
#endif
	case AF_INET:
		memcpy(&su->sin.sin_addr, he->h_addr_list[idx], he->h_length);
		#ifdef HAVE_SOCKADDR_SA_LEN
			su->sin.sin_len=sizeof(struct sockaddr_in);
		#endif
		su->sin.sin_port=htons(port);
		break;
	default:
		LOG(L_CRIT, "hostent2su: BUG: unknown address family %d\n", 
				he->h_addrtype);
		return -1;
	}
	return 0;
}



/* fast ip_addr -> string convertor;
 * it uses an internal buffer
 */
static inline char* ip_addr2a(struct ip_addr* ip)
{

	static char buff[40];/* 1234:5678:9012:3456:7890:1234:5678:9012\0 */
	int offset;
	register unsigned char a,b,c;
#ifdef USE_IPV6
	register unsigned char d;
	register unsigned short hex4;
#endif
	int r;
	#define HEXDIG(x) (((x)>=10)?(x)-10+'A':(x)+'0')
	
	
	offset=0;
	switch(ip->af){
	#ifdef USE_IPV6
		case AF_INET6:
			for(r=0;r<7;r++){
				hex4=ntohs(ip->u.addr16[r]);
				a=hex4>>12;
				b=(hex4>>8)&0xf;
				c=(hex4>>4)&0xf;
				d=hex4&0xf;
				if (a){
					buff[offset]=HEXDIG(a);
					buff[offset+1]=HEXDIG(b);
					buff[offset+2]=HEXDIG(c);
					buff[offset+3]=HEXDIG(d);
					buff[offset+4]=':';
					offset+=5;
				}else if(b){
					buff[offset]=HEXDIG(b);
					buff[offset+1]=HEXDIG(c);
					buff[offset+2]=HEXDIG(d);
					buff[offset+3]=':';
					offset+=4;
				}else if(c){
					buff[offset]=HEXDIG(c);
					buff[offset+1]=HEXDIG(d);
					buff[offset+2]=':';
					offset+=3;
				}else{
					buff[offset]=HEXDIG(d);
					buff[offset+1]=':';
					offset+=2;
				}
			}
			/* last int16*/
			hex4=ntohs(ip->u.addr16[r]);
			a=hex4>>12;
			b=(hex4>>8)&0xf;
			c=(hex4>>4)&0xf;
			d=hex4&0xf;
			if (a){
				buff[offset]=HEXDIG(a);
				buff[offset+1]=HEXDIG(b);
				buff[offset+2]=HEXDIG(c);
				buff[offset+3]=HEXDIG(d);
				buff[offset+4]=0;
			}else if(b){
				buff[offset]=HEXDIG(b);
				buff[offset+1]=HEXDIG(c);
				buff[offset+2]=HEXDIG(d);
				buff[offset+3]=0;
			}else if(c){
				buff[offset]=HEXDIG(c);
				buff[offset+1]=HEXDIG(d);
				buff[offset+2]=0;
			}else{
				buff[offset]=HEXDIG(d);
				buff[offset+1]=0;
			}
			break;
	#endif
		case AF_INET:
			for(r=0;r<3;r++){
				a=ip->u.addr[r]/100;
				c=ip->u.addr[r]%10;
				b=ip->u.addr[r]%100/10;
				if (a){
					buff[offset]=a+'0';
					buff[offset+1]=b+'0';
					buff[offset+2]=c+'0';
					buff[offset+3]='.';
					offset+=4;
				}else if (b){
					buff[offset]=b+'0';
					buff[offset+1]=c+'0';
					buff[offset+2]='.';
					offset+=3;
				}else{
					buff[offset]=c+'0';
					buff[offset+1]='.';
					offset+=2;
				}
			}
			/* last number */
			a=ip->u.addr[r]/100;
			c=ip->u.addr[r]%10;
			b=ip->u.addr[r]%100/10;
			if (a){
				buff[offset]=a+'0';
				buff[offset+1]=b+'0';
				buff[offset+2]=c+'0';
				buff[offset+3]=0;
			}else if (b){
				buff[offset]=b+'0';
				buff[offset+1]=c+'0';
				buff[offset+2]=0;
			}else{
				buff[offset]=c+'0';
				buff[offset+1]=0;
			}
			break;
		
		default:
			LOG(L_CRIT, "BUG: ip_addr2a: unknown address family %d\n",
					ip->af);
			return 0;
	}
	
	return buff;
}



/* converts an ip_addr structure to a hostent, returns pointer to internal
 * statical structure */
static inline struct hostent* ip_addr2he(str* name, struct ip_addr* ip)
{
	static struct hostent he;
	static char hostname[256];
	static char* p_aliases[1];
	static char* p_addr[2];
	static char address[16];
	
	p_aliases[0]=0; /* no aliases*/
	p_addr[1]=0; /* only one address*/
	p_addr[0]=address;
	strncpy(hostname, name->s, (name->len<256)?(name->len)+1:256);
	if (ip->len>16) return 0;
	memcpy(address, ip->u.addr, ip->len);
	
	he.h_addrtype=ip->af;
	he.h_length=ip->len;
	he.h_addr_list=p_addr;
	he.h_aliases=p_aliases;
	he.h_name=hostname;
	return &he;
}
#endif
