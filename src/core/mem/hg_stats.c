/*
 * slab allocator with elastic arenas - statistics and live-cell walk
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

#include <stdlib.h>

#include "hg_malloc.h"
#include "hg_arena_int.h"
#include "../dprint.h"

/* carved but unused cell bytes: free, as qm/fm report free fragments */
unsigned long hg_slab_recycled(struct hg_block *hb)
{
	unsigned long capacity, live;

	/* running sum kept at carve and GC, read unlocked (a sample) */
	capacity = hb->slab_capacity;
	live = hg_cell_live(hb);
	return capacity > live ? capacity - live : 0;
}

void hg_arena_stats(
		struct hg_block *hb, unsigned int *nchunks, unsigned long *bytes)
{
	hg_lock_enter(hb, HG_LK_STATS);
	*nchunks = hb->nchunks;
	*bytes = hb->real_used;
	hg_lock_leave(hb);
}

/* live-cell walk: free cells into a system-malloc hash set, then every
 * cell not in it; exact before fork only */
/* the free-cell set: open addressing in one allocation, load 0.5 */
struct hg_free_set
{
	void **slots; /* NULL = empty; power-of-two count */
	unsigned int nslots;
	unsigned int nused;
	unsigned int overflow; /* inserts refused - must stay 0, see below */
};

static inline unsigned int hg_ptr_hash(const void *p, unsigned int nbuckets)
{
	unsigned long v = (unsigned long)p;

	v ^= v >> 16;
	v *= 2654435761UL; /* Knuth multiplicative hash */
	v ^= v >> 13;
	return (unsigned int)(v & (nbuckets - 1));
}

static int hg_free_set_init(struct hg_free_set *set, unsigned int expected)
{
	unsigned int n = 64;

	while(n < expected * 2 && n < (1U << 24))
		n <<= 1;
	set->slots = calloc(n, sizeof(*set->slots));
	if(!set->slots)
		return -1;
	set->nslots = n;
	set->nused = 0;
	set->overflow = 0;
	return 0;
}

static void hg_free_set_add(struct hg_free_set *set, void *addr)
{
	unsigned int h = hg_ptr_hash(addr, set->nslots);

	/* full: a free list is longer than its counter; refuse and flag it */
	if(set->nused * 2 >= set->nslots) {
		set->overflow++;
		return;
	}
	while(set->slots[h]) {
		if(set->slots[h] == addr)
			return;
		h = (h + 1) & (set->nslots - 1);
	}
	set->slots[h] = addr;
	set->nused++;
}

static int hg_free_set_has(struct hg_free_set *set, void *addr)
{
	unsigned int h = hg_ptr_hash(addr, set->nslots);

	while(set->slots[h]) {
		if(set->slots[h] == addr)
			return 1;
		h = (h + 1) & (set->nslots - 1);
	}
	return 0;
}

static void hg_free_set_destroy(struct hg_free_set *set)
{
	free(set->slots);
	set->slots = NULL;
}

/* exact number of cells on every free list this process can see, counted
 * under hb->lock so the table can be sized once and never grown */
static unsigned int hg_free_set_count(struct hg_block *hb)
{
	struct hg_palloc *pl = hg_palloc_mine(hb);
	unsigned int n = 0;
	void *cur;
	int c;

	/* the pool's free cells are read from each block's map by the
	 * walker itself; only this thread's private cells need the set */
	if(pl)
		for(c = 0; c < HG_NCLASSES; c++)
			for(cur = pl->cls[c].free_head; cur; cur = cell_next(cur))
				n++;
	return n;
}

static void hg_free_set_populate(struct hg_block *hb, struct hg_free_set *set)
{
	int c;
	void *cur;
	struct hg_palloc *pl = hg_palloc_mine(hb);

	/* the pool's free cells come from the per-block map (see the
	 * walk); the set is this thread's private chains only */
	for(c = 0; c < HG_NCLASSES; c++) {
		if(pl)
			for(cur = pl->cls[c].free_head; cur; cur = cell_next(cur))
				hg_free_set_add(set, cur);
	}
}

void hg_arena_walk_live(
		struct hg_block *hb, void (*cb)(void *payload, void *ctx), void *ctx)
{
	struct hg_free_set set;
	struct hg_chunk *ch;
	unsigned int i, total_free = 0;
	char *cell_start;

	hg_lock_enter(hb, HG_LK_STATS);

	/* count exactly, private cache included, under the same lock */
	total_free = hg_free_set_count(hb);

	if(hg_free_set_init(&set, total_free) < 0) {
		hg_lock_leave(hb);
		LM_ERR("%s: out of memory building the live-cell diagnostic "
			   "set - skipping the walk\n",
				hb->name);
		return;
	}
	hg_free_set_populate(hb, &set);

	/* must not happen; say so rather than dump free cells as live */
	if(set.overflow)
		LM_CRIT("%s: live-cell set overflowed by %u - the dump below "
				"may report free cells as live\n",
				hb->name, set.overflow);

	for(ch = hb->chunks; ch; ch = ch->next) {
		for(i = 0; i < ch->cells; i++) {
			cell_start = (char *)ch + sizeof(struct hg_chunk)
						 + (unsigned long)i * ch->cell_size;
			/* pool-free by the block's own map, exact and free of
			 * any allocation; private-free by the set */
			if(map_test(ch, (long)i))
				continue;
			/* and only LIVE or LIVE_G cells: cells in another process's cache,
			 * or never handed out, are not */
			if(!hg_st_live(hg_hdr_state(cell_start)))
				continue;
			if(!hg_free_set_has(&set, cell_start))
				cb(cell_start + HG_CELL_HDR, ctx);
		}
	}

	hg_free_set_destroy(&set);
	hg_lock_leave(hb);
}

/* statistics: the shm arena's figures as "hgmem" counters */

#include "../counters.h"

/* this thread's stats slot, -1 = the process's own (see hg_pstat_mine) */
__thread int hg_pstat_slot = -1;

void hg_thread_stats_init(void)
{
	struct hg_block *hb = (struct hg_block *)hg_shm_block;
	static unsigned int local_next = HG_STAT_SLOTS - 1;
	static int said;
	/* the process slots are 1..nprocs: a thread slot must lie above them */
	unsigned int floor = hg_nprocs() + 1, *next, s;

	if(hg_pstat_slot >= 0)
		return;
	/* the shared slot counter when shm is ours, else per process */
	next = (hb && HG_SERVES_SHM()) ? &hb->thread_slot_next : &local_next;
	s = __atomic_fetch_sub(next, 1, __ATOMIC_ACQ_REL);
	if(s <= floor || s >= HG_STAT_SLOTS) {
		/* out of thread slots: share the process slot */
		if(!said) {
			said = 1;
			LM_WARN("no free per-thread stats slot (%u processes, %u slots) - "
					"this thread shares its process's slot\n",
					floor - 1, HG_STAT_SLOTS);
		}
		return;
	}
	hg_pstat_slot = (int)s;
}

/* --- lock-stall bookkeeping (called after the release) ----------------- */
const char *const hg_lk_reason_str[HG_LK_REASONS] = {
		"refill", "return", "flush", "large", "policy", "stats"};

void hg_lock_assert_held(struct hg_block *hb, const char *where)
{
	if(hb->lk_cur != HG_LK_NONE)
		return;
	LM_BUG("%s: %s without hb->lock held - a lock-dropping callee broke its "
		   "contract (hold on entry, held again on return)",
			hb->name, where);
	hg_corrupt(hb, HG_C_INTERNAL);
}

void hg_lock_stall_note(struct hg_block *hb, int reason, unsigned long held_ns)
{
	/* once per second per process: a stall storm must not become a log
	 * storm; the counters and the deferred warning carry the magnitude */
	static unsigned long last_warn_ns;
	unsigned long now = hg_now_ns();

	if(last_warn_ns && now - last_warn_ns < 1000000000UL)
		return;
	last_warn_ns = now;
	LM_WARN("%s arena lock held for %lu us by the %s path (threshold "
			"hg_lock_stall_us=%d; %lu stalls so far, worst %lu us by %s in "
			"process %d)\n",
			hb->name, held_ns / 1000,
			reason >= 0 && reason < HG_LK_REASONS ? hg_lk_reason_str[reason]
												  : "?",
			hg_lock_stall_us, hb->lk_stalls, hb->lk_worst_ns / 1000,
			hb->lk_worst_reason >= 0 && hb->lk_worst_reason < HG_LK_REASONS
					? hg_lk_reason_str[hb->lk_worst_reason]
					: "?",
			hb->lk_worst_proc);
}

enum hg_stat_field
{
	HGS_TIER = 0,
	HGS_TOTAL,
	HGS_PINNED_BYTES,
	HGS_CARVED,
	HGS_CARVED_PEAK,
	HGS_CHUNKS,
	HGS_FREE_TO_CARVE,
	HGS_LIVE,
	HGS_LIVE_PEAK,
	HGS_PAYLOAD,
	HGS_CELLS,
	HGS_SLAB_LIVE,
	HGS_SLAB_RECYCLED,
	/* reclaim rates as counters */
	HGS_BLOCKS_CARVED,
	HGS_BLOCKS_RETURNED,
	HGS_GC_PASSES,
	HGS_CACHE_FLUSHES,
	HGS_CELLS_FLUSHED,
	HGS_PRESSURE_FLUSHES,
	HGS_PRESSURE_CELLS,
	HGS_GC_TICK_BLOCKS,
	HGS_GC_INLINE_BLOCKS,
	HGS_GC_KEPT_BLOCKS,
	HGS_SWEEPS_SKIPPED,
	HGS_BUDDY_SPLITS,
	HGS_BUDDY_MERGES,
	HGS_BUDDY_FREE_LEAVES,
	/* large tier footprint, and the reserve floor */
	HGS_LARGE_BACKING,
	HGS_LARGE_LIVE,
	HGS_LARGE_RECYCLED,
	HGS_LARGE_CHUNKS_CARVED,
	HGS_LARGE_CHUNKS_RETURNED,
	HGS_RESERVE_FLOOR,
	HGS_BELOW_FLOOR,
	HGS_FLOOR_CROSSINGS,
	/* one number an operator can alert on */
	HGS_CORRUPTION,
	/* elastic arena: committed, cap and the grow ledger; grow_refused
	 * rising means demand hit the cap or the host */
	HGS_COMMITTED,
	HGS_CAP,
	HGS_GROWS,
	HGS_GROW_BYTES,
	HGS_GROW_REFUSED,
	HGS_SHRINKS,
	HGS_SHRINK_BYTES,
	HGS_PUNCHES,
	HGS_PUNCH_BYTES,
	HGS_PUNCHED_PAGES,
	HGS_WHOLE_FREE_PAGES,
	HGS_REPINS,
	HGS_REPIN_BYTES,
	HGS_REPIN_FAILED,
	HGS_HOLE_REGROWS,
	/* 1 while a host-side refusal is latched (a cap refusal never latches) */
	HGS_GROW_BLOCKED,
	HGS_LOCK_STALLS,
	HGS_LOCK_HOLD_MAX_US,
	HGS_GROWS_PROACTIVE,
	HGS_GROWS_EXHAUSTION,
};

static unsigned long hg_shm_stat(void *ctx)
{
	struct hg_block *hb = (struct hg_block *)hg_shm_block;

	if(!hb)
		return 0;
	switch((enum hg_stat_field)(long)ctx) {
		case HGS_TIER:
			return hb->tier;
		case HGS_TOTAL:
			return hb->size;
		/* in bytes, like the other counters */
		case HGS_PINNED_BYTES:
			return (unsigned long)hb->locked_mb << 20;
		/* carved: slab blocks and large chunks; falls as GC returns blocks */
		case HGS_CARVED:
			return hb->real_used;
		case HGS_CARVED_PEAK:
			return hb->max_real_used;
		case HGS_CHUNKS:
			return hb->nchunks;
		case HGS_FREE_TO_CARVE:
			return hb->size > hb->real_used ? hb->size - hb->real_used : 0;
		/* live: handed out now; the gap to carved is cached free cells */
		case HGS_LIVE:
			return hg_get_real_used(hb);
		case HGS_LIVE_PEAK:
			return hb->max_live_used;
		case HGS_PAYLOAD:
			return hg_used(hb);
		case HGS_CELLS:
			return hg_fragments(hb);
		/* slab only: large allocations go to the boundary-tag tier and never
		 * appear here, so this is not comparable with payload */
		case HGS_SLAB_LIVE:
			return hg_cell_live(hb);
		case HGS_SLAB_RECYCLED:
			return hg_slab_recycled(hb);
		/* blocks carved and returned; the ratio shows how reclaim keeps up */
		case HGS_BLOCKS_CARVED:
			return hb->blocks_carved;
		case HGS_BLOCKS_RETURNED:
			return hb->gc_blocks_returned;
		case HGS_GC_PASSES:
			return hb->gc_passes;
		case HGS_CACHE_FLUSHES:
			return hb->cache_flushes;
		case HGS_CELLS_FLUSHED:
			return hb->cells_flushed;
		case HGS_PRESSURE_FLUSHES:
			return hb->pressure_flushes;
		case HGS_PRESSURE_CELLS:
			return hb->pressure_cells;
		case HGS_GC_TICK_BLOCKS:
			return hb->gc_tick_blocks;
		case HGS_GC_INLINE_BLOCKS:
			return hb->gc_inline_blocks;
		case HGS_SWEEPS_SKIPPED:
			return hb->sweeps_skipped;
		case HGS_GC_KEPT_BLOCKS: {
			unsigned long n = 0;
			int c;

			for(c = 0; c < HG_NCLASSES; c++)
				n += hb->ndrained[c];
			return n;
		}
		case HGS_BUDDY_SPLITS:
			return hb->buddy_splits;
		case HGS_BUDDY_MERGES:
			return hb->buddy_merges;
		case HGS_BUDDY_FREE_LEAVES:
			return hb->buddy_free_leaves;
		case HGS_LARGE_BACKING:
			return hb->large_backing;
		case HGS_LARGE_LIVE:
			return hb->large_live;
		case HGS_LARGE_RECYCLED:
			return hg_large_recycled(hb);
		case HGS_LARGE_CHUNKS_CARVED:
			return hb->large_chunks_carved;
		case HGS_LARGE_CHUNKS_RETURNED:
			return hb->large_chunks_returned;
		case HGS_RESERVE_FLOOR:
			return hb->reserve_floor;
		case HGS_BELOW_FLOOR:
			return hb->below_floor;
		case HGS_FLOOR_CROSSINGS:
			return hb->floor_crossings;
		case HGS_CORRUPTION:
			return hg_corrupt_total(hb);
		case HGS_COMMITTED:
			return hb->committed_bytes;
		case HGS_CAP:
			return hb->hcap;
		case HGS_GROWS:
			return hb->grows;
		case HGS_GROW_BYTES:
			return hb->grow_bytes;
		case HGS_GROW_REFUSED:
			return hb->grow_refused;
		case HGS_GROW_BLOCKED:
			return hb->grow_blocked;
		case HGS_LOCK_STALLS:
			return hb->lk_stalls;
		case HGS_LOCK_HOLD_MAX_US:
			return hb->lk_worst_ns / 1000;
		case HGS_GROWS_PROACTIVE:
			return hb->grows_proactive;
		case HGS_GROWS_EXHAUSTION:
			return hb->grows_exhaustion;
		case HGS_SHRINKS:
			return hb->shrinks;
		case HGS_SHRINK_BYTES:
			return hb->shrink_bytes;
		case HGS_PUNCHES:
			return hb->punches;
		case HGS_PUNCH_BYTES:
			return hb->punch_bytes;
		case HGS_PUNCHED_PAGES:
			return hb->punched_pages;
		case HGS_WHOLE_FREE_PAGES:
			return hb->nfree[hb->buddy_top];
		case HGS_REPINS:
			return hb->repins;
		case HGS_REPIN_BYTES:
			return hb->repin_bytes;
		case HGS_REPIN_FAILED:
			return hb->repin_failed;
		case HGS_HOLE_REGROWS:
			return hb->hole_regrows;
	}
	return 0;
}

static const struct
{
	const char *name;
	enum hg_stat_field field;
} hg_stat_defs[] = {{"hg_shm_tier", HGS_TIER}, {"hg_shm_total_size", HGS_TOTAL},
		{"hg_shm_pinned_bytes", HGS_PINNED_BYTES},
		{"hg_shm_carved", HGS_CARVED}, {"hg_shm_carved_peak", HGS_CARVED_PEAK},
		{"hg_shm_chunks", HGS_CHUNKS},
		{"hg_shm_free_to_carve", HGS_FREE_TO_CARVE}, {"hg_shm_live", HGS_LIVE},
		{"hg_shm_live_peak", HGS_LIVE_PEAK},
		{"hg_shm_live_payload", HGS_PAYLOAD}, {"hg_shm_live_cells", HGS_CELLS},
		{"hg_shm_slab_live", HGS_SLAB_LIVE},
		{"hg_shm_slab_recycled", HGS_SLAB_RECYCLED},
		{"hg_shm_blocks_carved", HGS_BLOCKS_CARVED},
		{"hg_shm_blocks_returned", HGS_BLOCKS_RETURNED},
		{"hg_shm_gc_passes", HGS_GC_PASSES},
		{"hg_shm_cache_flushes", HGS_CACHE_FLUSHES},
		{"hg_shm_cells_flushed", HGS_CELLS_FLUSHED},
		{"hg_shm_pressure_flushes", HGS_PRESSURE_FLUSHES},
		{"hg_shm_pressure_cells", HGS_PRESSURE_CELLS},
		{"hg_shm_gc_tick_blocks", HGS_GC_TICK_BLOCKS},
		{"hg_shm_gc_inline_blocks", HGS_GC_INLINE_BLOCKS},
		{"hg_shm_gc_kept_blocks", HGS_GC_KEPT_BLOCKS},
		{"hg_shm_sweeps_skipped", HGS_SWEEPS_SKIPPED},
		{"hg_shm_buddy_splits", HGS_BUDDY_SPLITS},
		{"hg_shm_buddy_merges", HGS_BUDDY_MERGES},
		{"hg_shm_buddy_free_leaves", HGS_BUDDY_FREE_LEAVES},
		{"hg_shm_large_backing", HGS_LARGE_BACKING},
		{"hg_shm_large_live", HGS_LARGE_LIVE},
		{"hg_shm_large_recycled", HGS_LARGE_RECYCLED},
		{"hg_shm_large_chunks_carved", HGS_LARGE_CHUNKS_CARVED},
		{"hg_shm_large_chunks_returned", HGS_LARGE_CHUNKS_RETURNED},
		{"hg_shm_reserve_floor", HGS_RESERVE_FLOOR},
		{"hg_shm_below_floor", HGS_BELOW_FLOOR},
		{"hg_shm_floor_crossings", HGS_FLOOR_CROSSINGS},
		{"hg_shm_corruption", HGS_CORRUPTION},
		{"hg_shm_committed", HGS_COMMITTED}, {"hg_shm_cap", HGS_CAP},
		{"hg_shm_grows", HGS_GROWS}, {"hg_shm_grow_bytes", HGS_GROW_BYTES},
		{"hg_shm_grow_refused", HGS_GROW_REFUSED},
		{"hg_shm_grow_blocked", HGS_GROW_BLOCKED},
		{"hg_shm_lock_stalls", HGS_LOCK_STALLS},
		{"hg_shm_lock_hold_max_us", HGS_LOCK_HOLD_MAX_US},
		{"hg_shm_grows_proactive", HGS_GROWS_PROACTIVE},
		{"hg_shm_grows_exhaustion", HGS_GROWS_EXHAUSTION},
		{"hg_shm_shrinks", HGS_SHRINKS},
		{"hg_shm_shrink_bytes", HGS_SHRINK_BYTES},
		{"hg_shm_punches", HGS_PUNCHES},
		{"hg_shm_punch_bytes", HGS_PUNCH_BYTES},
		{"hg_shm_punched_pages", HGS_PUNCHED_PAGES},
		{"hg_shm_whole_free_pages", HGS_WHOLE_FREE_PAGES},
		{"hg_shm_repins", HGS_REPINS}, {"hg_shm_repin_bytes", HGS_REPIN_BYTES},
		{"hg_shm_repin_failed", HGS_REPIN_FAILED},
		{"hg_shm_hole_regrows", HGS_HOLE_REGROWS}, {NULL, 0}};

static counter_val_t hg_shm_cnt(counter_handle_t h, void *param)
{
	return (counter_val_t)hg_shm_stat(param);
}

static counter_val_t hg_pkg_cnt(counter_handle_t h, void *param)
{
	return (counter_val_t)hg_pkg_pub_stat(param);
}

/* the "hgmem" counter group: shm arena figures and two pkg sums, registered
 * before the counters are set up for children */
int hg_register_stats(void)
{
	int i;

#ifdef PKG_MALLOC
	if(HG_SERVES_PKG()
			&& (counter_register(NULL, "hgmem", "hg_pkg_grow_refused",
						CNT_F_NO_RESET, hg_pkg_cnt, (void *)0L,
						"pkg arena grows refused, all processes",
						0) < 0
					|| counter_register(NULL, "hgmem", "hg_pkg_grow_blocked",
							   CNT_F_NO_RESET, hg_pkg_cnt, (void *)1L,
							   "processes whose pkg arena cannot grow", 0)
							   < 0)) {
		LM_ERR("failed to add the pkg growth statistics\n");
		return -1;
	}
#endif

	if(!HG_SERVES_SHM())
		return 0; /* a different allocator is in use - nothing to say */

	for(i = 0; hg_stat_defs[i].name; i++) {
		if(counter_register(NULL, "hgmem", hg_stat_defs[i].name, CNT_F_NO_RESET,
				   hg_shm_cnt, (void *)(long)hg_stat_defs[i].field,
				   "hg shm arena", 0)
				< 0) {
			LM_ERR("failed to add the %s statistic\n", hg_stat_defs[i].name);
			return -1;
		}
	}
	return 0;
}

#endif /* HG_MALLOC */
