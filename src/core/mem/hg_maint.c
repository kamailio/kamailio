/*
 * slab allocator with elastic arenas - maintenance process
 *
 * Copyright (C) 2026 Yury Kirsanov
 *
 * This file is part of Kamailio, a free SIP server.
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

/* maintenance process: grows elastic shared arenas ahead of demand, runs
 * their block GC and shrink ticks, and the shm arena's once-a-second duties */

#ifdef HG_MALLOC

#include <string.h>

#include "../dprint.h"
#include "../globals.h"
#include "../pt.h"
#include "../sr_module.h"
#include "../cfg/cfg_struct.h"
#include "hg_malloc.h"
#include "hg_arena.h"
#include "hg_buddy.h"

#define HG_MAINT_TICK_MS 1000
#define HG_MAINT_SLEEP_US 100000
/* the shm arena's sweep-interval duties, and the GROW-BLOCKED re-warning
 * every HG_GROW_REWARN_SWEEPS of them (5 minutes) while the latch holds */
#define HG_SHARED_INTERVAL 30
#define HG_GROW_REWARN_SWEEPS 10

static unsigned long maint_last_ns;
static unsigned int maint_ticks;
static unsigned int maint_secs;

/* a shared arena with a cap above its initial size */
static int maint_arena(const struct hg_block *hb)
{
	return hb && hb->shared && hb->buddy_ready && hb->hcap > hb->committed_min;
}

static int hg_maint_wanted(void)
{
	int i;

	if(HG_SERVES_SHM())
		return 1;
	for(i = 0; i < HG_ARENA_REG_MAX; i++)
		if(maint_arena(hg_arena_reg[i].hb))
			return 1;
	return 0;
}

int hg_maint_count_processes(void)
{
	/* main_loop() forks it only when it forks at all */
	return (!dont_fork && hg_maint_wanted()) ? 1 : 0;
}

static void hg_maint_tick(void)
{
	unsigned long now = hg_now_ns();
	int i, slow;

	/* the loop wakes every 100 ms; tick once a second */
	if(now - maint_last_ns < (unsigned long)HG_MAINT_TICK_MS * 1000000UL)
		return;
	maint_last_ns = now;
	slow = (++maint_ticks >= (unsigned int)hg_scaling_cycle);
	if(slow)
		maint_ticks = 0;

	for(i = 0; i < HG_ARENA_REG_MAX; i++) {
		struct hg_block *hb = hg_arena_reg[i].hb;

		if(!maint_arena(hb))
			continue;
		hg_lock_enter(hb, HG_LK_POLICY);
		hg_grow_headroom_tick(hb);
		/* the deferred GC - drained blocks beyond keep go back here, a
		 * budget per tick, instead of on the workers' donate path */
		hg_gc_tick(hb, HG_GC_TICK_BUDGET);
		if(slow) {
			hg_gc_cycle(hb); /* next cycle's keep */
			hg_shrink_tick(hb);
		} else {
			hg_drain_tick(hb); /* the decision's execution */
		}
		hg_lock_leave(hb);
		/* pages a worker's grow left on 4 KB - collapsed here, where
		 * nobody waits for it; takes the lock per page itself */
		hg_collapse_deferred(hb);
	}
}

/* shm arena: live peak each second; once per interval the GROW-BLOCKED tick,
 * then the deferred warnings and failed-allocation summary, unlocked */
static void hg_maint_shared_tick(void)
{
	struct hg_block *hb = (struct hg_block *)hg_shm_block;
	int due, committed_mb, cap_mb, refused, stall_reason, stall_proc;
	unsigned int stall_due;
	unsigned long stall_ns, stalls;

	hg_sample_live_peaks();
	if(++maint_secs % HG_SHARED_INTERVAL)
		return;
	hg_oom_report_shared();
	if(!hb || !HG_SERVES_SHM())
		return;

	hg_lock_enter(hb, HG_LK_POLICY);
	/* promote (or disarm) an armed episode first, so a latch earns its
	 * warning in the same tick that detects it */
	hg_grow_blocked_tick(hb);
	if(!hb->maint_active) { /* else hg_maint_tick() ticks it */
		hg_grow_tick(hb);
		hg_shrink_tick(hb);
	}
	due = hb->grow_event_due;
	hb->grow_event_due = 0;
	if(!due && hb->grow_blocked
			&& (maint_secs / HG_SHARED_INTERVAL) % HG_GROW_REWARN_SWEEPS == 0)
		due = 1; /* still blocked - re-warn */
	committed_mb = (int)(hb->committed_bytes >> 20);
	cap_mb = (int)(hb->hcap >> 20);
	refused = (int)hb->grow_refused;
	stall_due = hb->lk_stall_event_due;
	hb->lk_stall_event_due = 0;
	stall_ns = hb->lk_last_stall_ns;
	stall_reason = hb->lk_last_stall_reason;
	stall_proc = hb->lk_last_stall_proc;
	stalls = hb->lk_stalls;
	hg_lock_leave(hb);

	if(stall_due)
		LM_WARN("shm arena lock held %lu us by process %d for %s, %lu "
				"stalls so far\n",
				stall_ns / 1000, stall_proc,
				stall_reason >= 0 && stall_reason < HG_LK_REASONS
						? hg_lk_reason_str[stall_reason]
						: "?",
				stalls);
	if(due)
		LM_WARN("shm arena cannot grow: %d of %d MB committed, %d refusals "
				"(hg_shm_grow_blocked is 1 while this lasts)\n",
				committed_mb, cap_mb, refused);
}

int hg_maint_start(void)
{
	int pid, i;
	char names[256] = "";

	if(dont_fork || !hg_maint_wanted())
		return 0;

	pid = fork_process(PROC_NOCHLDINIT, "hg maintenance", 0);
	if(pid < 0) {
		LM_CRIT("cannot fork the hg maintenance process\n");
		return -1;
	}
	if(pid > 0)
		return 0; /* parent */

	/* the child */
	if(cfg_child_init())
		goto error;
	for(i = 0; i < HG_ARENA_REG_MAX; i++) {
		struct hg_block *hb = hg_arena_reg[i].hb;

		if(!maint_arena(hb))
			continue;
		__atomic_store_n(&hb->maint_active, 1, __ATOMIC_RELEASE);
		if(strlen(names) + strlen(hb->name) + 2 < sizeof names) {
			if(*names)
				strcat(names, ",");
			strcat(names, hb->name);
		}
	}
	maint_last_ns = hg_now_ns();
	if(*names)
		LM_NOTICE("hg maintenance process started: arena(s) %s grow ahead "
				  "of demand here (headroom tick every %d ms, policy every %d "
				  "s)\n",
				names, HG_MAINT_TICK_MS, hg_scaling_cycle);
	else
		LM_NOTICE("hg maintenance process started: no elastic arena, "
				  "shm statistics and warnings only\n");

	for(;;) {
		sleep_us(HG_MAINT_SLEEP_US);
		cfg_update();
		if(hg_now_ns() - maint_last_ns
				< (unsigned long)HG_MAINT_TICK_MS * 1000000UL)
			continue;
		hg_maint_tick();
		hg_maint_shared_tick();
	}

error:
	LM_ERR("the hg maintenance process failed to initialise, exiting\n");
	for(i = 0; i < HG_ARENA_REG_MAX; i++)
		if(maint_arena(hg_arena_reg[i].hb))
			__atomic_store_n(
					&hg_arena_reg[i].hb->maint_active, 0, __ATOMIC_RELEASE);
	exit(-1);
}

#endif /* HG_MALLOC */
