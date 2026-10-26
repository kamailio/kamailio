/*
 * slab allocator with elastic arenas - per-thread caches and sweep
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
#ifdef HG_MALLOC

#include <string.h>
#include <limits.h>

#include "hg_malloc.h"
#include "hg_arena.h"
#include "hg_arena_int.h"
#include "hg_dbg.h"
#include "../dprint.h"

/* give back every cell this thread caches, all classes; only the owning
 * thread can (TLS). hb->lock held, returns the cells handed over */
unsigned int hg_cache_flush_locked(struct hg_block *hb, struct hg_palloc *pl)
{
	unsigned int c, n = 0;

	if(!pl)
		return 0;

	hb->gc_deferred = 1;
	for(c = 0; c < HG_NCLASSES; c++) {
		void *cur = pl->cls[c].free_head;

		while(cur) {
			void *nxt;

			/* a foreign cell means an overwritten link: stop, the rest leaks */
			if(!cell_block(hb, c, cur)) {
				LM_CRIT("%s: class %d private chain broken at %p after %u "
						"cells - dropping the rest\n",
						hb->name, c, cur, n);
				break;
			}
			nxt = cell_next(cur);
			if(hg_gpool_push(hb, c, cur) < 0) {
				/* refused (a double free, most likely): the copy already
				 * in the pool owns this cell's link now - stop here */
				LM_CRIT("%s: class %d private chain stops at refused cell "
						"%p after %u cells - dropping the rest\n",
						hb->name, c, cur, n);
				break;
			}
			cur = nxt;
			n++;
		}
		pl->cls[c].free_head = NULL;
		pl->cls[c].nfree = 0;
	}
	hb->gc_deferred = 0;

	/* now blocks may go; under pressure the keep is waived */
	for(c = 0; c < HG_NCLASSES; c++)
		if(hb->ndrained[c])
			hb->gc_inline_blocks += hg_gc_class(hb, c, 0, UINT_MAX);

	return n;
}

/* sweep generation: threads with no sweep point of their own (the TCP
 * reactor pool) compare it at a job boundary and flush when it moved */
volatile unsigned long hg_sweep_gen;

unsigned long long hg_hdr_secret;

static inline int hg_floor_stuck(struct hg_block *hb)
{
	return hb->committed_bytes >= hb->hcap || hb->grow_blocked;
}

/* reserve floor, checked on every grid alloc and free: below it the reactor
 * threads are asked to flush (hg_sweep_gen); WARN only if it cannot grow */
void hg_reserve_floor_check(struct hg_block *hb)
{
	if(!hb->buddy_ready || !hb->reserve_floor)
		return;

	if(hb->buddy_free_leaves < hb->reserve_floor) {
		if(!hb->below_floor) {
			hb->below_floor = 1;
			hb->floor_crossings++;
		} else if(hb->floor_warned || !hg_floor_stuck(hb)) {
			return;
		}
		hg_sweep_gen++;
		if(hg_floor_stuck(hb)) {
			hb->floor_warned = 1;
			LM_WARN("%s: free space fell below the reserve floor "
					"(%lu of %lu leaves free, floor %lu); raise -m/-M if "
					"this repeats\n",
					hb->name, hb->buddy_free_leaves,
					hb->npages * hg_leaves_per_page(hb), hb->reserve_floor);
		} else {
			LM_DBG("%s: free space fell below the reserve floor "
				   "(%lu of %lu leaves free, floor %lu); the arena can "
				   "still grow\n",
					hb->name, hb->buddy_free_leaves,
					hb->npages * hg_leaves_per_page(hb), hb->reserve_floor);
		}
	} else if(hb->below_floor
			  && hb->buddy_free_leaves > hb->reserve_floor * 2) {
		hb->below_floor = 0;
		if(hb->floor_warned)
			LM_NOTICE("%s: free space recovered above the reserve floor "
					  "(%lu leaves free)\n",
					hb->name, hb->buddy_free_leaves);
		else
			LM_DBG("%s: free space recovered above the reserve floor "
				   "(%lu leaves free)\n",
					hb->name, hb->buddy_free_leaves);
		hb->floor_warned = 0;
		/* recovery past 2x the floor also ends a grow-blocked episode */
		hg_grow_unblock(hb, "demand fell back below the floor");
	}
}

/* sweep flush: lock per class and per HG_FLUSH_BATCH cells; active classes
 * keep a residue unless below the floor; counters and ticks after */
#define HG_FLUSH_BATCH 64 /* cells per lock hold */
#define HG_FLUSH_KEEP 8	  /* residue cap per active class */

static inline unsigned int flush_residue(
		struct hg_block *hb, struct hg_palloc *pl, int c)
{
	unsigned int keep;

	if(!pl->cls[c].touched || hb->below_floor)
		return 0;
	keep = hb->priv_max[c] / 4;
	return keep > HG_FLUSH_KEEP ? HG_FLUSH_KEEP : keep;
}

/* hand class @c's private chain down to @keep cells; returns cells pushed */
static unsigned int cache_flush_class(
		struct hg_block *hb, struct hg_palloc *pl, int c, unsigned int keep)
{
	unsigned int claimed = pl->cls[c].nfree, n = 0;
	int drained = 0;

	while(!drained) {
		unsigned int b = 0;

		hg_lock_enter(hb, HG_LK_FLUSH);
		hb->gc_deferred = 1;
		while(b < HG_FLUSH_BATCH) {
			void *cur = pl->cls[c].free_head;

			if(!cur) {
				/* chain exhausted; non-zero nfree is counter drift */
				drained = 1;
				break;
			}
			if(keep && pl->cls[c].nfree <= keep) {
				drained = 1;
				break;
			}
			if(!cell_block(hb, c, cur)) {
				/* an overwritten link - stop, leak the rest */
				LM_CRIT("%s: class %d private chain broken at %p - "
						"dropping the rest of the chain\n",
						hb->name, c, cur);
				pl->cls[c].free_head = NULL;
				pl->cls[c].nfree = 0;
				drained = 1;
				break;
			}
			pl->cls[c].free_head = cell_next(cur);
			if(pl->cls[c].nfree)
				pl->cls[c].nfree--;
			if(hg_gpool_push(hb, c, cur) < 0) {
				LM_CRIT("%s: class %d private chain stops at refused cell "
						"%p - dropping the rest of the chain\n",
						hb->name, c, cur);
				pl->cls[c].free_head = NULL;
				pl->cls[c].nfree = 0;
				drained = 1;
				break;
			}
			b++;
		}
		hb->gc_deferred = 0;
		/* only class c's blocks can have drained in this batch */
		if(hb->ndrained[c] > hb->gc_keep[c])
			hb->gc_inline_blocks +=
					hg_gc_class(hb, c, hb->gc_keep[c], UINT_MAX);
		hg_lock_leave(hb);
		n += b;
		/* a batch that ended exactly at the chain end or at the residue
		 * is done: decide here, not by taking one more hold to find out */
		if(!pl->cls[c].free_head || (keep && pl->cls[c].nfree <= keep))
			drained = 1;
	}
	if(!pl->cls[c].free_head && pl->cls[c].nfree) {
		unsigned int left = pl->cls[c].nfree;

		pl->cls[c].nfree = 0;
		LM_CRIT("%s: class %d chain ran dry after %u cells but nfree "
				"claimed %u (%u still claimed at the end) - counter resynced "
				"to 0, pool accounting drifted\n",
				hb->name, c, n, claimed, left);
		hg_corrupt(hb, HG_C_NFREE_UNDERFLOW);
	}
	return n;
}

/* each class's keep: the larger of this cycle's carves and half the last
 * keep, capped in bytes and HG_GC_KEEP_MAX blocks; hb->lock held */
void hg_gc_cycle(struct hg_block *hb)
{
	unsigned long budget = hb->size >> HG_GC_KEEP_BUDGET_SHIFT;
	int c;

	for(c = 0; c < HG_NCLASSES; c++) {
		unsigned int demand = hb->carves_cycle[c], keep = hb->gc_keep[c] / 2;
		unsigned long bsz = hg_chunk_size_for(hb, c);
		unsigned long cap = bsz ? budget / bsz : 0;

		hb->carves_cycle[c] = 0;
		if(demand > keep)
			keep = demand;
		if(cap > HG_GC_KEEP_MAX)
			cap = HG_GC_KEEP_MAX;
		hb->gc_keep[c] = keep > cap ? (unsigned int)cap : keep;
	}
}

/* return up to @budget drained blocks beyond keep, round robin over
 * classes; hb->lock held */
unsigned int hg_gc_tick(struct hg_block *hb, unsigned int budget)
{
	unsigned int done = 0, n;
	int i, c;

	for(i = 0; i < HG_NCLASSES && done < budget; i++) {
		c = (hb->gc_next_class + i) % HG_NCLASSES;
		if(hb->ndrained[c] <= hb->gc_keep[c])
			continue;
		n = hg_gc_class(hb, c, hb->gc_keep[c], budget - done);
		done += n;
	}
	hb->gc_next_class = (hb->gc_next_class + 1) % HG_NCLASSES;
	hb->gc_tick_blocks += done;
	return done;
}

/* does process @proc hold anything in its shared-arena caches since its
 * last flush? Read unlocked: a stale answer costs one interval */
int hg_proc_cache_dirty(int proc)
{
	struct hg_block *hb = (struct hg_block *)hg_shm_block;

	if(!hb || proc < 0)
		return 1;
	return hb->pstat[((unsigned int)(proc + 1)) % HG_STAT_SLOTS].dirty != 0;
}

static __thread unsigned long hg_sweep_seen;

enum hg_flush_mode
{
	HG_FL_SWEEP,  /* the process's idle sweep: a residue is kept */
	HG_FL_THREAD, /* a reactor thread's sweep: the same, see dirty below */
	HG_FL_FORK,	  /* before fork: shared arenas, everything back */
	HG_FL_EXIT	  /* thread exit: every arena, everything back */
};
static void cache_flush(enum hg_flush_mode mode);

void hg_cache_flush_if_due(void)
{
	unsigned long g = hg_sweep_gen;

	if(g == hg_sweep_seen)
		return;
	hg_sweep_seen = g;
	cache_flush(HG_FL_THREAD);
}

/* hand the unused rest of each own chunk to the pool; FRESH cells pool
 * like FREE ones. Returns the cells pushed */
static unsigned int cache_return_bump(struct hg_block *hb, struct hg_palloc *pl)
{
	unsigned int n = 0;
	int c;

	for(c = 0; c < HG_NCLASSES; c++) {
		if(!pl->cls[c].left)
			continue;
		hg_lock_enter(hb, HG_LK_FLUSH);
		hb->gc_deferred = 1;
		while(pl->cls[c].left) {
			char *cs = pl->cls[c].bump;

			pl->cls[c].bump += hg_cell_total_size((unsigned char)c);
			pl->cls[c].left--;
			if(hg_gpool_push(hb, c, cs) < 0) {
				/* refused with a CRIT: leak the rest of the range */
				pl->cls[c].left = 0;
				break;
			}
			n++;
		}
		hb->gc_deferred = 0;
		if(hb->ndrained[c] > hb->gc_keep[c])
			hb->gc_inline_blocks +=
					hg_gc_class(hb, c, hb->gc_keep[c], UINT_MAX);
		hg_lock_leave(hb);
	}
	return n;
}

static const char *const hg_flush_what[] = {
		"idle sweep", "thread sweep", "fork", "thread exit"};

/* flush this thread's caches in the arenas it caches in; must run on the
 * owning thread */
static void cache_flush(enum hg_flush_mode mode)
{
	int i, c, full = (mode == HG_FL_FORK || mode == HG_FL_EXIT);
	unsigned long oom_n, oom_req;

	for(i = 0; i < HG_MAX_INSTANCES; i++) {
		struct hg_palloc *pl = hg_palloc_slot(i);
		struct hg_block *hb = pl->owner;
		unsigned int n = 0;

		if(!hb || (mode == HG_FL_FORK && !hb->shared))
			continue;
		/* clean before the walk, so a free during it is not lost - by the
		 * process's sweep, or a thread with a slot of its own: a thread on
		 * the process's slot must not clear what the main thread holds */
		if(mode == HG_FL_SWEEP || (mode == HG_FL_THREAD && hg_pstat_slot >= 0))
			hg_pstat_mine(hb)->dirty = 0;
		/* the quarantine drains into the chain first, checked as usual */
		if(full && pl->quar)
			hg_quar_drain(hb, pl);
		for(c = 0; c < HG_NCLASSES; c++) {
			unsigned int keep = full ? 0 : flush_residue(hb, pl, c);

			pl->cls[c].touched = 0;
			/* nothing to hand back: empty, or already at its residue */
			if(!pl->cls[c].free_head || (keep && pl->cls[c].nfree <= keep))
				continue;
			n += cache_flush_class(hb, pl, c, keep);
		}
		if(full)
			n += cache_return_bump(hb, pl);
		hg_lock_enter(hb, HG_LK_FLUSH);
		hb->cache_flushes++;
		hb->cells_flushed += n;
		oom_n = hb->shared ? 0 : hg_oom_take(hb, &oom_req);
		/* a private arena's ticks run here: only its owner can shrink it */
		if(!hb->shared) {
			/* only the arena pkg_malloc() serves grows, latches and is
			 * published, not a child's copy of the pre-fork pkg arena */
			int own = (hb == (struct hg_block *)hg_pkg_block);

			if(own)
				hg_grow_blocked_tick(hb);
			/* no maintenance process for a private arena: its sweep is its GC
			 * cycle and tick */
			hg_gc_cycle(hb);
			hg_gc_tick(hb, HG_GC_TICK_BUDGET);
			if(own)
				hg_grow_tick(hb);
			hg_shrink_tick(hb);
			(void)hg_get_real_used(hb); /* samples the live peak */
			if(own)
				hg_pkg_publish(hb);
		}
		hg_lock_leave(hb);
		if(oom_n)
			hg_oom_report(hb, oom_n, oom_req);
		if(n)
			LM_DBG("%s: %s returned %u cached cells\n", hb->name,
					hg_flush_what[mode], n);
	}
}

void hg_cache_flush_self(void)
{
	cache_flush(HG_FL_SWEEP);
}

/* before fork(): the child gets a copy of this thread's caches, so the
 * shared-arena cells in them go back to the pool first */
void hg_cache_fork_prepare(void)
{
	cache_flush(HG_FL_FORK);
}

/* thread exit: everything back, residue and bump remainder included */
void hg_cache_drain_self(void)
{
	cache_flush(HG_FL_EXIT);
}

#endif /* HG_MALLOC */
