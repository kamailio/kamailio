/* 
 * Copyright (C) 2009 iptelorg GmbH
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
 * \brief Kamailio core :: tcp_stats.h - tcp statistics macros
 * \ingroup core
 */

#ifndef __tcp_stats_h
#define __tcp_stats_h

/* enable tcp stats by default */
#ifndef NO_TCP_STATS
#define USE_TCP_STATS
#endif

#ifndef USE_TCP_STATS

#define INIT_TCP_STATS() 0 /* success */
#define DESTROY_TCP_STATS()

#define TCP_STATS_ESTABLISHED(state)
#define TCP_STATS_CONNECT_FAILED()
#define TCP_STATS_LOCAL_REJECT()
#define TCP_STATS_CON_TIMEOUT()
#define TCP_STATS_CON_RESET()
#define TCP_STATS_SEND_TIMEOUT()
#define TCP_STATS_SENDQ_FULL()

#else /* USE_TCP_STATS */

#include "counters.h"

struct tcp_counters_h {
	counter_handle_t established;
	counter_handle_t passive_open;
	counter_handle_t connect_success;
	counter_handle_t connect_failed;
	counter_handle_t local_reject;
	counter_handle_t con_timeout;
	counter_handle_t con_reset;
	counter_handle_t send_timeout;
	counter_handle_t sendq_full;
};

extern struct tcp_counters_h tcp_cnts_h;

int tcp_stats_init(void);
void tcp_stats_destroy(void);

#define INIT_TCP_STATS() tcp_stats_init()

#define DESTROY_TCP_STATS() tcp_stats_destroy()


/** called each time a new tcp connection is established.
 *  @param state - S_CONN_ACCEPT if it was the result of an accept()
 *               - S_CONN_CONNECT if it was the result of a connect()
 * Note: in general it will be called when the first packet was received or
 *   sent on the new connection and not immediately after accept() or 
 *   connect()
 */
#define TCP_STATS_ESTABLISHED(state) \
	do { \
		counter_inc(tcp_cnts_h.established); \
		if (state == S_CONN_ACCEPT) \
			counter_inc(tcp_cnts_h.passive_open); \
		else \
			counter_inc(tcp_cnts_h.connect_success); \
	}while(0)

/** called each time a new outgoing connection fails.  */
#define TCP_STATS_CONNECT_FAILED() \
	counter_inc(tcp_cnts_h.connect_failed)

/** called each time a new incoming connection is rejected.
 * (accept() denied due to maximum number of TCP connections being exceeded)
 */
#define TCP_STATS_LOCAL_REJECT() \
	counter_inc(tcp_cnts_h.local_reject)


/** called each time a connection lifetime expires.
  * (the connection is closed for being idle for too long)
  */
#define TCP_STATS_CON_TIMEOUT() \
	counter_inc(tcp_cnts_h.con_timeout)


/** called each time a TCP RST is received on an established connection.  */
#define TCP_STATS_CON_RESET() \
	counter_inc(tcp_cnts_h.con_reset)

/** called each time a send operation fails due to a timeout.
  * FIXME: it works only in async mode (in sync. mode a send might timeout
  *  but the stats won't be increased).
  */
#define TCP_STATS_SEND_TIMEOUT() \
	counter_inc(tcp_cnts_h.send_timeout)

/** called each time a send fails due to the buffering capacity being exceeded.
  * (used only in tcp async mode)
  */
#define TCP_STATS_SENDQ_FULL() \
	counter_inc(tcp_cnts_h.sendq_full)

#endif /* USE_TCP_STATS */

#endif /*__tcp_stats_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
