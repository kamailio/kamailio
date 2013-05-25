/* 
 * $Id$
 * 
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
/** sctp statistics.
 * @file sctp_stats.c
 * @ingroup:  core (sctp)
 */
/*
 * History:
 * --------
 *  2010-08-09  initial version (andrei)
*/

#ifdef USE_SCTP

#include "sctp_stats.h"

#ifdef USE_SCTP_STATS

#include "../../counters.h"

#include "sctp_server.h"

struct sctp_counters_h sctp_cnts_h;


enum sctp_info_req { SCTP_INFO_NONE, SCTP_INFO_CONN_NO, SCTP_INFO_TRACKED_NO };
static counter_val_t sctp_info(counter_handle_t h, void* what);



/* sctp counters definitions */
counter_def_t sctp_cnt_defs[] =  {
	{&sctp_cnts_h.established, "established", 0, 0, 0,
		"incremented each time a new association is established."},
	{&sctp_cnts_h.connect_failed, "connect_failed", 0, 0, 0,
		"incremented each time a new outgoing connection fails."},
	{&sctp_cnts_h.local_reject, "local_reject", 0, 0, 0,
		"number of rejected incoming connections."},
	{&sctp_cnts_h.remote_shutdown, "remote_shutdown", 0, 0, 0,
		"incremented each time an association is closed by the peer."},
	{&sctp_cnts_h.assoc_shutdown, "assoc_shutdown", 0, 0, 0,
		"incremented each time an association is shutdown."},
	{&sctp_cnts_h.comm_lost, "comm_lost", 0, 0, 0,
		"incremented each time an established connection is close due to"
			"some error."},
	{&sctp_cnts_h.sendq_full, "sendq_full", 0, 0, 0,
		"number of failed send attempt due to exceeded buffering capacity"
	    " (full kernel buffers)."},
	{&sctp_cnts_h.send_failed, "send_failed", 0, 0, 0,
		"number of failed send attempt for any reason except full buffers."},
	{&sctp_cnts_h.send_force_retry, "send_force_retry", 0, 0, 0,
		"incremented each time a failed send is force-retried"
			"(possible only if sctp_send_retries ! = 0"},
	{0, "current_opened_connections", 0,
		sctp_info, (void*)(long)SCTP_INFO_CONN_NO,
		"number of currently opened associations."},
	{0, "current_tracked_connections", 0,
		sctp_info, (void*)(long)SCTP_INFO_TRACKED_NO,
		"number of currently tracked associations."},
	{0, 0, 0, 0, 0, 0 }
};



/** helper function for some stats (which are kept internally inside sctp).
 */
static counter_val_t sctp_info(counter_handle_t h, void* what)
{
	enum sctp_info_req w;
	struct sctp_gen_info i;

	if (sctp_disable)
		return 0;
	w = (int)(long)what;
	sctp_get_info(&i);
	switch(w) {
		case SCTP_INFO_CONN_NO:
			return i.sctp_connections_no;
		case SCTP_INFO_TRACKED_NO:
			return i.sctp_tracked_no;
		case SCTP_INFO_NONE:
			break;
	};
	return 0;
}

/** intialize sctp statistics.
 *  Must be called before forking.
 * @return < 0 on errror, 0 on success.
 */
int sctp_stats_init()
{
	if (counter_register_array("sctp", sctp_cnt_defs) < 0)
		goto error;
	return 0;
error:
	return -1;
}


void sctp_stats_destroy()
{
	/* do nothing */
}

#endif /* USE_SCTP_STATS */
#endif /* USE_SCTP */

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
