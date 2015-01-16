/* 
 * Copyright (C) 2007 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*! \file
 * \brief tcp options
 *
 */

#ifndef tcp_options_h
#define tcp_options_h

#ifdef USE_TCP

#ifndef NO_TCP_ASYNC
#define TCP_ASYNC /* enabled async mode */
#endif

#if !defined(NO_TCP_CONNECT_WAIT) && defined(TCP_ASYNC)
#define TCP_CONNECT_WAIT /* enable pending connects support */
#endif

#if defined(TCP_CONNECT_WAIT) && !defined(TCP_ASYNC)
/* check for impossible configuration: TCP_CONNECT_WAIT w/o TCP_ASYNC */
#warning "disabling TCP_CONNECT_WAIT because TCP_ASYNC is not defined"
#undef TCP_CONNECT_WAIT
#endif

#ifndef NO_TCP_FD_CACHE
#define TCP_FD_CACHE /* enable fd caching */
#endif



/* defer accept */
#ifndef  NO_TCP_DEFER_ACCEPT
#ifdef __OS_linux
#define HAVE_TCP_DEFER_ACCEPT
#elif defined __OS_freebsd
#define HAVE_TCP_ACCEPT_FILTER
#endif /* __OS_ */
#endif /* NO_TCP_DEFER_ACCEPT */


/* syn count */
#ifndef NO_TCP_SYNCNT
#ifdef __OS_linux
#define HAVE_TCP_SYNCNT
#endif /* __OS_*/
#endif /* NO_TCP_SYNCNT */

/* tcp linger2 */
#ifndef NO_TCP_LINGER2
#ifdef __OS_linux
#define HAVE_TCP_LINGER2
#endif /* __OS_ */
#endif /* NO_TCP_LINGER2 */

/* keepalive */
#ifndef NO_TCP_KEEPALIVE
#define HAVE_SO_KEEPALIVE
#endif /* NO_TCP_KEEPALIVE */

/* keepintvl */
#ifndef NO_TCP_KEEPINTVL
#ifdef __OS_linux
#define HAVE_TCP_KEEPINTVL
#endif /* __OS_ */
#endif /* NO_TCP_KEEPIDLE */

/* keepidle */
#ifndef NO_TCP_KEEPIDLE
#ifdef __OS_linux
#define HAVE_TCP_KEEPIDLE
#endif /* __OS_*/
#endif /* NO_TCP_KEEPIDLE */


/* keepcnt */
#ifndef NO_TCP_KEEPCNT
#ifdef __OS_linux
#define HAVE_TCP_KEEPCNT
#endif /* __OS_ */
#endif /* NO_TCP_KEEPCNT */


/* delayed ack (quick_ack) */
#ifndef NO_TCP_QUICKACK
#ifdef __OS_linux
#define HAVE_TCP_QUICKACK
#endif /* __OS_ */
#endif /* NO_TCP_QUICKACK */

#endif /* USE_TCP */

struct cfg_group_tcp{
	/* ser tcp options, low level */
	int connect_timeout_s; /* in s */
	int send_timeout; /* in ticks (s fixed to ticks) */
	int con_lifetime; /* in ticks (s fixed to ticks) */
	int max_connections; /* max tcp connections (includes tls connections) */
	int max_tls_connections; /* max tls connections */
	int no_connect; /* do not open any new tcp connection (but accept them) */
	int fd_cache; /* on /off */
	/* tcp async options */
	int async; /* on / off */
	int tcp_connect_wait; /* on / off, depends on async */
	unsigned int tcpconn_wq_max; /* maximum queue len per connection */
	unsigned int tcp_wq_max; /* maximum overall queued bytes */

	/* tcp socket options */
	int defer_accept; /* on / off */
	int delayed_ack; /* delay ack on connect */ 
	int syncnt;     /* numbers of SYNs retrs. before giving up connecting */
	int linger2;    /* lifetime of orphaned  FIN_WAIT2 state sockets */
	int keepalive;  /* on /off */
	int keepidle;   /* idle time (s) before tcp starts sending keepalives */
	int keepintvl;  /* interval between keep alives */
	int keepcnt;    /* maximum no. of keepalives before giving up */
	
	/* other options */
	int crlf_ping;  /* on/off - reply to double CRLF keepalives */
	int accept_aliases;
	int alias_flags;
	int new_conn_alias_flags;
	int accept_no_cl;  /* on/off - accept messages without content-length */

	/* internal, "fixed" vars */
	unsigned int rd_buf_size; /* read buffer size (should be > max. datagram)*/
	unsigned int wq_blk_size; /* async write block size (debugging use) */
};

extern struct cfg_group_tcp tcp_default_cfg;

/* tcp config handle*/
extern void* tcp_cfg;


void init_tcp_options(void);
void tcp_options_check(void);
int tcp_register_cfg(void);
void tcp_options_get(struct cfg_group_tcp* t);

#ifdef USE_TCP
int tcp_set_clone_rcvbuf(int v);
#endif /* USE_TCP */

#endif /* tcp_options_h */
