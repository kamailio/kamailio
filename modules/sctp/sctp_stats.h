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
 * sctp_stats.h - sctp statistics macros
 */
/*
 * History:
 * --------
 *  2009-04-28  initial version (andrei)
*/

#ifndef __sctp_stats_h
#define __sctp_stats_h


/* enable sctp stats by default */
#ifndef NO_SCTP_STATS
#define USE_SCTP_STATS
#endif

#ifndef USE_SCTP_STATS

#define INIT_SCTP_STATS() 0 /* success */
#define DESTROY_SCTP_STATS()

#define SCTP_STATS_ESTABLISHED()
#define SCTP_STATS_CONNECT_FAILED()
#define SCTP_STATS_LOCAL_REJECT()
#define SCTP_STATS_REMOTE_SHUTDOWN()
#define SCTP_STATS_ASSOC_SHUTDOWN()
#define SCTP_STATS_COMM_LOST()
#define SCTP_STATS_SENDQ_FULL()
#define SCTP_STATS_SEND_FAILED()
#define SCTP_STATS_SEND_FORCE_RETRY()

#else /* USE_SCTP_STATS */

#include "../../counters.h"

struct sctp_counters_h {
	counter_handle_t established;
	counter_handle_t connect_failed;
	counter_handle_t local_reject;
	counter_handle_t remote_shutdown;
	counter_handle_t assoc_shutdown;
	counter_handle_t comm_lost;
	counter_handle_t sendq_full;
	counter_handle_t send_failed;
	counter_handle_t send_force_retry;
};

extern struct sctp_counters_h sctp_cnts_h;

int sctp_stats_init();
void sctp_stats_destroy();

#define INIT_SCTP_STATS() sctp_stats_init() /* success */

#define DESTROY_SCTP_STATS() sctp_stats_destroy()


/** called each time a new sctp assoc. is established.
 * sctp notification: SCTP_COMM_UP.
 */
#define SCTP_STATS_ESTABLISHED() \
	counter_inc(sctp_cnts_h.established)

/** called each time a new outgoing connection/assoc open fails.
 *  sctp notification: SCTP_CANT_STR_ASSOC
 */
#define SCTP_STATS_CONNECT_FAILED() \
	counter_inc(sctp_cnts_h.connect_failed)

/** called each time a new incoming connection is rejected.  */
#define SCTP_STATS_LOCAL_REJECT() \
	counter_inc(sctp_cnts_h.local_reject)


/** called each time a connection is closed by the peer.
  * sctp notification: SCTP_SHUTDOWN_EVENT
  */
#define SCTP_STATS_REMOTE_SHUTDOWN() \
	counter_inc(sctp_cnts_h.remote_shutdown)


/** called each time a connection is shutdown.
  * sctp notification: SCTP_SHUTDOWN_COMP
  */
#define SCTP_STATS_ASSOC_SHUTDOWN() \
	counter_inc(sctp_cnts_h.assoc_shutdown)


/** called each time an established connection is closed due to some error.
  * sctp notification: SCTP_COMM_LOST
  */
#define SCTP_STATS_COMM_LOST() \
	counter_inc(sctp_cnts_h.comm_lost)


/** called each time a send fails due to the buffering capacity being exceeded.
  * (send fails due to full kernel buffers)
  */
#define SCTP_STATS_SENDQ_FULL() \
	counter_inc(sctp_cnts_h.sendq_full)


/** called each time a send fails.
  * (send fails for any reason except buffers full)
  * sctp notification: SCTP_SEND_FAILED
  */
#define SCTP_STATS_SEND_FAILED() \
	counter_inc(sctp_cnts_h.send_failed)

/** called each time a failed send is force-retried.
  * (possible only if sctp_send_retries is != 0)
  */
#define SCTP_STATS_SEND_FORCE_RETRY() \
	counter_inc(sctp_cnts_h.send_force_retry)

#endif /* USE_SCTP_STATS */

#endif /*__sctp_stats_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
