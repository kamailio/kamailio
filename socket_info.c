/* $Id$
 *
 * find & manage listen addresses 
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
 * This file contains code that initializes and handles ser listen addresses
 * lists (struct socket_info). It is used mainly on startup.
 * 
 * History:
 * --------
 *  2003-10-22  created by andrei
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



/* another helper function, it just creates a socket_info struct */
static inline struct socket_info* new_sock_info(	char* name,
								unsigned short port, unsigned short proto,
								enum si_flags flags)
{
	struct socket_info* si;
	
	si=(struct socket_info*) pkg_malloc(sizeof(struct socket_info));
	if (si==0) goto error;
	memset(si, 0, sizeof(struct socket_info));
	si->name.len=strlen(name);
	si->name.s=(char*)pkg_malloc(si->name.len+1); /* include \0 */
	if (si->name.s==0) goto error;
	memcpy(si->name.s, name, si->name.len+1);
	/* set port & proto */
	si->port_no=port;
	si->proto=proto;
	si->flags=flags;
	return si;
error:
	LOG(L_ERR, "ERROR: new_sock_info: memory allocation error\n");
	if (si) pkg_free(si);
	return 0;
}



/*  delete a socket_info struct */
static void free_sock_info(struct socket_info* si)
{
	if(si){
		if(si->name.s) pkg_free(si->name.s);
		if(si->address_str.s) pkg_free(si->address_str.s);
		if(si->port_no_str.s) pkg_free(si->port_no_str.s);
	}
}



static char* get_proto_name(unsigned short proto)
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
		default:
			return "unknown";
	}
}


static struct socket_info** get_sock_info_list(unsigned short proto)
{
	
	switch(proto){
		case PROTO_UDP:
			return &udp_listen;
			break;
#ifdef USE_TCP
		case PROTO_TCP:
			return &tcp_listen;
			break;
#endif
#ifdef USE_TLS
		case PROTO_TLS:
			return &tls_listen;
			break;
#endif
		default:
			LOG(L_CRIT, "BUG: get_sock_info_list: invalid proto %d\n", proto);
	}
	return 0;
}



/* adds a new sock_info structure to the corresponding list
 * return  0 on success, -1 on error */
int new_sock2list(char* name, unsigned short port, unsigned short proto,
						enum si_flags flags, struct socket_info** list)
{
	struct socket_info* si;
	
	si=new_sock_info(name, port, proto, flags);
	if (si==0){
		LOG(L_ERR, "ERROR: add_listen_iface: new_sock_info failed\n");
		goto error;
	}
	sock_listadd(list, si);
	return 0;
error:
	return -1;
}



/* adds a sock_info structure to the corresponding proto list
 * return  0 on success, -1 on error */
int add_listen_iface(char* name, unsigned short port, unsigned short proto,
						enum si_flags flags)
{
	struct socket_info** list;
	unsigned short c_proto;
	
	c_proto=(proto)?proto:PROTO_UDP;
	do{
		list=get_sock_info_list(c_proto);
		if (list==0){
			LOG(L_ERR, "ERROR: add_listen_iface: get_sock_info_list failed\n");
			goto error;
		}
		if (new_sock2list(name, port, c_proto, flags, list)<0){
			LOG(L_ERR, "ERROR: add_listen_iface: new_sock2list failed\n");
			goto error;
		}
	}while( (proto==0) && (c_proto=next_proto(c_proto)));
	return 0;
error:
	return -1;
}



/* add all family type addresses of interface if_name to the socket_info array
 * if if_name==0, adds all addresses on all interfaces
 * WARNING: it only works with ipv6 addresses on FreeBSD
 * return: -1 on error, 0 on success
 */
int add_interfaces(char* if_name, int family, unsigned short port,
					unsigned short proto,
					struct socket_info** list)
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
			LOG(L_ERR, "ERROR: add_interfaces: memory allocation failure\n");
			goto error;
		}
		if (ioctl(s, SIOCGIFCONF, &ifc)==-1){
			if(errno==EBADF) return 0; /* invalid descriptor => no such ifs*/
			LOG(L_ERR, "ERROR: add_interfaces: ioctl failed: %s\n",
					strerror(errno));
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
			p+=(sizeof(ifr.ifr_name)+
			#ifdef  HAVE_SOCKADDR_SA_LEN
				MAX(ifr.ifr_addr.sa_len, sizeof(struct sockaddr))
			#else
				( (ifr.ifr_addr.sa_family==AF_INET)?
					sizeof(struct sockaddr_in):
					((ifr.ifr_addr.sa_family==AF_INET6)?
						sizeof(struct sockaddr_in6):sizeof(struct sockaddr)) )
			#endif
				)
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
			/* add it to one of the lists */
			if (new_sock2list(tmp, port, proto, flags, list)!=0){
				LOG(L_ERR, "ERROR: add_interfaces: new_sock2list failed\n");
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



/* fixes a socket list => resolve addresses, 
 * interface names, fills missing members, remove duplicates */
static int fix_socket_list(struct socket_info **list)
{
	struct socket_info* si;
	struct socket_info* l;
	struct socket_info* next;
	char* tmp;
	int len;
	struct hostent* he;
	char** h;
	
	/* try to change all the interface names into addresses
	 *  --ugly hack */
	
	for (si=*list;si;){
		next=si->next;
		if (add_interfaces(si->name.s, AF_INET, si->port_no,
							si->proto, list)!=-1){
			/* success => remove current entry (shift the entire array)*/
			sock_listrm(list, si);
			free_sock_info(si);
		}
		si=next;
	}
	/* get ips & fill the port numbers*/
#ifdef EXTRA_DEBUG
	DBG("Listening on \n");
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
			LOG(L_ERR, "ERROR: fix_socket_list: bad port number: %d\n", 
						si->port_no);
			goto error;
		}
		si->port_no_str.s=(char*)pkg_malloc(len+1);
		if (si->port_no_str.s==0){
			LOG(L_ERR, "ERROR: fix_socket_list: out of memory.\n");
			goto error;
		}
		strncpy(si->port_no_str.s, tmp, len+1);
		si->port_no_str.len=len;
		
		/* get "official hostnames", all the aliases etc. */
		he=resolvehost(si->name.s);
		if (he==0){
			LOG(L_ERR, "ERROR: fix_socket_list: could not resolve %s\n",
					si->name.s);
			goto error;
		}
		/* check if we got the official name */
		if (strcasecmp(he->h_name, si->name.s)!=0){
			if (add_alias(si->name.s, si->name.len,
							si->port_no, si->proto)<0){
				LOG(L_ERR, "ERROR: fix_socket_list: add_alias failed\n");
			}
			/* change the oficial name */
			pkg_free(si->name.s);
			si->name.s=(char*)pkg_malloc(strlen(he->h_name)+1);
			if (si->name.s==0){
				LOG(L_ERR,  "ERROR: fix_socket_list: out of memory.\n");
				goto error;
			}
			si->name.len=strlen(he->h_name);
			strncpy(si->name.s, he->h_name, si->name.len+1);
		}
		/* add the aliases*/
		for(h=he->h_aliases; h && *h; h++)
			if (add_alias(*h, strlen(*h), si->port_no, si->proto)<0){
				LOG(L_ERR, "ERROR: fix_socket_list: add_alias failed\n");
			}
		hostent2ip_addr(&si->address, he, 0); /*convert to ip_addr 
														 format*/
		if ((tmp=ip_addr2a(&si->address))==0) goto error;
		si->address_str.s=(char*)pkg_malloc(strlen(tmp)+1);
		if (si->address_str.s==0){
			LOG(L_ERR, "ERROR: fix_socket_list: out of memory.\n");
			goto error;
		}
		strncpy(si->address_str.s, tmp, strlen(tmp)+1);
		/* set is_ip (1 if name is an ip address, 0 otherwise) */
		si->address_str.len=strlen(tmp);
		if 	(	(si->address_str.len==si->name.len)&&
				(strncasecmp(si->address_str.s, si->name.s,
								si->address_str.len)==0)
			){
				si->flags|=SI_IS_IP;
				/* do rev. dns on it (for aliases)*/
				he=rev_resolvehost(&si->address);
				if (he==0){
					LOG(L_WARN, "WARNING: fix_socket_list:"
							" could not rev. resolve %s\n",
							si->name.s);
				}else{
					/* add the aliases*/
					if (add_alias(he->h_name, strlen(he->h_name),
									si->port_no, si->proto)<0){
						LOG(L_ERR, "ERROR: fix_socket_list:"
									"add_alias failed\n");
					}
					for(h=he->h_aliases; h && *h; h++)
						if (add_alias(*h,strlen(*h),si->port_no,si->proto)<0){
							LOG(L_ERR, "ERROR: fix_socket_list:"
									" add_alias failed\n");
						}
				}
		}

#ifdef USE_MCAST
		     /* Check if it is an multicast address and
		      * set the flag if so
		      */
		if (is_mcast(&si->address)) {
			si->flags |= SI_IS_MCAST;
		}
#endif /* USE_MCAST */

#ifdef EXTRA_DEBUG
		printf("              %.*s [%s]:%s%s\n", si->name.len, 
				si->name.s, si->address_str.s, si->port_no_str.s,
		                si->flags & SI_IS_MCAST ? " mcast" : "");
#endif
	}
	/* removing duplicate addresses*/
	for (si=*list;si; si=si->next){
		for (l=si->next;l;){
			next=l->next;
			if ((si->port_no==l->port_no) &&
				(si->address.af==l->address.af) &&
				(memcmp(si->address.u.addr, l->address.u.addr, si->address.len)
					== 0)
				){
#ifdef EXTRA_DEBUG
				printf("removing duplicate %s [%s] ==  %s [%s]\n",
						si->name.s, si->address_str.s,
						 l->name.s, l->address_str.s);
#endif
				/* add the name to the alias list*/
				if ((!(l->flags& SI_IS_IP)) && (
						(l->name.len!=si->name.len)||
						(strncmp(l->name.s, si->name.s, si->name.len)!=0))
					)
					add_alias(l->name.s, l->name.len, l->port_no, l->proto);
						
				/* remove l*/
				sock_listrm(list, l);
				free_sock_info(l);
			}
			l=next;
		}
	}

#ifdef USE_MCAST
	     /* Remove invalid multicast entries */
	si=*list;
	while(si){
		if ((si->flags & SI_IS_MCAST) && 
		    ((si->proto == PROTO_TCP)
#ifdef USE_TLS
		    || (si->proto == PROTO_TLS)
#endif /* USE_TLS */
		    )){
			LOG(L_WARN, "WARNING: removing entry %s:%s [%s]:%s\n",
			    get_proto_name(si->proto), si->name.s, 
			    si->address_str.s, si->port_no_str.s);
			l = si;
			si=si->next;
			sock_listrm(list, l);
			free_sock_info(l);
		} else {
			si=si->next;
		}
	}
#endif /* USE_MCAST */

	return 0;
error:
	return -1;
}



/* fix all 3 socket lists
 * return 0 on success, -1 on error */
int fix_all_socket_lists()
{
	struct utsname myname;
	
	if ((udp_listen==0)
#ifdef USE_TCP
			&& (tcp_listen==0)
#ifdef USE_TLS
			&& (tls_listen==0)
#endif
#endif
		){
		/* get all listening ipv4 interfaces */
		if (add_interfaces(0, AF_INET, 0,  PROTO_UDP, &udp_listen)==0){
			/* if ok, try to add the others too */
#ifdef USE_TCP
			if (add_interfaces(0, AF_INET, 0,  PROTO_TCP, &tcp_listen)!=0)
				goto error;
#ifdef USE_TLS
			if (add_interfaces(0, AF_INET, 0, PROTO_TLS, &tls_listen)!=0)
				goto error;
#endif
#endif
		}else{
			/* if error fall back to get hostname */
			/* get our address, only the first one */
			if (uname (&myname) <0){
				LOG(L_ERR, "ERROR: fix_all_socket_lists: cannot determine"
						" hostname, try -l address\n");
				goto error;
			}
			if (add_listen_iface(myname.nodename, 0, 0, 0)!=0){
				LOG(L_ERR, "ERROR: fix_all_socket_lists: add_listen_iface "
						"failed \n");
				goto error;
			}
		}
	}
	if (fix_socket_list(&udp_listen)!=0){
		LOG(L_ERR, "ERROR: fix_all_socket_lists: fix_socket_list"
				" udp failed\n");
		goto error;
	}
#ifdef USE_TCP
	if (fix_socket_list(&tcp_listen)!=0){
		LOG(L_ERR, "ERROR: fix_all_socket_lists: fix_socket_list"
				" tcp failed\n");
		goto error;
	}
#ifdef USE_TLS
	if (fix_socket_list(&tls_listen)!=0){
		LOG(L_ERR, "ERROR: fix_all_socket_lists: fix_socket_list"
				" tls failed\n");
		goto error;
	}
#endif
#endif
	if ((udp_listen==0)
#ifdef USE_TCP
			&& (tcp_listen==0)
#ifdef USE_TLS
			&& (tls_listen==0)
#endif
#endif
		){
		LOG(L_ERR, "ERROR: fix_all_socket_lists: no listening sockets\n");
		goto error;
	}
	return 0;
error:
	return -1;
}



void print_all_socket_lists()
{
	struct socket_info *si;
	struct socket_info** list;
	unsigned short proto;
	
	
	proto=PROTO_UDP;
	do{
		list=get_sock_info_list(proto);
		for(si=list?*list:0; si; si=si->next){
			printf("             %s: %s [%s]:%s%s\n", get_proto_name(proto),
			       si->name.s, si->address_str.s, si->port_no_str.s, 
			       si->flags & SI_IS_MCAST ? " mcast" : "");
		}
	}while((proto=next_proto(proto)));
}


void print_aliases()
{
	struct host_alias* a;

	for(a=aliases; a; a=a->next) 
		if (a->port)
			printf("             %s: %.*s:%d\n", get_proto_name(a->proto), 
					a->alias.len, a->alias.s, a->port);
		else
			printf("             %s: %.*s:*\n", get_proto_name(a->proto), 
					a->alias.len, a->alias.s);
}
