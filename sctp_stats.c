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

#include "sctp_stats.h"
#include "counters.h"

struct sctp_counters_h sctp_cnts_h;

/** intialize sctp statistics.
 *  Must be called before forking.
 * @return < 0 on errror, 0 on success.
 */
int sctp_stats_init()
{
#define SCTP_REG_COUNTER(name) \
	if (counter_register(&sctp_cnts_h.name, "sctp", # name, 0, 0, 0, 0) < 0) \
		goto error;

	SCTP_REG_COUNTER(established);
	SCTP_REG_COUNTER(connect_failed);
	SCTP_REG_COUNTER(local_reject);
	SCTP_REG_COUNTER(remote_shutdown);
	SCTP_REG_COUNTER(assoc_shutdown);
	SCTP_REG_COUNTER(comm_lost);
	SCTP_REG_COUNTER(sendq_full);
	SCTP_REG_COUNTER(send_force_retry);
	return 0;
error:
	return -1;
}


void sctp_stats_destroy()
{
	/* do nothing */
}

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
