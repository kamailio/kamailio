/* 
 *
 * ip address family related structures
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
/*!
* \file
* \brief Kamailio core :: ip address family related structures
* \ingroup core
* Module: \ref core
*/

#ifndef ip_addr_h
#define ip_addr_h

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "str.h"
#include "compiler_opt.h"
#include "ut.h"


#include "dprint.h"

enum sip_protos { PROTO_NONE, PROTO_UDP, PROTO_TCP, PROTO_TLS, PROTO_SCTP, PROTO_WS, PROTO_WSS, PROTO_OTHER };
#define PROTO_LAST PROTO_OTHER

#ifdef USE_COMP
enum comp_methods { COMP_NONE, COMP_SIGCOMP, COMP_SERGZ };
#endif

struct ip_addr{
	unsigned int af; /* address family: AF_INET6 or AF_INET */
	unsigned int len;    /* address len, 16 or 4 */
	
	/* 64 bits aligned address */
	union {
		unsigned long  addrl[16/sizeof(long)]; /* long format*/
		unsigned int   addr32[4];
		unsigned short addr16[8];
		unsigned char  addr[16];
	}u;
};

typedef struct ip_addr ip_addr_t;

struct net{
	struct ip_addr ip;
	struct ip_addr mask;
};

union sockaddr_union{
		struct sockaddr     s;
		struct sockaddr_in  sin;
		struct sockaddr_in6 sin6;
};



enum si_flags { SI_NONE=0, SI_IS_IP=1, SI_IS_LO=2, SI_IS_MCAST=4,
				 SI_IS_ANY=8, SI_IS_MHOMED=16 };

struct addr_info{
	str name; /* name - eg.: foo.bar or 10.0.0.1 */
	struct ip_addr address; /*ip address */
	str address_str;        /*ip address converted to string -- optimization*/
	enum si_flags flags; /* SI_IS_IP | SI_IS_LO | SI_IS_MCAST */
	union sockaddr_union su;
	struct addr_info* next;
	struct addr_info* prev;
};

struct advertise_info {
	str name; /* name - eg.: foo.bar or 10.0.0.1 */
	unsigned short port_no;  /* port number */
	str port_no_str; /* port number converted to string -- optimization*/
	str address_str;        /*ip address converted to string -- optimization*/
	struct ip_addr address; /* ip address */
	str sock_str; /* Socket proto, ip, and port as string */
};

struct socket_info{
	int socket;
	str name; /* name - eg.: foo.bar or 10.0.0.1 */
	struct ip_addr address; /* ip address */
	str address_str;        /*ip address converted to string -- optimization*/
	str port_no_str; /* port number converted to string -- optimization*/
	enum si_flags flags; /* SI_IS_IP | SI_IS_LO | SI_IS_MCAST */
	union sockaddr_union su; 
	struct socket_info* next;
	struct socket_info* prev;
	unsigned short port_no;  /* port number */
	char proto; /* tcp or udp*/
	str sock_str; /* Socket proto, ip, and port as string */
	struct addr_info* addr_info_lst; /* extra addresses (e.g. SCTP mh) */
	int workers; /* number of worker processes for this socket */
	int workers_tcpidx; /* index of workers in tcp children array */
	struct advertise_info useinfo; /* details to be used in SIP msg */
};


struct receive_info{
	struct ip_addr src_ip;
	struct ip_addr dst_ip;
	unsigned short src_port; /* host byte order */
	unsigned short dst_port; /* host byte order */
	int proto_reserved1; /* tcp stores the connection id here */
	int proto_reserved2;
	union sockaddr_union src_su; /* useful for replies*/
	struct socket_info* bind_address; /* sock_info structure on which 
									  the msg was received*/
	char proto;
#ifdef USE_COMP
	short comp; /* compression */
#endif
	/* no need for dst_su yet */
};


/* send flags */
#define SND_F_FORCE_CON_REUSE	1 /* reuse an existing connection or fail */
#define SND_F_CON_CLOSE			2 /* close the connection after sending */
#define SND_F_FORCE_SOCKET		4 /* send socket in dst is forced */

struct snd_flags {
	unsigned char f;          /* snd flags */
	unsigned char blst_imask; /* blacklist ignore mask */
};


typedef struct snd_flags  snd_flags_t;

#define SND_FLAGS_INIT(sflags) \
	do{ \
		(sflags)->f=0; \
		(sflags)->blst_imask=0; \
	}while(0)

#define SND_FLAGS_OR(dst, src1, src2) \
	do{ \
		(dst)->f = (src1)->f | (src2)->f; \
		(dst)->blst_imask = (src1)->blst_imask | (src2)->blst_imask; \
	}while(0)

#define SND_FLAGS_AND(dst, src1, src2) \
	do{ \
		(dst)->f = (src1)->f & (src2)->f; \
		(dst)->blst_imask = (src1)->blst_imask & (src2)->blst_imask; \
	}while(0)

struct dest_info{
	struct socket_info* send_sock;
	union sockaddr_union to;
	int id; /* tcp stores the connection id here */ 
	char proto;
	snd_flags_t send_flags;
#ifdef USE_COMP
	short comp;
#endif
};


/* list of names for multi-homed sockets that need to bind on
 * multiple addresses in the same time (sctp ) */
struct name_lst{
	char* name;
	struct name_lst* next;
	int flags;
};


struct socket_id{
	struct name_lst* addr_lst; /* address list, the first one must
								  be present and is the main one
								  (in case of multihoming sctp)*/
	int flags;
	int proto;
	int port;
	struct socket_id* next;
};



/* len of the sockaddr */
#ifdef HAVE_SOCKADDR_SA_LEN
#define sockaddru_len(su)	((su).s.sa_len)
#else
#define sockaddru_len(su)	\
			(((su).s.sa_family==AF_INET6)?sizeof(struct sockaddr_in6):\
					sizeof(struct sockaddr_in))
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
#define AF2PF(af)   (((af)==AF_INET)?PF_INET:((af)==AF_INET6)?PF_INET6:(af))




struct net* mk_new_net(struct ip_addr* ip, struct ip_addr* mask);
struct net* mk_new_net_bitlen(struct ip_addr* ip, unsigned int bitlen);
int mk_net(struct net* n, struct ip_addr* ip, struct ip_addr* mask);
int mk_net_bitlen(struct net* n, struct ip_addr* ip, unsigned int bitlen);
int mk_net_str(struct net* dst, str* s);

void print_ip(char* prefix, struct ip_addr* ip, char* suffix);
void stdout_print_ip(struct ip_addr* ip);
void print_net(struct net* net);

char* get_proto_name(unsigned int proto);
#define proto2a get_proto_name

int get_valid_proto_string(unsigned int iproto, int utype, int vtype,
		str *sproto);

#ifdef USE_MCAST
/* Returns 1 if the given address is a multicast address */
int is_mcast(struct ip_addr* ip);
#endif /* USE_MCAST */

/* returns 1 if the given ip address is INADDR_ANY or IN6ADDR_ANY,
 * 0 otherwise */
inline static int ip_addr_any(struct ip_addr* ip)
{
	int r;
	int l;
	
	l=ip->len/4;
	for (r=0; r<l; r++)
		if (ip->u.addr32[r]!=0)
			return 0;
	return 1;
}



/* returns 1 if the given ip address is a loopback address
 * 0 otherwise */
inline static int ip_addr_loopback(struct ip_addr* ip)
{
	if (ip->af==AF_INET)
		return ip->u.addr32[0]==htonl(INADDR_LOOPBACK);
	else if (ip->af==AF_INET6)
		return IN6_IS_ADDR_LOOPBACK((struct in6_addr*)ip->u.addr32);
	return 0;
}



/* creates an ANY ip_addr (filled with 0, af and len properly set) */
inline static void ip_addr_mk_any(int af, struct ip_addr* ip)
{
	ip->af=af;
	if (likely(af==AF_INET)){
		ip->len=4;
		ip->u.addr32[0]=0;
	}
	else{
		ip->len=16;
#if (defined (ULONG_MAX) && ULONG_MAX > 4294967295) || defined LP64
		/* long is 64 bits */
		ip->u.addrl[0]=0;
		ip->u.addrl[1]=0;
#else
		ip->u.addr32[0]=0;
		ip->u.addr32[1]=0;
		ip->u.addr32[2]=0;
		ip->u.addr32[3]=0;
#endif /* ULONG_MAX */
	}
}

/* returns 1 if ip & net.mask == net.ip ; 0 otherwise & -1 on error 
	[ diff. address families ]) */
inline static int matchnet(struct ip_addr* ip, struct net* net)
{
	unsigned int r;

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
	case AF_INET6:
			ip->af=AF_INET6;
			ip->len=16;
			memcpy(ip->u.addr, &((struct sockaddr_in6*)sa)->sin6_addr, 16);
			break;
	default:
			LM_CRIT("unknown address family %d\n", sa->sa_family);
	}
}



/* compare 2 ip_addrs (both args are pointers)*/
#define ip_addr_cmp(ip1, ip2) \
	(((ip1)->af==(ip2)->af)&& \
	 	(memcmp((ip1)->u.addr, (ip2)->u.addr, (ip1)->len)==0))



/* compare 2 sockaddr_unions */
static inline int su_cmp(const union sockaddr_union* s1,
						 const union sockaddr_union* s2)
{
	if (s1->s.sa_family!=s2->s.sa_family) return 0;
	switch(s1->s.sa_family){
		case AF_INET:
			return (s1->sin.sin_port==s2->sin.sin_port)&&
					(memcmp(&s1->sin.sin_addr, &s2->sin.sin_addr, 4)==0);
		case AF_INET6:
			return (s1->sin6.sin6_port==s2->sin6.sin6_port)&&
					(memcmp(&s1->sin6.sin6_addr, &s2->sin6.sin6_addr, 16)==0);
		default:
			LM_CRIT("unknown address family %d\n", s1->s.sa_family);
			return 0;
	}
}



/* gets the port number (host byte order) */
static inline unsigned short su_getport(const union sockaddr_union* su)
{
	switch(su->s.sa_family){
		case AF_INET:
			return ntohs(su->sin.sin_port);
		case AF_INET6:
			return ntohs(su->sin6.sin6_port);
		default:
			LM_CRIT("unknown address family %d\n", su->s.sa_family);
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
		case AF_INET6:
			 su->sin6.sin6_port=htons(port);
			 break;
		default:
			LM_CRIT("unknown address family %d\n", su->s.sa_family);
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
	case AF_INET6:
					ip->af=AF_INET6;
					ip->len=16;
					memcpy(ip->u.addr, &su->sin6.sin6_addr, 16);
					break;
	default:
					LM_CRIT("unknown address family %d\n", su->s.sa_family);
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
	case	AF_INET6:
		memcpy(&su->sin6.sin6_addr, ip->u.addr, ip->len); 
		#ifdef HAVE_SOCKADDR_SA_LEN
			su->sin6.sin6_len=sizeof(struct sockaddr_in6);
		#endif
		su->sin6.sin6_port=htons(port);
		break;
	case AF_INET:
		memcpy(&su->sin.sin_addr, ip->u.addr, ip->len);
		#ifdef HAVE_SOCKADDR_SA_LEN
			su->sin.sin_len=sizeof(struct sockaddr_in);
		#endif
		su->sin.sin_port=htons(port);
		break;
	default:
		LM_CRIT("unknown address family %d\n", ip->af);
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
	case	AF_INET6:
		memcpy(&su->sin6.sin6_addr, he->h_addr_list[idx], he->h_length);
		#ifdef HAVE_SOCKADDR_SA_LEN
			su->sin6.sin6_len=sizeof(struct sockaddr_in6);
		#endif
		su->sin6.sin6_port=htons(port);
		break;
	case AF_INET:
		memcpy(&su->sin.sin_addr, he->h_addr_list[idx], he->h_length);
		#ifdef HAVE_SOCKADDR_SA_LEN
			su->sin.sin_len=sizeof(struct sockaddr_in);
		#endif
		su->sin.sin_port=htons(port);
		break;
	default:
		LM_CRIT("unknown address family %d\n", he->h_addrtype);
		return -1;
	}
	return 0;
}



/* maximum size of a str returned by ip_addr2str */
#define IP6_MAX_STR_SIZE 39 /*1234:5678:9012:3456:7890:1234:5678:9012*/
#define IP4_MAX_STR_SIZE 15 /*123.456.789.012*/

/* converts a raw ipv6 addr (16 bytes) to ascii */
static inline int ip6tosbuf(unsigned char* ip6, char* buff, int len)
{
	int offset;
	register unsigned char a,b,c;
	register unsigned char d;
	register unsigned short hex4;
	int r;
	#define HEXDIG(x) (((x)>=10)?(x)-10+'A':(x)+'0')
	
	
	offset=0;
	if (unlikely(len<IP6_MAX_STR_SIZE))
		return 0;
	for(r=0;r<7;r++){
		hex4=((unsigned char)ip6[r*2]<<8)+(unsigned char)ip6[r*2+1];
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
	hex4=((unsigned char)ip6[r*2]<<8)+(unsigned char)ip6[r*2+1];
	a=hex4>>12;
	b=(hex4>>8)&0xf;
	c=(hex4>>4)&0xf;
	d=hex4&0xf;
	if (a){
		buff[offset]=HEXDIG(a);
		buff[offset+1]=HEXDIG(b);
		buff[offset+2]=HEXDIG(c);
		buff[offset+3]=HEXDIG(d);
		offset+=4;
	}else if(b){
		buff[offset]=HEXDIG(b);
		buff[offset+1]=HEXDIG(c);
		buff[offset+2]=HEXDIG(d);
		offset+=3;
	}else if(c){
		buff[offset]=HEXDIG(c);
		buff[offset+1]=HEXDIG(d);
		offset+=2;
	}else{
		buff[offset]=HEXDIG(d);
		offset+=1;
	}
	
	return offset;
}



/* converts a raw ipv4 addr (4 bytes) to ascii */
static inline int ip4tosbuf(unsigned char* ip4, char* buff, int len)
{
	int offset;
	register unsigned char a,b,c;
	int r;
	
	
	offset=0;
	if (unlikely(len<IP4_MAX_STR_SIZE))
		return 0;
	for(r=0;r<3;r++){
		a=(unsigned char)ip4[r]/100;
		c=(unsigned char)ip4[r]%10;
		b=(unsigned char)ip4[r]%100/10;
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
	a=(unsigned char)ip4[r]/100;
	c=(unsigned char)ip4[r]%10;
	b=(unsigned char)ip4[r]%100/10;
	if (a){
		buff[offset]=a+'0';
		buff[offset+1]=b+'0';
		buff[offset+2]=c+'0';
		offset+=3;
	}else if (b){
		buff[offset]=b+'0';
		buff[offset+1]=c+'0';
		offset+=2;
	}else{
		buff[offset]=c+'0';
		offset+=1;
	}
	
	return offset;
}



/* fast ip_addr -> string converter;
 * returns number of bytes written in buf on success, <=0 on error
 * The buffer must have enough space to hold the maximum size ip address
 *  of the corresponding address (see IP[46] above) or else the function
 *  will return error (no detailed might fit checks are made, for example
 *   if len==7 the function will fail even for 1.2.3.4).
 */
static inline int ip_addr2sbuf(struct ip_addr* ip, char* buff, int len)
{
	switch(ip->af){
		case AF_INET6:
			return ip6tosbuf(ip->u.addr, buff, len);
			break;
		case AF_INET:
			return ip4tosbuf(ip->u.addr, buff, len);
			break;
		default:
			LM_CRIT("unknown address family %d\n", ip->af);
			return 0;
	}
	return 0;
}



/* maximum size of a str returned by ip_addr2a (including \0) */
#define IP_ADDR_MAX_STR_SIZE (IP6_MAX_STR_SIZE+1) /* ip62ascii +  \0*/
/* fast ip_addr -> string converter;
 * it uses an internal buffer
 */
static inline char* ip_addr2a(struct ip_addr* ip)
{

	static char buff[IP_ADDR_MAX_STR_SIZE];
	int len;
	
	
	len=ip_addr2sbuf(ip, buff, sizeof(buff)-1);
	buff[len]=0;

	return buff;
}

/* full address in text representation, including [] for ipv6 */
static inline char* ip_addr2strz(struct ip_addr* ip)
{

	static char buff[IP_ADDR_MAX_STR_SIZE+2];
	char *p;
	int len;

	p = buff;
	if(ip->af==AF_INET6) {
		*p++ = '[';
	}
	len=ip_addr2sbuf(ip, p, sizeof(buff)-3);
	p += len;
	if(ip->af==AF_INET6) {
		*p++ = ']';
	}
	*p=0;

	return buff;
}

#define SU2A_MAX_STR_SIZE  (IP6_MAX_STR_SIZE + 2 /* [] */+\
								1 /* : */ + USHORT2SBUF_MAX_LEN + 1 /* \0 */)
/* returns an asciiz string containing the ip and the port
 *  (<ip_addr>:port or [<ipv6_addr>]:port)
 */
static inline char* su2a(union sockaddr_union* su, int su_len)
{
	static char buf[SU2A_MAX_STR_SIZE];
	int offs;

	if (unlikely(su->s.sa_family==AF_INET6)){
		if (unlikely(su_len<sizeof(su->sin6)))
			return "<addr. error>";
		buf[0]='[';
		offs=1+ip6tosbuf((unsigned char*)su->sin6.sin6_addr.s6_addr, &buf[1],
							sizeof(buf)-4);
		buf[offs]=']';
		offs++;
	}else
	if (unlikely(su_len<sizeof(su->sin)))
		return "<addr. error>";
	else
		offs=ip4tosbuf((unsigned char*)&su->sin.sin_addr, buf, sizeof(buf)-2);
	buf[offs]=':';
	offs+=1+ushort2sbuf(su_getport(su), &buf[offs+1], sizeof(buf)-(offs+1)-1);
	buf[offs]=0;
	return buf;
}

#define SUIP2A_MAX_STR_SIZE  (IP6_MAX_STR_SIZE + 2 /* [] */ + 1 /* \0 */)
/* returns an asciiz string containing the ip
 *  (<ipv4_addr> or [<ipv6_addr>])
 */
static inline char* suip2a(union sockaddr_union* su, int su_len)
{
	static char buf[SUIP2A_MAX_STR_SIZE];
	int offs;

	if (unlikely(su->s.sa_family==AF_INET6)){
		if (unlikely(su_len<sizeof(su->sin6)))
			return "<addr. error>";
		buf[0]='[';
		offs=1+ip6tosbuf((unsigned char*)su->sin6.sin6_addr.s6_addr, &buf[1],
							sizeof(buf)-4);
		buf[offs]=']';
		offs++;
	}else
	if (unlikely(su_len<sizeof(su->sin)))
		return "<addr. error>";
	else
		offs=ip4tosbuf((unsigned char*)&su->sin.sin_addr, buf, sizeof(buf)-2);
	buf[offs]=0;
	return buf;
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



/* init a dest_info structure */
#define init_dest_info(dst) \
	do{ \
		memset((dst), 0, sizeof(struct dest_info)); \
	} while(0) 



/* init a dest_info structure from a recv_info structure */
inline static void init_dst_from_rcv(struct dest_info* dst,
									struct receive_info* rcv)
{
		dst->send_sock=rcv->bind_address;
		dst->to=rcv->src_su;
		dst->id=rcv->proto_reserved1;
		dst->proto=rcv->proto;
		dst->send_flags.f=0;
		dst->send_flags.blst_imask=0;
#ifdef USE_COMP
		dst->comp=rcv->comp;
#endif
}

/**
 * match ip address with net address and bitmask
 * - return 0 on match, -1 otherwise
 */
int ip_addr_match_net(ip_addr_t *iaddr, ip_addr_t *naddr, int mask);

int si_get_signaling_data(struct socket_info *si, str **addr, str **port);

#endif
