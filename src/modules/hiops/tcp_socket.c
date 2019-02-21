/*
 * Copyright (C) 2019 Mojtaba Esfandiari.S, Nasim-Telecom
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
#include "tcp_socket.h"

char buffer[BUFSIZE];


int socket_open(char *poroto, char *ipaddress, unsigned int port, unsigned int *sock) {

    unsigned int server_sock=-1;
    struct addrinfo *ainfo=0,*res=0,hints;
    char buf[256],host[256],serv[256];
    int error=0;
    unsigned int option, len;

    memset (&hints, 0, sizeof(hints));
    //hints.ai_protocol = IPPROTO_SCTP;         //deleted
    //hints.ai_protocol = IPPROTO_TCP;          //deleted
    hints.ai_flags = AI_PASSIVE|AI_ADDRCONFIG;
    hints.ai_socktype = SOCK_STREAM;

//    str str1 = str_init(portno);      //deleted
//    str2int(&str1, &port);            //deleted

    sprintf(buf,"%d",port);
    len = strlen(ipaddress);

    if (len){
        error = getaddrinfo(ipaddress, buf, &hints, &res);
        if (error!=0){
            LM_WARN("create_socket(): Error opening %.*s port %d while doing gethostbyname >%s\n",
                    len,ipaddress,port,gai_strerror(error));
            goto error;
        }
    }else{
        error = getaddrinfo(NULL, buf, &hints, &res);
        if (error!=0){
            LM_WARN("create_socket(): Error opening ANY port %d while doing gethostbyname >%s\n",
                    port,gai_strerror(error));
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
    if (res)
        freeaddrinfo(res);
    return 1;

    error:
        if (res) freeaddrinfo(res);
        if (server_sock!=-1) close(server_sock);
    return 0;

}

int socket_close(unsigned int *sock){
    close((int)*sock);
    return 1;
}

int socket_listen(int sock) {
    unsigned int length;
    int new_sock;
    sockaddr_in_t remote;

    length = sizeof(sockaddr_in_t);
    new_sock = accept(sock, (struct sockaddr *) &remote, &length);
    return new_sock;
}

int socket_send(int sock, char *buf) {
    int n;
//    n = send(sock, buf, strlen(buf), 0);
    n = write(sock, buf, strlen(buf));
    return n;
}

int socket_recv(int sock) {
    int n;
    bzero(buffer, BUFSIZE);
//    n = recv(sock, buffer, BUFSIZE, 0);
    n = read(sock, buffer, sizeof(buffer));
    return n;
}


