/*
 * $Id: init_socks.c,v 1.1 2006/02/23 19:57:31 andrei Exp $
 *
 * Copyright (C) 2006 iptelorg GmbH
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
/* History:
 * --------
 *  2006-02-14  created by andrei
 *  2007        ported to libbinrpc (bpintea)
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

#define LISTEN_BACKLOG	128


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



static inline int init_srv_sock(brpc_addr_t *addr)
{
	int sockfd;

	if ((sockfd = brpc_socket(addr, /*block.*/false, /*named*/true)) < 0) {
		ERR("failed to bind listen socket: %s [%d].\n", brpc_strerror(),
				brpc_errno);
		goto err;
	}
	/* listen on stream sockets */
	if ((BRPC_ADDR_TYPE(addr)==SOCK_STREAM) && 
			(listen(sockfd, LISTEN_BACKLOG)==-1)){
		ERR("listen: %s [%d]\n", strerror(errno), errno);
		goto err;
	}
	return sockfd;
err:
	if (0 <= sockfd)
		close(sockfd);
	return -1;
}

/* opens, binds and listens-on a control unix socket of type 'type' 
 * it will change the permissions to perm, if perm!=0
 * and the ownership to uid.gid if !=-1
 * returns socket fd or -1 on error */
int init_unix_sock(brpc_addr_t* addr, int perm, int uid, int gid)
{
	int sockfd;
	char *name;

	if ((sockfd = init_srv_sock(addr)) < 0)
		return sockfd;

	name = BRPC_ADDR_UN(addr)->sun_path;
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
	return sockfd;
error:
	close(sockfd);
	return -1;
}


/* opens, binds and listens-on a control tcp socket
 * returns socket fd or -1 on error */
int init_tcpudp_sock(brpc_addr_t *addr)
{
	return init_srv_sock(addr);
}

/* set all socket/fd options:  disable nagle, tos lowdelay, non-blocking
 * return -1 on error */
//int init_sock_opt(int s, enum socket_protos type)
int init_sock_opt(int s, sa_family_t domain, int socktype)
{
	int optval;
#ifdef DISABLE_NAGLE
	int flags;
#if 0
	struct protoent* pe;
#endif
#endif

	if (domain != PF_LOCAL) {
#ifdef DISABLE_NAGLE
		if (socktype == SOCK_STREAM) {
			flags=1;
#if 0
			/* TODO: "getprotobyname() [...] is expensive, slow, and 
			 * generally useless" - check out if "useless". */
			if (tcp_proto_no==-1){ /* if not already set */
				pe=getprotobyname("tcp");
				if (pe!=0){
					tcp_proto_no=pe->p_proto;
				}
			}
			if ( (tcp_proto_no!=-1) && (setsockopt(s, tcp_proto_no,
#endif
			if ((setsockopt(s, IPPROTO_TCP, TCP_NODELAY, 
					&flags, sizeof(flags))) < 0) {
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
