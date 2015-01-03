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

/** Kamailio core :: TCP statistics.
 * @file tcp_stats.c
 * @ingroup:  core
 * Module: \ref core
 */

#ifdef USE_TCP
#include "tcp_stats.h"

#ifdef USE_TCP_STATS

#include "counters.h"
#include "tcp_info.h"

struct tcp_counters_h tcp_cnts_h;


enum tcp_info_req { TCP_INFO_NONE, TCP_INFO_CONN_NO, TCP_INFO_WR_QUEUE_SZ };

static counter_val_t tcp_info(counter_handle_t h, void* what);

/* tcp counters definitions */
counter_def_t tcp_cnt_defs[] =  {
	{&tcp_cnts_h.established, "established", 0, 0, 0,
		"incremented each time a tcp connection is established."},
	{&tcp_cnts_h.passive_open, "passive_open", 0, 0, 0,
		"total number of accepted connections (so far)."},
	{&tcp_cnts_h.connect_success, "connect_success", 0, 0, 0,
		"total number of successfully active opened connections"
			" (successful connect()s)."},
	{&tcp_cnts_h.connect_failed, "connect_failed", 0, 0, 0,
		"number of failed active connection attempts."},
	{&tcp_cnts_h.local_reject, "local_reject", 0, 0, 0,
		"number of rejected incoming connections."},
	{&tcp_cnts_h.con_timeout, "con_timeout", 0, 0, 0,
		"total number of connections that did timeout (idle for too long)."},
	{&tcp_cnts_h.con_reset, "con_reset", 0, 0, 0,
		"total number of TCP_RSTs received on established connections."},
	{&tcp_cnts_h.send_timeout, "send_timeout", 0, 0, 0,
		"number of send attempts that failed due to a timeout"
			"(note: works only in tcp async mode)."},
	{&tcp_cnts_h.sendq_full, "sendq_full", 0, 0, 0,
		"number of send attempts that failed because of exceeded buffering"
			"capacity (send queue full, works only in tcp async mode)."},
	{0, "current_opened_connections", 0,
		tcp_info, (void*)(long)TCP_INFO_CONN_NO,
		"number of currently opened connections."},
	{0, "current_write_queue_size", 0,
		tcp_info, (void*)(long)TCP_INFO_WR_QUEUE_SZ,
		"current sum of all the connections write queue sizes."},
	{0, 0, 0, 0, 0, 0 }
};



/** helper function for some stats (which are kept internally inside tcp).
 */
static counter_val_t tcp_info(counter_handle_t h, void* what)
{
	enum tcp_info_req w;
	struct tcp_gen_info ti;

	if (tcp_disable)
		return 0;
	w = (int)(long)what;
	tcp_get_info(&ti);
	switch(w) {
		case TCP_INFO_CONN_NO:
			return ti.tcp_connections_no;
		case TCP_INFO_WR_QUEUE_SZ:
			return ti.tcp_write_queued;
		case TCP_INFO_NONE:
			break;
	};
	return 0;
}

/** intialize tcp statistics.
 *  Must be called before forking.
 * @return < 0 on errror, 0 on success.
 */
int tcp_stats_init()
{
#define TCP_REG_COUNTER(name) \
	if (counter_register(&tcp_cnts_h.name, "tcp", # name, 0, 0, 0, 0) < 0) \
		goto error;

	if (counter_register_array("tcp", tcp_cnt_defs) < 0)
		goto error;
	return 0;
error:
	return -1;
}


void tcp_stats_destroy()
{
	/* do nothing */
}


#endif /* USE_TCP_STATS */
#endif /* USE_TCP */
/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
