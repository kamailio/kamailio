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

#include "../../globals.h"
#include "../../pt.h"  /* process_count */
#include "../../timer.h"
#include "../../timer_ticks.h"
#include "../../tsend.h"
#include "../../mem/mem.h"
#include "../../rpc.h" /* who & ls rpcs */
#include "../../ut.h"
#include "../../cfg/cfg_struct.h"

#include "ctrl_socks.h"
#include "binrpc_run.h"
#ifdef USE_FIFO
#include "fifo_server.h"
#endif

#include "io_listener.h"
#include "ctl.h"

#define HANDLE_IO_INLINE
#include "../../io_wait.h"
#include <fcntl.h> /* required by io_wait.h if SIGIO_RT is used */
#include <sys/uio.h> /* iovec */

#define IO_STREAM_CONN_TIMEOUT		S_TO_TICKS(120)
#define IO_LISTEN_TIMEOUT			10 /* in s,  how often the timer 
										  will be run */

#define IO_LISTEN_TX_TIMEOUT	10 /* ms */
#define DGRAM_BUF_SIZE	65535
#define STREAM_BUF_SIZE	65535


/* 0 has a special meaning, don't use it (see io_wait.h)*/
enum fd_type { F_T_RESERVED=0,  F_T_CTRL_DGRAM, F_T_CTRL_STREAM,
				 F_T_READ_STREAM
#ifdef USE_FIFO
				, F_T_FIFO
#endif
};


struct stream_req{
	unsigned char *end; /* end of read data */
	unsigned char* proc; /* processed so far */
	int bytes_to_go; /* how many bytes we still have to read */
	unsigned char buf[STREAM_BUF_SIZE];
};


struct stream_connection{
	struct stream_connection* next;
	struct stream_connection* prev;
	int fd;
	enum payload_proto p_proto;
	struct ctrl_socket* parent;
	struct stream_req req;
	void* saved_state; /* connection/datagram saved state */
	ticks_t expire;
	union sockaddr_u from;
};



typedef int (*send_ev_f)(void* send_h , struct iovec* v, size_t count);



static io_wait_h io_h;
static int io_read_connections=0;
static struct stream_connection stream_conn_lst; /* list head */

static struct stream_connection* s_conn_new(int sock, 
											struct ctrl_socket* cs,
											union sockaddr_u* from)
{
	struct stream_connection* s_c;
	
	s_c=ctl_malloc(sizeof(struct stream_connection));
	if (s_c){
		memset(s_c, 0, sizeof(struct stream_connection));
		s_c->fd=sock;
		s_c->req.end=&s_c->req.buf[0];
		s_c->req.proc=s_c->req.end;
		s_c->req.bytes_to_go=0; /* BINRPC_MIN_HDR ? */
		s_c->expire=get_ticks_raw()+IO_STREAM_CONN_TIMEOUT;
		s_c->p_proto=cs->p_proto;
		s_c->from=*from;
		s_c->parent=cs;
	}
	return s_c;
}


#define s_conn_add(s_c) clist_append(&stream_conn_lst, s_c, next, prev)



inline static void s_conn_rm(struct stream_connection* sc)
{
	clist_rm(sc, next, prev);
	ctl_free(sc);
	io_read_connections--;
}




/*
 * sends on a "disconnected" socket/fd (e.g. not connected udp socket,
 *  unix datagram socket)
 * returns: number  of bytes written on success,
 *          <0  on error (-1 tsend* error, -2 packet too big)
 */
inline static int sendv_disc(struct send_handle* sh, struct iovec* v,
								size_t count)
{
	char buf[DGRAM_BUF_SIZE];
	char* p;
	char* end;
	int r;
	
	p=buf;
	end=p+DGRAM_BUF_SIZE;
	for (r=0; r<count; r++){
		if ((p+v[r].iov_len)>end) 
			goto error_overflow;
		memcpy(p, v[r].iov_base, v[r].iov_len);
		p+=v[r].iov_len;
	}
	return tsend_dgram(sh->fd, buf, (int)(p-buf), &sh->from.sa_in.s,
						sh->from_len, IO_LISTEN_TX_TIMEOUT);
error_overflow:
	return -2;
}



/* returns: number of bytes written on success,
 *          <0 on error (-1 send error, -2 too big)
 */
int sock_send_v(void *h, struct iovec* v, size_t count)
{
	struct send_handle* sh;
	
	sh=(struct send_handle*)h;
	if (sh->type==S_CONNECTED)
		return tsend_dgram_ev(sh->fd, v, count, IO_LISTEN_TX_TIMEOUT);
	else
		return sendv_disc(sh, v, count);
};



void io_listen_loop(int fd_no, struct ctrl_socket* cs_lst)
{
	int max_fd_no;
	char* poll_err;
	int poll_method;
	struct ctrl_socket *cs;
	int type;
	
	clist_init(&stream_conn_lst, next, prev);
	type=UNKNOWN_SOCK;
#if 0
	/* estimate used fd numbers -- FIXME: broken, make it a function in pt.h */
	max_fd_no=get_max_procs()*3 -1 /* timer */ +3; /* stdin/out/err*/;
	max_fd_no+=fd_no+MAX_IO_READ_CONNECTIONS; /*our listen fds + max.
												allowed tmp. fds */
#endif
	max_fd_no=get_max_open_fds();
	/* choose/fix the poll method */
	/* FIXME: make it a config param? */
#if USE_TCP
	poll_method=tcp_poll_method; /* try to resue the tcp poll method */
	poll_err=check_poll_method(poll_method);
#else
	poll_method = 0; /* make check for TCP poll method fail */
	poll_err = NULL;
#endif
	
	/* set an appropiate poll method */
	if (poll_err || (poll_method==0)){
		poll_method=choose_poll_method();
		if (poll_err){
			LOG(L_ERR, "ERROR: io_listen_loop: %s, using %s instead\n",
					poll_err, poll_method_name(poll_method));
		}else{
			LOG(L_INFO, "io_listen_loop: using %s as the io watch method"
					" (auto detected)\n", poll_method_name(poll_method));
		}
	}else{
			LOG(L_INFO, "io_listen_loop:  using %s io watch method (config)\n",
					poll_method_name(poll_method));
	}
	
	if (init_io_wait(&io_h, max_fd_no, poll_method)<0)
		goto error;
	/* add all the sockets we listen on for connections */
	for (cs=cs_lst; cs; cs=cs->next){
		switch(cs->transport){
			case UDP_SOCK:
			case UNIXD_SOCK:
				type=F_T_CTRL_DGRAM;
				break;
			case TCP_SOCK:
			case UNIXS_SOCK:
				type=F_T_CTRL_STREAM;
				break;
#ifdef USE_FIFO
			case FIFO_SOCK:
				type=F_T_FIFO; /* special */
				cs->data=s_conn_new(cs->fd, cs, &cs->u);/* reuse stream conn */
				if (cs->data==0){
					LOG(L_ERR, "ERROR: io_listen_loop: out of memory\n");
					goto error;
				}
			break;
#endif
			case UNKNOWN_SOCK:
				LOG(L_CRIT, "BUG: io_listen_loop: bad control socket transport"
						" %d\n", cs->transport); 
				goto error;
		}
		DBG("io_listen_loop: adding socket %d, type %d, transport"
					" %d (%s)\n", cs->fd, type, cs->transport, cs->name);
		if (io_watch_add(&io_h, cs->fd, POLLIN, type, cs)<0){
			LOG(L_CRIT, "ERROR: io_listen_loop: init: failed to add"
					"listen socket to the fd list\n");
			goto error;
		}
	}

	/* initialize the config framework */
	if (cfg_child_init()) goto error;

	/* main loop */
	switch(io_h.poll_method){
		case POLL_POLL:
			while(1){
				io_wait_loop_poll(&io_h, IO_LISTEN_TIMEOUT, 0);
			}
			break;
#ifdef HAVE_SELECT
		case POLL_SELECT:
			while(1){
				io_wait_loop_select(&io_h, IO_LISTEN_TIMEOUT, 0);
			}
			break;
#endif
#ifdef HAVE_SIGIO_RT
		case POLL_SIGIO_RT:
			while(1){
				io_wait_loop_sigio_rt(&io_h, IO_LISTEN_TIMEOUT);
			}
			break;
#endif
#ifdef HAVE_EPOLL
		case POLL_EPOLL_LT:
			while(1){
				io_wait_loop_epoll(&io_h, IO_LISTEN_TIMEOUT, 0);
			}
			break;
		case POLL_EPOLL_ET:
			while(1){
				io_wait_loop_epoll(&io_h, IO_LISTEN_TIMEOUT, 1);
			}
			break;
#endif
#ifdef HAVE_KQUEUE
		case POLL_KQUEUE:
			while(1){
				io_wait_loop_kqueue(&io_h, IO_LISTEN_TIMEOUT, 0);
			}
			break;
#endif
#ifdef HAVE_DEVPOLL
		case POLL_DEVPOLL:
			while(1){
				io_wait_loop_devpoll(&io_h, IO_LISTEN_TIMEOUT, 0);
			}
			break;
#endif
		default:
			LOG(L_CRIT, "BUG: io_listen_loop: no support for poll method "
					" %s (%d)\n", 
					poll_method_name(io_h.poll_method), io_h.poll_method);
			goto error;
	}
/* should never reach this point under normal (non-error) circumstances */
error:
	LOG(L_CRIT, "ERROR: io_listen_loop exiting ...\n");
}



/* handles an io event on one of the watched dgram connections
 * (it can read the whole packet)
 * 
 * params: cs - pointer to the control socket for which we have an io ev.
 * returns:  handle_* return convention:
 *         -1 on error, or when we are not interested any more on reads
 *            from this fd (e.g.: we are closing it )
 *          0 on EAGAIN or when by some other way it is known that no more 
 *            io events are queued on the fd (the receive buffer is empty).
 *            Usefull to detect when there are no more io events queued for
 *            sigio_rt, epoll_et, kqueue.
 *         >0 on successfull read from the fd (when there might be more io
 *            queued -- the receive buffer might still be non-empty)
 *
 */
static int handle_ctrl_dgram(struct ctrl_socket* cs)
{
	unsigned char buf[DGRAM_BUF_SIZE];
	int bytes;
	int bytes_needed;
	int ret;
	struct send_handle sh;
	void* saved_state;
	
	saved_state=0; /* we get always a new datagram */
	sh.fd=cs->fd;
	sh.type=S_DISCONNECTED;
	sh.from_len=(cs->transport==UDP_SOCK)?sockaddru_len(cs->u.sa_in):
				sizeof(cs->u.sa_un);
again:
	bytes=recvfrom(cs->fd, buf, DGRAM_BUF_SIZE, 0, &sh.from.sa_in.s,
					&sh.from_len);
	if (bytes==-1){
		if ((errno==EAGAIN)||(errno==EWOULDBLOCK)){
			ret=0;
			goto skip;
		}else if (errno==EINTR){
			goto again;
		}
		LOG(L_ERR, "ERROR; handle_ctrl_dgram: recvfrom on %s: [%d] %s\n",
				cs->name, errno, strerror(errno));
		goto error;
	}
	DBG("handle_ctrl_dgram: new packet  on %s\n", cs->name);
	ret=1;
#ifdef USE_FIFO
	if (cs->p_proto==P_FIFO)
		fifo_process((char*)buf, bytes, &bytes_needed, &sh, &saved_state);
	else
#endif
		process_rpc_req(buf, bytes, &bytes_needed, &sh, &saved_state);
	/* if too few bytes received, just ignore it */
skip:
	return ret;
error:
	return -1;
}



/* handles an new connect on one of the watched stream connections
 * 
 * params: cs - pointer to the control socket for which we have an io ev.
 * returns:  handle_* return convention:
 *         -1 on error, or when we are not interested any more on accepts
 *            from this fd (e.g.: we are closing it )
 *          0 on EAGAIN or when by some other way it is known that no more 
 *            io events are queued on the fd (the receive buffer is empty).
 *            Usefull to detect when there are no more io events queued for
 *            sigio_rt, epoll_et, kqueue.
 *         >0 on successfull accept from the fd (when there might be more io
 *            queued -- the receive buffer might still be non-empty)
 *
 */
static int handle_new_connect(struct ctrl_socket* cs)
{
	int ret;
	union sockaddr_u from;
	unsigned int from_len;
	int new_sock;
	struct stream_connection* s_conn;
	
	from_len=(cs->transport==UDP_SOCK)?sockaddru_len(cs->u.sa_in):
				sizeof(cs->u.sa_un);
again:
	new_sock=accept(cs->fd, &from.sa_in.s, &from_len);
	if (new_sock==-1){
		if ((errno==EAGAIN)||(errno==EWOULDBLOCK)){
			ret=0;
			goto skip;
		}else if (errno==EINTR){
			goto again;
		}
		LOG(L_ERR, "ERROR: io_listen: handle_new_connect:"
				" error while accepting connection on %s: [%d] %s\n",
				cs->name, errno, strerror(errno));
		goto error;
	}
	ret=1;
	if (io_read_connections>=MAX_IO_READ_CONNECTIONS){
		LOG(L_ERR, "ERROR: io listen: maximum number of connections"
				" exceeded: %d/%d\n",
				io_read_connections, MAX_IO_READ_CONNECTIONS);
		close(new_sock);
		goto skip; /* success because accept was successful */
	}
	if (init_sock_opt(new_sock, cs->transport)<0){
		LOG(L_ERR, "ERROR: io listen: handle_new_connect:"
				" init_sock_opt failed\n");
		close(new_sock);
		goto skip;
	}
	/* add socket to the list */
	s_conn=s_conn_new(new_sock, cs, &from);
	if (s_conn){
		s_conn_add(s_conn);
		io_watch_add(&io_h, s_conn->fd, POLLIN, F_T_READ_STREAM, s_conn);
	}else{
		LOG(L_ERR, "ERROR: io listen: handle_new_connect:"
				" s_conn_new failed\n");
		close(new_sock);
		goto skip;
	}
	io_read_connections++;
	DBG("handle_stream read: new connection (%d) on %s\n",
			io_read_connections, cs->name);
skip:
	return ret;
error:
	return -1;
}



/* handles a read event on one of the accepted connections
 * 
 * params: s_c - pointer to the stream_connection for which we have an io ev.
 *         idx - index in the fd_array -> pass this to io_watch_del for 
 *               faster deletes
 * returns:  handle_* return convention:
 *         -1 on error, or when we are not interested any more on reads
 *            from this fd (e.g.: we are closing it )
 *          0 on EAGAIN or when by some other way it is known that no more 
 *            io events are queued on the fd (the receive buffer is empty).
 *            Usefull to detect when there are no more io events queued for
 *            sigio_rt, epoll_et, kqueue.
 *         >0 on successfull read from the fd (when there might be more io
 *            queued -- the receive buffer might still be non-empty)
 *
 */
static int handle_stream_read(struct stream_connection* s_c, int idx)
{
	int bytes_free;
	int bytes_read;
	int bytes_needed;
	int bytes_processed;
	struct stream_req* r;
	struct send_handle sh;
	
	sh.fd=s_c->fd;
	sh.type=S_CONNECTED;
	sh.from_len=0;
	r=&s_c->req;
	bytes_free=STREAM_BUF_SIZE-(int)(r->end-r->buf);
	if (bytes_free==0){
		LOG(L_ERR, "ERROR: handle_stream_read: buffer overrun\n");
		goto close_connection;
	}
again:
	bytes_read=read(s_c->fd, r->end, bytes_free);
	if (bytes_read==-1){
		if ((errno==EAGAIN)||(errno==EWOULDBLOCK)){
			goto no_read; /* nothing has been read */
		}else if (errno==EINTR) goto again;
		LOG(L_ERR, "ERROR: handle_stream_read: error reading: %s [%d]\n",
				strerror(errno), errno);
		goto error_read;
	}else if(bytes_read==0){ /* eof */
		DBG("handle_stream read: eof on %s\n", s_c->parent->name);
		goto close_connection;
	}
	r->end+=bytes_read;
	if (bytes_read && (bytes_read<r->bytes_to_go)){
		r->bytes_to_go-=bytes_read;
		goto skip; /* not enough bytes read, no point in trying to process
					  them */
	}
	do{
#ifdef USE_FIFO
		if (s_c->p_proto==P_FIFO)
			bytes_processed=fifo_process((char*)r->proc, (int)(r->end-r->proc),
										&bytes_needed, &sh, &s_c->saved_state);
		else
#endif
			bytes_processed=process_rpc_req(r->proc, (int)(r->end-r->proc),
										&bytes_needed, &sh, &s_c->saved_state);
		if (bytes_processed<0){
			/* error while processing the packet => close the connection */
			goto close_connection;
		}
		r->proc+=bytes_processed;
		r->bytes_to_go=bytes_needed;
		if (bytes_needed>0){
			if (bytes_read==0){ /*want more bytes, but we have eof*/
				LOG(L_ERR, "ERROR: handle_stream_read: unexpected EOF\n");
				goto close_connection;
			}
			break; /* no more read bytes ready for processing */
		}
		/* if (bytes_needed==0) -- packet fully processed */
		s_c->saved_state=0; /* reset per datagram state */
	}while(r->proc<r->end);
	/* free some space in the buffer */
	if (r->proc>r->buf){
		if (r->end>r->proc){
			memmove(r->buf, r->proc, (int)(r->end-r->proc));
			r->end-=(int)(r->proc-r->buf);
		}else{
			r->end=r->buf;
		}
		r->proc=r->buf;
	}
skip:
	/* everything went fine, we just have to read more */
	s_c->expire=get_ticks_raw()+IO_STREAM_CONN_TIMEOUT; /* update timeout*/
	return 1;
	
no_read:
	/* false alarm */
	return 0;
close_connection:
	io_watch_del(&io_h, s_c->fd, idx, IO_FD_CLOSING);
	close(s_c->fd);
	s_conn_rm(s_c);
	return 0;
error_read:
	io_watch_del(&io_h, s_c->fd, idx, IO_FD_CLOSING);
	close(s_c->fd);
	s_conn_rm(s_c);
	return -1;
	/* close connection */
}



#ifdef USE_FIFO
/* handles a read event on one of the fifos 
 * 
 * params: s_c - pointer to the stream_connection for which we have an io ev.
 *         idx - index in the fd_array -> pass this to io_watch_del for 
 *               faster deletes
 * returns:  handle_* return convention:
 *         -1 on error, or when we are not interested any more on reads
 *            from this fd (e.g.: we are closing it )
 *          0 on EAGAIN or when by some other way it is known that no more 
 *            io events are queued on the fd (the receive buffer is empty).
 *            Usefull to detect when there are no more io events queued for
 *            sigio_rt, epoll_et, kqueue.
 *         >0 on successfull read from the fd (when there might be more io
 *            queued -- the receive buffer might still be non-empty)
 *
 */
static int handle_fifo_read(struct ctrl_socket* cs, int idx)
{
	int bytes_free;
	int bytes_read;
	int bytes_needed;
	int bytes_processed;
	struct stream_req* r;
	struct send_handle sh;
	struct stream_connection* sc;
	
	sh.fd=-1;
	sh.type=S_FIFO;
	sh.from_len=0;
	sc=(struct stream_connection *)cs->data;
	r=&(sc->req);
	bytes_free=STREAM_BUF_SIZE-(int)(r->end-r->buf);
	if (bytes_free==0){
		LOG(L_ERR, "ERROR: handle_stream_read: buffer overrun\n");
		goto error;
	}
again:
	bytes_read=read(cs->fd, r->end, bytes_free);
	if (bytes_read==-1){
		if ((errno==EAGAIN)||(errno==EWOULDBLOCK)){
			goto no_read; /* nothing has been read */
		}else if (errno==EINTR) goto again;
		LOG(L_ERR, "ERROR: handle_fifo_read: error reading: %s [%d]\n",
				strerror(errno), errno);
		goto error_read;
	}else if(bytes_read==0){ /* eof */
		DBG("handle_fifo_read: eof on %s\n", cs->name);
	}
	r->end+=bytes_read;
	if (bytes_read && (bytes_read<r->bytes_to_go)){
		r->bytes_to_go-=bytes_read;
		goto skip; /* not enough bytes read, no point in trying to process
					  them */
	}
	do{
		bytes_processed=fifo_process((char*)r->proc, (int)(r->end-r->proc),
										&bytes_needed, &sh, &sc->saved_state);
		if (bytes_processed<0){
			/* error while processing the packet => skip */
			goto discard;
		}
		r->proc+=bytes_processed;
		r->bytes_to_go=bytes_needed;
		if (bytes_needed>0){
			if (bytes_read==0){ /*want more bytes, but we have eof*/
				LOG(L_ERR, "ERROR: handle_fifo_read: unexpected EOF\n");
				goto discard; /* discard buffered contents */
			}
			break; /* no more read bytes ready for processing */
		}
		/* if (bytes_needed==0) -- packet fully processed */
		sc->saved_state=0; /* reset per datagram state */
		if (bytes_processed==0){
			/* nothing processed, nothing needed, no error - looks like
			 * a bug */
			LOG(L_ERR, "ERROR: handle_fifo_read: unexpected return\n");
			goto discard;
		}
	}while(r->proc<r->end);
	/* free some space in the buffer */
	if (r->proc>r->buf){
		if (r->end>r->proc){
			memmove(r->buf, r->proc, (int)(r->end-r->proc));
			r->end-=(int)(r->proc-r->buf);
		}else{
			r->end=r->buf;
		}
		r->proc=r->buf;
	}
skip:
	/* everything went fine, we just have to read more */
	return 1;
error_read:
	/* temporary read error ? */
	r->proc=r->buf;
	r->end=r->buf;
	return -1;
discard:
	/* reset the whole receive buffer */
	r->proc=r->buf;
	r->end=r->buf;
	sc->saved_state=0; /* reset saved state */
	return 1;
no_read:
	/* false alarm */
	return 0;
error:
	return 1; /* there's nothing wrong with the fifo, just a bad  application
				 packet */
	/* close connection */
}
#endif



/* generic handle io routine, it will call the appropiate
 *  handle_xxx() based on the fd_map type
 *
 * params:  fm  - pointer to a fd hash entry
 *          idx - index in the fd_array (or -1 if not known)
 * return: -1 on error
 *          0 on EAGAIN or when by some other way it is known that no more 
 *            io events are queued on the fd (the receive buffer is empty).
 *            Usefull to detect when there are no more io events queued for
 *            sigio_rt, epoll_et, kqueue.
 *         >0 on successfull read from the fd (when there might be more io
 *            queued -- the receive buffer might still be non-empty)
 */
inline static int handle_io(struct fd_map* fm, short events, int idx)
{
	int ret;

	/* update the local config */
	cfg_update();

	switch(fm->type){
		case F_T_CTRL_DGRAM:
			ret=handle_ctrl_dgram((struct ctrl_socket*)fm->data);
			break;
		case F_T_CTRL_STREAM:
			ret=handle_new_connect((struct ctrl_socket*)fm->data);
			break;
		case F_T_READ_STREAM:
			ret=handle_stream_read((struct stream_connection*)fm->data, idx);
			break;
#ifdef USE_FIFO
		case F_T_FIFO:
			ret=handle_fifo_read((struct ctrl_socket*)fm->data, idx);
			break;
#endif
		case F_T_RESERVED:
			LOG(L_CRIT, "BUG: io listen handle_io: emtpy fd map\n");
			goto error;
		default:
			LOG(L_CRIT, "BUG: io listen handle_io: unknown fd type %d\n",
					fm->type);
			goto error;
	}
	return ret;
error:
	return -1;
}



void io_listen_who_rpc(rpc_t* rpc, void* ctx)
{
	struct stream_connection* sc;
	struct ip_addr ip;
	int port;
	int i;
	
	i=0;
	/* check if called from another process */
	if (stream_conn_lst.next==0){
		rpc->fault(ctx, 606, "rpc available only over binrpc (ctl)");
		return;
	}
	/* p_proto transport from sport to tport*/
	clist_foreach(&stream_conn_lst, sc, next){
		i++;
		rpc->add(ctx, "ss", payload_proto_name(sc->parent->p_proto),
								socket_proto_name(sc->parent->transport));
		switch(sc->parent->transport){
			case UDP_SOCK:
			case TCP_SOCK:
				su2ip_addr(&ip, &sc->from.sa_in);
				port=su_getport(&sc->from.sa_in);
				rpc->add(ctx, "ss", ip_addr2a(&ip), int2str(port, 0));
				su2ip_addr(&ip, &sc->parent->u.sa_in);
				port=su_getport(&sc->parent->u.sa_in);
				rpc->add(ctx, "ss", ip_addr2a(&ip), int2str(port, 0));
				break;
			case UNIXS_SOCK:
			case UNIXD_SOCK:
#ifdef USE_FIFO
			case FIFO_SOCK:
#endif		
				rpc->add(ctx, "ss", "<anonymous unix socket>", "" );
				rpc->add(ctx, "ss", sc->parent->name, "");
				break;
			default:
				rpc->add(ctx, "ssss", "<bug unknown protocol>",
								"", "", "", "");
		}
		
		/* idle time
		 * rpc->add(ctx, "d", TICKS_TO_MS(get_ticks_raw()-
								(sc->expire-IO_STREAM_CONN_TIMEOUT)));*/
	}
	if (i==0){
		rpc->fault(ctx, 400, "no open stream connection");
	}
}



void io_listen_conn_rpc(rpc_t* rpc, void* ctx)
{
	/* check if called from another process */
	if (stream_conn_lst.next==0){
		rpc->fault(ctx, 606, "rpc available only over binrpc (ctl)");
		return;
	}
	rpc->add(ctx, "d", io_read_connections);
}
