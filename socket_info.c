
/* 
 * find & manage listen addresses 
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
 * \brief Kamailio core :: find & manage listen addresses 
 *
 * This file contains code that initializes and handles Kamailio listen addresses
 * lists (struct socket_info). It is used mainly on startup.
 * \ingroup core
 * Module: \ref core
 */

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <stdio.h>

#include <sys/ioctl.h>
#include <net/if.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#include "globals.h"
#include "socket_info.h"
#include "dprint.h"
#include "mem/mem.h"
#include "ut.h"
#include "resolve.h"
#include "name_alias.h"



/* list manip. functions (internal use only) */


/* append */
#define sock_listadd(head, el) \
	do{\
		if (*(head)==0) *(head)=(el); \
		else{ \
			for((el)->next=*(head); (el)->next->next;\
					(el)->next=(el)->next->next); \
			(el)->next->next=(el); \
			(el)->prev=(el)->next; \
			(el)->next=0; \
		}\
	}while(0)


/* insert after "after" */
#define sock_listins(el, after) \
	do{ \
		if ((after)){\
			(el)->next=(after)->next; \
			if ((after)->next) (after)->next->prev=(el); \
			(after)->next=(el); \
			(el)->prev=(after); \
		}else{ /* after==0 = list head */ \
			(after)=(el); \
			(el)->next=(el)->prev=0; \
		}\
	}while(0)


#define sock_listrm(head, el) \
	do {\
		if (*(head)==(el)) *(head)=(el)->next; \
		if ((el)->next) (el)->next->prev=(el)->prev; \
		if ((el)->prev) (el)->prev->next=(el)->next; \
	}while(0)


#define addr_info_listadd sock_listadd
#define addr_info_listins sock_listins
#define addr_info_listrm sock_listrm

inline static void addr_info_list_ins_lst(struct addr_info* lst,
										struct addr_info* after)
{
	struct addr_info* l;
	struct addr_info* n;
	
	if (lst){
		n=after->next;
		after->next=lst;
		lst->prev=after;
		if (n){
			for(l=lst; l->next; l=l->next);
			l->next=n;
			n->prev=l;
		}
	}
}


/* protocol order, filled by init_proto_order() */
enum sip_protos nxt_proto[PROTO_LAST+1]=
{ PROTO_UDP, PROTO_TCP, PROTO_TLS, PROTO_SCTP, 0 };
/* Deliberately left PROTO_WS and PROTO_WSS out of this as they are just
   upgraded TCP and TLS connections */



/* another helper function, it just fills a struct addr_info
 * returns: 0 on success, -1 on error*/
static int init_addr_info(struct addr_info* a,
								char* name, enum si_flags flags)
{

	memset(a, 0, sizeof(*a));
	a->name.len=strlen(name);
	a->name.s=pkg_malloc(a->name.len+1); /* include \0 */
	if (a->name.s==0) goto error;
	memcpy(a->name.s, name, a->name.len+1);
	a->flags=flags;
	return 0;
error:
	LM_ERR("memory allocation error\n");
	return -1;
}



/* returns 0 on error, new addr_info_lst element on success */
static inline struct addr_info* new_addr_info(char* name, 
													enum si_flags gf)
{
	struct addr_info* al;
	
	al=pkg_malloc(sizeof(*al));
	if (al==0) goto error;
	al->next=0;
	al->prev=0;
	if (init_addr_info(al, name, gf)!=0) goto error;
	return al;
error:
	LM_ERR("memory allocation error\n");
	if (al){
		if (al->name.s) pkg_free(al->name.s);
		pkg_free(al);
	}
	return 0;
}



static inline void free_addr_info(struct addr_info* a)
{
	if (a){
		if (a->name.s){
			pkg_free(a->name.s);
			a->name.s=0;
		}
		pkg_free(a);
	}
}



static inline void free_addr_info_lst(struct addr_info** lst)
{
	struct addr_info* a;
	struct addr_info* tmp;
	
	a=*lst;
	while(a){
		tmp=a;
		a=a->next;
		free_addr_info(tmp);
	}
}



/* adds a new add_info_lst element to the corresponding list
 * returns 0 on success, -1 on error */
static int new_addr_info2list(char* name, enum si_flags f,
								struct addr_info** l)
{
	struct addr_info * al;
	
	al=new_addr_info(name, f);
	if (al==0) goto error;
	addr_info_listadd(l, al);
	return 0;
error:
	return -1;
}



/* another helper function, it just creates a socket_info struct */
static inline struct socket_info* new_sock_info(	char* name,
								struct name_lst* addr_l,
								unsigned short port, unsigned short proto,
								char *usename, unsigned short useport,
								enum si_flags flags)
{
	struct socket_info* si;
	struct name_lst* n;
	struct hostent* he;
	char *p;
	
	si=(struct socket_info*) pkg_malloc(sizeof(struct socket_info));
	if (si==0) goto error;
	memset(si, 0, sizeof(struct socket_info));
	si->socket=-1;
	si->name.len=strlen(name);
	si->name.s=(char*)pkg_malloc(si->name.len+1); /* include \0 */
	if (si->name.s==0) goto error;
	memcpy(si->name.s, name, si->name.len+1);
	/* set port & proto */
	si->port_no=port;
	si->proto=proto;
	si->flags=flags;
	si->addr_info_lst=0;
	for (n=addr_l; n; n=n->next){
		if (new_addr_info2list(n->name, n->flags, &si->addr_info_lst)!=0){
			LM_ERR("new_addr_info2list failed\n");
			goto error;
		}
	}
	if(usename!=NULL)
	{
		si->useinfo.name.len=strlen(usename);
		si->useinfo.name.s=(char*)pkg_malloc(si->useinfo.name.len+1);
		if (si->useinfo.name.s==0)
			goto error;
		strcpy(si->useinfo.name.s, usename);
		if(usename[0]=='[' && usename[si->useinfo.name.len-1]==']')
		{
			si->useinfo.address_str.len = si->useinfo.name.len - 2;
			p = si->useinfo.name.s + 1;
		} else {
			si->useinfo.address_str.len = si->useinfo.name.len;
			p = si->useinfo.name.s;
		}
		si->useinfo.address_str.s=(char*)pkg_malloc(si->useinfo.address_str.len+1);
		if(si->useinfo.address_str.s==NULL)
			goto error;
		strncpy(si->useinfo.address_str.s, p, si->useinfo.address_str.len);
		si->useinfo.address_str.s[si->useinfo.address_str.len] = '\0';

		p = int2str(useport, &si->useinfo.port_no_str.len);
		if(p==NULL)
			goto error;
		si->useinfo.port_no_str.s=(char*)pkg_malloc(si->useinfo.port_no_str.len+1);
		if(si->useinfo.port_no_str.s==NULL)
			goto error;
		strcpy(si->useinfo.port_no_str.s, p);
		si->useinfo.port_no = useport;

		he=resolvehost(si->useinfo.name.s);
		if (he==0){
			LM_ERR("unable to resolve advertised name %s\n", si->useinfo.name.s);
			goto error;
		}
		hostent2ip_addr(&si->useinfo.address, he, 0);
	}
	return si;
error:
	LM_ERR("memory allocation error\n");
	if (si) {
		if(si->name.s)
			pkg_free(si->name.s);
		pkg_free(si);
	}
	return 0;
}



/*  delete a socket_info struct */
static void free_sock_info(struct socket_info* si)
{
	if(si){
		if(si->name.s) pkg_free(si->name.s);
		if(si->address_str.s) pkg_free(si->address_str.s);
		if(si->port_no_str.s) pkg_free(si->port_no_str.s);
		if(si->addr_info_lst) free_addr_info_lst(&si->addr_info_lst);
		if(si->sock_str.s) pkg_free(si->sock_str.s);
		if(si->useinfo.name.s) pkg_free(si->useinfo.name.s);
		if(si->useinfo.port_no_str.s) pkg_free(si->useinfo.port_no_str.s);
		if(si->useinfo.sock_str.s) pkg_free(si->useinfo.sock_str.s);
	}
}



char* get_valid_proto_name(unsigned short proto)
{
	switch(proto){
		case PROTO_NONE:
			return "*";
		case PROTO_UDP:
			return "udp";
#ifdef USE_TCP
		case PROTO_TCP:
			return "tcp";
#endif
#ifdef USE_TLS
		case PROTO_TLS:
			return "tls";
#endif
#ifdef USE_SCTP
		case PROTO_SCTP:
			return "sctp";
#endif
		default:
			return "unknown";
	}
}

/** Convert socket to its textual representation.
 *
 * This function converts the transport protocol, the IP address and the port
 * number in a comma delimited string of form proto:ip:port. The resulting
 * string is NOT zero terminated
 *
 * @param s is a pointer to the destination memory buffer
 * @param len is a pointer to an integer variable. Initially the variable
 *        should contain the size of the buffer in s. The value of the variable
 *        will be changed to the length of the resulting string on success and
 *        to the desired size of the destination buffer if it is too small
 * @param si is a pointer to the socket_info structure to be printed
 * @return -1 on error and 0 on success
 */
int socket2str(char* s, int* len, struct socket_info* si)
{
	return socketinfo2str(s, len, si, 0);
}

int socketinfo2str(char* s, int* len, struct socket_info* si, int mode)
{
	str proto;
	int l;
	
	proto.s = get_valid_proto_name(si->proto);
	proto.len = strlen(proto.s);
	
	if(mode==1)
		l = proto.len + si->useinfo.name.len + si->useinfo.port_no_str.len + 2;
	else
		l = proto.len + si->address_str.len + si->port_no_str.len + 2;

	if(si->address.af==AF_INET6)
		l += 2;
	
	if (*len < l) {
		LM_ERR("Destionation buffer too short\n");
		*len = l;
		return -1;
	}
	
	memcpy(s, proto.s, proto.len);
	s += proto.len;
	*s = ':'; s++;
	if(mode==1){
		memcpy(s, si->useinfo.name.s, si->useinfo.name.len);
		s += si->useinfo.name.len;
		*s = ':'; s++;
		memcpy(s, si->useinfo.port_no_str.s, si->useinfo.port_no_str.len);
		s += si->useinfo.port_no_str.len;
	} else {
		if(si->address.af==AF_INET6) {
			*s = '['; s++;
		}
		memcpy(s, si->address_str.s, si->address_str.len);
		s += si->address_str.len;
		if(si->address.af==AF_INET6) {
			*s = ']'; s++;
		}
		*s = ':'; s++;
		memcpy(s, si->port_no_str.s, si->port_no_str.len);
		s += si->port_no_str.len;
	}

	*len = l;
	return 0;
}



/* Fill si->sock_str with string representing the socket_info structure,
 * format of the string is 'proto:address:port'. Returns 0 on success and
 * negative number on failure.
 */
static int fix_sock_str(struct socket_info* si)
{
	int len = MAX_SOCKET_STR;

	if (si->sock_str.s) pkg_free(si->sock_str.s);
	
	si->sock_str.s = pkg_malloc(len + 1);
	if (si->sock_str.s == NULL) {
		LM_ERR("No memory left\n");
		return -1;
	}
	if (socketinfo2str(si->sock_str.s, &len, si, 0) < 0) {
		BUG("fix_sock_str: Error in socket to str\n");
		return -1;
	}
	si->sock_str.s[len] = '\0';
	si->sock_str.len = len;
	if(si->useinfo.name.s!=NULL)
	{
		len = MAX_SOCKET_STR;

		if (si->useinfo.sock_str.s) pkg_free(si->useinfo.sock_str.s);

		si->useinfo.sock_str.s = pkg_malloc(len + 1);
		if (si->useinfo.sock_str.s == NULL) {
			LM_ERR("No memory left\n");
			return -1;
		}
		if (socketinfo2str(si->useinfo.sock_str.s, &len, si, 1) < 0) {
			BUG("fix_sock_str: Error in socket to str\n");
			return -1;
		}
		si->useinfo.sock_str.s[len] = '\0';
		si->useinfo.sock_str.len = len;
	}
	return 0;
}


/* returns 0 if support for the protocol is not compiled or if proto is 
   invalid */
struct socket_info** get_sock_info_list(unsigned short proto)
{
	
	switch(proto){
		case PROTO_UDP:
			return &udp_listen;
			break;
		case PROTO_TCP:
		case PROTO_WS:
#ifdef USE_TCP
			return &tcp_listen;
#endif
			break;
		case PROTO_TLS:
		case PROTO_WSS:
#ifdef USE_TLS
			return &tls_listen;
#endif
			break;
		case PROTO_SCTP:
#ifdef USE_SCTP
			return &sctp_listen;
#endif
			break;
		default:
			LM_CRIT("invalid proto %d\n", proto);
	}
	return 0;
}


/* helper function for grep_sock_info
 * params:
 *  host - hostname to compare with
 *  name - official name
 *  addr_str - name's resolved ip address converted to string
 *  ip_addr - name's ip address 
 *  flags - set to SI_IS_IP if name contains an IP
 *
 * returns 0 if host matches, -1 if not */
inline static int si_hname_cmp(str* host, str* name, str* addr_str, 
								struct ip_addr* ip_addr, int flags)
{
	struct ip_addr* ip6;
	
	if ( (host->len==name->len) && 
		(strncasecmp(host->s, name->s, name->len)==0) /*slower*/)
		/* comp. must be case insensitive, host names
		 * can be written in mixed case, it will also match
		 * ipv6 addresses if we are lucky*/
		goto found;
	/* check if host == ip address */
	/* ipv6 case is uglier, host can be [3ffe::1] */
	ip6=str2ip6(host);
	if (ip6){
		if (ip_addr_cmp(ip6, ip_addr))
			goto found; /* match */
		else
			return -1; /* no match, but this is an ipv6 address
						 so no point in trying ipv4 */
	}
	/* ipv4 */
	if ( (!(flags&SI_IS_IP)) && (host->len==addr_str->len) && 
			(memcmp(host->s, addr_str->s, addr_str->len)==0) )
		goto found;
	return -1;
found:
	return 0;
}


/* checks if the proto: host:port is one of the address we listen on
 * and returns the corresponding socket_info structure.
 * if port==0, the  port number is ignored
 * if proto==0 (PROTO_NONE) the protocol is ignored
 * returns  0 if not found
 * WARNING: uses str2ip6 so it will overwrite any previous
 *  unsaved result of this function (static buffer)
 */
struct socket_info* grep_sock_info(str* host, unsigned short port,
												unsigned short proto)
{
	str hname;
	struct socket_info* si;
	struct socket_info** list;
	struct addr_info* ai;
	unsigned short c_proto;
	
	hname=*host;
	if ((hname.len>2)&&((*hname.s)=='[')&&(hname.s[hname.len-1]==']')){
		/* ipv6 reference, skip [] */
		hname.s++;
		hname.len-=2;
	}

	c_proto=(proto!=PROTO_NONE)?proto:PROTO_UDP;
retry:
	do{
		/* get the proper sock_list */
		list=get_sock_info_list(c_proto);
	
		if (list==0) /* disabled or unknown protocol */
			continue;
		for (si=*list; si; si=si->next){
			LM_DBG("checking if host==us: %d==%d && [%.*s] == [%.*s]\n",
						hname.len,
						si->name.len,
						hname.len, hname.s,
						si->name.len, si->name.s
				);
			if (port) {
				LM_DBG("checking if port %d (advertise %d) matches port %d\n",
						si->port_no, si->useinfo.port_no, port);
				if (si->port_no!=port && si->useinfo.port_no!=port) {
					continue;
				}
			}
			if (si_hname_cmp(&hname, &si->name, &si->address_str, 
								&si->address, si->flags)==0)
				goto found;
			if(si->useinfo.name.s!=NULL)
			{
				LM_DBG("checking advertise if host==us:"
						" %d==%d && [%.*s] == [%.*s]\n",
						hname.len,
						si->useinfo.name.len,
						hname.len, hname.s,
						si->useinfo.name.len, si->useinfo.name.s
				);
				if (si_hname_cmp(&hname, &si->useinfo.name,
							&si->useinfo.address_str, &si->useinfo.address,
							si->flags)==0)
					goto found;
			}
			/* try among the extra addresses */
			for (ai=si->addr_info_lst; ai; ai=ai->next)
				if (si_hname_cmp(&hname, &ai->name, &ai->address_str, 
									&ai->address, ai->flags)==0)
					goto found;
		}

	}while( (proto==0) && (c_proto=next_proto(c_proto)) );

#ifdef USE_TLS
	if (unlikely(c_proto == PROTO_WS)) {
		c_proto = PROTO_WSS;
		goto retry;
	}
#endif
/* not_found: */
	return 0;
found:
	return si;
}

/* checks if the proto:port is one of the ports we listen on
 * and returns the corresponding socket_info structure.
 * if proto==0 (PROTO_NONE) the protocol is ignored
 * returns  0 if not found
 */
struct socket_info* grep_sock_info_by_port(unsigned short port, 
											unsigned short proto)
{
	struct socket_info* si;
	struct socket_info** list;
	unsigned short c_proto;

	if (!port) {
		goto not_found;
	}
	c_proto=(proto!=PROTO_NONE)?proto:PROTO_UDP;
	do{
		/* get the proper sock_list */
		list=get_sock_info_list(c_proto);
	
		if (list==0) /* disabled or unknown protocol */
			continue;
		
		for (si=*list; si; si=si->next){
			LM_DBG("checking if port %d matches port %d\n", si->port_no, port);
			if (si->port_no==port) {
				goto found;
			}
		}
	}while( (proto==0) && (c_proto=next_proto(c_proto)) );
not_found:
	return 0;
found:
	return si;
}



/* checks if the proto: ip:port is one of the address we listen on
 * and returns the corresponding socket_info structure.
 * (same as grep_socket_info, but use ip addr instead)
 * if port==0, the  port number is ignored
 * if proto==0 (PROTO_NONE) the protocol is ignored
 * returns  0 if not found
 * WARNING: uses str2ip6 so it will overwrite any previous
 *  unsaved result of this function (static buffer)
 */
struct socket_info* find_si(struct ip_addr* ip, unsigned short port,
												unsigned short proto)
{
	struct socket_info* si;
	struct socket_info** list;
	struct addr_info* ai;
	unsigned short c_proto;
	
	c_proto=(proto!=PROTO_NONE)?proto:PROTO_UDP;
	do{
		/* get the proper sock_list */
		list=get_sock_info_list(c_proto);
	
		if (list==0) /* disabled or unknown protocol */
			continue;
		
		for (si=*list; si; si=si->next){
			if (port) {
				if (si->port_no!=port) {
					continue;
				}
			}
			if (ip_addr_cmp(ip, &si->address)
					|| ip_addr_cmp(ip, &si->useinfo.address))
				goto found;
			for (ai=si->addr_info_lst; ai; ai=ai->next)
				if (ip_addr_cmp(ip, &ai->address))
					goto found;
		}
	}while( (proto==0) && (c_proto=next_proto(c_proto)) );
/* not_found: */
	return 0;
found:
	return si;
}



/* append a new sock_info structure to the corresponding list
 * return  new sock info on success, 0 on error */
static struct socket_info* new_sock2list(char* name, struct name_lst* addr_l,
									unsigned short port,
									unsigned short proto,
									char *usename, unsigned short useport,
									enum si_flags flags,
									struct socket_info** list)
{
	struct socket_info* si;
	
	si=new_sock_info(name, addr_l, port, proto, usename, useport, flags);
	if (si==0){
		LM_ERR("new_sock_info failed\n");
		goto error;
	}
	if(socket_workers>0) {
		si->workers = socket_workers;
		socket_workers = 0;
	}
	sock_listadd(list, si);
	return si;
error:
	return 0;
}



/* adds a new sock_info structure immediately after "after"
 * return  new sock info on success, 0 on error */
static struct socket_info* new_sock2list_after(char* name,
									struct name_lst* addr_l,
									unsigned short port,
									unsigned short proto,
									char *usename,
									unsigned short useport,
									enum si_flags flags,
									struct socket_info* after)
{
	struct socket_info* si;
	
	si=new_sock_info(name, addr_l, port, proto, usename, useport, flags);
	if (si==0){
		LM_ERR("new_sock_info failed\n");
		goto error;
	}
	sock_listins(si, after);
	return si;
error:
	return 0;
}



/* adds a sock_info structure to the corresponding proto list
 * return  0 on success, -1 on error */
int add_listen_advertise_iface(char* name, struct name_lst* addr_l,
						unsigned short port, unsigned short proto,
						char *usename, unsigned short useport,
						enum si_flags flags)
{
	struct socket_info** list;
	unsigned short c_proto;
	struct name_lst* a_l;
	unsigned short c_port;
	
	c_proto=(proto!=PROTO_NONE)?proto:PROTO_UDP;
	do{
		list=get_sock_info_list(c_proto);
		if (list==0) /* disabled or unknown protocol */
			continue;
		
		if (port==0){ /* use default port */
			c_port=
#ifdef USE_TLS
				((c_proto)==PROTO_TLS)?tls_port_no:
#endif
				port_no;
		}
#ifdef USE_TLS
		else if ((c_proto==PROTO_TLS) && (proto==0)){
			/* -l  ip:port => on udp:ip:port; tcp:ip:port and tls:ip:port+1?*/
			c_port=port+1;
		}
#endif
		else{
			c_port=port;
		}
		if (c_proto!=PROTO_SCTP){
			if (new_sock2list(name, 0, c_port, c_proto, usename, useport,
								flags & ~SI_IS_MHOMED, list)==0){
				LM_ERR("new_sock2list failed\n");
				goto error;
			}
			/* add the other addresses in the list as separate sockets
			 * since only SCTP can bind to multiple addresses */
			for (a_l=addr_l; a_l; a_l=a_l->next){
				if (new_sock2list(a_l->name, 0, c_port, 
									c_proto, usename, useport,
									flags & ~SI_IS_MHOMED, list)==0){
					LM_ERR("new_sock2list failed\n");
					goto error;
				}
			}
		}else{
			if (new_sock2list(name, addr_l, c_port, c_proto, usename, useport,
						flags, list)==0){
				LM_ERR("new_sock2list failed\n");
				goto error;
			}
		}
	}while( (proto==0) && (c_proto=next_proto(c_proto)));
	return 0;
error:
	return -1;
}

/* adds a sock_info structure to the corresponding proto list
 * return  0 on success, -1 on error */
int add_listen_iface(char* name, struct name_lst* addr_l,
						unsigned short port, unsigned short proto,
						enum si_flags flags)
{
	return add_listen_advertise_iface(name, addr_l, port, proto, 0, 0, flags);
}
#ifdef __OS_linux

#include "linux/types.h"
#include "linux/netlink.h"
#include "linux/rtnetlink.h"
#include "arpa/inet.h"


#define MAX_IF_LEN 64
struct idx
{
	struct idx * 	next;
	int 		family;
	unsigned	ifa_flags;
	char		addr[MAX_IF_LEN];

};

struct idxlist{
	struct idx* 	addresses;
	int 		index;
	char 		name[MAX_IF_LEN];
	unsigned 	flags;
};

#define MAX_IFACE_NO 32

static struct idxlist *ifaces = NULL;
static int seq = 0;

#define SADDR(s) ((struct sockaddr_in*)s)->sin_addr.s_addr

#define NLMSG_TAIL(nmsg) \
	((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

int addattr_l(struct nlmsghdr *n, int maxlen, int type, const void *data,
	      int alen)
{
	int len = RTA_LENGTH(alen);
	struct rtattr *rta;

	if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len) > maxlen) {
		fprintf(stderr, "addattr_l ERROR: message exceeded bound of %d\n",maxlen);
		return -1;
	}
	rta = NLMSG_TAIL(n);
	rta->rta_type = type;
	rta->rta_len = len;
	memcpy(RTA_DATA(rta), data, alen);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);
	return 0;
}



static int nl_bound_sock(void)
{
	int sock;
	struct sockaddr_nl la;

	sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if(sock <= 0){
		LM_ERR("could not create NETLINK sock to get interface list");
		goto error;
	}

	/* bind NETLINK socket to pid */
	bzero(&la, sizeof(la));
	la.nl_family = AF_NETLINK;
	la.nl_pad = 0;
	la.nl_pid = getpid();
	la.nl_groups = 0;
	if ( bind(sock, (struct sockaddr*) &la, sizeof(la)) < 0){
		LM_ERR("could not bind NETLINK sock to sockaddr_nl\n");
		goto error;
	}

	return sock;
error:
	if(sock > 0) close(sock);
	return -1;
}

#define fill_nl_req(req, type, family) do {\
	memset(&req, 0, sizeof(req));\
	req.nlh.nlmsg_len = sizeof(req);\
	req.nlh.nlmsg_type = type;\
	req.nlh.nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST|NLM_F_DUMP;\
	req.nlh.nlmsg_pid = getpid();\
	req.nlh.nlmsg_seq = seq++;\
	req.g.rtgen_family = family;\
	} while(0);

	
static int get_flags(int family){
	struct {
		struct nlmsghdr nlh;
		struct rtgenmsg g;
	} req;
	int rtn = 0;
	struct nlmsghdr*  nlp;
	struct ifinfomsg *ifi;
	char buf[8192];
	char *p = buf;
	int nll = 0;
        int nl_sock = 0;

	fill_nl_req(req, RTM_GETLINK, AF_INET);

	if((nl_sock = nl_bound_sock()) < 0) return -1;

	if(send(nl_sock, (void*)&req, sizeof(req), 0) < 0)
	{
		LM_ERR("error sending NETLINK request\n");
		goto error;
	}

	while(1) {
		rtn = recv(nl_sock, p, sizeof(buf) - nll, 0);
		nlp = (struct nlmsghdr *) p;
		if(nlp->nlmsg_type == NLMSG_DONE){
			LM_DBG("done\n");
			 break;
		}
		if(nlp->nlmsg_type == NLMSG_ERROR){
			 LM_DBG("Error on message to netlink");
			 break;
		}
		p += rtn;

		nll += rtn;
	}

	nlp = (struct nlmsghdr *) buf;
	for(;NLMSG_OK(nlp, nll);nlp=NLMSG_NEXT(nlp, nll)){
		ifi = NLMSG_DATA(nlp);

		if (nlp->nlmsg_len < NLMSG_LENGTH(sizeof(ifi)))
			goto error;

		LM_ERR("Interface with index %d has flags %d\n", ifi->ifi_index, ifi->ifi_flags);
		if(ifaces == NULL){
			LM_ERR("get_flags must not be called on empty interface list");
			goto error;
		}
		if(ifi->ifi_index >= MAX_IFACE_NO){
			LM_ERR("invalid network interface index returned %d", ifi->ifi_index);
			goto error;
		}
		ifaces[ifi->ifi_index].flags = ifi->ifi_flags;
	}

	if(nl_sock>0) close(nl_sock);
	return 0;

error:
	if(nl_sock>0) close(nl_sock);
	return -1;
}

static int build_iface_list(void)
{
	struct {
		struct nlmsghdr nlh;
		struct rtgenmsg g;
	} req;

	int seq = 0;
	int rtn = 0;
	struct nlmsghdr*  nlp;
	struct ifaddrmsg *ifi;
	int rtl;
	char buf[8192];
	char *p = buf;
	int nll = 0;
	struct rtattr * rtap;
	int index, i;
	struct idx* entry;
	struct idx* tmp;
        int nl_sock = 0;
        int families[] = {AF_INET, AF_INET6};
        char name[MAX_IF_LEN];
	int is_link_local = 0;

	if(ifaces == NULL){
		if((ifaces = (struct idxlist*)pkg_malloc(MAX_IFACE_NO*sizeof(struct idxlist))) == NULL){
			LM_ERR("No more pkg memory\n");
			return -1;
		}
		memset(ifaces, 0, sizeof(struct idxlist)*MAX_IFACE_NO);
	}

	/* bind netlink socket */
	if((nl_sock = nl_bound_sock()) < 0) return -1;

	for (i = 0 ; i < sizeof(families)/sizeof(int); i++) {
		fill_nl_req(req, RTM_GETADDR, families[i]);

		if(send(nl_sock, (void*)&req, sizeof(req), 0) < 0){
			LM_ERR("error sending NETLINK request\n");
			goto error;
		};

		memset(buf, 0, sizeof(buf));
		nll = 0;
		p = buf;
		while(1) {
			rtn = recv(nl_sock, p, sizeof(buf) - nll, 0);
			LM_DBG("received %d byles \n", rtn);
			nlp = (struct nlmsghdr *) p;
			if(nlp->nlmsg_type == NLMSG_DONE){
				LM_DBG("done receiving netlink info \n");
				 break;
			}
			if(nlp->nlmsg_type == NLMSG_ERROR){
				 LM_ERR("Error on message to netlink");
				 break;
			}
			p += rtn;

			nll += rtn;
		}

		nlp = (struct nlmsghdr *) buf;
		for(;NLMSG_OK(nlp, nll);nlp=NLMSG_NEXT(nlp, nll)){
			ifi = NLMSG_DATA(nlp);

			if (nlp->nlmsg_len < NLMSG_LENGTH(sizeof(ifi)))
				continue;
			// init all the strings
			// inner loop: loop thru all the attributes of
			// one route entry
			rtap = (struct rtattr *) IFA_RTA(ifi);

			rtl = IFA_PAYLOAD(nlp);

			index = ifi->ifa_index;
			if(index >= MAX_IFACE_NO){
				LM_ERR("Invalid interface index returned: %d\n", index);
				goto error;
			}

			entry = (struct idx*)pkg_malloc(sizeof(struct idx));
			if(entry == 0)
			{
				LM_ERR("could not allocate memory\n");
				goto error;
			}

			entry->next = 0;
			entry->family = families[i];
			entry->ifa_flags = ifi->ifa_flags;
                        is_link_local = 0;

			for(;RTA_OK(rtap, rtl);rtap=RTA_NEXT(rtap,rtl)){
				switch(rtap->rta_type){
					case IFA_ADDRESS:
						if((*(int*)RTA_DATA(rtap))== htons(0xfe80)){
							LM_DBG("Link Local Address, ignoring ...\n");
							is_link_local = 1;
							break;
						}
						inet_ntop(families[i], RTA_DATA(rtap), entry->addr, MAX_IF_LEN);
						LM_DBG("iface <IFA_ADDRESS> addr is  %s\n", entry->addr);
						break;
					case IFA_LOCAL:
						if((*(int*)RTA_DATA(rtap))== htons(0xfe80)){
							LM_DBG("Link Local Address, ignoring ...\n");
							is_link_local = 1;
						}
						inet_ntop(families[i], RTA_DATA(rtap), entry->addr, MAX_IF_LEN);
						LM_DBG("iface <IFA_LOCAL> addr is %s\n", entry->addr);
						break;
					case IFA_LABEL:
						LM_DBG("iface name is %s\n", (char*)RTA_DATA(rtap));
						strncpy(name, (char*)RTA_DATA(rtap), MAX_IF_LEN);
						break;
					case IFA_BROADCAST:
					case IFA_ANYCAST:
					case IFA_UNSPEC:
					case IFA_CACHEINFO:
					default:
						break;
				}
			}
			if(is_link_local) continue;    /* link local addresses are not bindable */

			if(strlen(ifaces[index].name)==0)
				strncpy(ifaces[index].name, name, MAX_IF_LEN);

			ifaces[index].index = index;

			if(ifaces[index].addresses == 0 )
				ifaces[index].addresses = entry;
			else {
				for(tmp = ifaces[index].addresses; tmp->next ; tmp = tmp->next)/*empty*/;
				tmp->next = entry;
			}
		}
	}
	if(nl_sock>0) close(nl_sock);
	/* the socket should be closed so we can bind again */
	for(i = 0; i < sizeof(families)/sizeof(int); i++){
		/* get device flags */
		get_flags(families[i]); /* AF_INET or AF_INET6 */
	}

	return 0;
error:
	if(nl_sock>0) close(nl_sock);
	return -1;

}
/* add all family type addresses of interface if_to the socket_info array
 * if if_name==0, adds all addresses on all interfaces
 * uses RTNETLINK sockets to get addresses on the present interface on LINUX
 * return: -1 on error, 0 on success
 */
int add_interfaces_via_netlink(char* if_name, int family, unsigned short port,
					unsigned short proto,
					struct addr_info** ai_l)
{
	int i;
	struct idx* tmp;
	enum si_flags flags;

	if(ifaces == NULL && (build_iface_list()!=0)){
		LM_ERR("Could not get network interface list\n");
		return -1;
	}

	flags=SI_NONE;
	for(i=0; i< MAX_IFACE_NO; ++i){
		if(ifaces[i].addresses == NULL) continue; /* not present/configured */
		if ((if_name==0)||
			(strncmp(if_name, ifaces[i].name, strlen(ifaces[i].name))==0)){

			/* check if iface is up */
			//if(! (ifaces[i].flags & IFF_UP) ) continue;

			for(tmp = ifaces[i].addresses; tmp; tmp = tmp->next){
				LM_DBG("in add_iface_via_netlink Name %s Address %s\n",
							ifaces[i].name, tmp->addr);
					/* match family */
					if (family == tmp->family){
					/* check if loopback */
					if (ifaces[i].flags & IFF_LOOPBACK){
						LM_DBG("INTERFACE %s is loopback", ifaces[i].name);
						flags|=SI_IS_LO;
					}
					/* save the info */
					if (new_addr_info2list(tmp->addr, flags, ai_l)!=0){
						LM_ERR("new_addr_info2list failed\n");
						goto error;
			    		}
				}
			}
		}
	}
	return 0;
error:
	return -1;
}
#endif /* __OS_linux */

/* add all family type addresses of interface if_name to the socket_info array
 * if if_name==0, adds all addresses on all interfaces
 * WARNING: it only works with ipv6 addresses on FreeBSD
 * return: -1 on error, 0 on success
 */
int add_interfaces(char* if_name, int family, unsigned short port,
					unsigned short proto,
					struct addr_info** ai_l)
{
	struct ifconf ifc;
	struct ifreq ifr;
	struct ifreq ifrcopy;
	char*  last;
	char* p;
	int size;
	int lastlen;
	int s;
	char* tmp;
	struct ip_addr addr;
	int ret;
	enum si_flags flags;

#ifdef HAVE_SOCKADDR_SA_LEN
	#ifndef MAX
		#define MAX(a,b) ( ((a)>(b))?(a):(b))
	#endif
#endif
	/* ipv4 or ipv6 only*/
	flags=SI_NONE;
	s=socket(family, SOCK_DGRAM, 0);
	ret=-1;
	lastlen=0;
	ifc.ifc_req=0;
	for (size=100; ; size*=2){
		ifc.ifc_len=size*sizeof(struct ifreq);
		ifc.ifc_req=(struct ifreq*) pkg_malloc(size*sizeof(struct ifreq));
		if (ifc.ifc_req==0){
			LM_ERR("memory allocation failure\n");
			goto error;
		}
		if (ioctl(s, SIOCGIFCONF, &ifc)==-1){
			if(errno==EBADF) return 0; /* invalid descriptor => no such ifs*/
			LM_ERR("ioctl failed: %s\n", strerror(errno));
			goto error;
		}
		if  ((lastlen) && (ifc.ifc_len==lastlen)) break; /*success,
														   len not changed*/
		lastlen=ifc.ifc_len;
		/* try a bigger array*/
		pkg_free(ifc.ifc_req);
	}
	
	last=(char*)ifc.ifc_req+ifc.ifc_len;
	for(p=(char*)ifc.ifc_req; p<last;
			p+=
			#ifdef __OS_linux
				sizeof(ifr) /* works on x86_64 too */
			#else
				(sizeof(ifr.ifr_name)+
				#ifdef  HAVE_SOCKADDR_SA_LEN
					MAX(ifr.ifr_addr.sa_len, sizeof(struct sockaddr))
				#else
					( (ifr.ifr_addr.sa_family==AF_INET)?
						sizeof(struct sockaddr_in):
						((ifr.ifr_addr.sa_family==AF_INET6)?
						sizeof(struct sockaddr_in6):sizeof(struct sockaddr)) )
				#endif
				)
			#endif
		)
	{
		/* copy contents into ifr structure
		 * warning: it might be longer (e.g. ipv6 address) */
		memcpy(&ifr, p, sizeof(ifr));
		if (ifr.ifr_addr.sa_family!=family){
			/*printf("strange family %d skipping...\n",
					ifr->ifr_addr.sa_family);*/
			continue;
		}
		
		/*get flags*/
		ifrcopy=ifr;
		if (ioctl(s, SIOCGIFFLAGS,  &ifrcopy)!=-1){ /* ignore errors */
			/* ignore down ifs only if listening on all of them*/
			if (if_name==0){ 
				/* if if not up, skip it*/
				if (!(ifrcopy.ifr_flags & IFF_UP)) continue;
			}
		}
		
		
		
		if ((if_name==0)||
			(strncmp(if_name, ifr.ifr_name, sizeof(ifr.ifr_name))==0)){
			
			/*add address*/
			sockaddr2ip_addr(&addr, 
					(struct sockaddr*)(p+(long)&((struct ifreq*)0)->ifr_addr));
			if ((tmp=ip_addr2a(&addr))==0) goto error;
			/* check if loopback */
			if (ifrcopy.ifr_flags & IFF_LOOPBACK) 
				flags|=SI_IS_LO;
			/* save the info */
			if (new_addr_info2list(tmp, flags, ai_l)!=0){
				LM_ERR("new_addr_info2list failed\n");
				goto error;
			}
			ret=0;
		}
			/*
			printf("%s:\n", ifr->ifr_name);
			printf("        ");
			print_sockaddr(&(ifr->ifr_addr));
			printf("        ");
			ls_ifflags(ifr->ifr_name, family, options);
			printf("\n");*/
	}
	pkg_free(ifc.ifc_req); /*clean up*/
	close(s);
	return  ret;
error:
	if (ifc.ifc_req) pkg_free(ifc.ifc_req);
	close(s);
	return -1;
}



/* internal helper function: resolve host names and add aliases
 * name is a value result parameter: it should contain the hostname that
 * will be used to fill all the other members, including name itself
 * in some situation (name->s should be a 0 terminated pkg_malloc'ed string)
 * return 0 on success and -1 on error */
static int fix_hostname(str* name, struct ip_addr* address, str* address_str,
						enum si_flags* flags, int* type_flags,
						struct socket_info* s)
{
	struct hostent* he;
	char* tmp;
	char** h;
	
	/* get "official hostnames", all the aliases etc. */
	he=resolvehost(name->s);
	if (he==0){
		LM_ERR("could not resolve %s\n", name->s);
		goto error;
	}
	/* check if we got the official name */
	if (strcasecmp(he->h_name, name->s)!=0){
		if (sr_auto_aliases && 
				add_alias(name->s, name->len, s->port_no, s->proto)<0){
			LM_ERR("add_alias failed\n");
		}
		/* change the official name */
		pkg_free(name->s);
		name->s=(char*)pkg_malloc(strlen(he->h_name)+1);
		if (name->s==0){
			LM_ERR("out of memory.\n");
			goto error;
		}
		name->len=strlen(he->h_name);
		strncpy(name->s, he->h_name, name->len+1);
	}
	/* add the aliases*/
	for(h=he->h_aliases; sr_auto_aliases && h && *h; h++)
		if (add_alias(*h, strlen(*h), s->port_no, s->proto)<0){
			LM_ERR("add_alias failed\n");
		}
	hostent2ip_addr(address, he, 0); /*convert to ip_addr format*/
	if (type_flags){
		*type_flags|=(address->af==AF_INET)?SOCKET_T_IPV4:SOCKET_T_IPV6;
	}
	if ((tmp=ip_addr2a(address))==0) goto error;
	address_str->s=pkg_malloc(strlen(tmp)+1);
	if (address_str->s==0){
		LM_ERR("out of memory.\n");
		goto error;
	}
	strncpy(address_str->s, tmp, strlen(tmp)+1);
	/* set is_ip (1 if name is an ip address, 0 otherwise) */
	address_str->len=strlen(tmp);
	if (sr_auto_aliases && (address_str->len==name->len) &&
		(strncasecmp(address_str->s, name->s, address_str->len)==0)){
		*flags|=SI_IS_IP;
		/* do rev. DNS on it (for aliases)*/
		he=rev_resolvehost(address);
		if (he==0){
			LM_WARN("could not rev. resolve %s\n", name->s);
		}else{
			/* add the aliases*/
			if (add_alias(he->h_name, strlen(he->h_name), s->port_no,
							s->proto)<0){
				LM_ERR("add_alias failed\n");
			}
			for(h=he->h_aliases; h && *h; h++)
				if (add_alias(*h, strlen(*h), s->port_no, s->proto) < 0){
					LM_ERR("add_alias failed\n");
				}
		}
	}
	
#ifdef USE_MCAST
	/* Check if it is an multicast address and
	 * set the flag if so
	 */
	if (is_mcast(address)){
		*flags |= SI_IS_MCAST;
	}
#endif /* USE_MCAST */
	
	/* check if INADDR_ANY */
	if (ip_addr_any(address))
		*flags|=SI_IS_ANY;
	else if (ip_addr_loopback(address)) /* check for loopback */
		*flags|=SI_IS_LO;
	
	return 0;
error:
	return -1;
}



/* append new elements to a socket_info list after "list"
 * each element is created  from addr_info_lst + port, protocol and flags
 * return 0 on succes, -1 on error
 */
static int addr_info_to_si_lst(struct addr_info* ai_lst, unsigned short port,
								char proto, enum si_flags flags,
								struct socket_info** list)
{
	struct addr_info* ail;
	
	for (ail=ai_lst; ail; ail=ail->next){
		if(new_sock2list(ail->name.s, 0, port, proto, 0, 0,
					ail->flags | flags, list)==0)
			return -1;
	}
	return 0;
}



/* insert new elements to a socket_info list after "el", 
 * each element is created from addr_info_lst + port, * protocol and flags
 * return 0 on succes, -1 on error
 */
static int addr_info_to_si_lst_after(struct addr_info* ai_lst,
										unsigned short port,
										char proto, enum si_flags flags,
										struct socket_info* el)
{
	struct addr_info* ail;
	struct socket_info* new_si;
	
	for (ail=ai_lst; ail; ail=ail->next){
		if((new_si=new_sock2list_after(ail->name.s, 0, port, proto,
								0, 0, ail->flags | flags, el))==0)
			return -1;
		el=new_si;
	}
	return 0;
}



/* fixes a socket list => resolve addresses, 
 * interface names, fills missing members, remove duplicates
 * fills type_flags if not null with SOCKET_T_IPV4 and/or SOCKET_T_IPV6*/
static int fix_socket_list(struct socket_info **list, int* type_flags)
{
	struct socket_info* si;
	struct socket_info* new_si;
	struct socket_info* l;
	struct socket_info* next;
	struct socket_info* next_si;
	struct socket_info* del_si;
	struct socket_info* keep_si;
	char* tmp;
	int len;
	struct addr_info* ai_lst;
	struct addr_info* ail;
	struct addr_info* tmp_ail;
	struct addr_info* tmp_ail_next;
	struct addr_info* ail_next;

	if (type_flags)
		*type_flags=0;
	/* try to change all the interface names into addresses
	 *  --ugly hack */
	for (si=*list;si;){
		next=si->next;
		ai_lst=0;
		if (add_interfaces(si->name.s, AF_INET, si->port_no,
							si->proto, &ai_lst)!=-1){
			if (si->flags & SI_IS_MHOMED){
				if((new_si=new_sock2list_after(ai_lst->name.s, 0, si->port_no,
											si->proto, si->useinfo.name.s,
											si->useinfo.port_no,
											ai_lst->flags|si->flags, si))==0)
					break;
				ail=ai_lst;
				ai_lst=ai_lst->next;
				free_addr_info(ail); /* free the first elem. */
				if (ai_lst){
					ai_lst->prev=0;
					/* find the end */
					for (ail=ai_lst; ail->next; ail=ail->next);
					/* add the mh list after the last position in ai_lst */
					addr_info_list_ins_lst(si->addr_info_lst, ail);
					new_si->addr_info_lst=ai_lst;
					si->addr_info_lst=0; /* detached and moved to new_si */
					ail=ail->next; /* ail== old si->addr_info_lst */
				}else{
					ail=si->addr_info_lst;
					new_si->addr_info_lst=ail;
					si->addr_info_lst=0; /* detached and moved to new_si */
				}
				
			}else{
				/* add all addr. as separate  interfaces */
				if (addr_info_to_si_lst_after(ai_lst, si->port_no, si->proto,
						 						si->flags, si)!=0)
					goto error;
				/* ai_lst not needed anymore */
				free_addr_info_lst(&ai_lst);
				ail=0;
				new_si=0;
			}
			/* success => remove current entry (shift the entire array)*/
			sock_listrm(list, si);
			free_sock_info(si);
		}else{
			new_si=si;
			ail=si->addr_info_lst;
		}
		
		if (ail){
			if (new_si && (new_si->flags & SI_IS_MHOMED)){
				ai_lst=0;
				for (; ail;){
					ail_next=ail->next;
					if (add_interfaces(ail->name.s, AF_INET, new_si->port_no,
											new_si->proto, &ai_lst)!=-1){
						/* add the resolved list after the current position */
						addr_info_list_ins_lst(ai_lst, ail);
						/* success, remove the current entity */
						addr_info_listrm(&new_si->addr_info_lst, ail);
						free_addr_info(ail);
						ai_lst=0;
					}
					ail=ail_next;
				}
			}
		}
		si=next;
	}
	/* get ips & fill the port numbers*/
#ifdef EXTRA_DEBUG
	LM_DBG("Listening on\n");
#endif
	for (si=*list;si;si=si->next){
		/* fix port number, port_no should be !=0 here */
		if (si->port_no==0){
#ifdef USE_TLS
			si->port_no= (si->proto==PROTO_TLS)?tls_port_no:port_no;
#else
			si->port_no= port_no;
#endif
		}
		tmp=int2str(si->port_no, &len);
		if (len>=MAX_PORT_LEN){
			LM_ERR("bad port number: %d\n", si->port_no);
			goto error;
		}
		si->port_no_str.s=(char*)pkg_malloc(len+1);
		if (si->port_no_str.s==0){
			LM_ERR("out of memory.\n");
			goto error;
		}
		strncpy(si->port_no_str.s, tmp, len+1);
		si->port_no_str.len=len;
		
		if (fix_hostname(&si->name, &si->address, &si->address_str,
						&si->flags, type_flags, si) !=0 )
			goto error;
		/* fix hostnames in mh addresses */
		for (ail=si->addr_info_lst; ail; ail=ail->next){
			if (fix_hostname(&ail->name, &ail->address, &ail->address_str,
						&ail->flags, type_flags, si) !=0 )
				goto error;
		}

		if (fix_sock_str(si) < 0) goto error;
		
#ifdef EXTRA_DEBUG
		printf("              %.*s [%s]:%s%s\n", si->name.len, 
				si->name.s, si->address_str.s, si->port_no_str.s,
		                si->flags & SI_IS_MCAST ? " mcast" : "");
#endif
	}
	/* removing duplicate addresses*/
	for (si=*list;si; ){
		next_si=si->next;
		for (l=si->next;l;){
			next=l->next;
			if ((si->port_no==l->port_no) &&
				(si->address.af==l->address.af) &&
				(memcmp(si->address.u.addr, l->address.u.addr,
						si->address.len) == 0)
				){
				/* remove the socket with no  extra addresses.,
				 * if both of them have extra addresses, remove one of them
				 * and merge the extra addresses into the other */
				if (l->addr_info_lst==0){
					del_si=l;
					keep_si=si;
				}else if (si->addr_info_lst==0){
					del_si=si;
					keep_si=l;
				}else{
					/* move l->addr_info_lst to si->addr_info_lst */
					/* find last elem */
					for (ail=si->addr_info_lst; ail->next; ail=ail->next);
					/* add the l list after the last position in si lst */
					addr_info_list_ins_lst(l->addr_info_lst, ail);
					l->addr_info_lst=0; /* detached */
					del_si=l; /* l will be removed */
					keep_si=l;
				}
#ifdef EXTRA_DEBUG
				printf("removing duplicate %s [%s] ==  %s [%s]\n",
						keep_si->name.s, keep_si->address_str.s,
						 del_si->name.s, del_si->address_str.s);
#endif
				/* add the name to the alias list*/
				if ((!(del_si->flags& SI_IS_IP)) && (
						(del_si->name.len!=keep_si->name.len)||
						(strncmp(del_si->name.s, keep_si->name.s,
								 del_si->name.len)!=0))
					)
					add_alias(del_si->name.s, del_si->name.len,
								l->port_no, l->proto);
				/* make sure next_si doesn't point to del_si */
				if (del_si==next_si)
					next_si=next_si->next;
				/* remove del_si*/
				sock_listrm(list, del_si);
				free_sock_info(del_si);
			}
			l=next;
		}
		si=next_si;
	}
	/* check for duplicates in extra_addresses */
	for (si=*list;si; si=si->next){
		/* check  for & remove internal duplicates: */
		for (ail=si->addr_info_lst; ail;){
			ail_next=ail->next;
			/* 1. check if the extra addresses contain a duplicate for the 
			 * main  one */
			if ((ail->address.af==si->address.af) &&
				(memcmp(ail->address.u.addr, si->address.u.addr,
							ail->address.len) == 0)){
				/* add the name to the alias list*/
				if ((!(ail->flags& SI_IS_IP)) && (
					(ail->name.len!=si->name.len)||
					(strncmp(ail->name.s, si->name.s, ail->name.len)!=0)))
					add_alias(ail->name.s, ail->name.len, si->port_no,
								si->proto);
					/* remove ail*/
				addr_info_listrm(&si->addr_info_lst, ail);
				free_addr_info(ail);
				ail=ail_next;
				continue;
			}
			/* 2. check if the extra addresses contain a duplicates for 
			 *  other addresses in the same list */
			for (tmp_ail=ail->next; tmp_ail;){
				tmp_ail_next=tmp_ail->next;
				if ((ail->address.af==tmp_ail->address.af) &&
					(memcmp(ail->address.u.addr, tmp_ail->address.u.addr,
							ail->address.len) == 0)){
					/* add the name to the alias list*/
					if ((!(tmp_ail->flags& SI_IS_IP)) && (
						(ail->name.len!=tmp_ail->name.len)||
						(strncmp(ail->name.s, tmp_ail->name.s,
										tmp_ail->name.len)!=0))
						)
						add_alias(tmp_ail->name.s, tmp_ail->name.len,
									si->port_no, si->proto);
						/* remove tmp_ail*/
					addr_info_listrm(&si->addr_info_lst, tmp_ail);
					free_addr_info(tmp_ail);
				}
				tmp_ail=tmp_ail_next;
			}
			ail=ail_next;
		}
		/* check for duplicates between extra addresses (e.g. sctp MH)
		 * and other main addresses, on conflict remove the corresponding
		 * extra addresses (another possible solution would be to join
		 * the 2 si entries into one). */
		for (ail=si->addr_info_lst; ail;){
			ail_next=ail->next;
			for (l=*list;l; l=l->next){
				if (l==si) continue;
				if (si->port_no==l->port_no){
					if ((ail->address.af==l->address.af) &&
						(memcmp(ail->address.u.addr, l->address.u.addr,
										ail->address.len) == 0)){
						/* add the name to the alias list*/
						if ((!(ail->flags& SI_IS_IP)) && (
							(ail->name.len!=l->name.len)||
							(strncmp(ail->name.s, l->name.s, l->name.len)!=0))
							)
							add_alias(ail->name.s, ail->name.len,
										l->port_no, l->proto);
						/* remove ail*/
						addr_info_listrm(&si->addr_info_lst, ail);
						free_addr_info(ail);
						break;
					}
					/* check for duplicates with other  extra addresses
					 * lists */
					for (tmp_ail=l->addr_info_lst; tmp_ail; ){
						tmp_ail_next=tmp_ail->next;
						if ((ail->address.af==tmp_ail->address.af) &&
							(memcmp(ail->address.u.addr,
									tmp_ail->address.u.addr,
									ail->address.len) == 0)){
							/* add the name to the alias list*/
							if ((!(tmp_ail->flags& SI_IS_IP)) && (
									(ail->name.len!=tmp_ail->name.len)||
									(strncmp(ail->name.s, tmp_ail->name.s,
											tmp_ail->name.len)!=0))
								)
								add_alias(tmp_ail->name.s, tmp_ail->name.len,
										l->port_no, l->proto);
							/* remove tmp_ail*/
							addr_info_listrm(&l->addr_info_lst, tmp_ail);
							free_addr_info(tmp_ail);
						}
						tmp_ail=tmp_ail_next;
					}
				}
			}
			ail=ail_next;
		}
	}

#ifdef USE_MCAST
	     /* Remove invalid multicast entries */
	si=*list;
	while(si){
		if ((si->proto == PROTO_TCP)
#ifdef USE_TLS
		    || (si->proto == PROTO_TLS)
#endif /* USE_TLS */
#ifdef USE_SCTP
			|| (si->proto == PROTO_SCTP)
#endif
			){
			if (si->flags & SI_IS_MCAST){
				LM_WARN("removing entry %s:%s [%s]:%s\n",
					get_valid_proto_name(si->proto), si->name.s, 
					si->address_str.s, si->port_no_str.s);
				l = si;
				si=si->next;
				sock_listrm(list, l);
				free_sock_info(l);
			}else{
				ail=si->addr_info_lst;
				while(ail){
					if (ail->flags & SI_IS_MCAST){
						LM_WARN("removing mh entry %s:%s"
								" [%s]:%s\n",
								get_valid_proto_name(si->proto), ail->name.s, 
								ail->address_str.s, si->port_no_str.s);
						tmp_ail=ail;
						ail=ail->next;
						addr_info_listrm(&si->addr_info_lst, tmp_ail);
						free_addr_info(tmp_ail);
					}else{
						ail=ail->next;
					}
				}
				si=si->next;
			}
		} else {
			si=si->next;
		}
	}
#endif /* USE_MCAST */

	return 0;
error:
	return -1;
}

int socket_types = 0;

/* fix all 3 socket lists, fills socket_types if non-null
 * return 0 on success, -1 on error */
int fix_all_socket_lists()
{
	struct utsname myname;
	int flags;
	struct addr_info* ai_lst;
	
	ai_lst=0;
	
	if ((udp_listen==0)
#ifdef USE_TCP
			&& (tcp_listen==0)
#ifdef USE_TLS
			&& (tls_listen==0)
#endif
#endif
#ifdef USE_SCTP
			&& (sctp_listen==0)
#endif
		){
		/* get all listening ipv4/ipv6 interfaces */
		if ( ( (add_interfaces(0, AF_INET, 0,  PROTO_UDP, &ai_lst)==0)
#ifdef __OS_linux
		&&  (!auto_bind_ipv6 || add_interfaces_via_netlink(0, AF_INET6, 0, PROTO_UDP, &ai_lst) == 0)
#else
		&& ( !auto_bind_ipv6 || add_interfaces(0, AF_INET6, 0,  PROTO_UDP, &ai_lst) !=0 ) /* add_interface does not work for IPv6 on Linux */
#endif /* __OS_linux */
			 ) && (addr_info_to_si_lst(ai_lst, 0, PROTO_UDP, 0, &udp_listen)==0)){
			free_addr_info_lst(&ai_lst);
			ai_lst=0;
			/* if ok, try to add the others too */
#ifdef USE_TCP
			if (!tcp_disable){
				if ( ((add_interfaces(0, AF_INET, 0,  PROTO_TCP, &ai_lst)!=0)
#ifdef __OS_linux
    				|| (auto_bind_ipv6 && add_interfaces_via_netlink(0, AF_INET6, 0, PROTO_TCP, &ai_lst) != 0)
#else
				|| (auto_bind_ipv6 && add_interfaces(0, AF_INET6, 0,  PROTO_TCP, &ai_lst) !=0 )
#endif /* __OS_linux */
				) || (addr_info_to_si_lst(ai_lst, 0, PROTO_TCP, 0,
										 				&tcp_listen)!=0))
					goto error;
				free_addr_info_lst(&ai_lst);
				ai_lst=0;
#ifdef USE_TLS
				if (!tls_disable){
					if (((add_interfaces(0, AF_INET, 0, PROTO_TLS,
										&ai_lst)!=0)
#ifdef __OS_linux
    				|| (auto_bind_ipv6 && add_interfaces_via_netlink(0, AF_INET6, 0, PROTO_TLS, &ai_lst) != 0)
#else
				|| (auto_bind_ipv6 && add_interfaces(0, AF_INET6, 0,  PROTO_TLS, &ai_lst)!=0)
#endif /* __OS_linux */
					) || (addr_info_to_si_lst(ai_lst, 0, PROTO_TLS, 0,
										 				&tls_listen)!=0))
						goto error;
				}
				free_addr_info_lst(&ai_lst);
				ai_lst=0;
#endif
			}
#endif
#ifdef USE_SCTP
			if (!sctp_disable){
				if (((add_interfaces(0, AF_INET, 0,  PROTO_SCTP, &ai_lst)!=0)
#ifdef __OS_linux
    				|| (auto_bind_ipv6 && add_interfaces_via_netlink(0, AF_INET6, 0, PROTO_SCTP, &ai_lst) != 0)
#else
				|| (auto_bind_ipv6 && add_interfaces(0, AF_INET6, 0,  PROTO_SCTP, &ai_lst) != 0)
#endif /* __OS_linux */
					) || (addr_info_to_si_lst(ai_lst, 0, PROTO_SCTP, 0,
							 				&sctp_listen)!=0))
					goto error;
				free_addr_info_lst(&ai_lst);
				ai_lst=0;
			}
#endif /* USE_SCTP */
		}else{
			/* if error fall back to get hostname */
			/* get our address, only the first one */
			if (uname (&myname) <0){
				LM_ERR("cannot determine hostname, try -l address\n");
				goto error;
			}
			if (add_listen_iface(myname.nodename, 0, 0, 0, 0)!=0){
				LM_ERR("add_listen_iface failed \n");
				goto error;
			}
		}
	}
	flags=0;
	if (fix_socket_list(&udp_listen, &flags)!=0){
		LM_ERR("fix_socket_list udp failed\n");
		goto error;
	}
	if (flags){
		socket_types|=flags|SOCKET_T_UDP;
	}
#ifdef USE_TCP
	flags=0;
	if (!tcp_disable && (fix_socket_list(&tcp_listen, &flags)!=0)){
		LM_ERR("fix_socket_list tcp failed\n");
		goto error;
	}
	if (flags){
		socket_types|=flags|SOCKET_T_TCP;
	}
#ifdef USE_TLS
	flags=0;
	if (!tls_disable && (fix_socket_list(&tls_listen, &flags)!=0)){
		LM_ERR("fix_socket_list tls failed\n");
		goto error;
	}
	if (flags){
		socket_types|=flags|SOCKET_T_TLS;
	}
#endif
#endif
#ifdef USE_SCTP
	flags=0;
	if (!sctp_disable && (fix_socket_list(&sctp_listen, &flags)!=0)){
		LM_ERR("fix_socket_list sctp failed\n");
		goto error;
	}
	if (flags){
		socket_types|=flags|SOCKET_T_SCTP;
	}
#endif /* USE_SCTP */
	if ((udp_listen==0)
#ifdef USE_TCP
			&& (tcp_listen==0)
#ifdef USE_TLS
			&& (tls_listen==0)
#endif
#endif
#ifdef USE_SCTP
			&& (sctp_listen==0)
#endif
		){
		LM_ERR("no listening sockets\n");
		goto error;
	}
	return 0;
error:
	if (ai_lst) free_addr_info_lst(&ai_lst);
	return -1;
}



void print_all_socket_lists()
{
	struct socket_info *si;
	struct socket_info** list;
	struct addr_info* ai;
	unsigned short proto;
	
	
	proto=PROTO_UDP;
	do{
		list=get_sock_info_list(proto);
		for(si=list?*list:0; si; si=si->next){
			if (si->addr_info_lst){
				printf("             %s: (%s",
						get_valid_proto_name(proto),
						si->address_str.s);
				for (ai=si->addr_info_lst; ai; ai=ai->next)
					printf(", %s", ai->address_str.s);
				printf("):%s%s%s\n",
						si->port_no_str.s, 
						si->flags & SI_IS_MCAST ? " mcast" : "",
						si->flags & SI_IS_MHOMED? " mhomed" : "");
			}else{
				printf("             %s: %s",
						get_valid_proto_name(proto),
						si->name.s);
				if (!si->flags & SI_IS_IP)
					printf(" [%s]", si->address_str.s);
				printf( ":%s%s%s",
						si->port_no_str.s, 
						si->flags & SI_IS_MCAST ? " mcast" : "",
						si->flags & SI_IS_MHOMED? " mhomed" : "");
				if (si->useinfo.name.s)
					printf(" advertise %s:%d", si->useinfo.name.s, si->useinfo.port_no);
				printf("\n");
			}
		}
	}while((proto=next_proto(proto)));
}


void print_aliases()
{
	struct host_alias* a;

	for(a=aliases; a; a=a->next) 
		if (a->port)
			printf("             %s: %.*s:%d\n", get_valid_proto_name(a->proto), 
					a->alias.len, a->alias.s, a->port);
		else
			printf("             %s: %.*s:*\n", get_valid_proto_name(a->proto), 
					a->alias.len, a->alias.s);
}



void init_proto_order()
{
	int r;
	
	/* fix proto list  (remove disabled protocols)*/
#ifdef USE_TCP
	if (tcp_disable)
#endif
		for(r=PROTO_NONE; r<=PROTO_LAST; r++){
			if (nxt_proto[r]==PROTO_TCP)
				nxt_proto[r]=nxt_proto[PROTO_TCP];
		}
#ifdef USE_TCP
#ifdef USE_TLS
	if (tls_disable || tcp_disable)
#endif
#endif
		for(r=PROTO_NONE; r<=PROTO_LAST; r++){
			if (nxt_proto[r]==PROTO_TLS)
				nxt_proto[r]=nxt_proto[PROTO_TLS];
		}
#ifdef USE_SCTP
	if (sctp_disable)
#endif
		for(r=PROTO_NONE; r<=PROTO_LAST; r++){
			if (nxt_proto[r]==PROTO_SCTP)
				nxt_proto[r]=nxt_proto[PROTO_SCTP];
		}

	/* Deliberately skipping PROTO_WS and PROTO_WSS here as these
	   are just upgraded TCP and TLS connections */
}


/**
 * parse '[port:]host[:port]' string to a broken down structure
 */
int parse_protohostport(str* ins, sr_phostp_t *r)
{
	char* first; /* first ':' occurrence */
	char* second; /* second ':' occurrence */
	char* p;
	int bracket;
	char* tmp;

	first=second=0;
	bracket=0;
	memset(r, 0, sizeof(sr_phostp_t));

	/* find the first 2 ':', ignoring possible ipv6 addresses
	 * (substrings between [])
	 */
	for(p=ins->s; p<ins->s+ins->len; p++){
		switch(*p){
			case '[':
				bracket++;
				if (bracket>1) goto error_brackets;
				break;
			case ']':
				bracket--;
				if (bracket<0) goto error_brackets;
				break;
			case ':':
				if (bracket==0){
					if (first==0) first=p;
					else if( second==0) second=p;
					else goto error_colons;
				}
				break;
		}
	}
	if (p==ins->s) return -1;
	if (*(p-1)==':') goto error_colons;

	if (first==0) { /* no ':' => only host */
		r->host.s=ins->s;
		r->host.len=(int)(p-ins->s);
		goto end;
	}
	if (second) { /* 2 ':' found => check if valid */
		if (parse_proto((unsigned char*)ins->s, first-ins->s, &r->proto)<0)
			goto error_proto;
		r->port=strtol(second+1, &tmp, 10);
		if ((tmp==0)||(*tmp)||(tmp==second+1)) goto error_port;
		r->host.s=first+1;
		r->host.len=(int)(second-r->host.s);
		goto end;
	}
	/* only 1 ':' found => it's either proto:host or host:port */
	r->port=strtol(first+1, &tmp, 10);
	if ((tmp==0)||(*tmp)||(tmp==first+1)){
		/* invalid port => it's proto:host */
		if (parse_proto((unsigned char*)ins->s, first-ins->s, &r->proto)<0)
			goto error_proto;
		r->host.s=first+1;
		r->host.len=(int)(p-r->host.s);
	}else{
		/* valid port => its host:port */
		r->host.s=ins->s;
		r->host.len=(int)(first-r->host.s);
	}
end:
	return 0;
error_brackets:
	LM_ERR("too many brackets in %.*s\n", ins->len, ins->s);
	return -1;
error_colons:
	LM_ERR("too many colons in %.*s\n", ins->len, ins->s);
	return -1;
error_proto:
	LM_ERR("bad protocol in %.*s\n", ins->len, ins->s);
	return -1;
error_port:
	LM_ERR("bad port number in %.*s\n", ins->len, ins->s);
	return -1;
}

/**
 * lookup a local socket by '[port:]host[:port]' string
 */
struct socket_info* lookup_local_socket(str *phostp)
{
	sr_phostp_t r;
	if(parse_protohostport(phostp, &r)<0)
		return NULL;
	return grep_sock_info(&r.host, (unsigned short)r.port,
			(unsigned short)r.proto);
}
