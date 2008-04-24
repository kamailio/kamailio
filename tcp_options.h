/* 
 * $Id$
 * 
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
/*
 * tcp options
 *
 * History:
 * --------
 *  2007-11-28  created by andrei
 */

#ifndef tcp_options_h
#define tcp_options_h


#ifndef NO_TCP_BUF_WRITE
#define TCP_BUF_WRITE /* enabled buffered writing */
#endif 

#if !defined(NO_TCP_CONNECT_WAIT) && defined(TCP_BUF_WRITE)
#define TCP_CONNECT_WAIT /* enable pending connects support */
#endif

#if defined(TCP_CONNECT_WAIT) && !defined(TCP_BUF_WRITE)
/* check for impossible configuration: TCP_CONNECT_WAIT w/o TCP_BUF_WRITE */
#warning "disabling TCP_CONNECT_WAIT because TCP_BUF_WRITE is not defined"
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


struct tcp_cfg_options{
	/* ser tcp options */
	int fd_cache; /* on /off */
	/* tcp buf. write options */
	int tcp_buf_write; /* on / off */
	int tcp_connect_wait; /* on / off, depends on tcp_buf_write */
	unsigned int tcpconn_wq_max; /* maximum queue len per connection */
	unsigned int tcp_wq_max; /* maximum overall queued bytes */
	unsigned int tcp_wq_timeout;      /* timeout for queue writes */

	/* tcp socket options */
	int defer_accept; /* on / off */
	int delayed_ack; /* delay ack on connect */ 
	int syncnt;     /* numbers of SYNs retrs. before giving up connecting */
	int linger2;    /* lifetime of orphaned  FIN_WAIT2 state sockets */
	int keepalive;  /* on /off */
	int keepidle;   /* idle time (s) before tcp starts sending keepalives */
	int keepintvl;  /* interval between keep alives */
	int keepcnt;    /* maximum no. of keepalives before giving up */
	int crlf_ping;  /* on/off - reply to double CRLF keepalives */
};


extern struct tcp_cfg_options tcp_options;

void init_tcp_options();
void tcp_options_check();
void tcp_options_get(struct tcp_cfg_options* t);

#endif /* tcp_options_h */
