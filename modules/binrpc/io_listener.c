/*
 * $Id: io_listener.c,v 1.5 2006/11/17 20:07:36 andrei Exp $
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
 *  2006-02-15  created by andrei
 *  2007        ported to libbinrpc (bpintea)
 */

#include "../../globals.h"
#include "../../pt.h"  /* process_count */
#include "../../timer.h"
#include "../../timer_ticks.h"
#include "../../tsend.h"
#include "../../mem/mem.h"
#include "../../rpc.h" /* who & ls rpcs */
#include "../../ut.h"
#include "../../dprint.h"
#include "../../pass_fd.h"
#ifdef EXTRA_DEBUG
#include <assert.h>
#endif

#include "libbinrpc_wrapper.h"
#include "proctab.h"
#include "ctrl_socks.h"
//#include "binrpc_run.h"
#ifdef USE_FIFO
#include "fifo_server.h"
#endif

#include "io_listener.h"

#define HANDLE_IO_INLINE
#include "../../io_wait.h"
#include <fcntl.h> /* required by io_wait.h if SIGIO_RT is used */
#include <sys/uio.h> /* iovec */

#define MAX_IO_READ_CONNECTIONS		128 /* FIXME: make it a config var */
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
				, F_T_FDPASS
};


struct stream_connection{
	struct stream_connection* next;
	struct stream_connection* prev;
	struct ctrl_socket* parent;
	brpc_strd_t rds; /* ReaD State */
	ticks_t expire;
	union sockaddr_u from;
};

/* is there an extra process to manage connections */
int have_connection_listener = 0;


typedef int (*send_ev_f)(void* send_h , struct iovec* v, size_t count);



static io_wait_h io_h;
static int io_read_connections=0;
static struct stream_connection stream_conn_lst; /* list head */

static struct stream_connection* s_conn_new(int sock, 
											struct ctrl_socket* cs,
											union sockaddr_u* from)
{
	struct stream_connection* s_c;
	
	s_c=pkg_malloc(sizeof(struct stream_connection));
	if (s_c){
		memset(s_c, 0, sizeof(struct stream_connection));
		brpc_strd_init(&s_c->rds, sock);
		s_c->expire=get_ticks_raw()+IO_STREAM_CONN_TIMEOUT;
		s_c->from=*from;
		s_c->parent=cs;
	}
	return s_c;
}


#define s_conn_add(s_c) clist_append(&stream_conn_lst, s_c, next, prev)



inline static void s_conn_rm(struct stream_connection* sc)
{
	clist_rm(sc, next, prev);
	pkg_free(sc);
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



void io_listen_loop(struct ctrl_socket* cs_lst)
{
	int max_fd_no;
	char* poll_err;
	int poll_method;
	struct ctrl_socket *cs;
	int type;
	
	clist_init(&stream_conn_lst, next, prev);
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
		if (cs->fd < 0) {
			/* stream address is disabled (handled by dedicated process) */
#ifdef EXTRA_DEBUG
			assert(BRPC_ADDR_TYPE(&cs->addr) == SOCK_STREAM);
#endif
			continue;
		}
		switch (cs->addr.domain) {
			case PF_LOCAL:
				if (cs->p_proto == P_FDPASS) {
					type = F_T_FDPASS;
					break;
				}
				/* no breaks! */
			case PF_INET:
			case PF_INET6:
				switch (cs->addr.socktype) {
					case SOCK_DGRAM:
						type=F_T_CTRL_DGRAM;
						break;
					case SOCK_STREAM:
						type=F_T_CTRL_STREAM;
						break;
					default:
						BUG("unknown control socket transport [%d:%d]\n", 
								cs->addr.domain, cs->addr.socktype);
						goto error;
				}
				break;

#ifdef USE_FIFO
			case PF_FIFO:
				type=F_T_FIFO; /* special */
				cs->data=s_conn_new(cs->fd, cs, 
						/* reuse stream conn */
						(union sockaddr_u *)&cs->addr.sockaddr);
				if (cs->data==0){
					LOG(L_ERR, "ERROR: io_listen_loop: out of memory\n");
					goto error;
				}
				break;
#endif
			default:
				BUG("unknown socket domain %d.\n", cs->addr.domain);
				goto error;
		} /* outermost switch */
		
		DEBUG("io_listen_loop: adding socket %d, type %d, transport"
				" [%d:%d] (%s)\n", cs->fd, type, cs->addr.domain, 
				cs->addr.socktype, CTRLSOCK_URI(cs));
		if (io_watch_add(&io_h, cs->fd, POLLIN, type, cs)<0){
			LOG(L_CRIT, "ERROR: io_listen_loop: init: failed to add"
					"listen socket to the fd list\n");
			goto error;
		}
	}
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
	CRIT("io_listen_loop exiting ...\n");
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
/* TODO: udpate comment above */
static int handle_dgram(struct ctrl_socket* cs)
{
	struct reply_to sndto;
	brpc_t *raw_req;

	sndto.fd = cs->fd;
	sndto.addr = cs->addr;
	raw_req = brpc_recvfrom(cs->fd, &sndto.addr, 0);
	if (! raw_req) {
		ERR("failed to received new datagram (on %s): %s [%d].\n", 
				CTRLSOCK_URI(cs), brpc_strerror(), brpc_errno);
		return -1;
	}
	DEBUG("new datagram received (on %s).\n", CTRLSOCK_URI(cs));
#ifdef USE_FIFO
	if (cs->p_proto==P_FIFO)
		fifo_process((char*)buf, bytes, &bytes_needed, &sh, &saved_state);
	else
#endif
		return (binrpc_run(raw_req, sndto) == BRPC_RUN_SUCCESS) ? 0 : -1;
}

/**
 * Add a socket to a connections list and insert it into IO monitored sockets.
 */
static inline int add_stream_connection(int new_sock, struct ctrl_socket *cs, 
		union sockaddr_u *from)
{
	struct stream_connection* s_conn;
	/* add socket to the list */
	s_conn=s_conn_new(new_sock, cs, from);
	if (s_conn){
		s_conn_add(s_conn);
		io_watch_add(&io_h, s_conn->rds.fd, POLLIN, F_T_READ_STREAM, s_conn);
	}else{
		LOG(L_ERR, "ERROR: io listen: handle_new_connect:"
				" s_conn_new failed\n");
		return -1;
	}
	io_read_connections++;
	return 0;
}

static inline void del_stream_connection(struct stream_connection* s_c,int idx)
{
	struct ctrl_socket *cs;

	io_watch_del(&io_h, s_c->rds.fd, idx, IO_FD_CLOSING);
	close(s_c->rds.fd);
	/* TODO: is have_connection_listener necessary to check?! */
	if (have_connection_listener 
			&& (BRPC_ADDR_TYPE(&s_c->parent->addr) == SOCK_STREAM)) {
		/* stream connection for passed descriptor*/
		/* FIXME: obtain the ref to P_FDPASS cs more efficiently */
		for (cs = ctrl_sock_lst; cs; cs = cs->next) {
			if (cs->p_proto != P_FDPASS)
				continue;
			if (send_all(cs->fd, "", 1) < 1) {
				WARN("failed to signal connection termination to binrpc "
						"connections listener: %s [%d].\n", strerror(errno), 
						errno);
#ifdef EXTRA_DEBUG
				abort();
#endif
			}
			break;
		}
	}
	s_conn_rm(s_c);

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
	size_t connections;
	ssize_t worker = 0 /*4gcc*/;
	struct ctrl_socket *w_cs;

	DEBUG("new incomming connection on %s.\n", CTRLSOCK_URI(cs));
	from_len = cs->addr.addrlen; /* can only be same address length */
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
				CTRLSOCK_URI(cs), errno, strerror(errno));
		goto error;
	}
	ret=1;
	if (have_connection_listener) {
		if ((worker = pt_least_loaded(&connections)) < 0)
			goto close;
	} else {
		connections = io_read_connections;
	}
	if (connections>=MAX_IO_READ_CONNECTIONS){
		LOG(L_ERR, "ERROR: io listen: maximum number of connections"
				" exceeded: %d/%d\n",
				io_read_connections, MAX_IO_READ_CONNECTIONS);
		goto close; /* success because accept was successful */
	}
	if (init_sock_opt(new_sock, cs->addr.domain, cs->addr.socktype)<0){
		LOG(L_ERR, "ERROR: io listen: handle_new_connect:"
				" init_sock_opt failed\n");
		goto close;
	}
	if (have_connection_listener) {
		/* pass socket to worker */
		if (! (w_cs = pt_update_load(worker, /*inc*/1))) {
			BUG("invalid (NULL) child FDPASS control socket.\n");
#ifdef EXTRA_DEBUG
			abort();
#endif
			goto close;
		}
		if (send_fd(w_cs->fd, &cs, sizeof(cs/*ptr!*/), new_sock)<sizeof(cs)) {
			ERR("failed to pass connection descriptor: %s [%d].\n", 
					strerror(errno), errno);
			pt_update_load(worker, /*dec*/-1);
			goto close;
		}
		close(new_sock); /* no longer needed in this process */
		DEBUG("new connection dispatched to binrpc worker PID#%d.\n", 
				pt_pid(worker));
	} else {
#if 0
		/* TODO: XXX: FIXME: used uninitialized! */
		//if (add_stream_connection(new_sock, w_cs, &from) < 0)
#endif
		if (add_stream_connection(new_sock, cs, &from) < 0)
			goto close;
	}
	DEBUG("handle_stream read: new connection on %s\n", CTRLSOCK_URI(cs));
skip:
	return ret;
close:
	if (0 <= new_sock)
		close(new_sock);
	return ret;
error:
	return -1;
}



static int handle_read(struct stream_connection* s_c, int idx)
{
	uint8_t *buff;
	size_t pkt_len;
	struct reply_to sndto;
	brpc_t *raw_req;
#ifdef USE_FIFO
	ssize_t bytes_processed, bytes_needed;
#endif

	memset((char *)&sndto, 0, sizeof(struct reply_to));
	sndto.fd = s_c->rds.fd;

	DEBUG("new packet (fragment) received on %s.\n", CTRLSOCK_URI(s_c->parent));
	brpc_errno = 0;
	if (! brpc_strd_read(&s_c->rds)) {
		switch (brpc_errno) {
			case 0:
				DEBUG("connection FD:%d (on %s) closed%s.\n", s_c->rds.fd, 
						CTRLSOCK_URI(s_c->parent), 
						s_c->rds.offset ? " (in middle of packet)" :
						" (cleanly)");
				goto close_connection;
			case EAGAIN:
#if EAGAIN != EWOULDBLOCK
			case EWOULDBLOCK:
#endif
				goto no_read;
			default:
				ERR("while reading connection %d (with %s): %s [%d].\n",
						s_c->rds.fd, CTRLSOCK_URI(s_c->parent), 
						brpc_strerror(), brpc_errno);
				goto error_read;
		}
	} else {
#ifdef USE_FIFO
		if (s_c->parent->p_proto==P_FIFO)
			bytes_processed=fifo_process((char*)r->proc, (int)(r->end-r->proc),
										&bytes_needed, &sh, &s_c->saved_state);
		else
#endif
		{
			brpc_errno = 0;
			while ((buff = brpc_strd_wirepkt(&s_c->rds, &pkt_len))) {
				DEBUG("new packet buffer of size %zd.\n", pkt_len);
				if (! (raw_req = brpc_raw(buff, pkt_len))) {
					ERR("BINRPC raw request reception failed: %s [%d]; RPC "
							"dropped.\n", brpc_strerror(), brpc_errno);
					goto skip;
				}
				switch (binrpc_run(raw_req, sndto)) {
					case BRPC_RUN_ABORT:
						WARN("connection fatal error while serving on %s.\n",
								CTRLSOCK_URI(s_c->parent));
						goto close_connection;
					case BRPC_RUN_VOID:
						WARN("nothing sent back for request on %s.\n", 
								CTRLSOCK_URI(s_c->parent));
						break;
					case BRPC_RUN_SUCCESS:
						DEBUG("succes serving on %s.\n", 
								CTRLSOCK_URI(s_c->parent));
						break;
					default:
						BUG("invalid context state while serving on %s.\n", 
								CTRLSOCK_URI(s_c->parent));
						goto close_connection;
				}
			}
			switch (brpc_errno) {
				case 0:
					DEBUG("partial read on %s.\n", CTRLSOCK_URI(s_c->parent));
					goto skip;
				case EMSGSIZE:
					/* TODO: read pending bytes to save the connection */
					ERR("packet to receive on connection %d (with %s) is too"
							" large: shutding down connection.\n", 
							s_c->rds.fd, CTRLSOCK_URI(s_c->parent));
					goto close_connection;
			}
		}
	}

skip:
	/* everything went fine, we just have to read more */
	s_c->expire=get_ticks_raw()+IO_STREAM_CONN_TIMEOUT; /* update timeout*/
	return 1;
	
no_read:
	/* false alarm */
	return 0;
/* TODO: what's the subtle diffrence between the two? the while's till >0.. */
close_connection:
	del_stream_connection(s_c, idx);
	return 0;
error_read:
	del_stream_connection(s_c, idx);
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
		LOG(L_ERR, "ERROR: handle_fifo_read: buffer overrun\n");
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
		DEBUG("handle_fifo_read: eof on %s\n", cs->name);
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

static int handle_fd_pass(struct ctrl_socket* cs)
{
	char rcvbuff[MAX_IO_READ_CONNECTIONS];
	ssize_t rcvd;
	int i, new_sock;
	struct ctrl_socket *rcvd_cs;
	union sockaddr_u from;
	socklen_t from_len;

	if (cs->child != 0) { /* binrpc connection listener */
		do {
			rcvd = read(cs->fd, rcvbuff, sizeof(rcvbuff));
			if (0 < rcvd) {
				DEBUG("binrpc child PID#%d released %zd connection(s).\n",
						cs->child, rcvd);
				for (i = 0; i < rcvd; i ++)
					pt_update_load(pt_worker(cs->child), /*decrease load*/-1);
			} else if (rcvd < 0) {
				if (errno == EINTR) {
					continue;
				} else {
					ERR("failed to read binrpc child data: %s [%d].\n",
							strerror(errno), errno);
					goto fail;
				}
			} else { /* reset */
				ERR("binrpc worker disconnected!\n");
				goto fail;
			}
			break;
		} while (1);
	} else { /* binrpc worker */
		new_sock = -1;
		if (receive_fd(cs->fd, &rcvd_cs, sizeof(rcvd_cs/*ptr!*/), &new_sock, 
				MSG_WAITALL) < sizeof(rcvd_cs)) {
			ERR("failed to receive connection descriptor: %s [%d].\n", 
					strerror(errno), errno);
			goto fail;
		}
		from_len = rcvd_cs->addr.addrlen; /* can only be same address length */
		if (getpeername(new_sock, &from.sa_in.s, &from_len) < 0) {
			WARN("failed to get peer's address from passed connection"
					" descriptor: %s [%d].\n", strerror(errno), errno);
			/* TODO: check if fatal */
		}
		if (add_stream_connection(new_sock, rcvd_cs, &from) < 0)
			goto close;
	}
	return 0;
fail:
	/* fail safe */
	exit(-1);
close:
	if (0 <= new_sock)
		close(new_sock);
	return 1;
}

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
	
	switch(fm->type){
		case F_T_FDPASS:
			ret = handle_fd_pass((struct ctrl_socket*)fm->data);
			break;
		case F_T_CTRL_DGRAM:
#if 0
			ret=handle_ctrl_dgram((struct ctrl_socket*)fm->data);
#else
			ret=handle_dgram((struct ctrl_socket*)fm->data);
#endif
			break;
		case F_T_CTRL_STREAM:
			ret=handle_new_connect((struct ctrl_socket*)fm->data);
			break;
		case F_T_READ_STREAM:
#if 0
			ret=handle_stream_read((struct stream_connection*)fm->data, idx);
#else
			ret=handle_read((struct stream_connection*)fm->data, idx);
#endif
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
	union sockaddr_union *sa_in;
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
								socket_proto_name(&sc->parent->addr));
		switch (sc->parent->addr.domain) {
			case PF_INET:
			case PF_INET6:
				su2ip_addr(&ip, &sc->from.sa_in);
				port=su_getport(&sc->from.sa_in);
				rpc->add(ctx, "ss", ip_addr2a(&ip), int2str(port, 0));
				sa_in = (union sockaddr_union *)&sc->parent->addr.sockaddr;
				su2ip_addr(&ip, sa_in);
				port=su_getport(sa_in);
				rpc->add(ctx, "ss", ip_addr2a(&ip), int2str(port, 0));
				break;
			case PF_LOCAL:
				/* TODO: is this buggy in ctl? */
				rpc->add(ctx, "ss", "<anonymous unix socket>", "" );
				break;
#ifdef USE_FIFO
			case PF_FIFO:
#endif		
				/* TODO: just use CTRLSOCK_URI */
				rpc->add(ctx, "ss", sc->parent->name, "");
				break;
			default:
				BUG("invalid socket domain %d.\n", sc->parent->addr.domain);
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
