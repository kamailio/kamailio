/*
 * $Id$
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


#ifdef USE_TCP


#ifndef SHM_MEM
#error "shared memory support needed (add -DSHM_MEM to Makefile.defs)"
#endif

#include <sys/select.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>

#include <errno.h>
#include <string.h>



#include "ip_addr.h"
#include "pass_fd.h"
#include "tcp_conn.h"
#include "globals.h"
#include "pt.h"
#include "mem/mem.h"
#include "mem/shm_mem.h"
#include "timer.h"
#include "tcp_server.h"



#define local_malloc pkg_malloc
#define local_free   pkg_free

#define MAX_TCP_CHILDREN 100

struct tcp_child{
	pid_t pid;
	int unix_sock; /* unix sock fd, copied from pt*/
	int busy;
	int n_reqs; /* number of requests serviced so far */
};



struct tcp_connection** conn_list=0;
struct tcp_child tcp_children[MAX_TCP_CHILDREN];
static int connection_id=1; /*  unique for each connection, used for 
								quickly finding the corresponding connection
								for a reply */
int unix_tcp_sock;



struct tcp_connection* tcpconn_new(int sock, union sockaddr_union* su, int i)
{
	struct tcp_connection *c;
	

	c=(struct tcp_connection*)shm_malloc(sizeof(struct tcp_connection));
	if (c==0){
		LOG(L_ERR, "ERROR: tcpconn_add: mem. allocation failure\n");
		goto error;
	}
	c->s=sock;
	c->fd=sock;
	c->su=*su;
	c->sock_idx=i;
	c->refcnt=0;
	su2ip_addr(&c->ip, su);
	c->port=su_getport(su);
	init_tcp_req(&c->req);
	c->timeout=get_ticks()+TCP_CON_TIMEOUT;
	c->id=connection_id++;
	return c;
	
error:
	return 0;
}



struct tcp_connection* tcpconn_connect(union sockaddr_union* server)
{
	int s;

	s=socket(AF2PF(server->s.sa_family), SOCK_STREAM, 0);
	if (s<0){
		LOG(L_ERR, "ERROR: tcpconn_connect: socket: (%d) %s\n",
				errno, strerror(errno));
		goto error;
	}
	if (connect(s, &server->s, sockaddru_len(*server))<0){
		LOG(L_ERR, "ERROR: tcpconn_connect: connect: (%d) %s\n",
				errno, strerror(errno));
		goto error;
	}
	return tcpconn_new(s, server, 0); /*FIXME: set sock idx! */
error:
	return 0;
}



struct tcp_connection*  tcpconn_add(struct tcp_connection *c)
{
	TCPCONN_LOCK;
	/* add it at the begining of the list*/
	if (c) tcpconn_listadd(*conn_list, c, next, prev);
	TCPCONN_UNLOCK;
	return c;
}



void tcpconn_rm(struct tcp_connection* c)
{
	TCPCONN_LOCK;
	tcpconn_listrm(*conn_list, c, next, prev);
	TCPCONN_UNLOCK;
	shm_free(c);
}


/* finds a connection, if id=0 uses the ip addr & port */
struct tcp_connection* tcpconn_find(int id, struct ip_addr* ip, int port)
{

	struct tcp_connection *c;
	
	DBG("tcpconn_find: %d ",id ); print_ip(ip); DBG(" %d\n", port);
	for (c=*conn_list; c; c=c->next){
		DBG("c=%p, c->id=%d, ip=",c, c->id);
		print_ip(&c->ip);
		DBG(" port=%d\n", c->port);
		if (id){
			if (id==c->id) return c;
		}else if ((port==c->port)&&(ip_addr_cmp(ip, &c->ip))) return c;
	}
	return 0;
}



struct tcp_connection* tcpconn_get(int id, struct ip_addr* ip, int port)
{
	struct tcp_connection* c;
	TCPCONN_LOCK;
	c=tcpconn_find(id, ip, port);
	if (c) c->refcnt++;
	TCPCONN_UNLOCK;
	return c;
}



void tcpconn_put(struct tcp_connection* c)
{
	c->refcnt--; /* FIXME: atomic_dec */
}



/* finds a tcpconn & sends on it */
int tcp_send(char* buf, unsigned len, union sockaddr_union* to, int id)
{
	struct tcp_connection *c;
	struct ip_addr ip;
	int port;
	long response[2];
	int n;
	
	su2ip_addr(&ip, to);
	port=su_getport(to);
	
	c=tcpconn_get(id, &ip, port); /* lock ;inc refcnt; unlock */
	if (id){
		if (c==0) {
		LOG(L_ERR, "ERROR: tcp_send: id %d not found, dropping\n",
					id);
			return -1;
		}
	}else{
		if (c==0){
			DBG("tcp_send: no open tcp connection found, opening new one\n");
			/* create tcp connection */
			if ((c=tcpconn_connect(to))==0){
				LOG(L_ERR, "ERROR: tcp_send: connect failed\n");
				return 0;
			}
			c->refcnt++;
			
			/* send the new tcpconn to "tcp main" */
			response[0]=(long)c;
			response[1]=CONN_NEW;
			n=write(unix_tcp_sock, response, sizeof(response));
			n=send_fd(unix_tcp_sock, &c, sizeof(c), c->s);
		}else{
			DBG("tcp_send: tcp connection found, acquiring fd\n");
			/* get the fd */
			response[0]=(long)c;
			response[1]=CONN_GET_FD;
			n=write(unix_tcp_sock, response, sizeof(response));
			n=receive_fd(unix_tcp_sock, &c, sizeof(c), &c->fd);
		}
	
	}
	DBG("tcp_send: sending...\n");
	n=write(c->fd, buf, len);
	close(c->fd);
	tcpconn_put(c); /* release c (lock; dec refcnt; unlock) */
	return n;
}



/* very ineficient for now, use hashtable some day - FIXME*/
void tcpconn_timeout(fd_set* set)
{
	struct tcp_connection *c, *next;
	int ticks;;
	
	
	ticks=get_ticks();
	c=*conn_list;
	while(c){
		next=c->next;
		if ((c->refcnt==0) && (ticks>c->timeout)) {
			DBG("tcpconn_timeout: timeout for %p (%d > %d)\n",
					c, ticks, c->timeout);
			if (c->s>0) {
				FD_CLR(c->s, set);
				close(c->s);
			}
			tcpconn_rm(c);
		}
		c=next;
	}
}



int tcp_init_sock(struct socket_info* sock_info)
{
	union sockaddr_union* addr;
	
	addr=&sock_info->su;
	sock_info->proto=SOCKET_TCP;
	if (init_su(addr, &sock_info->address, htons(sock_info->port_no))<0){
		LOG(L_ERR, "ERROR: tcp_init: could no init sockaddr_union\n");
		goto error;
	}
	sock_info->socket=socket(AF2PF(addr->s.sa_family), SOCK_STREAM, 0);
	if (sock_info->socket==-1){
		LOG(L_ERR, "ERROR: tcp_init: socket: %s\n", strerror(errno));
		goto error;
	}
	if (bind(sock_info->socket, &addr->s, sockaddru_len(*addr))==-1){
		LOG(L_ERR, "ERROR: tcp_init: bind(%x, %p, %d) on %s: %s\n",
				sock_info->socket, &addr->s, 
				sockaddru_len(*addr),
				sock_info->address_str.s,
				strerror(errno));
		goto error;
	}
	if (listen(sock_info->socket, 10)==-1){
		LOG(L_ERR, "ERROR: tcp_init: listen(%x, %p, %d) on %s: %s\n",
				sock_info->socket, &addr->s, 
				sockaddru_len(*addr),
				sock_info->address_str.s,
				strerror(errno));
		goto error;
	}
	
	return 0;
error:
	if (sock_info->socket!=-1){
		close(sock_info->socket);
		sock_info->socket=-1;
	}
	return -1;
}



static int send2child(struct tcp_connection* tcpconn)
{
	int i;
	int min_busy;
	int idx;
	
	min_busy=tcp_children[0].busy;
	idx=0;
	for (i=0; i<tcp_children_no; i++){
		if (!tcp_children[i].busy){
			idx=i;
			min_busy=0;
			break;
			return 0;
		}else if (min_busy>tcp_children[i].busy){
			min_busy=tcp_children[i].busy;
			idx=i;
		}
	}
	
	tcp_children[idx].busy++;
	tcp_children[idx].n_reqs++;
	tcpconn->refcnt++;
	if (min_busy){
		LOG(L_WARN, "WARNING: send2child:no free tcp receiver, "
				" connection passed to the least busy one (%d)\n",
				min_busy);
	}
	DBG("send2child: to child %d, %ld\n", idx, (long)tcpconn);
	send_fd(tcp_children[idx].unix_sock, &tcpconn, sizeof(tcpconn),
			tcpconn->s);
	
	return 0; /* just to fix a warning*/
}


void tcp_main_loop()
{
	int r;
	int n;
	fd_set master_set;
	fd_set sel_set;
	int maxfd;
	int new_sock;
	union sockaddr_union su;
	struct tcp_connection* tcpconn;
	long response[2];
	int cmd;
	int bytes;
	socklen_t su_len;
	struct timeval timeout;

	/*init */
	maxfd=0;
	FD_ZERO(&master_set);
	/* set all the listen addresses */
	for (r=0; r<sock_no; r++){
		if ((tcp_info[r].proto==SOCKET_TCP) &&(tcp_info[r].socket!=-1)){
			FD_SET(tcp_info[r].socket, &master_set);
			if (tcp_info[r].socket>maxfd) maxfd=tcp_info[r].socket;
		}
	}
	/* set all the unix sockets used for child comm */
	for (r=0; r<process_no; r++){
		if (pt[r].unix_sock>=0){
			FD_SET(pt[r].unix_sock, &master_set);
			if (pt[r].unix_sock>maxfd) maxfd=pt[r].unix_sock;
		}
	}
	
	
	/* main loop*/
	
	while(1){
		sel_set=master_set;
		timeout.tv_sec=TCP_MAIN_SELECT_TIMEOUT;
		timeout.tv_usec=0;
		n=select(maxfd+1, &sel_set, 0 ,0 , &timeout);
		if (n<0){
			if (errno==EINTR) continue; /* just a signal */
			/* errors */
			LOG(L_ERR, "ERROR: tcp_main_loop: select:(%d) %s\n", errno,
					strerror(errno));
			n=0;
		}
		
		for (r=0; r<sock_no && n; r++){
			if ((tcp_info[r].proto==SOCKET_TCP) &&
					(FD_ISSET(tcp_info[r].socket, &sel_set))){
				/* got a connection on r */
				su_len=sizeof(su);
				new_sock=accept(tcp_info[r].socket, &(su.s), &su_len);
				n--;
				if (new_sock<0){
					LOG(L_ERR,  "WARNING: tcp_main_loop: error while accepting"
							" connection(%d): %s\n", errno, strerror(errno));
					continue;
				}
				
				/* add socket to list */
				tcpconn=tcpconn_new(new_sock, &su, r);
				if (tcpconn){
					tcpconn_add(tcpconn);
					DBG("tcp_main_loop: new connection: %p %d\n",
						tcpconn, tcpconn->s);
					/* pass it to a child */
					if(send2child(tcpconn)<0){
						LOG(L_ERR,"ERROR: tcp_main_loop: no children "
								"available\n");
						close(tcpconn->s);
						tcpconn_rm(tcpconn);
					}
				}
			}
		}
		
		/* check all the read fds (from the tcpconn list) */
		
		for(tcpconn=*conn_list; tcpconn && n; tcpconn=tcpconn->next){
			if ((tcpconn->refcnt==0)&&(FD_ISSET(tcpconn->s, &sel_set))){
				/* new data available */
				n--;
				/* pass it to child, so remove it from select list */
				DBG("tcp_main_loop: data available on %p %d\n",
						tcpconn, tcpconn->s);
				FD_CLR(tcpconn->s, &master_set);
				if (send2child(tcpconn)<0){
					LOG(L_ERR,"ERROR: tcp_main_loop: no children available\n");
					close(tcpconn->s);
					tcpconn_rm(tcpconn);
				}
			}
		}
		
		/* check unix sockets & listen | destroy connections */
		/* start from 1, the "main" process does not transmit anything*/
		for (r=1; r<process_no && n; r++){
			if ( (pt[r].unix_sock>=0) && FD_ISSET(pt[r].unix_sock, &sel_set)){
				n--;
				/* errno==EINTR !!! TODO*/
read_again:
				bytes=read(pt[r].unix_sock, response, sizeof(response));
				if (bytes==0){
					/* EOF -> bad, child has died */
					LOG(L_CRIT, "BUG: tcp_main_loop: dead child %d\n", r);
					/* don't listen on it any more */
					FD_CLR(pt[r].unix_sock, &master_set);
					/*exit(-1)*/;
				}else if (bytes<0){
					if (errno==EINTR) goto read_again;
					else{
						LOG(L_CRIT, "ERROR: tcp_main_loop: read from child: "
								" %s\n", strerror(errno));
						/* try to continue ? */
					}
				}
					
				DBG("tcp_main_loop: read response= %lx, %ld from %d (%d)\n",
						response[0], response[1], r, pt[r].pid);
				cmd=response[1];
				switch(cmd){
					case CONN_RELEASE:
						if (pt[r].idx>=0){
							tcp_children[pt[r].idx].busy--;
						}else{
							LOG(L_CRIT, "BUG: tcp_main_loop: CONN_RELEASE\n");
						}
						tcpconn=(struct tcp_connection*)response[0];
						if (tcpconn){
							tcpconn->refcnt--;
							DBG("tcp_main_loop: %p refcnt= %d\n", 
									tcpconn, tcpconn->refcnt);
								FD_SET(tcpconn->s, &master_set);
								if (maxfd<tcpconn->s) maxfd=tcpconn->s;
								/* update the timeout*/
								tcpconn->timeout=get_ticks()+TCP_CON_TIMEOUT;
						}
						break;
					case CONN_ERROR:
					case CONN_DESTROY:
					case CONN_EOF:
						if (pt[r].idx>=0){
							tcp_children[pt[r].idx].busy--;
						}else{
							LOG(L_CRIT, "BUG: tcp_main_loop: CONN_RELEASE\n");
						}
						tcpconn=(struct tcp_connection*)response[0];
						if (tcpconn){
							tcpconn->refcnt--;
							if (tcpconn->refcnt==0){
								DBG("tcp_main_loop: destroying connection\n");
								close(tcpconn->s);
								tcpconn_rm(tcpconn);
							}else{
								DBG("tcp_main_loop: delaying ...\n");
							}
						}
						break;
					case CONN_GET_FD:
						/* send the requested FD  */
						tcpconn=(struct tcp_connection*)response[0];
						/* WARNING: take care of setting refcnt properly to
						 * avoid race condition */
						if (tcpconn){
							send_fd(pt[r].unix_sock, &tcpconn,
									sizeof(tcpconn), tcpconn->s);
						}else{
							LOG(L_CRIT, "BUG: tcp_main_loop: null pointer\n");
						}
						break;
					case CONN_NEW:
						/* update the fd in the requested tcpconn*/
						tcpconn=(struct tcp_connection*)response[0];
						/* WARNING: take care of setting refcnt properly to
						 * avoid race condition */
						if (tcpconn){
							receive_fd(pt[r].unix_sock, &tcpconn,
										sizeof(tcpconn), &tcpconn->s);
							/* add tcpconn to the list*/
							tcpconn_add(tcpconn);
							FD_SET(tcpconn->s, &master_set);
							if (maxfd<tcpconn->s) maxfd=tcpconn->s;
							/* update the timeout*/
							tcpconn->timeout=get_ticks()+TCP_CON_TIMEOUT;
						}else{
							LOG(L_CRIT, "BUG: tcp_main_loop: null pointer\n");
						}
						break;
					default:
							LOG(L_CRIT, "BUG: tcp_main_loop: unknown cmd %d\n",
									cmd);
				}
			}
		}
		
		/* remove old connections */
		tcpconn_timeout(&master_set);
	
	}
}



int init_tcp()
{
	/* allocate list head*/
	conn_list=shm_malloc(sizeof(struct tcp_connection*));
	if (conn_list==0){
		LOG(L_CRIT, "ERROR: tcp_init: memory allocation failure\n");
		goto error;
	}
	*conn_list=0;
	return 0;
error:
		return -1;
}



/* starts the tcp processes */
int tcp_init_children()
{
	int r;
	int sockfd[2];
	pid_t pid;
	
	
	/* create the tcp sock_info structures */
	/* copy the sockets*/
	for (r=0; r<sock_no ; r++){
		tcp_info[r]=sock_info[r];
		tcp_init_sock(&tcp_info[r]);
	}
	
	/* fork children & create the socket pairs*/
	for(r=0; r<tcp_children_no; r++){
		if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sockfd)<0){
			LOG(L_ERR, "ERROR: tcp_main: socketpair failed: %s\n",
					strerror(errno));
			goto error;
		}
		
		process_no++;
		pid=fork();
		if (pid<0){
			LOG(L_ERR, "ERROR: tcp_main: fork failed: %s\n",
					strerror(errno));
			goto error;
		}else if (pid>0){
			/* parent */
			close(sockfd[1]);
			tcp_children[r].pid=pid;
			tcp_children[r].busy=0;
			tcp_children[r].n_reqs=0;
			tcp_children[r].unix_sock=sockfd[0];
			pt[process_no].pid=pid;
			pt[process_no].unix_sock=sockfd[0];
			pt[process_no].idx=r;
			strncpy(pt[process_no].desc, "tcp receiver", MAX_PT_DESC);
		}else{
			/* child */
			close(sockfd[0]);
			unix_tcp_sock=sockfd[1];
			tcp_receive_loop(sockfd[1]);
		}
	}
	return 0;
error:
	return -1;
}

#endif
