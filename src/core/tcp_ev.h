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
/*!
 * \brief Kamailio core :: tcp_ev.h - tcp events
 * \ingroup core
 */

#ifndef __tcp_ev_h
#define __tcp_ev_h

#include <errno.h>
#include <string.h>

#include "ip_addr.h"


/** a connect attempt got a RST from the peer
 * Note: the RST might be for the connect() itself (SYN), for the first
 *  send() attempt on the connection (unlikely) or received immediately after
 * the connect() succeeded (unlikely, the remote host would have a very small
 *  window after accepting a connection to send a RST before it receives
 * any data).
 *
 * @param err - if 0 it should be ignored (no corresp. libc error), if non-0
 *                it will contain the errno.
 * @param lip   - pointer to an ip_addr containing the local ip
 *                   or 0 if dynamic (WARNING can be 0).
 * @param lport - pointer to an ip_addr containing the local port or 0
 *                   if unknown/dynamic.
 * @param dst   - pointer to a sockaddr_union containing the destination.
 * @param proto - protocol used
 */
#define TCP_EV_CONNECT_RST(err, lip, lport, dst, proto) \
	LM_ERR("connect %s failed (RST) %s\n", \
			su2a(dst, sizeof(*(dst))), (err)?strerror(err):"")

/** a connect failed because the remote host/network is unreachable. */
#define TCP_EV_CONNECT_UNREACHABLE(err, lip, lport, dst, proto) \
	LM_ERR("connect %s failed (unreachable) %s\n", \
			su2a(dst, sizeof(*(dst))), (err)?strerror(err):"")

/** a connect attempt did timeout. */
#define TCP_EV_CONNECT_TIMEOUT(err, lip, lport, dst, proto) \
	LM_ERR("connect %s failed (timeout) %s\n", \
			su2a(dst, sizeof(*(dst))), (err)?strerror(err):"")

/** a connect attempt failed because the local ports are exhausted. */
#define TCP_EV_CONNECT_NO_MORE_PORTS(err, lip, lport, dst, proto) \
	LM_ERR("connect %s failed (no more ports) %s\n", \
			su2a(dst, sizeof(*(dst))), (err)?strerror(err):"")

/** a connect attempt failed for some unknown reason.  */
#define TCP_EV_CONNECT_ERR(err, lip, lport, dst, proto) \
	LM_ERR("connect %s failed %s\n", \
			su2a(dst, sizeof(*(dst))), (err)?strerror(err):"")


/** send failed due to timeout.
 * @param err   - if 0 it should be ignored (no corresp. libc error), if non-0
 *                it will contain the errno.
 * @param rcv   - pointer to rcv_info structure
 * 
 */
#define TCP_EV_SEND_TIMEOUT(err, rcv)

/** send failed due to buffering capacity being exceeded.
  * (only in async mode) */
#define TCP_EV_SENDQ_FULL(err, rcv)

/** established connection closed for being idle too long. */
#define TCP_EV_IDLE_CONN_CLOSED(err, rcv)




#endif /*__tcp_ev_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
