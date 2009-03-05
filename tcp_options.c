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
 *  2009-03-05  use cfg framework (andrei)
 */

#include "tcp_options.h"
#include "dprint.h"
#include "globals.h"
#include "timer_ticks.h"
#include "cfg/cfg.h"



/* default/initial values for tcp config options
   NOTE: all the options are initialized in init_tcp_options()
   depending on compile time defines */
struct cfg_group_tcp tcp_default_cfg;
#if 0
{
	1, /* fd_cache, default on */
	/* tcp async options */
	0, /* tcp_buf_write / tcp_async, default off */
	1, /* tcp_connect_wait - depends on tcp_async */
	32*1024, /* tcpconn_wq_max - max. write queue len per connection (32k) */
	10*1024*1024, /* tcp_wq_max - max.  overall queued bytes  (10MB)*/
	S_TO_TICKS(tcp_send_timeout), /* tcp_wq_timeout - timeout for queued 
									 writes, depends on tcp_send_timeout */
	/* tcp socket options */
	0, /* defer_accept - on/off*/
	1, /* delayed_ack - delay ack on connect (on/off)*/
	0, /* syncnt - numbers of SYNs retrs. before giving up (0 = OS default) */
	0, /* linger2 - lifetime of orphaned FIN_WAIT2 sockets (0 = OS default)*/
	1, /* keepalive - on/off */
	0, /* keepidle - idle time (s) before tcp starts sending keepalives */
	0, /* keepintvl - interval between keep alives (0 = OS default) */
	0, /* keepcnt - maximum no. of keepalives (0 = OS default)*/
	
	/* other options */
	1 /* crlf_ping - respond to double CRLF ping/keepalive (on/off) */
	
};
#endif



/* cfg_group_tcp description (for the config framework)*/
static cfg_def_t tcp_cfg_def[] = {
	/*   name        , type |input type| chg type, min, max, fixup, proc. cbk 
	      description */
	{ "fd_cache",     CFG_VAR_INT | CFG_READONLY,    0,   1,     0,         0,
		"file descriptor cache for tcp_send"},
	/* tcp async options */
	{ "async",        CFG_VAR_INT | CFG_READONLY,    0,   1,      0,         0,
		"async mode for writes and connects"},
	{ "connect_wait", CFG_VAR_INT | CFG_READONLY,    0,   1,      0,         0,
		"parallel simultaneous connects to the same dst. (0) or one connect"},
	{ "conn_wq_max",  CFG_VAR_INT | CFG_READONLY,    0, 1024*1024, 0,        0,
		"maximum bytes queued for write per connection (depends on async)"},
	{ "wq_max",       CFG_VAR_INT | CFG_READONLY,    0,  1<<30,    0,        0,
		"maximum bytes queued for write allowed globally (depends on async)"},
	{ "wq_timeout",   CFG_VAR_INT | CFG_READONLY,    1,  1<<30,    0,        0,
		"timeout for queued writes (in ticks, use send_timeout for seconds)"},
	/* tcp socket options */
	{ "defer_accept", CFG_VAR_INT | CFG_READONLY,    0,   3600,   0,         0,
		"0/1 on linux, seconds on freebsd (see docs)"},
	{ "delayed_ack",  CFG_VAR_INT | CFG_READONLY,    0,      1,   0,         0,
		"initial ack will be delayed and sent with the first data segment"},
	{ "syncnt",       CFG_VAR_INT | CFG_READONLY,    0,      1,   0,         0,
		"number of syn retransmissions before aborting a connect (0=not set)"},
	{ "linger2",      CFG_VAR_INT | CFG_READONLY,    0,   3600,   0,         0,
		"lifetime of orphaned sockets in FIN_WAIT2 state in s (0=not set)"},
	{ "keepalive",    CFG_VAR_INT | CFG_READONLY,    0,      1,   0,         0,
		"enables/disables keepalives for tcp"},
	{ "keepidle",     CFG_VAR_INT | CFG_READONLY,    0, 24*3600,  0,         0,
		"time before sending a keepalive if the connection is idle (linux)"},
	{ "keepintvl",    CFG_VAR_INT | CFG_READONLY,    0, 24*3600,  0,         0,
		"time interval between keepalive probes on failure (linux)"},
	{ "keepcnt",     CFG_VAR_INT | CFG_READONLY,    0,    1<<10,  0,         0,
		"number of failed keepalives before dropping the connection (linux)"},
	/* other options */
	{ "crlf_ping",   CFG_VAR_INT | CFG_READONLY,    0,        1,  0,         0,
		"enable responding to CRLF SIP-level keepalives "},
	{0, 0, 0, 0, 0, 0, 0}
};


void* tcp_cfg; /* tcp config handle */

/* set defaults */
void init_tcp_options()
{
#ifdef TCP_BUF_WRITE
	tcp_default_cfg.tcp_buf_write=0;
	tcp_default_cfg.tcpconn_wq_max=32*1024; /* 32 k */
	tcp_default_cfg.tcp_wq_max=10*1024*1024; /* 10 MB */
	tcp_default_cfg.tcp_wq_timeout=S_TO_TICKS(tcp_send_timeout);
#ifdef TCP_CONNECT_WAIT
	tcp_default_cfg.tcp_connect_wait=1;
#endif /* TCP_CONNECT_WAIT */
#endif /* TCP_BUF_WRITE */
#ifdef TCP_FD_CACHE
	tcp_default_cfg.fd_cache=1;
#endif
#ifdef HAVE_SO_KEEPALIVE
	tcp_default_cfg.keepalive=1;
#endif
/*
#if defined HAVE_TCP_DEFER_ACCEPT || defined HAVE_TCP_ACCEPT_FILTER
	tcp_default_cfg.defer_accept=1;
#endif
*/
#ifdef HAVE_TCP_QUICKACK
	tcp_default_cfg.delayed_ack=1;
#endif
	tcp_default_cfg.crlf_ping=1;
}



#define W_OPT_NC(option) \
	if (tcp_default_cfg.option){\
		WARN("tcp_options: tcp_" #option \
				" cannot be enabled (recompile needed)\n"); \
		tcp_default_cfg.option=0; \
	}



#define W_OPT_NS(option) \
	if (tcp_default_cfg.option){\
		WARN("tcp_options: tcp_" #option \
				" cannot be enabled (no OS support)\n"); \
		tcp_default_cfg.option=0; \
	}


/* checks & warns if some tcp_option cannot be enabled */
void tcp_options_check()
{
#ifndef TCP_FD_CACHE
	W_OPT_NC(defer_accept);
#endif

#ifndef TCP_BUF_WRITE
	W_OPT_NC(tcp_buf_write);
	W_OPT_NC(tcpconn_wq_max);
	W_OPT_NC(tcp_wq_max);
	W_OPT_NC(tcp_wq_timeout);
#endif /* TCP_BUF_WRITE */
#ifndef TCP_CONNECT_WAIT
	W_OPT_NC(tcp_connect_wait);
#endif /* TCP_CONNECT_WAIT */
	
	if (tcp_default_cfg.tcp_connect_wait && !tcp_default_cfg.tcp_buf_write){
		WARN("tcp_options: tcp_connect_wait depends on tcp_buf_write, "
				" disabling...\n");
		tcp_default_cfg.tcp_connect_wait=0;
	}
	
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
	if (tcp_default_cfg.keepintvl || tcp_default_cfg.keepidle || 
			tcp_default_cfg.keepcnt){
		tcp_default_cfg.keepalive=1; /* force on */
	}
#ifndef HAVE_SO_KEEPALIVE
	W_OPT_NS(keepalive);
#endif
#ifndef HAVE_TCP_QUICKACK
	W_OPT_NS(delayed_ack);
#endif
}



void tcp_options_get(struct cfg_group_tcp* t)
{
	*t=tcp_default_cfg;
}



/** register tcp config into the configuration framework.
 *  @return 0 on succes, -1 on error*/
int tcp_register_cfg()
{
	if (cfg_declare("tcp", tcp_cfg_def, &tcp_default_cfg, cfg_sizeof(tcp),
					&tcp_cfg))
		return -1;
	if (tcp_cfg==0){
		BUG("null tcp cfg");
		return -1;
	}
	return 0;
}
