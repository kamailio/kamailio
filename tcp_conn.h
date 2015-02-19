/*
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
 *
 */

#ifndef _tcp_conn_h
#define _tcp_conn_h

#include "tcp_init.h"
#include "tcp_options.h"

#include "ip_addr.h"
#include "locking.h"
#include "atomic_ops.h"
#include "timer_ticks.h"
#include "timer.h"

/* maximum number of port aliases x search wildcard possibilities */
#define TCP_CON_MAX_ALIASES (4*3) 

#define TCP_CHILD_TIMEOUT 5 /* after 5 seconds, the child "returns" 
							 the connection to the tcp master process */
#define TCP_MAIN_SELECT_TIMEOUT 5 /* how often "tcp main" checks for timeout*/
#define TCP_CHILD_SELECT_TIMEOUT 2 /* the same as above but for children */


/* tcp connection flags */
#define F_CONN_READ_W       2 /* watched for READ ev. in main */
#define F_CONN_WRITE_W      4 /* watched for WRITE (main) */
#define F_CONN_READER       8 /* handled by a tcp reader */
#define F_CONN_HASHED      16 /* in tcp_main hash */
#define F_CONN_FD_CLOSED   32 /* fd was already closed */
#define F_CONN_PENDING     64 /* pending connect  (fd not known yet in main) */
#define F_CONN_MAIN_TIMER 128 /* timer active in the tcp_main process */
#define F_CONN_EOF_SEEN   256 /* FIN or RST have been received */
#define F_CONN_FORCE_EOF  512 /* act as if an EOF was received */
#define F_CONN_OOB_DATA  1024 /* out of band data on the connection */
#define F_CONN_WR_ERROR  2048 /* write error on the fd */
#define F_CONN_WANTS_RD  4096  /* conn. should be watched for READ */
#define F_CONN_WANTS_WR  8192  /* conn. should be watched for WRITE */
#define F_CONN_PASSIVE  16384 /* conn. created via accept() and not connect()*/

#ifndef NO_READ_HTTP11
#define READ_HTTP11
#endif

#ifndef NO_READ_MSRP
#define READ_MSRP
#endif

#ifndef NO_READ_WS
#define READ_WS
#endif

enum tcp_req_errors {	TCP_REQ_INIT, TCP_REQ_OK, TCP_READ_ERROR,
						TCP_REQ_OVERRUN, TCP_REQ_BAD_LEN };
enum tcp_req_states {	H_SKIP_EMPTY, H_SKIP_EMPTY_CR_FOUND,
		H_SKIP_EMPTY_CRLF_FOUND, H_SKIP_EMPTY_CRLFCR_FOUND,
		H_SKIP, H_LF, H_LFCR,  H_BODY, H_STARTWS,
		H_CONT_LEN1, H_CONT_LEN2, H_CONT_LEN3, H_CONT_LEN4, H_CONT_LEN5,
		H_CONT_LEN6, H_CONT_LEN7, H_CONT_LEN8, H_CONT_LEN9, H_CONT_LEN10,
		H_CONT_LEN11, H_CONT_LEN12, H_CONT_LEN13, H_L_COLON, 
		H_CONT_LEN_BODY, H_CONT_LEN_BODY_PARSE,
		H_STUN_MSG, H_STUN_READ_BODY, H_STUN_FP, H_STUN_END, H_PING_CRLF
#ifdef READ_HTTP11
		, H_HTTP11_CHUNK_START, H_HTTP11_CHUNK_SIZE,
		H_HTTP11_CHUNK_BODY, H_HTTP11_CHUNK_END, H_HTTP11_CHUNK_FINISH
#endif
#ifdef READ_MSRP
		, H_MSRP_BODY, H_MSRP_BODY_LF, H_MSRP_BODY_END, H_MSRP_FINISH
#endif
	};

enum tcp_conn_states { S_CONN_ERROR=-2, S_CONN_BAD=-1,
						S_CONN_OK=0, /* established (write or read) */
						S_CONN_INIT, /* initial state (invalid) */
						S_CONN_EOF,
						S_CONN_ACCEPT, S_CONN_CONNECT
					};


/* fd communication commands */
enum conn_cmds {
	CONN_DESTROY=-3 /* destroy connection & auto-dec. refcnt */,
	CONN_ERROR=-2   /* error on connection & auto-dec. refcnt */,
	CONN_EOF=-1     /* eof received or conn. closed & auto-dec refcnt */,
	CONN_NOP=0      /* do-nothing (invalid for tcp_main) */,
	CONN_RELEASE    /* release a connection from tcp_read back into tcp_main
					   & auto-dec refcnt */,
	CONN_GET_FD     /* request a fd from tcp_main */,
	CONN_NEW        /* update/set a fd int a new tcp connection; refcnts are
					  not touched */,
	CONN_QUEUED_WRITE /* new write queue: start watching the fd for write &
						 auto-dec refcnt */,
	CONN_NEW_PENDING_WRITE /* like CONN_NEW+CONN_QUEUED_WRITE: set fd and
							  start watching it for write (write queue
							  non-empty); refcnts are not touced */,
	CONN_NEW_COMPLETE  /* like CONN_NEW_PENDING_WRITE, but there is no
						  pending write (the write queue might be empty) */
};
/* CONN_RELEASE, EOF, ERROR, DESTROY can be used by "reader" processes
 * CONN_GET_FD, CONN_NEW*, CONN_QUEUED_WRITE only by writers */

struct tcp_req{
	struct tcp_req* next;
	/* sockaddr ? */
	char* buf; /* bytes read so far (+0-terminator)*/
	char* start; /* where the message starts, after all the empty lines are
					skipped*/
	char* pos; /* current position in buf */
	char* parsed; /* last parsed position */
	char* body; /* body position */
	unsigned int b_size; /* buffer size-1 (extra space for 0-term)*/
	int content_len;
#ifdef READ_HTTP11
	int chunk_size;
#endif
	unsigned short flags; /* F_TCP_REQ_HAS_CLEN | F_TCP_REQ_COMPLETE */
	int bytes_to_go; /* how many bytes we have still to read from the body*/
	enum tcp_req_errors error;
	enum tcp_req_states state;
};

/* tcp_req flags */
#define F_TCP_REQ_HAS_CLEN 1
#define F_TCP_REQ_COMPLETE 2
#ifdef READ_HTTP11
#define F_TCP_REQ_BCHUNKED 4
#endif
#ifdef READ_MSRP
#define F_TCP_REQ_MSRP_NO     8
#define F_TCP_REQ_MSRP_FRAME  16
#define F_TCP_REQ_MSRP_BODY   32
#endif

#define TCP_REQ_HAS_CLEN(tr)  ((tr)->flags & F_TCP_REQ_HAS_CLEN)
#define TCP_REQ_COMPLETE(tr)  ((tr)->flags & F_TCP_REQ_COMPLETE)
#ifdef READ_HTTP11
#define TCP_REQ_BCHUNKED(tr)  ((tr)->flags & F_TCP_REQ_BCHUNKED)
#endif


struct tcp_connection;

/* tcp port alias structure */
struct tcp_conn_alias{
	struct tcp_connection* parent;
	struct tcp_conn_alias* next;
	struct tcp_conn_alias* prev;
	unsigned short port; /* alias port */
	unsigned short hash; /* hash index in the address hash */
};


#ifdef TCP_ASYNC
	struct tcp_wbuffer{
		struct tcp_wbuffer* next;
		unsigned int b_size;
		char buf[1];
	};

	struct tcp_wbuffer_queue{
		struct tcp_wbuffer* first;
		struct tcp_wbuffer* last;
		ticks_t wr_timeout; /* write timeout*/
		unsigned int queued; /* total size */
		unsigned int offset; /* offset in the first wbuffer were data
								starts */
		unsigned int last_used; /* how much of the last buffer is used */
	};
#endif


struct tcp_connection{
	int s; /*socket, used by "tcp main" */
	int fd; /* used only by "children", don't modify it! private data! */
	gen_lock_t write_lock;
	int id; /* id (unique!) used to retrieve a specific connection when
	           reply-ing*/
	int reader_pid; /* pid of the active reader process */
	struct receive_info rcv; /* src & dst ip, ports, proto a.s.o*/
	struct tcp_req req; /* request data */
	atomic_t refcnt;
	enum sip_protos type; /* PROTO_TCP or a protocol over it, e.g. TLS */
	unsigned short flags; /* connection related flags */
	snd_flags_t send_flags; /* special send flags */
	enum tcp_conn_states state; /* connection state */
	void* extra_data; /* extra data associated to the connection, 0 for tcp*/
	struct timer_ln timer;
	ticks_t timeout;/* connection timeout, after this it will be removed*/
	ticks_t lifetime;/* connection lifetime */
	unsigned id_hash; /* hash index in the id_hash */
	struct tcp_connection* id_next; /* next, prev in id hash table */
	struct tcp_connection* id_prev;
	struct tcp_connection* c_next; /* child next prev (use locally) */
	struct tcp_connection* c_prev;
	struct tcp_conn_alias con_aliases[TCP_CON_MAX_ALIASES];
	int aliases; /* aliases number, at least 1 */
#ifdef TCP_ASYNC
	struct tcp_wbuffer_queue wbuf_q;
#endif
};


/* helper macros */

#define tcpconn_set_send_flags(c, snd_flags) \
	SND_FLAGS_OR(&(c)->send_flags, &(c)->send_flags, &(snd_flags))

#define tcpconn_close_after_send(c)	((c)->send_flags.f & SND_F_CON_CLOSE)

#define TCP_RCV_INFO(c) (&(c)->rcv)

#define TCP_RCV_LADDR(r) (&((r).dst_ip))
#define TCP_RCV_LPORT(r) ((r).dst_port)
#define TCP_RCV_PADDR(r)  (&((r).src_ip))
#define TCP_RCV_PPORT(r)  ((r).src_port)
#define TCP_RCV_PSU(r)   (&(r).src_su)
#define TCP_RCV_SOCK_INFO(r)  ((r).bind_address)
#define TCP_RCV_PROTO(r)      ((r).proto)
#ifdef USE_COMP
#define TCP_RCV_COMP(r)       ((r).comp)
#else
#define TCP_RCV_COMP(r)  0
#endif /* USE_COMP */

#define TCP_LADDR(c) TCP_RCV_LADDR(c->rcv)
#define TCP_LPORT(c) TCP_RCV_LPORT(c->rcv)
#define TCP_PADDR(c) TCP_RCV_PADDR(c->rcv)
#define TCP_PPORT(c) TCP_RCV_PPORT(c->rcv)
#define TCP_PSU(c)   TCP_RCV_PSU(c->rcv)
#define TCP_SOCK_INFO(c) TCP_RCV_SOCK_INFO(c->rcv)
#define TCP_PROTO(c) TCP_RCV_PROTO(c->rcv)
#define TCP_COMP(c) TCP_RCV_COMP(c->rcv)



#define tcpconn_ref(c) atomic_inc(&((c)->refcnt))
#define tcpconn_put(c) atomic_dec_and_test(&((c)->refcnt))


#define init_tcp_req( r, rd_buf, rd_buf_size) \
	do{ \
		memset( (r), 0, sizeof(struct tcp_req)); \
		(r)->buf=(rd_buf) ;\
		(r)->b_size=(rd_buf_size)-1; /* space for 0 term. */ \
		(r)->parsed=(r)->pos=(r)->start=(r)->buf; \
		(r)->error=TCP_REQ_OK;\
		(r)->state=H_SKIP_EMPTY; \
	}while(0)


/* add a tcpconn to a list*/
/* list head, new element, next member, prev member */
#define tcpconn_listadd(head, c, next, prev) \
	do{ \
		/* add it at the begining of the list*/ \
		(c)->next=(head); \
		(c)->prev=0; \
		if ((head)) (head)->prev=(c); \
		(head)=(c); \
	} while(0)


/* remove a tcpconn from a list*/
#define tcpconn_listrm(head, c, next, prev) \
	do{ \
		if ((head)==(c)) (head)=(c)->next; \
		if ((c)->next) (c)->next->prev=(c)->prev; \
		if ((c)->prev) (c)->prev->next=(c)->next; \
	}while(0)


#define TCPCONN_LOCK lock_get(tcpconn_lock);
#define TCPCONN_UNLOCK lock_release(tcpconn_lock);

#define TCP_ALIAS_HASH_SIZE 4096
#define TCP_ID_HASH_SIZE 1024

/* hash (dst_ip, dst_port, local_ip, local_port) */
static inline unsigned tcp_addr_hash(	struct ip_addr* ip, 
										unsigned short port,
										struct ip_addr* l_ip,
										unsigned short l_port)
{
	unsigned h;

	if(ip->len==4)
		h=(ip->u.addr32[0]^port)^(l_ip->u.addr32[0]^l_port);
	else if (ip->len==16) 
		h= (ip->u.addr32[0]^ip->u.addr32[1]^ip->u.addr32[2]^
				ip->u.addr32[3]^port) ^
			(l_ip->u.addr32[0]^l_ip->u.addr32[1]^l_ip->u.addr32[2]^
				l_ip->u.addr32[3]^l_port);
	else{
		LM_CRIT("bad len %d for an ip address\n", ip->len);
		return 0;
	}
	/* make sure the first bits are influenced by all 32
	 * (the first log2(TCP_ALIAS_HASH_SIZE) bits should be a mix of all
	 *  32)*/
	h ^= h>>17;
	h ^= h>>7;
	return h & (TCP_ALIAS_HASH_SIZE-1);
}

#define tcp_id_hash(id) (id&(TCP_ID_HASH_SIZE-1))

struct tcp_connection* tcpconn_get(int id, struct ip_addr* ip, int port,
									union sockaddr_union* local_addr,
									ticks_t timeout);

typedef struct tcp_event_info {
	int type;
	char *buf;
	unsigned int len;
	struct receive_info *rcv;
	struct tcp_connection *con;
} tcp_event_info_t;

typedef struct ws_event_info {
	int type;
	char *buf;
	unsigned int len;
	int id;
} ws_event_info_t;

#endif


