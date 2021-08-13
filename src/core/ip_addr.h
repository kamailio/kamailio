/**
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

extern str ksr_ipv6_hex_style;

typedef enum sip_protos { PROTO_NONE, PROTO_UDP, PROTO_TCP, PROTO_TLS, PROTO_SCTP,
	PROTO_WS, PROTO_WSS, PROTO_OTHER } sip_protos_t;
#define PROTO_LAST PROTO_OTHER

#ifdef USE_COMP
typedef enum comp_methods { COMP_NONE, COMP_SIGCOMP, COMP_SERGZ } comp_methods_t;
#endif

typedef struct ip_addr {
	unsigned int af;	/* address family: AF_INET6 or AF_INET */
	unsigned int len;	/* address len, 16 or 4 */

	/* 64 bits aligned address */
	union {
		unsigned long  addrl[16/sizeof(long)]; /* long format*/
		unsigned int   addr32[4];
		unsigned short addr16[8];
		unsigned char  addr[16];
	}u;
} ip_addr_t;

typedef struct net {
	struct ip_addr ip;
	struct ip_addr mask;
} sr_net_t;

typedef union sockaddr_union{
	struct sockaddr     s;
	struct sockaddr_in  sin;
	struct sockaddr_in6 sin6;
	struct sockaddr_storage sas;
} sr_sockaddr_union_t;


typedef enum si_flags {
	SI_NONE         = 0,
	SI_IS_IP        = (1<<0),
	SI_IS_LO        = (1<<1),
	SI_IS_MCAST     = (1<<2),
	SI_IS_ANY       = (1<<3),
	SI_IS_MHOMED    = (1<<4),
} si_flags_t;

typedef struct addr_info {
	str name; /* name - eg.: foo.bar or 10.0.0.1 */
	struct ip_addr address; /*ip address */
	str address_str;        /*ip address converted to string -- optimization*/
	enum si_flags flags; /* SI_IS_IP | SI_IS_LO | SI_IS_MCAST */
	union sockaddr_union su;
	struct addr_info* next;
	struct addr_info* prev;
} addr_info_t;


typedef struct advertise_info {
	str name; /* name - eg.: foo.bar or 10.0.0.1 */
	unsigned short port_no;  /* port number */
	short port_pad; /* padding field */
	str port_no_str; /* port number converted to string -- optimization*/
	str address_str;        /*ip address converted to string -- optimization*/
	struct ip_addr address; /* ip address */
	str sock_str; /* Socket proto, ip, and port as string */
} advertise_info_t;

typedef struct socket_info {
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
	char proto_pad0; /* padding field */
	short proto_pad1; /* padding field */
	str sock_str; /* Socket proto, ip, and port as string */
	struct addr_info* addr_info_lst; /* extra addresses (e.g. SCTP mh) */
	int workers; /* number of worker processes for this socket */
	int workers_tcpidx; /* index of workers in tcp children array */
	str sockname; /* socket name given in config listen value */
	struct advertise_info useinfo; /* details to be used in SIP msg */
#ifdef USE_MCAST
	str mcast; /* name of interface that should join multicast group*/
#endif /* USE_MCAST */
} socket_info_t;


/* send flags */
typedef enum send_flags {
	SND_F_FORCE_CON_REUSE   = (1 << 0), /* reuse an existing connection or fail */
	SND_F_CON_CLOSE         = (1 << 1), /* close the connection after sending */
	SND_F_FORCE_SOCKET      = (1 << 2), /* send socket in dst is forced */
} send_flags_t;

typedef struct snd_flags {
	unsigned short f;          /* snd flags */
	unsigned short blst_imask; /* blocklist ignore mask */
} snd_flags_t;

/* recv flags */
typedef enum recv_flags {
	RECV_F_INTERNAL     = (1 << 0), /* message dispatched internally */
	RECV_F_PREROUTING   = (1 << 1), /* message in pre-routing */
} recv_flags_t;

typedef struct receive_info {
	struct ip_addr src_ip;
	struct ip_addr dst_ip;
	unsigned short src_port; /* host byte order */
	unsigned short dst_port; /* host byte order */
	int proto_reserved1; /* tcp stores the connection id here */
	int proto_reserved2;
	union sockaddr_union src_su; /* useful for replies*/
	struct socket_info* bind_address; /* sock_info structure on which
										* the msg was received */
	recv_flags_t rflags; /* flags */
	char proto;
#ifdef USE_COMP
	char proto_pad0;  /* padding field */
	short comp; /* compression */
#else
	char proto_pad0;  /* padding field */
	short proto_pad1; /* padding field */
#endif
	/* no need for dst_su yet */
} receive_info_t;


typedef struct dest_info {
	struct socket_info* send_sock;
	union sockaddr_union to;
	int id; /* tcp stores the connection id here */
	snd_flags_t send_flags;
	char proto;
#ifdef USE_COMP
	char proto_pad0;  /* padding field */
	short comp;
#else
	char proto_pad0;  /* padding field */
	short proto_pad1; /* padding field */
#endif
} dest_info_t;


typedef struct ksr_coninfo {
	ip_addr_t src_ip;
	ip_addr_t dst_ip;
	unsigned short src_port; /* host byte order */
	unsigned short dst_port; /* host byte order */
	int proto;
	socket_info_t *csocket;
} ksr_coninfo_t;

typedef struct sr_net_info {
	str data;
	receive_info_t* rcv;
	dest_info_t* dst;
} sr_net_info_t;

sr_net_info_t *ksr_evrt_rcvnetinfo_get(void);

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


/* list of names for multi-homed sockets that need to bind on
 * multiple addresses in the same time (sctp ) */
typedef struct name_lst {
	char* name;
	struct name_lst* next;
	int flags;
} name_lst_t;


typedef struct socket_id {
	struct name_lst* addr_lst; /* address list, the first one must
								* be present and is the main one
								* (in case of multihoming sctp) */
	int flags;
	int proto;
	int port;
	struct socket_id* next;
} socket_id_t;


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
 * [ diff. address families ]) */
inline static int matchnet(struct ip_addr* ip, struct net* net)
{
	unsigned int r;

	if (ip->af == net->ip.af){
		for(r=0; r<ip->len/4; r++){ /* ipv4 & ipv6 addresses are
									 * all multiple of 4*/
			if ((ip->u.addr32[r]&net->mask.u.addr32[r])
					!=net->ip.u.addr32[r]) {
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
			memset(ip, 0, sizeof(ip_addr_t));
	}
}


/* ip_addr2su -> the same as init_su*/
#define ip_addr2su init_su

/* inits a struct sockaddr_union from a struct ip_addr and a port no
 * returns 0 if ok, -1 on error (unknown address family)
 * the port number is in host byte order */
static inline int init_su( union sockaddr_union* su,
		struct ip_addr* ip,
		unsigned short port )
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
int hostent2su(union sockaddr_union* su,
		struct hostent* he,
		unsigned int idx,
		unsigned short port);


/* maximum size of a str returned by ip_addr2str */
/* POSIX INET6_ADDRSTRLEN (RFC 4291 section 2.2) - IPv6 with IPv4 tunneling
 * (39): 1234:5678:9012:3456:7890:1234:5678:9012
 * (45): ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255 */
#ifdef INET6_ADDRSTRLEN
#define IP6_MAX_STR_SIZE (INET6_ADDRSTRLEN-1)
#else
#define IP6_MAX_STR_SIZE 45
#endif
/*123.456.789.123*/
#ifdef INET_ADDRSTRLEN
#define IP4_MAX_STR_SIZE (INET_ADDRSTRLEN-1)
#else
#define IP4_MAX_STR_SIZE 15
#endif

/* converts a raw ipv6 addr (16 bytes) to ascii */
int ip6tosbuf(unsigned char* ip6, char* buff, int len);


/* converts a raw ipv4 addr (4 bytes) to ascii */
int ip4tosbuf(unsigned char* ip4, char* buff, int len);


/* fast ip_addr -> string converter;
 * returns number of bytes written in buf on success, <=0 on error
 * The buffer must have enough space to hold the maximum size ip address
 *  of the corresponding address (see IP[46] above) or else the function
 *  will return error (no detailed might fit checks are made, for example
 *   if len==7 the function will fail even for 1.2.3.4).
 */
int ip_addr2sbuf(struct ip_addr* ip, char* buff, int len);


/* same as ip_addr2sbuf, but with [  ] around IPv6 addresses */
int ip_addr2sbufz(struct ip_addr* ip, char* buff, int len);


/* maximum size of a str returned by ip_addr2a (including \0) */
#define IP_ADDR_MAX_STR_SIZE (IP6_MAX_STR_SIZE+1) /* ip62ascii +  \0*/
#define IP_ADDR_MAX_STRZ_SIZE (IP6_MAX_STR_SIZE+3) /* ip62ascii + [ + ] + \0*/

/* fast ip_addr -> string converter;
 * it uses an internal buffer
 */
char* ip_addr2a(struct ip_addr* ip);


/* full address in text representation, including [] for ipv6 */
char* ip_addr2strz(struct ip_addr* ip);


#define SU2A_MAX_STR_SIZE  (IP6_MAX_STR_SIZE + 2 /* [] */+\
		1 /* : */ + USHORT2SBUF_MAX_LEN + 1 /* \0 */)


/* returns an asciiz string containing the ip and the port
 *  (<ip_addr>:port or [<ipv6_addr>]:port)
 */
char* su2a(union sockaddr_union* su, int su_len);

#define SUIP2A_MAX_STR_SIZE  (IP6_MAX_STR_SIZE + 2 /* [] */ + 1 /* \0 */)

/* returns an asciiz string containing the ip
 *  (<ipv4_addr> or [<ipv6_addr>])
 */
char* suip2a(union sockaddr_union* su, int su_len);


/* converts an ip_addr structure to a hostent, returns pointer to internal
 * statical structure */
struct hostent* ip_addr2he(str* name, struct ip_addr* ip);


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
