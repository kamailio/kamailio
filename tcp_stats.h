/* 
 * $Id$
 * 
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
/*
 * tcp_stats.h - tcp statistics macros
 */
/*
 * History:
 * --------
 *  2009-04-08  initial version (andrei)
*/

#ifndef __tcp_stats_h
#define __tcp_stats_h

/** called each time a new tcp connection is established.
 *  @param state - S_CONN_ACCEPT if it was the result of an accept()
 *               - S_CONN_CONNECT if it was the result of a connect()
 * Note: in general it will be called when the first packet was received or
 *   sent on the new connection and not immediately after accept() or 
 *   connect()
 */
#define TCP_STATS_ESTABLISHED(state)

/** called each time a new outgoing connection fails.  */
#define TCP_STATS_CONNECT_FAILED()

/** called each time a new incoming connection is rejected.
 * (accept() denied due to maximum number of TCP connections being exceeded)
 */
#define TCP_STATS_LOCAL_REJECT()


/** called each time a connection lifetime expires.
  * (the connection is closed for being idle for too long)
  */
#define TCP_STATS_CON_TIMEOUT()


/** called each time a TCP RST is received on an established connection.  */
#define TCP_STATS_CON_RESET()

/** called each time a send operation fails due to a timeout.
  * FIXME: it works only in async mode (in sync. mode a send might timeout
  *  but the stats won't be increased).
  */
#define TCP_STATS_SEND_TIMEOUT()

/** called each time a send fails due to the buffering capacity being exceeded.
  * (used only in tcp async mode)
  */
#define TCP_STATS_SENDQ_FULL()



#endif /*__tcp_stats_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
