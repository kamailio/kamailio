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
/*
 * History:
 * --------
 *  2002-11-29  created by andrei
 *  2002-12-11  added tcp_send (andrei)
 *  2003-01-20  locking fixes, hashtables (andrei)
 *  2003-02-20  s/lock_t/gen_lock_t/ to avoid a conflict on solaris (andrei)
 *  2003-02-25  Nagle is disabled if -DDISABLE_NAGLE (andrei)
 *  2003-03-29  SO_REUSEADDR before calling bind to allow
 *              server restart, Nagle set on the (hopefuly) 
 *              correct socket (jiri)
 *  2003-03-31  always try to find the corresponding tcp listen socket for
 *               a temp. socket and store in in *->bind_address: added
 *               find_tcp_si, modified tcpconn_connect (andrei)
 *  2003-04-14  set sockopts to TOS low delay (andrei)
 *  2003-06-30  moved tcp new connect checking & handling to
 *               handle_new_connect (andrei)
 *  2003-07-09  tls_close called before closing the tcp connection (andrei)
 *  2003-10-24  converted to the new socket_info lists (andrei)
 *  2003-10-27  tcp port aliases support added (andrei)
 *  2003-11-04  always lock before manipulating refcnt; sendchild
 *              does not inc refcnt by itself anymore (andrei)
 *  2003-11-07  different unix sockets are used for fd passing
 *              to/from readers/writers (andrei)
 *  2003-11-17  handle_new_connect & tcp_connect will close the 
 *              new socket if tcpconn_new return 0 (e.g. out of mem) (andrei)
 */


#ifdef USE_TCP


#ifndef SHM_MEM
#error "shared memory support needed (add -DSHM_MEM to Makefile.defs)"
#endif


#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/uio.h>  /* writev*/
#include <netdb.h>

#include <unistd.h>

#include <errno.h>
#include <string.h>



#include "ip_addr.h"
#include "pass_fd.h"
#include "tcp_conn.h"
#include "globals.h"
#include "pt.h"
#include "locking.h"
#include "mem/mem.h"
#include "mem/shm_mem.h"
#include "timer.h"
#include "sr_module.h"
#include "tcp_server.h"
#include "tcp_init.h"
#ifdef USE_TLS
#include "tls/tls_server.h"
#endif




#define local_malloc pkg_malloc
#define local_free   pkg_free

#define MAX_TCP_CHILDREN 100

struct tcp_child{
	pid_t pid;
	int proc_no; /* ser proc_no, for debugging */
	int unix_sock; /* unix "read child" sock fd */
	int busy;
	int n_reqs; /* number of requests serviced so far */
};


int tcp_accept_aliases=0; /* by default don't accept aliases */

/* connection hash table (after ip&port) , includes also aliases */
struct tcp_conn_alias** tcpconn_aliases_hash=0;
/* connection hash table (after connection id) */
struct tcp_connection** tcpconn_id_hash=0;
gen_lock_t* tcpconn_lock=0;

struct tcp_child tcp_children[MAX_TCP_CHILDREN];
static int* connection_id=0; /*  unique for each connection, used for 
								quickly finding the corresponding connection
								for a reply */
int unix_tcp_sock;

int tcp_proto_no=-1; /* tcp protocol number as returned by getprotobyname */


struct tcp_connection* tcpconn_new(int sock, union sockaddr_union* su,
									struct socket_info* ba, int type, 
									int state)
{
	struct tcp_connection *c;
	
	c=(struct tcp_connection*)shm_malloc(sizeof(struct tcp_connection));
	if (c==0){
		LOG(L_ERR, "ERROR: tcpconn_new: mem. allocation failure\n");
		goto error;
	}
	memset(c, 0, sizeof(struct tcp_connection)); /* zero init */
	c->s=sock;
	c->fd=-1; /* not initialized */
	if (lock_init(&c->write_lock)==0){
		LOG(L_ERR, "ERROR: tcpconn_new: init lock failed\n");
		goto error;
	}
	
	c->rcv.src_su=*su;
	
	c->refcnt=0;
	su2ip_addr(&c->rcv.src_ip, su);
	c->rcv.src_port=su_getport(su);
	c->rcv.bind_address=ba;
	if (ba){
		c->rcv.dst_ip=ba->address;
		c->rcv.dst_port=ba->port_no;
	}
	print_ip("tcpconn_new: new tcp connection: ", &c->rcv.src_ip, "\n");
	DBG(     "tcpconn_new: on port %d, type %d\n", c->rcv.src_port, type);
	init_tcp_req(&c->req);
	c->id=(*connection_id)++;
	c->rcv.proto_reserved1=0; /* this will be filled before receive_message*/
	c->rcv.proto_reserved2=0;
	c->state=state;
	c->extra_data=0;
#ifdef USE_TLS
	if (type==PROTO_TLS){
		if (tls_tcpconn_init(c, sock)==-1) goto error;
	}else
#endif /* USE_TLS*/
	{
		c->type=PROTO_TCP;
		c->rcv.proto=PROTO_TCP;
		c->flags=0;
		c->timeout=get_ticks()+TCP_CON_TIMEOUT;
	}
			
		
	return c;
	
error:
	if (c) shm_free(c);
	return 0;
}




struct socket_info* find_tcp_si(union sockaddr_union* s)
{
	struct ip_addr ip;
	struct socket_info* si;
	
	su2ip_addr(&ip, s);
	for (si=tcp_listen; si; si=si->next)
		if (ip_addr_cmp(&ip, &si->address)){
			/* found it, we use first match */
			return si;
		}
	return 0; /* no match */
}


struct tcp_connection* tcpconn_connect(union sockaddr_union* server, int type)
{
	int s;
	struct socket_info* si;
	union sockaddr_union my_name;
	socklen_t my_name_len;
	int optval;
	struct tcp_connection* con;
#ifdef DISABLE_NAGLE
	int flag;
#endif

	s=socket(AF2PF(server->s.sa_family), SOCK_STREAM, 0);
	if (s==-1){
		LOG(L_ERR, "ERROR: tcpconn_connect: socket: (%d) %s\n",
				errno, strerror(errno));
		goto error;
	}
#ifdef DISABLE_NAGLE
	flag=1;
	if ( (tcp_proto_no!=-1) && (setsockopt(s, tcp_proto_no , TCP_NODELAY,
					&flag, sizeof(flag))<0) ){
		LOG(L_ERR, "ERROR: tcp_connect: could not disable Nagle: %s\n",
				strerror(errno));
	}
#endif
	/* tos*/
	optval=IPTOS_LOWDELAY;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (void*)&optval, sizeof(optval)) ==-1){
		LOG(L_WARN, "WARNING: tcpconn_connect: setsockopt tos: %s\n",
				strerror(errno));
		/* continue since this is not critical */
	}

	if (connect(s, &server->s, sockaddru_len(*server))<0){
		LOG(L_ERR, "ERROR: tcpconn_connect: connect: (%d) %s\n",
				errno, strerror(errno));
		goto error;
	}
	my_name_len=sizeof(my_name);
	if (getsockname(s, &my_name.s, &my_name_len)!=0){
		LOG(L_ERR, "ERROR: tcp_connect: getsockname failed: %s(%d)\n",
				strerror(errno), errno);
		si=0; /* try to go on */
	}
#ifdef USE_TLS
	if (type==PROTO_TLS)
		si=find_tls_si(&my_name);
	else
#endif
		si=find_tcp_si(&my_name);

	if (si==0){
		LOG(L_ERR, "ERROR: tcp_connect: could not find coresponding"
				" listening socket, using default...\n");
		if (server->s.sa_family==AF_INET) si=sendipv4_tcp;
#ifdef USE_IPV6
		else si=sendipv6_tcp;
#endif
	}
	con=tcpconn_new(s, server, si, type, S_CONN_CONNECT);
	if (con==0){
		LOG(L_ERR, "ERROR: tcp_connect: tcpconn_new failed, closing the "
				 " socket\n");
		goto error;
	}
	return con;
	/*FIXME: set sock idx! */
error:
	if (s!=-1) close(s); /* close the opened socket */
	return 0;
}



struct tcp_connection*  tcpconn_add(struct tcp_connection *c)
{
	unsigned hash;

	if (c){
		TCPCONN_LOCK;
		/* add it at the begining of the list*/
		hash=tcp_id_hash(c->id);
		c->id_hash=hash;
		tcpconn_listadd(tcpconn_id_hash[hash], c, id_next, id_prev);
		
		hash=tcp_addr_hash(&c->rcv.src_ip, c->rcv.src_port);
		/* set the first alias */
		c->con_aliases[0].port=c->rcv.src_port;
		c->con_aliases[0].hash=hash;
		c->con_aliases[0].parent=c;
		tcpconn_listadd(tcpconn_aliases_hash[hash], &c->con_aliases[0],
						next, prev);
		c->aliases++;
		TCPCONN_UNLOCK;
		DBG("tcpconn_add: hashes: %d, %d\n", hash, c->id_hash);
		return c;
	}else{
		LOG(L_CRIT, "tcpconn_add: BUG: null connection pointer\n");
		return 0;
	}
}


/* unsafe tcpconn_rm version (nolocks) */
void _tcpconn_rm(struct tcp_connection* c)
{
	int r;
	tcpconn_listrm(tcpconn_id_hash[c->id_hash], c, id_next, id_prev);
	/* remove all the aliases */
	for (r=0; r<c->aliases; r++)
		tcpconn_listrm(tcpconn_aliases_hash[c->con_aliases[r].hash], 
						&c->con_aliases[r], next, prev);
	lock_destroy(&c->write_lock);
#ifdef USE_TLS
	if (c->type==PROTO_TLS) tls_tcpconn_clean(c);
#endif
	shm_free(c);
}



void tcpconn_rm(struct tcp_connection* c)
{
	int r;
	TCPCONN_LOCK;
	tcpconn_listrm(tcpconn_id_hash[c->id_hash], c, id_next, id_prev);
	/* remove all the aliases */
	for (r=0; r<c->aliases; r++)
		tcpconn_listrm(tcpconn_aliases_hash[c->con_aliases[r].hash], 
						&c->con_aliases[r], next, prev);
	TCPCONN_UNLOCK;
	lock_destroy(&c->write_lock);
#ifdef USE_TLS
	if ((c->type==PROTO_TLS)&&(c->extra_data)) tls_tcpconn_clean(c);
#endif
	shm_free(c);
}


/* finds a connection, if id=0 uses the ip addr & port (host byte order)
 * WARNING: unprotected (locks) use tcpconn_get unless you really
 * know what you are doing */
struct tcp_connection* _tcpconn_find(int id, struct ip_addr* ip, int port)
{

	struct tcp_connection *c;
	struct tcp_conn_alias* a;
	unsigned hash;
	
#ifdef EXTRA_DEBUG
	DBG("tcpconn_find: %d  port %d\n",id, port);
	if (ip) print_ip("tcpconn_find: ip ", ip, "\n");
#endif
	if (id){
		hash=tcp_id_hash(id);
		for (c=tcpconn_id_hash[hash]; c; c=c->id_next){
#ifdef EXTRA_DEBUG
			DBG("c=%p, c->id=%d, port=%d\n",c, c->id, c->rcv.src_port);
			print_ip("ip=", &c->rcv.src_ip, "\n");
#endif
			if ((id==c->id)&&(c->state!=S_CONN_BAD)) return c;
		}
	}else if (ip){
		hash=tcp_addr_hash(ip, port);
		for (a=tcpconn_aliases_hash[hash]; a; a=a->next){
#ifdef EXTRA_DEBUG
			DBG("a=%p, c=%p, c->id=%d, alias port= %d port=%d\n", a, a->parent,
					a->parent->id, a->port, a->parent->rcv.src_port);
			print_ip("ip=",&a->parent->rcv.src_ip,"\n");
#endif
			if ( (a->parent->state!=S_CONN_BAD) && (port==a->port) &&
					(ip_addr_cmp(ip, &a->parent->rcv.src_ip)) )
				return a->parent;
		}
	}
	return 0;
}



/* _tcpconn_find with locks and timeout */
struct tcp_connection* tcpconn_get(int id, struct ip_addr* ip, int port,
									int timeout)
{
	struct tcp_connection* c;
	TCPCONN_LOCK;
	c=_tcpconn_find(id, ip, port);
	if (c){ 
			c->refcnt++;
			c->timeout=get_ticks()+timeout;
	}
	TCPCONN_UNLOCK;
	return c;
}



/* add port as an alias for the "id" connection
 * returns 0 on success,-1 on failure */
int tcpconn_add_alias(int id, int port, int proto)
{
	struct tcp_connection* c;
	unsigned hash;
	struct tcp_conn_alias* a;
	
	a=0;
	/* fix the port */
	port=port?port:((proto==PROTO_TLS)?SIPS_PORT:SIP_PORT);
	TCPCONN_LOCK;
	/* check if alias already exists */
	c=_tcpconn_find(id, 0, 0);
	if (c){
		hash=tcp_addr_hash(&c->rcv.src_ip, port);
		/* search the aliases for an already existing one */
		for (a=tcpconn_aliases_hash[hash]; a; a=a->next){
			if ( (a->parent->state!=S_CONN_BAD) && (port==a->port) &&
					(ip_addr_cmp(&c->rcv.src_ip, &a->parent->rcv.src_ip)) ){
				/* found */
				if (a->parent!=c) goto error_sec;
				else goto ok;
			}
		}
		if (c->aliases>=TCP_CON_MAX_ALIASES) goto error_aliases;
		c->con_aliases[c->aliases].parent=c;
		c->con_aliases[c->aliases].port=port;
		c->con_aliases[c->aliases].hash=hash;
		tcpconn_listadd(tcpconn_aliases_hash[hash], 
								&c->con_aliases[c->aliases], next, prev);
		c->aliases++;
	}else goto error_not_found;
ok:
	TCPCONN_UNLOCK;
#ifdef EXTRA_DEBUG
	if (a) DBG("tcpconn_add_alias: alias already present\n");
	else   DBG("tcpconn_add_alias: alias port %d for hash %d, id %d\n",
			port, hash, c->id);
#endif
	return 0;
error_aliases:
	TCPCONN_UNLOCK;
	LOG(L_ERR, "ERROR: tcpconn_add_alias: too many aliases for connection %p"
				" (%d)\n", c, c->id);
	return -1;
error_not_found:
	TCPCONN_UNLOCK;
	LOG(L_ERR, "ERROR: tcpconn_add_alias: no connection found for id %d\n",id);
	return -1;
error_sec:
	TCPCONN_UNLOCK;
	LOG(L_ERR, "ERROR: tcpconn_add_alias: possible port hijack attemp\n");
	LOG(L_ERR, "ERROR: tcpconn_add_alias: alias already present and points"
			" to another connection (%d : %d and %d : %d)\n",
			a->parent->id,  port, c->id, port);
	return -1;
}



void tcpconn_ref(struct tcp_connection* c)
{
	TCPCONN_LOCK;
	c->refcnt++; /* FIXME: atomic_dec */
	TCPCONN_UNLOCK;
}



void tcpconn_put(struct tcp_connection* c)
{
	TCPCONN_LOCK;
	c->refcnt--; /* FIXME: atomic_dec */
	TCPCONN_UNLOCK;
}



/* finds a tcpconn & sends on it */
int tcp_send(int type, char* buf, unsigned len, union sockaddr_union* to,
				int id)
{
	struct tcp_connection *c;
	struct tcp_connection *tmp;
	struct ip_addr ip;
	int port;
	int fd;
	long response[2];
	int n;
	
	port=0;
	if (to){
		su2ip_addr(&ip, to);
		port=su_getport(to);
		c=tcpconn_get(id, &ip, port, TCP_CON_SEND_TIMEOUT); 
	}else if (id){
		c=tcpconn_get(id, 0, 0, TCP_CON_SEND_TIMEOUT);
	}else{
		LOG(L_CRIT, "BUG: tcp_send called with null id & to\n");
		return -1;
	}
	
	if (id){
		if (c==0) {
			if (to){
				/* try again w/o id */
				c=tcpconn_get(0, &ip, port, TCP_CON_SEND_TIMEOUT);
				goto no_id;
			}else{
				LOG(L_ERR, "ERROR: tcp_send: id %d not found, dropping\n",
						id);
				return -1;
			}
		}else goto get_fd;
	}
no_id:
		if (c==0){
			DBG("tcp_send: no open tcp connection found, opening new one\n");
			/* create tcp connection */
			if ((c=tcpconn_connect(to, type))==0){
				LOG(L_ERR, "ERROR: tcp_send: connect failed\n");
				return -1;
			}
			c->refcnt++; /* safe to do it w/o locking, it's not yet
							available to the rest of the world */
			fd=c->s;
			
			/* send the new tcpconn to "tcp main" */
			response[0]=(long)c;
			response[1]=CONN_NEW;
			n=send_all(unix_tcp_sock, response, sizeof(response));
			if (n<=0){
				LOG(L_ERR, "BUG: tcp_send: failed write: %s (%d)\n",
						strerror(errno), errno);
				n=-1;
				goto end;
			}	
			n=send_fd(unix_tcp_sock, &c, sizeof(c), c->s);
			if (n<=0){
				LOG(L_ERR, "BUG: tcp_send: failed send_fd: %s (%d)\n",
						strerror(errno), errno);
				n=-1;
				goto end;
			}
			goto send_it;
		}
get_fd:
			/* todo: see if this is not the same process holding
			 *  c  and if so send directly on c->fd */
			DBG("tcp_send: tcp connection found (%p), acquiring fd\n", c);
			/* get the fd */
			response[0]=(long)c;
			response[1]=CONN_GET_FD;
			n=send_all(unix_tcp_sock, response, sizeof(response));
			if (n<=0){
				LOG(L_ERR, "BUG: tcp_send: failed to get fd(write):%s (%d)\n",
						strerror(errno), errno);
				n=-1;
				goto release_c;
			}
			DBG("tcp_send, c= %p, n=%d\n", c, n);
			tmp=c;
			n=receive_fd(unix_tcp_sock, &c, sizeof(c), &fd);
			if (n<=0){
				LOG(L_ERR, "BUG: tcp_send: failed to get fd(receive_fd):"
							" %s (%d)\n", strerror(errno), errno);
				n=-1;
				goto release_c;
			}
			if (c!=tmp){
				LOG(L_CRIT, "BUG: tcp_send: get_fd: got different connection:"
						"  %p (id= %d, refcnt=%d state=%d != "
						"  %p (id= %d, refcnt=%d state=%d (n=%d)\n",
						  c,   c->id,   c->refcnt,   c->state,
						  tmp, tmp->id, tmp->refcnt, tmp->state, n
				   );
				n=-1; /* fail */
				goto end;
			}
			DBG("tcp_send: after receive_fd: c= %p n=%d fd=%d\n",c, n, fd);
		
	
	
send_it:
	DBG("tcp_send: sending...\n");
	lock_get(&c->write_lock);
#ifdef USE_TLS
	if (c->type==PROTO_TLS)
		n=tls_blocking_write(c, fd, buf, len);
	else
#endif
		n=send(fd, buf, len,
#ifdef HAVE_MSG_NOSIGNAL
			MSG_NOSIGNAL
#else
			0
#endif
			);
	lock_release(&c->write_lock);
	DBG("tcp_send: after write: c= %p n=%d fd=%d\n",c, n, fd);
	DBG("tcp_send: buf=\n%.*s\n", (int)len, buf);
	if (n<0){
		if (errno==EINTR) goto send_it; /* interrupted write, try again*/
										/* keep the lock or lock/unlock again?*/
		LOG(L_ERR, "ERROR: tcpsend: failed to send, n=%d: %s (%d)\n",
				n, strerror(errno), errno);
		/* error on the connection , mark it as bad and set 0 timeout */
		c->state=S_CONN_BAD;
		c->timeout=0;
		/* tell "main" it should drop this (optional it will t/o anyway?)*/
		response[0]=(long)c;
		response[1]=CONN_ERROR;
		n=send_all(unix_tcp_sock, response, sizeof(response));
		/* CONN_ERROR will auto-dec refcnt => we must not call tcpconn_put !!*/
		if (n<=0){
			LOG(L_ERR, "BUG: tcp_send: error return failed (write):%s (%d)\n",
					strerror(errno), errno);
			n=-1;
		}
		close(fd);
		return n; /* error return, no tcpconn_put */
	}
end:
	close(fd);
release_c:
	tcpconn_put(c); /* release c (lock; dec refcnt; unlock) */
	return n;
}



/* very ineficient for now - FIXME*/
void tcpconn_timeout(fd_set* set)
{
	struct tcp_connection *c, *next;
	int ticks;
	unsigned h;
	int fd;
	
	
	ticks=get_ticks();
	TCPCONN_LOCK; /* fixme: we can lock only on delete IMO */
	for(h=0; h<TCP_ID_HASH_SIZE; h++){
		c=tcpconn_id_hash[h];
		while(c){
			next=c->id_next;
			if ((c->refcnt==0) && (ticks>c->timeout)) {
				DBG("tcpconn_timeout: timeout for hash=%d - %p (%d > %d)\n",
						h, c, ticks, c->timeout);
				fd=c->s;
#ifdef USE_TLS
				if (c->type==PROTO_TLS)
					tls_close(c, fd);
#endif
				_tcpconn_rm(c);
				if (fd>0) {
					FD_CLR(fd, set);
					close(fd);
				}
			}
			c=next;
		}
	}
	TCPCONN_UNLOCK;
}



int tcp_init(struct socket_info* sock_info)
{
	union sockaddr_union* addr;
	int optval;
#ifdef DISABLE_NAGLE
	int flag;
	struct protoent* pe;

	if (tcp_proto_no==-1){ /* if not already set */
		pe=getprotobyname("tcp");
		if (pe==0){
			LOG(L_ERR, "ERROR: tcp_init: could not get TCP protocol number\n");
			tcp_proto_no=-1;
		}else{
			tcp_proto_no=pe->p_proto;
		}
	}
#endif
	
	addr=&sock_info->su;
	/* sock_info->proto=PROTO_TCP; */
	if (init_su(addr, &sock_info->address, sock_info->port_no)<0){
		LOG(L_ERR, "ERROR: tcp_init: could no init sockaddr_union\n");
		goto error;
	}
	sock_info->socket=socket(AF2PF(addr->s.sa_family), SOCK_STREAM, 0);
	if (sock_info->socket==-1){
		LOG(L_ERR, "ERROR: tcp_init: socket: %s\n", strerror(errno));
		goto error;
	}
#ifdef DISABLE_NAGLE
	flag=1;
	if ( (tcp_proto_no!=-1) &&
		 (setsockopt(sock_info->socket, tcp_proto_no , TCP_NODELAY,
					 &flag, sizeof(flag))<0) ){
		LOG(L_ERR, "ERROR: tcp_init: could not disable Nagle: %s\n",
				strerror(errno));
	}
#endif


#if  !defined(TCP_DONT_REUSEADDR) 
	/* Stevens, "Network Programming", Section 7.5, "Generic Socket
     * Options": "...server started,..a child continues..on existing
	 * connection..listening server is restarted...call to bind fails
	 * ... ALL TCP servers should specify the SO_REUSEADDRE option 
	 * to allow the server to be restarted in this situation
	 *
	 * Indeed, without this option, the server can't restart.
	 *   -jiri
	 */
	optval=1;
	if (setsockopt(sock_info->socket, SOL_SOCKET, SO_REUSEADDR,
				(void*)&optval, sizeof(optval))==-1) {
		LOG(L_ERR, "ERROR: tcp_init: setsockopt %s\n",
			strerror(errno));
		goto error;
	}
#endif
	/* tos */
	optval=IPTOS_LOWDELAY;
	if (setsockopt(sock_info->socket, IPPROTO_IP, IP_TOS, (void*)&optval, 
				sizeof(optval)) ==-1){
		LOG(L_WARN, "WARNING: tcp_init: setsockopt tos: %s\n", strerror(errno));
		/* continue since this is not critical */
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
		}else if (min_busy>tcp_children[i].busy){
			min_busy=tcp_children[i].busy;
			idx=i;
		}
	}
	
	tcp_children[idx].busy++;
	tcp_children[idx].n_reqs++;
	if (min_busy){
		LOG(L_WARN, "WARNING: send2child: no free tcp receiver, "
				" connection passed to the least busy one (%d)\n",
				min_busy);
	}
	DBG("send2child: to tcp child %d %d(%d), %p\n", idx, 
					tcp_children[idx].proc_no,
					tcp_children[idx].pid, tcpconn);
	if (send_fd(tcp_children[idx].unix_sock, &tcpconn, sizeof(tcpconn),
			tcpconn->s)<=0){
		LOG(L_ERR, "ERROR: send2child: send_fd failed\n");
		return -1;
	}
	
	return 0;
}


/* handle a new connection, called internally by tcp_main_loop */
static inline void handle_new_connect(struct socket_info* si,
										fd_set* sel_set, int* n)
{
	union sockaddr_union su;
	struct tcp_connection* tcpconn;
	socklen_t su_len;
	int new_sock;
	
	if ((FD_ISSET(si->socket, sel_set))){
		/* got a connection on r */
		su_len=sizeof(su);
		new_sock=accept(si->socket, &(su.s), &su_len);
		(*n)--;
		if (new_sock==-1){
			LOG(L_ERR,  "WARNING: tcp_main_loop: error while accepting"
					" connection(%d): %s\n", errno, strerror(errno));
			return;
		}
		
		/* add socket to list */
		tcpconn=tcpconn_new(new_sock, &su, si, si->proto, S_CONN_ACCEPT);
		if (tcpconn){
			tcpconn->refcnt++; /* safe, not yet available to the
								  outside world */
			tcpconn_add(tcpconn);
			DBG("tcp_main_loop: new connection: %p %d\n",
				tcpconn, tcpconn->s);
			/* pass it to a child */
			if(send2child(tcpconn)<0){
				LOG(L_ERR,"ERROR: tcp_main_loop: no children "
						"available\n");
				TCPCONN_LOCK;
				tcpconn->refcnt--;
				if (tcpconn->refcnt==0){
					close(tcpconn->s);
					_tcpconn_rm(tcpconn);
				}else tcpconn->timeout=0; /* force expire */
				TCPCONN_UNLOCK;
			}
		}else{ /*tcpconn==0 */
			LOG(L_ERR, "ERROR: tcp_main_loop: tcpconn_new failed, "
					"closing socket\n");
			close(new_sock);
		}
	}
}


/* used internally by tcp_main_loop() */
static void tcpconn_destroy(struct tcp_connection* tcpconn)
{
	int fd;

	TCPCONN_LOCK; /*avoid races w/ tcp_send*/
	tcpconn->refcnt--;
	if (tcpconn->refcnt==0){ 
		DBG("tcp_main_loop: destroying connection\n");
		fd=tcpconn->s;
#ifdef USE_TLS
		/*FIXME: lock ->writelock ? */
		if (tcpconn->type==PROTO_TLS)
			tls_close(tcpconn, fd);
#endif
		_tcpconn_rm(tcpconn);
		close(fd);
	}else{
		/* force timeout */
		tcpconn->timeout=0;
		tcpconn->state=S_CONN_BAD;
		DBG("tcp_main_loop: delaying ...\n");
		
	}
	TCPCONN_UNLOCK;
}


void tcp_main_loop()
{
	int r;
	int n;
	fd_set master_set;
	fd_set sel_set;
	int maxfd;
	struct tcp_connection* tcpconn;
	unsigned h;
	long response[2];
	int cmd;
	int bytes;
	struct timeval timeout;
	int fd;
	struct socket_info* si;

	/*init */
	maxfd=0;
	FD_ZERO(&master_set);
	/* set all the listen addresses */
	for (si=tcp_listen; si; si=si->next){
		if ((si->proto==PROTO_TCP) &&(si->socket!=-1)){
			FD_SET(si->socket, &master_set);
			if (si->socket>maxfd) maxfd=si->socket;
		}else{
			LOG(L_CRIT, "BUG: tcp_main_loop: non tcp address in tcp_listen\n");
		}
	}
#ifdef USE_TLS
	if (!tls_disable){
		for (si=tls_listen; si; si=si->next){
			if ((si->proto==PROTO_TLS) && (si->socket!=-1)){
				FD_SET(si->socket, &master_set);
				if (si->socket>maxfd) maxfd=si->socket;
			}else{
				LOG(L_CRIT, "BUG: tcp_main_loop: non tls address"
						" in tls_listen\n");
			}
		}
	}
#endif
	/* set all the unix sockets used for child comm */
	for (r=1; r<process_no; r++){
		if (pt[r].unix_sock>0){ /* we can't have 0, we never close it!*/
			FD_SET(pt[r].unix_sock, &master_set);
			if (pt[r].unix_sock>maxfd) maxfd=pt[r].unix_sock;
		}
	}
	for (r=0; r<tcp_children_no; r++){
		if (tcp_children[r].unix_sock>0){ /* we can't have 0, 
											 we never close it!*/
			FD_SET(tcp_children[r].unix_sock, &master_set);
			if (tcp_children[r].unix_sock>maxfd)
				maxfd=tcp_children[r].unix_sock;
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
		
		for (si=tcp_listen; si && n; si=si->next)
			handle_new_connect(si, &sel_set, &n);
#ifdef USE_TLS
			if (!tls_disable)
				for (si=tls_listen; si && n; si=si->next)
					handle_new_connect(si, &sel_set, &n);
#endif
		
		/* check all the read fds (from the tcpconn_addr_hash ) */
		for (h=0; h<TCP_ID_HASH_SIZE; h++){
			for(tcpconn=tcpconn_id_hash[h]; tcpconn && n; 
					tcpconn=tcpconn->id_next){
				/* FIXME: is refcnt==0 really necessary? */
				if ((tcpconn->refcnt==0)&&(FD_ISSET(tcpconn->s, &sel_set))){
					/* new data available */
					n--;
					/* pass it to child, so remove it from select list */
					DBG("tcp_main_loop: data available on %p [h:%d] %d\n",
							tcpconn, h, tcpconn->s);
					FD_CLR(tcpconn->s, &master_set);
					tcpconn_ref(tcpconn); /* refcnt ++ */
					if (send2child(tcpconn)<0){
						LOG(L_ERR,"ERROR: tcp_main_loop: no "
									"children available\n");
						TCPCONN_LOCK;
						tcpconn->refcnt--;
						if (tcpconn->refcnt==0){
							fd=tcpconn->s;
							_tcpconn_rm(tcpconn);
							close(fd);
						}else tcpconn->timeout=0; /* force expire*/
						TCPCONN_UNLOCK;
					}
				}
			}
		}
		/* check unix sockets & listen | destroy connections */
		/* tcp_children readers first */
		for (r=0; r<tcp_children_no && n; r++){
			if ( (tcp_children[r].unix_sock>0) && 
					FD_ISSET(tcp_children[r].unix_sock, &sel_set)){
				/* (we can't have a fd==0, 0 is never closed )*/
				n--;
				/* read until sizeof(response)
				 * (this is a SOCK_STREAM so read is not atomic */
				bytes=recv_all(tcp_children[r].unix_sock, response,
								sizeof(response));
				if (bytes==0){
					/* EOF -> bad, child has died */
					LOG(L_CRIT, "BUG: tcp_main_loop: dead tcp child %d\n", r);
					/* don't listen on it any more */
					FD_CLR(tcp_children[r].unix_sock, &master_set);
					/*exit(-1);*/
					continue; /* skip this and try the next one */
				}else if (bytes<0){
					LOG(L_CRIT, "ERROR: tcp_main_loop: read from tcp child %d "
							"%s\n", r, strerror(errno));
					/* try to ignore ? */
					continue; /* skip this and try the next one */
				}
					
				DBG("tcp_main_loop: reader response= %lx, %ld from %d \n",
						response[0], response[1], r);
				cmd=response[1];
				tcpconn=(struct tcp_connection*)response[0];
				switch(cmd){
					case CONN_RELEASE:
						tcp_children[r].busy--;
						if (tcpconn){
								if (tcpconn->state==S_CONN_BAD){ 
									tcpconn_destroy(tcpconn);
									break;
								}
								FD_SET(tcpconn->s, &master_set);
								if (maxfd<tcpconn->s) maxfd=tcpconn->s;
								/* update the timeout*/
								tcpconn->timeout=get_ticks()+TCP_CON_TIMEOUT;
								tcpconn_put(tcpconn);
								DBG("tcp_main_loop: CONN_RELEASE  %p"
										" refcnt= %d\n", 
										tcpconn, tcpconn->refcnt);
						}
						break;
					case CONN_ERROR:
					case CONN_DESTROY:
					case CONN_EOF:
						/* WARNING: this will auto-dec. refcnt! */
						tcp_children[pt[r].idx].busy--;
						if (tcpconn){
							if (tcpconn->s!=-1)
								FD_CLR(tcpconn->s, &master_set);
							tcpconn_destroy(tcpconn);
						}
						break;
					default:
							LOG(L_CRIT, "BUG: tcp_main_loop:  unknown cmd %d"
										" from tcp reader %d\n",
									cmd, r);
				}
			}
		}
		/* check "send" unix sockets & listen | destroy connections */
		/* start from 1, the "main" process does not transmit anything*/
		for (r=1; r<process_no && n; r++){
			if ( (pt[r].unix_sock>0) && FD_ISSET(pt[r].unix_sock, &sel_set)){
				/* (we can't have a fd==0, 0 is never closed )*/
				n--;
				/* read until sizeof(response)
				 * (this is a SOCK_STREAM so read is not atomic */
				bytes=recv_all(pt[r].unix_sock, response, sizeof(response));
				if (bytes==0){
					/* EOF -> bad, child has died */
					LOG(L_CRIT, "BUG: tcp_main_loop: dead child %d\n", r);
					/* don't listen on it any more */
					FD_CLR(pt[r].unix_sock, &master_set);
					/*exit(-1);*/
					continue; /* skip this and try the next one */
				}else if (bytes<0){
					LOG(L_CRIT, "ERROR: tcp_main_loop: read from child:  %s\n",
							strerror(errno));
					/* try to ignore ? */
					continue; /* skip this and try the next one */
				}
					
				DBG("tcp_main_loop: read response= %lx, %ld from %d (%d)\n",
						response[0], response[1], r, pt[r].pid);
				cmd=response[1];
				tcpconn=(struct tcp_connection*)response[0];
				switch(cmd){
					case CONN_ERROR:
						if (tcpconn){
							if (tcpconn->s!=-1)
								FD_CLR(tcpconn->s, &master_set);
							tcpconn_destroy(tcpconn);
						}
						break;
					case CONN_GET_FD:
						/* send the requested FD  */
						/* WARNING: take care of setting refcnt properly to
						 * avoid race condition */
						if (tcpconn){
							if (send_fd(pt[r].unix_sock, &tcpconn,
										sizeof(tcpconn), tcpconn->s)<=0){
								LOG(L_ERR, "ERROR: tcp_main_loop:"
										"send_fd failed\n");
							}
						}else{
							LOG(L_CRIT, "BUG: tcp_main_loop: null pointer\n");
						}
						break;
					case CONN_NEW:
						/* update the fd in the requested tcpconn*/
						/* WARNING: take care of setting refcnt properly to
						 * avoid race condition */
						if (tcpconn){
							bytes=receive_fd(pt[r].unix_sock, &tcpconn,
									sizeof(tcpconn), &tcpconn->s);
								if (bytes<sizeof(tcpconn)){
									if (bytes<0){
										LOG(L_CRIT, "BUG: tcp_main_loop:"
												" CONN_NEW: receive_fd "
												"failed\n");
									}else{
										LOG(L_CRIT, "BUG: tcp_main_loop:"
												" CONN_NEW: to few bytes "
												"received (%d)\n", bytes );
									}
									break; /* try to ignore */
								}
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
		} /* for */
		
		/* remove old connections */
		tcpconn_timeout(&master_set);
	
	}
}



int init_tcp()
{
	/* init lock */
	tcpconn_lock=lock_alloc();
	if (tcpconn_lock==0){
		LOG(L_CRIT, "ERROR: init_tcp: could not alloc lock\n");
		goto error;
	}
	if (lock_init(tcpconn_lock)==0){
		LOG(L_CRIT, "ERROR: init_tcp: could not init lock\n");
		lock_dealloc((void*)tcpconn_lock);
		tcpconn_lock=0;
		goto error;
	}
	/* init globals */
	connection_id=(int*)shm_malloc(sizeof(int));
	if (connection_id==0){
		LOG(L_CRIT, "ERROR: init_tcp: could not alloc globals\n");
		lock_destroy(tcpconn_lock);
		lock_dealloc((void*)tcpconn_lock);
		tcpconn_lock=0;
		goto error;
	}
	*connection_id=1;
	/* alloc hashtables*/
	tcpconn_aliases_hash=(struct tcp_conn_alias**)
			shm_malloc(TCP_ALIAS_HASH_SIZE* sizeof(struct tcp_conn_alias*));
	if (tcpconn_aliases_hash==0){
		LOG(L_CRIT, "ERROR: init_tcp: could not alloc address hashtable\n");
		shm_free(connection_id);
		connection_id=0;
		lock_destroy(tcpconn_lock);
		lock_dealloc((void*)tcpconn_lock);
		tcpconn_lock=0;
		goto error;
	}
	
	tcpconn_id_hash=(struct tcp_connection**)shm_malloc(TCP_ID_HASH_SIZE*
								sizeof(struct tcp_connection*));
	if (tcpconn_id_hash==0){
		LOG(L_CRIT, "ERROR: init_tcp: could not alloc id hashtable\n");
		shm_free(connection_id);
		connection_id=0;
		shm_free(tcpconn_aliases_hash);
		tcpconn_aliases_hash=0;
		lock_destroy(tcpconn_lock);
		lock_dealloc((void*)tcpconn_lock);
		tcpconn_lock=0;
		goto error;
	}
	/* init hashtables*/
	memset((void*)tcpconn_aliases_hash, 0, 
			TCP_ALIAS_HASH_SIZE * sizeof(struct tcp_conn_alias*));
	memset((void*)tcpconn_id_hash, 0, 
			TCP_ID_HASH_SIZE * sizeof(struct tcp_connection*));
	return 0;
error:
		return -1;
}



/* cleanup before exit */
void destroy_tcp()
{
	if (tcpconn_lock){
		lock_destroy(tcpconn_lock);
		lock_dealloc((void*)tcpconn_lock);
		tcpconn_lock=0;
	}
	if(tcpconn_aliases_hash){
		shm_free(tcpconn_aliases_hash);
		tcpconn_aliases_hash=0;
	}
	if(tcpconn_id_hash){
		shm_free(tcpconn_id_hash);
		tcpconn_id_hash=0;
	}
	
	if(connection_id){
		shm_free(connection_id);
		connection_id=0;
	}
}



/* starts the tcp processes */
int tcp_init_children()
{
	int r;
	int sockfd[2];
	int reader_fd[2]; /* for comm. with the tcp children read  */
	pid_t pid;
	
	
	/* create the tcp sock_info structures */
	/* copy the sockets --moved to main_loop*/
	
	/* fork children & create the socket pairs*/
	for(r=0; r<tcp_children_no; r++){
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd)<0){
			LOG(L_ERR, "ERROR: tcp_main: socketpair failed: %s\n",
					strerror(errno));
			goto error;
		}
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, reader_fd)<0){
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
			close(reader_fd[1]);
			tcp_children[r].pid=pid;
			tcp_children[r].proc_no=process_no;
			tcp_children[r].busy=0;
			tcp_children[r].n_reqs=0;
			tcp_children[r].unix_sock=reader_fd[0];
			pt[process_no].pid=pid;
			pt[process_no].unix_sock=sockfd[0];
			pt[process_no].idx=r;
			strncpy(pt[process_no].desc, "tcp receiver", MAX_PT_DESC);
		}else{
			/* child */
			close(sockfd[0]);
			unix_tcp_sock=sockfd[1];
			bind_address=0; /* force a SEGFAULT if someone uses a non-init.
							   bind address on tcp */
			if (init_child(r+children_no+1) < 0) {
				LOG(L_ERR, "init_children failed\n");
				goto error;
			}
			tcp_receive_loop(reader_fd[1]);
		}
	}
	return 0;
error:
	return -1;
}

#endif
