/*
 * Copyright (C) 2006 iptelorg GmbH
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

#include "init_socks.h"
#include "../../dprint.h"
#include "../../ip_addr.h"
#include "../../resolve.h"

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h> /*IPTOS_LOWDELAY*/
#include <netinet/tcp.h>
#include <sys/un.h>
#include <unistd.h> /* unlink */
#include <sys/stat.h> /* chmod */
#include <fcntl.h>

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif


static int tcp_proto_no=-1; /* tcp protocol number as returned by
							   getprotobyname */


/* returns -1 on error */
static int set_non_blocking(int s)
{
	int flags;
	/* non-blocking */
	flags=fcntl(s, F_GETFL);
	if (flags==-1){
		LOG(L_ERR, "ERROR: set_non_blocking: fnctl failed: (%d) %s\n",
				errno, strerror(errno));
		goto error;
	}
	if (fcntl(s, F_SETFL, flags|O_NONBLOCK)==-1){
		LOG(L_ERR, "ERROR: set_non_blocking: fcntl: set non-blocking failed:"
				" (%d) %s\n", errno, strerror(errno));
		goto error;
	}
	return 0;
error:
	return -1;
}




/* opens, binds and listens-on a control unix socket of type 'type' 
 * it will change the permissions to perm, if perm!=0
 * and the ownership to uid.gid if !=-1
 * returns socket fd or -1 on error */
int init_unix_sock(struct sockaddr_un* su, char* name, int type, int perm,
					int uid, int gid)
{
	struct sockaddr_un ifsun;
	int s;
	int len;
	int optval;
	
	s=-1;
	unlink(name);
	memset(&ifsun, 0, sizeof (struct sockaddr_un));
	len=strlen(name);
	if (len>UNIX_PATH_MAX){
		LOG(L_ERR, "ERROR: init_unix_sock: name too long (%d > %d): %s\n",
				len, UNIX_PATH_MAX, name);
		goto error;
	}
	ifsun.sun_family=AF_UNIX;
	memcpy(ifsun.sun_path, name, len);
#ifdef HAVE_SOCKADDR_SA_LEN
	ifsun.sun_len=len;
#endif
	s=socket(PF_UNIX, type, 0);
	if (s==-1){
		LOG(L_ERR, "ERROR: init_unix_sock: cannot create unix socket %s:"
				" %s [%d]\n", name, strerror(errno), errno);
		goto error;
	}
	optval=1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))==-1){
		LOG(L_ERR, "ERROR: init_unix_sock: setsockopt: %s [%d]\n", 
				strerror(errno), errno);
		/* continue */
	}
	if (set_non_blocking(s)==-1){
		LOG(L_ERR, "ERROR: init_unix_sock: set non blocking failed\n");
	}
	if (bind(s, (struct sockaddr *)&ifsun, sizeof(ifsun))==-1){
		LOG(L_ERR, "ERROR: init_unix_sock: bind: %s [%d]\n",
				strerror(errno), errno);
		goto error;
	}
	/* then the permissions */
	if (perm){ /* mode==0 doesn't make sense, nobody can read/write */
		if (chmod(name, perm)<0){
			LOG(L_ERR, "ERROR: init_unix_sock: failed to change the"
					" permissions for %s to %04o: %s[%d]\n",
					name, perm, strerror(errno), errno);
			goto error;
		}
	}
	/* try to change ownership */
	if ((uid!=-1) || (gid!=-1)){
		if (chown(name, uid, gid)<0){
			LOG(L_ERR, "ERROR: init_unix_sock: failed to change the"
					" owner/group for %s to %d.%d: %s[%d]\n",
					name, uid, gid, strerror(errno), errno);
			goto error;
		}
	}
	if ((type==SOCK_STREAM) && (listen(s, 128)==-1)){
		LOG(L_ERR, "ERROR: init_unix_sock: listen: %s [%d]\n",
				strerror(errno), errno);
		goto error;
	}
	*su=ifsun;
	return s;
error:
	if (s!=-1) close(s);
	return -1;
}



/* opens, binds and listens-on a control tcp socket
 * returns socket fd or -1 on error */
int init_tcpudp_sock(union sockaddr_union* sa_un, char* address, int port,
						enum socket_protos type)
{
	union sockaddr_union su;
	struct hostent* he;
	int s;
	int optval;
	
	s=-1;
	if ((type!=UDP_SOCK) && (type!=TCP_SOCK)){
		LOG(L_CRIT, "BUG: init_tcpudp_sock called with bad type: %d\n",
				type);
		goto error;
	}
	memset(&su, 0, sizeof (su));
	/* if no address specified, or address=='*', listen on all
	 * ipv4 addresses */
	if ((address==0)||((*address)==0)||((*address=='*') && (*(address+1)==0))){
		su.sin.sin_family=AF_INET;
		su.sin.sin_port=htons(port);
		su.sin.sin_addr.s_addr=INADDR_ANY;
#ifdef HAVE_SOCKADDR_SA_LEN
		su.sin.sin_len=sizeof(struct sockaddr_in);
#endif
	}else{
		he=resolvehost(address);
		if (he==0){
			LOG(L_ERR, "ERROR: init_tcpudp_sock: bad address %s\n", address);
			goto error;
		}
		if (hostent2su(&su, he, 0, port)==-1) goto error;
	}
	s=socket(AF2PF(su.s.sa_family), (type==TCP_SOCK)?SOCK_STREAM:SOCK_DGRAM,0);
	if (s==-1){
		LOG(L_ERR, "ERROR: init_tcpudp_sock: cannot create tcp socket:"
				" %s [%d]\n", strerror(errno), errno);
		goto error;
	}
	/* REUSEADDR */
	optval=1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))==-1){
		LOG(L_ERR, "ERROR: init_tcpudp_sock: setsockopt: %s [%d]\n", 
				strerror(errno), errno);
		/* continue */
	}
	/* tos */
	optval=IPTOS_LOWDELAY;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (void*)&optval,sizeof(optval)) ==-1){
		LOG(L_WARN, "WARNING: init_tcpudp_sock: setsockopt tos: %s\n",
				strerror(errno));
		/* continue since this is not critical */
	}
	if (set_non_blocking(s)==-1){
		LOG(L_ERR, "ERROR: init_tcpudp_sock: set non blocking failed\n");
	}
	
	if (bind(s, &su.s, sockaddru_len(su))==-1){
		LOG(L_ERR, "ERROR: init_tcpudp_sock: bind: %s [%d]\n",
				strerror(errno), errno);
		goto error;
	}
	if ((type==TCP_SOCK) && (listen(s, 128)==-1)){
		LOG(L_ERR, "ERROR: init_tcpudp_sock: listen: %s [%d]\n",
				strerror(errno), errno);
		goto error;
	}
	*sa_un=su;
	return s;
error:
	if (s!=-1) close(s);
	return -1;
}



/* set all socket/fd options:  disable nagle, tos lowdelay, non-blocking
 * return -1 on error */
int init_sock_opt(int s, enum socket_protos type)
{
	int optval;
#ifdef DISABLE_NAGLE
	int flags;
	struct protoent* pe;
#endif
	
	if ((type==UDP_SOCK)||(type==TCP_SOCK)){
#ifdef DISABLE_NAGLE
		if (type==TCP_SOCK){
			flags=1;
			if (tcp_proto_no==-1){ /* if not already set */
				pe=getprotobyname("tcp");
				if (pe!=0){
					tcp_proto_no=pe->p_proto;
				}
			}
			if ( (tcp_proto_no!=-1) && (setsockopt(s, tcp_proto_no,
							TCP_NODELAY, &flags, sizeof(flags))<0) ){
				LOG(L_WARN, "WARNING: init_sock_opt: could not disable"
							" Nagle: %s\n", strerror(errno));
			}
		}
#endif
		/* tos*/
		optval = IPTOS_LOWDELAY;
		if (setsockopt(s, IPPROTO_IP, IP_TOS, (void*)&optval,
						sizeof(optval)) ==-1){
			LOG(L_WARN, "WARNING: init_sock_opt: setsockopt tos: %s\n",
					strerror(errno));
			/* continue since this is not critical */
		}
	}
	if (set_non_blocking(s)==-1){
		LOG(L_ERR, "ERROR: init_sock_opt: set non blocking failed\n");
	}
	return 0;
}
