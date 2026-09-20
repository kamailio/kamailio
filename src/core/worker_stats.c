/*
 * Copyright (C) 2026 Phil Lavin <phil@lavin.me.uk>
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
 * \file
 * \brief Kamailio core :: worker and thread pool occupancy counters
 * \ingroup core
 */

#include "worker_stats.h"

counter_handle_t ksr_cnt_busy_children;
counter_handle_t ksr_cnt_sip_children;
counter_handle_t ksr_cnt_busy_udp_threads;
counter_handle_t ksr_cnt_udp_threads;
counter_handle_t ksr_cnt_busy_rthreads;
counter_handle_t ksr_cnt_rthreads;
counter_handle_t ksr_cnt_rdispatch_drops;

/* CNT_F_NO_RESET: stats.reset_statistics would skew the net gauges.
 * busy_children covers receive_msg() only, not timer/rtimer routes. */
static counter_def_t ksr_worker_cnt_defs[] = {
		{&ksr_cnt_busy_children, "busy_children", CNT_F_NO_RESET, 0, 0,
				"worker processes currently inside receive_msg()"},
		{&ksr_cnt_sip_children, "sip_children", CNT_F_NO_RESET, 0, 0,
				"worker processes that have processed at least one message"},
		{&ksr_cnt_busy_udp_threads, "busy_udp_threads", CNT_F_NO_RESET, 0, 0,
				"udp receiver threads currently enqueueing to the worker group"
				" (udp_receiver_mode > 0)"},
		{&ksr_cnt_udp_threads, "udp_threads", CNT_F_NO_RESET, 0, 0,
				"udp receiver threads seen (udp_receiver_mode > 0)"},
		{&ksr_cnt_busy_rthreads, "busy_reactor_threads", CNT_F_NO_RESET, 0, 0,
				"tcp reactor pool threads currently executing a job"
				" (tcp_main_threads = 2)"},
		{&ksr_cnt_rthreads, "reactor_threads", CNT_F_NO_RESET, 0, 0,
				"tcp reactor pool threads seen (tcp_main_threads = 2)"},
		{&ksr_cnt_rdispatch_drops, "reactor_dispatch_drops", CNT_F_NO_RESET, 0,
				0,
				"messages dropped because the reactor dispatch socket stayed"
				" full"},
		{0, 0, 0, 0, 0, 0}};

/* Must run before counters_prefork_init(). */
int ksr_worker_stats_init(void)
{
	return counter_register_array("core", ksr_worker_cnt_defs);
}
