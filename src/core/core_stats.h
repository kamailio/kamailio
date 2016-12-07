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

#else /* USE_CORE_STATS */

#include "events.h"

/** called each time a received request is dropped.
 * The request might be dropped explicitly (e.g. pre script callback)
 * or there might be an error while trying to forward it (e.g. send).
 */
#define STATS_REQ_FWD_DROP() sr_event_exec(SREV_CORE_STATS, (void*)3)


/** called each time forwarding a request succeeds (send).*/
#define STATS_REQ_FWD_OK() sr_event_exec(SREV_CORE_STATS, (void*)1)


/** called each time forwarding a reply fails.
 * The reply forwarding might fail due to send errors,
 * pre script callbacks (module denying forwarding) or explicit script
 * drop (drop or module function returning 0).
 */
#define STATS_RPL_FWD_DROP() sr_event_exec(SREV_CORE_STATS, (void*)4)


/* called each time forwarding a reply succeeds. */
#define STATS_RPL_FWD_OK() sr_event_exec(SREV_CORE_STATS, (void*)2)


/** called each time a received request is too bad to process.
  * For now it's called in case the message does not have any via.
  */
#define STATS_BAD_MSG() sr_event_exec(SREV_CORE_STATS, (void*)5)


/** called each time a received reply is too bad to process.
  * For now it's called in case the message does not have any via.
  */
#define STATS_BAD_RPL() sr_event_exec(SREV_CORE_STATS, (void*)6)


/** called each time uri parsing fails. */
#define STATS_BAD_URI() sr_event_exec(SREV_CORE_STATS, (void*)7)


/** called each time parsing some header fails. */
#define STATS_BAD_MSG_HDR() sr_event_exec(SREV_CORE_STATS, (void*)8)



#endif /* USE_CORE_STATS */

#endif /*__core_stats_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
