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

#ifndef _worker_stats_h
#define _worker_stats_h

#include "compiler_opt.h"
#include "counters.h"

/* Occupancy: counter rows are per-process and summed on read, so +1/-1 pairs
 * cancel and the total is what is busy right now. */
extern counter_handle_t ksr_cnt_busy_children;
extern counter_handle_t ksr_cnt_busy_udp_threads;
extern counter_handle_t ksr_cnt_busy_rthreads;

/* Pool sizes, self-reported on first use. */
extern counter_handle_t ksr_cnt_sip_children;
extern counter_handle_t ksr_cnt_udp_threads;
extern counter_handle_t ksr_cnt_rthreads;

extern counter_handle_t ksr_cnt_rdispatch_drops;

int ksr_worker_stats_init(void);

/* Count the caller into a pool total once. Thread-local, so it also works for
 * pool threads sharing one counter row. */
#define KSR_POOL_MEMBER_ONCE(tot_h)               \
	do {                                          \
		static _Thread_local int _ksr_pm_reg = 0; \
		if(unlikely(!_ksr_pm_reg)) {              \
			_ksr_pm_reg = 1;                      \
			counter_inc(tot_h);                   \
		}                                         \
	} while(0)

#endif
