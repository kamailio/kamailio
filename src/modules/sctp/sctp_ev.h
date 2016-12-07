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
 * sctp_ev.h - sctp events
 */
/*
 * History:
 * --------
 *  2009-04-28  initial version (andrei)
*/

#ifndef __sctp_ev_h
#define __sctp_ev_h

#include <errno.h>
#include <string.h>

#ifndef USE_SCTP_EV

#define SCTP_EV_ASSOC_CHANGE(lip, lport, src, reason, state)
#define SCTP_EV_PEER_ADDR_CHANGE(lip, lport, src, reason, state, addr_su)
#define SCTP_EV_REMOTE_ERROR(lip, lport, src, err)
#define SCTP_EV_SEND_FAILED(lip, lport, src, err)
#define SCTP_EV_SHUTDOWN_EVENT(lip, lport, src)
#define SCTP_EV_SENDER_DRY_EVENT(lip, lport, src)

#else /* USE_SCTP_EV */

#include "../../ip_addr.h"


/** an association has either been opened or closed.
 * called for each SCTP_ASSOC_CHANGE event.
 *
 * @param err - if 0 it should be ignored (no corresp. libc error), if non-0
 *                it will contain the errno.
 * @param lip   - pointer to an ip_addr containing the local ip
 *                   or 0 if dynamic (WARNING can be 0).
 * @param lport - pointer to an ip_addr containing the local port or 0
 *                   if unknown/dynamic.
 * @param src   - pointer to a sockaddr_union containing the src.
 * @param proto - protocol used
 */
#define SCTP_EV_ASSOC_CHANGE(lip, lport, src, reason, state) \
	DBG("SCTP_ASSOC_CHANGE from %s on %s:%d: %s\n", \
			su2a(src, sizeof(*(src))), ip_addr2a(lip), lport, reason)

/** an address part of an assoc. changed state.
 * called for the SCTP_PEER_ADDR_CHANGE event.*/
#define SCTP_EV_PEER_ADDR_CHANGE(lip, lport, src, reason, state, addr_su) \
	DBG("SCTP_PEER_ADDR_CHANGE from %s on %s:%d: %s\n", \
			su2a(src, sizeof(*(src))), ip_addr2a(lip), lport, reason)

/** remote operation error from the peer.
 * called for the SCTP_REMOTE_ERROR event.*/
#define SCTP_EV_REMOTE_ERROR(lip, lport, src, err) \
	DBG("SCTP_REMOTE_ERROR from %s on %s:%d: %d\n", \
			su2a(src, sizeof(*(src))), ip_addr2a(lip), lport, err)

/** send failed.
 * called for the SCTP_SEND_FAILED event.*/
#define SCTP_EV_SEND_FAILED(lip, lport, src, err) \
	DBG("SCTP_SEND_FAILED from %s on %s:%d: %d\n", \
			su2a(src, sizeof(*(src))), ip_addr2a(lip), lport, err)

/** the peer has sent a shutdown.
 * called for the SCTP_SHUTDOWN_EVENT event.*/
#define SCTP_EV_SHUTDOWN_EVENT(lip, lport, src) \
	DBG("SCTP_SHUTDOWN_EVENT from %s on %s:%d\n", \
			su2a(src, sizeof(*(src))), ip_addr2a(lip), lport)

/** kernel has finished sending all the queued data.
 * called for the SCTP_SENDER_DRY_EVENT event.*/
#define SCTP_EV_SENDER_DRY_EVENT(lip, lport, src) \
	DBG("SCTP_SENDER_DRY_EVENT from %s on %s:%d\n", \
			su2a(src, sizeof(*(src))), ip_addr2a(lip), lport)

#endif /* USE_SCTP_EV */

#endif /*__sctp_ev_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
