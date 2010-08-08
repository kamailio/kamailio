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
/** tcp statistics.
 * @file tcp_stats.c
 * @ingroup:  core
 */
/*
 * History:
 * --------
 *  2010-08-08  initial version (andrei)
*/

#include "tcp_stats.h"
#include "counters.h"

struct tcp_counters_h tcp_cnts_h;

/** intialize tcp statistics.
 *  Must be called before forking.
 * @return < 0 on errror, 0 on success.
 */
int tcp_stats_init()
{
#define TCP_REG_COUNTER(name) \
	if (counter_register(&tcp_cnts_h.name, "tcp", # name, 0, 0, 0, 0) < 0) \
		goto error;

	TCP_REG_COUNTER(established);
	TCP_REG_COUNTER(passive_open);
	TCP_REG_COUNTER(connect_success);
	TCP_REG_COUNTER(connect_failed);
	TCP_REG_COUNTER(local_reject);
	TCP_REG_COUNTER(con_timeout);
	TCP_REG_COUNTER(con_reset);
	TCP_REG_COUNTER(send_timeout);
	TCP_REG_COUNTER(sendq_full);
	return 0;
error:
	return -1;
}


void tcp_stats_destroy()
{
	/* do nothing */
}



/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
