/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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
 * 
 */


#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>

#include "utils.h"
#include "globals.h"
#include "tcp_accept.h"
#include "receiver.h"

#include "../../cfg/cfg_struct.h"

/* defined in ../diameter_peer.c */
int dp_add_pid(pid_t pid);


unsigned int *listening_socks=0;	/**< array of sockets listening for connections */


extern int h_errno;

/**
 * Creates a socket and binds it.
 * @param listen_port - port to listen to
 * @param bind_to - IP address to bind to - if empty, will bind to :: (0.0.0.0) (all)
 * @param sock - socket to be update with the identifier of the opened one
 * @returns 1 on success, 0 on error
 */
int create_socket(int listen_port,str bind_to,unsigned int *sock)
{
	unsigned int server_sock=-1;
	struct addrinfo *ainfo=0,*res=0,hints;
	char buf[256],host[256],serv[256];
	int error=0;
	unsigned int option;
	
	memset (&hints, 0, sizeof(hints));
	//hints.ai_protocol = IPPROTO_SCTP;
 	//hints.ai_protocol = IPPROTO_TCP;
 	hints.ai_flags = AI_PASSIVE|AI_ADDRCONFIG;
	hints.ai_socktype = SOCK_STREAM;

	sprintf(buf,"%d",listen_port);
	
	if (bind_to.len){
		error = getaddrinfo(bind_to.s, buf, &hints, &res);
		if (error!=0){
			LM_WARN("create_socket(): Error opening %.*s port %d while doing gethostbyname >%s\n",
				bind_to.len,bind_to.s,listen_port,gai_strerror(error));
			goto error;
		}
	}else{
		error = getaddrinfo(NULL, buf, &hints, &res);
		if (error!=0){
			LM_WARN("create_socket(): Error opening ANY port %d while doing gethostbyname >%s\n",
				listen_port,gai_strerror(error));
			goto error;
		}
	}
		
	LM_DBG("create_sockets: create socket and bind for IPv4...\n");

	for(ainfo = res;ainfo;ainfo = ainfo->ai_next)
	{
		if (getnameinfo(ainfo->ai_addr,ainfo->ai_addrlen,
			host,256,serv,256,NI_NUMERICHOST|NI_NUMERICSERV)==0){
				LM_WARN("create_socket(): Trying to open/bind/listen on %s port %s\n",
					host,serv);
		}				

		if ((server_sock = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol)) == -1) {
			LM_ERR("create_socket(): error creating server socket on %s port %s >"
				" %s\n",host,serv,strerror(errno));
			goto error;
		}
		option = 1;
		setsockopt(server_sock,SOL_SOCKET,SO_REUSEADDR,&option,sizeof(option));
		
		if (bind( 	server_sock,ainfo->ai_addr,ainfo->ai_addrlen)==-1 ) {
			LM_ERR("create_socket(): error binding on %s port %s >"
				" %s\n",host,serv,strerror(errno));
			goto error;
		}
	
		if (listen( server_sock, 5) == -1) {
			LM_ERR("create_socket(): error listening on %s port %s > %s\n",host,serv,strerror(errno) );
			goto error;
		}
	
		*sock = server_sock;	
		
		LM_WARN("create_socket(): Successful socket open/bind/listen on %s port %s\n",
					host,serv);
	}
	if (res) freeaddrinfo(res);	
	return 1;
error:
	if (res) freeaddrinfo(res);
	if (server_sock!=-1) close(server_sock);
	return 0;
	
}

/**
 * Accepts an incoming connection by forking a receiver process.
 * @param server_sock - socket that this connection came in to
 * @param new_sock - socket to be update with the value of the accepter socket
 * @returns 1 on success, 0 on error
 */
inline static int accept_connection(int server_sock,int *new_sock)
{
	unsigned int length;
	struct sockaddr_in remote;
		
	/* do accept */
	length = sizeof( struct sockaddr_in);
	*new_sock = accept( server_sock, (struct sockaddr*)&remote, &length);

	if (*new_sock==-1) {
		LM_ERR("accept_connection(): accept failed!\n");
		goto error;
	} else {
		LM_INFO("accept_connection(): new tcp connection accepted!\n");
		
	}
	
	receiver_send_socket(*new_sock,0);
	
	return 1;
error:
	return 0;
}

/**
 * Accept loop that listens for incoming connections on all listening sockets.
 * When a connection is received, accept_connection() is called.
 * @returns only on shutdown
 */
void accept_loop()
{
	fd_set listen_set;
	struct timeval timeout;
	int i=0,max_sock=0,nready;
	int new_sock;
	

	while(listening_socks[i]){
		if (listening_socks[i]>max_sock) max_sock=listening_socks[i];
		i++;
	}

	while(1){
		if (shutdownx && *shutdownx) break;
		
		cfg_update();
		
		timeout.tv_sec=2;
		timeout.tv_usec=0;	
		FD_ZERO(&listen_set);
		i=0;
		while(listening_socks[i]){
			FD_SET(listening_socks[i],&listen_set);					
			i++;
		}

		nready = select( max_sock+1, &listen_set, 0, 0, &timeout);
		if (nready == 0){
			LM_DBG("accept_loop(): No connection attempts\n");
			continue;
		}
		if (nready == -1) {
			if (errno == EINTR) {
				continue;
			} else {
				LM_ERR("accept_loop(): select fails: %s\n",
					strerror(errno));
				sleep(2);
				continue;
			}
		}

		i=0;
		while(listening_socks[i]){
			if (FD_ISSET(listening_socks[i],&listen_set)){
				accept_connection(listening_socks[i],&new_sock);
			}
			i++;
		}
	}
}

