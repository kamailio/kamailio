/* $Id$
 *
 * ip address family realted structures
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



struct ip_addr{
	unsigned int af; /* address family: AF_INET6 or AF_INET */
	unsigned int len;    /* address len, 16 or 4 */
	
	/* 64 bits alligned address */
	union {
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


struct socket_info{
	int socket;
	str name; /* name - eg.: foo.bar or 10.0.0.1 */
	struct ip_addr address; /* ip address */
	str address_str;        /* ip address converted to string -- optimization*/
	unsigned short port_no;  /* port number */
	str port_no_str; /* port number converted to string -- optimization*/
	int is_ip; /* 1 if name is an ip address, 0 if not  -- optimization*/
};



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

void print_ip(struct ip_addr* ip);
void stdout_print_ip(struct ip_addr* ip);
void print_net(struct net* net);




/* returns 1 if ip & net.mask == net.ip ; 0 otherwise & -1 on error 
	[ diff. adress fams ]) */
inline static int matchnet(struct ip_addr* ip, struct net* net)
{
	int r;
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



/* inits a struct sockaddr_union from a struct ip_addr and a port no 
 * returns 0 if ok, -1 on error (unknown address family) */
static inline int init_su( union sockaddr_union* su,
							struct ip_addr* ip,
							unsigned short   port ) 
{
	su->s.sa_family=ip->af;
	switch(ip->af){
#ifdef USE_IPV6
	case	AF_INET6:
		memcpy(&su->sin6.sin6_addr, ip->u.addr, ip->len); 
		#ifdef FreeBSD
			su->sin6.sin6_len=sizeof(struct sockaddr_in6);
		#endif
		su->sin6.sin6_port=port;
		break;
#endif
	case AF_INET:
		memcpy(&su->sin.sin_addr, ip->u.addr, ip->len);
		#ifdef FreeBSD
			su->sin.sin_len=sizeof(struct sockaddr_in);
		#endif
		su->sin.sin_port=port;
		break;
	default:
		LOG(L_CRIT, "init_ss: BUG: unknown address family %d\n", ip->af);
		return -1;
	}
	return 0;
}



/* inits a struct sockaddr_union from a struct hostent, an address index int
 * the hostent structure and a port no.
 * WARNING: no index overflow  checks!
 * returns 0 if ok, -1 on error (unknown address family) */
static inline int hostent2su( union sockaddr_union* su,
								struct hostent* he,
								unsigned int idx,
								unsigned short   port ) 
{
	su->s.sa_family=he->h_addrtype;
	switch(he->h_addrtype){
#ifdef USE_IPV6
	case	AF_INET6:
		memcpy(&su->sin6.sin6_addr, he->h_addr_list[idx], he->h_length);
		#ifdef FreeBSD
			su->sin6.sin6_len=sizeof(struct sockaddr_in6);
		#endif
		su->sin6.sin6_port=port;
		break;
#endif
	case AF_INET:
		memcpy(&su->sin.sin_addr, he->h_addr_list[idx], he->h_length);
		#ifdef FreeBSD
			su->sin.sin_len=sizeof(struct sockaddr_in);
		#endif
		su->sin.sin_port=port;
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





#endif
