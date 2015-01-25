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

/**
 * @file
 * @brief Kamailio TLS support :: TLS hooks for modules
 * @ingroup tls
 * Module: @ref tls
 */


#ifndef _tls_hooks_h
#define _tls_hooks_h

#ifdef TLS_HOOKS

#ifndef USE_TLS
#error "USE_TLS required and not defined (please compile with make \
	TLS_HOOKS=1)"
#endif

#ifdef CORE_TLS
#error "Conflict: CORE_TLS and TLS_HOOKS cannot be defined in the same time"
#endif

#include "tcp_conn.h"



struct tls_hooks{
	/* read using tls (should use tcp internal read functions to
	   get the data from the connection) */
	int  (*read)(struct tcp_connection* c, int* flags);
	/* process data for sending. Should replace pbuf & plen with
	   an internal buffer containing the tls records. If it was not able
	   to process the whole pbuf, it should set (rest_buf, rest_len) to
	   the remaining unprocessed part, else they must be set to 0.
	   send_flags are passed as a pointer and they can also be changed
	   (e.g. reset a FORCE_CLOSE flag if there is internal queued data
	    waiting to be written).
	   If rest_len or rest_buf are not 0 the call will be repeated after the
	   contents of pbuf is sent, with (rest_buf, rest_len) as input.
	   Should return *plen (if >=0).
	   If it returns < 0 => error (tcp connection will be closed).
	*/
	int (*encode)(struct tcp_connection* c,
					const char** pbuf, unsigned int* plen,
					const char** rest_buf, unsigned int* rest_len,
					snd_flags_t* send_flags);
	int  (*on_tcpconn_init)(struct tcp_connection *c, int sock);
	void (*tcpconn_clean)(struct tcp_connection* c);
	void (*tcpconn_close)(struct tcp_connection*c , int fd);
	
	/* per listening socket init, called on kamailio startup (after modules,
	 *  process table, init() and udp socket initialization)*/
	int (*init_si)(struct socket_info* si);
	/* generic init function (called at kamailio init, after module initialization
	 *  and process table creation)*/
	int (*init)(void);
	/* destroy function, called after the modules are destroyed, and 
	 * after  destroy_tcp() */
	void (*destroy)(void);
	/* generic pre-init function (called at kamailio start, before module
	 * initialization (after modparams) */
	int (*pre_init)(void);
};


extern struct tls_hooks tls_hook;

#ifdef __SUNPRO_C
	#define tls_hook_call(name, ret_not_set, ...) \
		((tls_hook.name)?(tls_hook.name(__VA_ARGS__)): (ret_not_set))
	#define tls_hook_call_v(name, __VA_ARGS__) \
		do{ \
			if (tls_hook.name) tls_hook.name(__VA_ARGS__); \
		}while(0)
#else
	#define tls_hook_call(name, ret_not_set, args...) \
		((tls_hook.name)?(tls_hook.name(args)): (ret_not_set))
	#define tls_hook_call_v(name, args...) \
		do{ \
			if (tls_hook.name) tls_hook.name(args); \
		}while(0)
#endif

/* hooks */

#define tls_tcpconn_init(c, s)	tls_hook_call(on_tcpconn_init, 0, (c), (s))
#define tls_tcpconn_clean(c)	tls_hook_call_v(tcpconn_clean, (c))
#define tls_encode(c, pbuf, plen, rbuf, rlen, sflags) \
	tls_hook_call(encode, -1, (c), (pbuf), (plen), (rbuf), (rlen), (sflags))
#define tls_close(conn, fd)		tls_hook_call_v(tcpconn_close, (conn), (fd))
#define tls_read(c, flags)				tls_hook_call(read, -1, (c), (flags))

int register_tls_hooks(struct tls_hooks* h);

#endif /* TLS_HOOKS */
#endif
