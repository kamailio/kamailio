/*
 * Copyright (C) 2010 iptelorg GmbH
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
 * @brief Macros used for various core statistics
 *
 * Macros used for various core statistics, (if USE_CORE_STATS is not defined
 * they won't do anything).
 * @file
 * @ingroup core
 * @author andrei
 */

#ifndef __core_stats_h
#define __core_stats_h

/* define USE_CORE_STATS to enable statistics events
   (SREV_CORE_STATS callbacks) */
/*#define USE_CORE_STATS */

#ifndef USE_CORE_STATS

#define STATS_REQ_FWD_DROP()
#define STATS_REQ_FWD_OK()
#define STATS_RPL_FWD_DROP()
#define STATS_RPL_FWD_OK()
#define STATS_BAD_MSG()
#define STATS_BAD_RPL()
#define STATS_BAD_URI()
#define STATS_BAD_MSG_HDR()
#define STATS_REQ_ROUTE_LATENCY(_method, _latency_us)
#define STATS_RPL_ROUTE_LATENCY(_method, _status_class, _latency_us)

#else /* USE_CORE_STATS */

#include "events.h"

#define CORE_STATS_EV_FWD_REQ_OK 1
#define CORE_STATS_EV_FWD_RPL_OK 2
#define CORE_STATS_EV_DROP_REQ 3
#define CORE_STATS_EV_DROP_RPL 4
#define CORE_STATS_EV_ERR_REQ 5
#define CORE_STATS_EV_ERR_RPL 6
#define CORE_STATS_EV_BAD_URI 7
#define CORE_STATS_EV_BAD_MSG_HDR 8
#define CORE_STATS_EV_REQ_ROUTE_LATENCY 100
#define CORE_STATS_EV_RPL_ROUTE_LATENCY 101

struct core_stats_latency_event
{
	int type;
	unsigned int method;
	unsigned int status_class;
	unsigned int latency_us;
};

/** called each time a received request is dropped.
 * The request might be dropped explicitly (e.g. pre script callback)
 * or there might be an error while trying to forward it (e.g. send).
 */
#define STATS_REQ_FWD_DROP() \
	sr_event_exec(SREV_CORE_STATS, (void *)CORE_STATS_EV_DROP_REQ)


/** called each time forwarding a request succeeds (send).*/
#define STATS_REQ_FWD_OK() \
	sr_event_exec(SREV_CORE_STATS, (void *)CORE_STATS_EV_FWD_REQ_OK)


/** called each time forwarding a reply fails.
 * The reply forwarding might fail due to send errors,
 * pre script callbacks (module denying forwarding) or explicit script
 * drop (drop or module function returning 0).
 */
#define STATS_RPL_FWD_DROP() \
	sr_event_exec(SREV_CORE_STATS, (void *)CORE_STATS_EV_DROP_RPL)


/* called each time forwarding a reply succeeds. */
#define STATS_RPL_FWD_OK() \
	sr_event_exec(SREV_CORE_STATS, (void *)CORE_STATS_EV_FWD_RPL_OK)


/** called each time a received request is too bad to process.
  * For now it's called in case the message does not have any via.
  */
#define STATS_BAD_MSG() \
	sr_event_exec(SREV_CORE_STATS, (void *)CORE_STATS_EV_ERR_REQ)


/** called each time a received reply is too bad to process.
  * For now it's called in case the message does not have any via.
  */
#define STATS_BAD_RPL() \
	sr_event_exec(SREV_CORE_STATS, (void *)CORE_STATS_EV_ERR_RPL)


/** called each time uri parsing fails. */
#define STATS_BAD_URI() \
	sr_event_exec(SREV_CORE_STATS, (void *)CORE_STATS_EV_BAD_URI)


/** called each time parsing some header fails. */
#define STATS_BAD_MSG_HDR() \
	sr_event_exec(SREV_CORE_STATS, (void *)CORE_STATS_EV_BAD_MSG_HDR)

#define STATS_REQ_ROUTE_LATENCY(_method, _latency_us) \
	do {                                              \
		struct core_stats_latency_event _ev;          \
		_ev.type = CORE_STATS_EV_REQ_ROUTE_LATENCY;   \
		_ev.method = (_method);                       \
		_ev.status_class = 0;                         \
		_ev.latency_us = (_latency_us);               \
		sr_event_exec(SREV_CORE_STATS, (void *)&_ev); \
	} while(0)

#define STATS_RPL_ROUTE_LATENCY(_method, _status_class, _latency_us) \
	do {                                                             \
		struct core_stats_latency_event _ev;                         \
		_ev.type = CORE_STATS_EV_RPL_ROUTE_LATENCY;                  \
		_ev.method = (_method);                                      \
		_ev.status_class = (_status_class);                          \
		_ev.latency_us = (_latency_us);                              \
		sr_event_exec(SREV_CORE_STATS, (void *)&_ev);                \
	} while(0)


#endif /* USE_CORE_STATS */

#endif /*__core_stats_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
