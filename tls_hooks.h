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
 * tls hooks for modules
 *
 * History:
 * --------
 *  2007-02-09  created by andrei
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
	int  (*read)(struct tcp_connection* c);
	int (*blocking_write)(struct tcp_connection* c, int fd, const char* buf,
							unsigned int len);
	int  (*on_tcpconn_init)(struct tcp_connection *c, int sock);
	void (*tcpconn_clean)(struct tcp_connection* c);
	void (*tcpconn_close)(struct tcp_connection*c , int fd);
	/* checks if a tls connection is fully established before a read, and if 
	 * not it runs tls_accept() or tls_connect() as needed
	 * (tls_accept and tls_connect are deferred to the "reader" process for
	 *  performance reasons) */
	int (*fix_read_con)(struct tcp_connection* c);
	
	/* per listening socket init, called on ser startup (after modules,
	 *  process table, init() and udp socket initialization)*/
	int (*init_si)(struct socket_info* si);
	/* generic init function (called at ser init, after module initialization
	 *  and process table creation)*/
	int (*init)();
	/* destroy function, called after the modules are destroyed, and 
	 * after  destroy_tcp() */
	void (*destroy)();
};


struct tls_hooks tls_hook;

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
#define tls_blocking_write(c, fd, buf, len) \
	tls_hook_call(blocking_write, -1, (c), (fd), (buf), (len))
#define tls_close(conn, fd)		tls_hook_call_v(tcpconn_close, (conn), (fd))
#define tls_read(c)				tls_hook_call(read, -1, (c))
#define tls_fix_read_conn(c)	tls_hook_call(fix_read_con, -1, (c))

int register_tls_hooks(struct tls_hooks* h);

#endif /* TLS_HOOKS */
#endif
