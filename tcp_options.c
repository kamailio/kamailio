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

#include "tcp_options.h"
#include "dprint.h"


struct tcp_cfg_options tcp_options;


/* set defaults */
void init_tcp_options()
{

#ifdef TCP_FD_CACHE
	tcp_options.fd_cache=1;
#endif
#ifdef HAVE_SO_KEEPALIVE
	tcp_options.keepalive=1;
#endif
/*
#if defined HAVE_TCP_DEFER_ACCEPT || defined HAVE_TCP_ACCEPT_FILTER
	tcp_options.defer_accept=1;
#endif
*/
#ifdef HAVE_TCP_QUICKACK
	tcp_options.delayed_ack=1;
#endif
}



#define W_OPT_NC(option) \
	if (tcp_options.option){\
		WARN("tcp_options: tcp_" ##option \
				"cannot be enabled (recompile needed)\n"); \
	}



#define W_OPT_NS(option) \
	if (tcp_options.option){\
		WARN("tcp_options: tcp_" ##option \
				"cannot be enabled (no OS support)\n"); \
	}


/* checks & warns if some tcp_option cannot be enabled */
void tcp_options_check()
{
#ifndef TCP_FD_CACHE
	W_OPT_NC(defer_accept);
#endif

#if ! defined HAVE_TCP_DEFER_ACCEPT && ! defined HAVE_TCP_ACCEPT_FILTER
	W_OPT_NS(defer_accept);
#endif
#ifndef HAVE_TCP_SYNCNT
	W_OPT_NS(syncnt);
#endif
#ifndef HAVE_TCP_LINGER2
	W_OPT_NS(linger2);
#endif
#ifndef HAVE_TCP_KEEPINTVL
	W_OPT_NS(keepintvl);
#endif
#ifndef HAVE_TCP_KEEPIDLE
	W_OPT_NS(keepidle);
#endif
#ifndef HAVE_TCP_KEEPCNT
	W_OPT_NS(keepcnt);
#endif
	if (tcp_options.keepintvl || tcp_options.keepidle || tcp_options.keepcnt){
		tcp_options.keepalive=1; /* force on */
	}
#ifndef HAVE_SO_KEEPALIVE
	W_OPT_NS(keepalive);
#endif
#ifndef HAVE_TCP_QUICKACK
	W_OPT_NS(delayed_ack);
#endif
}



void tcp_options_get(struct tcp_cfg_options* t)
{
	*t=tcp_options;
}
