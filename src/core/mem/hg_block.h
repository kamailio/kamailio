/*
 * slab allocator with elastic arenas - arena block
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

#ifndef hg_block_h
#define hg_block_h

#include "hg_cell.h"

/* per-thread private state for one arena, looked up per block */
struct hg_quar; /* hg_dbg.c: the debug checks' quarantine */

struct hg_palloc
{
	struct hg_block *owner; /* which block this state belongs to */
	/* this thread's quarantine for the block, mapped on the first free
	 * with the debug checks on (hgdbg); NULL otherwise */
	struct hg_quar *quar;
	struct
	{
		char *bump; /* next unused cell in own chunk */
		unsigned int left;
		void *free_head; /* private LIFO free stack */
		unsigned int nfree;
		/* allocated from since the last sweep: the flush leaves a residue */
		unsigned int touched;
	} cls[HG_NCLASSES];
};

/* per-process (and per-thread) used/fragments counters in their own cache
 * line slots, signed (frees cross processes); readers sum them. Process
 * @n uses slot n + 1, threads take slots above the process count */
#define HG_STAT_SLOTS 1024
#define HG_STAT_LINE 64

struct hg_pstat
{
	long used;		/* payload bytes handed out by this process */
	long fragments; /* live cells handed out by this process */
	/* set when its owner's free lists gain cells (free, refill, quarantine
	 * drain), cleared when its sweep starts */
	long dirty;
	/* slot bytes handed out by this process, for hg_slab_recycled() */
	long cell_live;
	char _pad[HG_STAT_LINE - 4 * sizeof(long)];
} __attribute__((aligned(HG_STAT_LINE)));
HG_STATIC_ASSERT(sizeof(struct hg_pstat) == HG_STAT_LINE,
		"a stats slot must fill exactly one line");

/* corruption kinds, grouped by what an operator would do about them */
enum hg_corrupt_kind
{
	HG_C_CLASS_MISMATCH = 0, /* cell resolves to a block of another class */
	HG_C_DOUBLE_FREE,		 /* block already fully free, or buddy re-freed */
	HG_C_NFREE_UNDERFLOW,	 /* free list shorter than nfree claims */
	HG_C_BAD_CLASS,			 /* cell header carries an impossible class id */
	HG_C_FOREIGN_PTR,		 /* pointer belongs to no live arena */
	HG_C_BUDDY_BAD_FREE,	 /* outside the grid, misaligned, wrong order */
	HG_C_INTERNAL,			 /* API misuse or accounting out of step: a bug */
	HG_C_HEADER,			 /* checksum fails: an overflow, or not a cell */
	HG_C_USE_AFTER_FREE,	 /* hgdbg: a freed cell was written to */
	HG_C_OVERFLOW,			 /* written past the request (hgdbg, mem_guard) */
	HG_CORRUPT_KINDS
};


/* what a hold of hb->lock is for, in the longest-hold and stall reports;
 * holds over hg_lock_stall_us count as stalls */
enum hg_lock_reason
{
	HG_LK_REFILL = 0, /* class refill: gpool pop, chunk carve, grow */
	HG_LK_RETURN,	  /* cell return: gpool push, donate, gc_class */
	HG_LK_FLUSH,	  /* per-process cell-cache flush (sweep / floor) */
	HG_LK_LARGE,	  /* large tier alloc / free */
	HG_LK_POLICY,	  /* grow/shrink ticks */
	HG_LK_STATS,	  /* statistics walks, live-cell enumeration */
	HG_LK_REASONS
};
#define HG_LK_NONE (-1) /* lk_cur while hb->lock is not held */

enum hg_grow_why
{
	HG_GROW_EXHAUSTION = 0, /* a request found nothing to carve */
	HG_GROW_PROACTIVE		/* the headroom rule, ahead of demand */
};

/* what refused a grow (hg_grow_refusal_str()) */
enum hg_grow_refusal
{
	HG_GR_NONE = 0,		/* never refused */
	HG_GR_CAP,			/* the -m/-M reservation is spent */
	HG_GR_WAIT_TIMEOUT, /* another grow stayed in flight too long */
	HG_GR_HOST_FLOOR,	/* the host would drop under the RAM floor */
	HG_GR_CGROUP,		/* the cgroup would drop under its floor */
	HG_GR_MEMLOCK,		/* RLIMIT_MEMLOCK cannot cover the mlock() */
	HG_GR_HUGETLB_POOL, /* the hugetlb pool has too few free pages */
	HG_GR_MLOCK,		/* mlock() failed, neither of the above */
	HG_GR_HOLE_REPIN,	/* no punched hugetlb page could be re-pinned */
	HG_GR_REASONS
};

struct hg_block
{
	/* fields the lock-free fast path reads, set once at init, on lines the
	 * slow path never writes */
	char *name; /* purpose of this memory block */
	/* the reservation this block owns; hb lives at its start */
	char *hbase;
	unsigned long hcap; /* VA reserved, >= span_end; immutable */
	/* per-thread cache bound per class, in cells (private_caps_init()) */
	unsigned int priv_max[HG_NCLASSES];
	unsigned int priv_donate[HG_NCLASSES];
	/* upper bound on one chunk, derived from the arena size at init -
	 * see chunk_size_for() in hg_arena.c */
	unsigned int chunk_max;
	/* the debug checks (hgdbg) serve this arena: tail red zones, poisoned frees
	 * and a quarantine (see hg_dbg.h). Set at creation, never changed. */
	unsigned int dbg;
	/* bytes of freed cells one thread may hold in quarantine, when dbg */
	unsigned long quar_bytes;

	/* slow paths only; alone on its line with the holder's bookkeeping */
	gen_lock_t lock __attribute__((aligned(64)));
	unsigned long lk_t0;
	int lk_cur;

	/* --- everything from here on is written under hb->lock --- */
	struct hg_chunk *chunks __attribute__((aligned(64)));
	unsigned int nchunks;
	/* sum of chunk capacities, so hg_slab_recycled() is O(1) */
	unsigned long slab_capacity;
	/* shared free cells per class as blocks graded by fullness; drained
	 * blocks are on drained[] */
	struct hg_chunk *bucket[HG_NCLASSES][HG_GRADES];

	/* large tier chunks, one shared free list; under hb->lock */
	struct hg_large_chunk *large_chunks;
	struct hg_lfrag *large_free;
	/* large tier: backing held from the buddy and the part handed out */
	unsigned long large_backing;
	unsigned long large_live;
	/* chunk churn counters */
	unsigned long large_chunks_carved;
	unsigned long large_chunks_returned;

	/* bytes carved into chunks (the footprint); under hb->lock */
	unsigned long real_used;
	unsigned long max_real_used;
	/* peak of the live figure, sampled each second (shared) or each sweep */
	unsigned long max_live_used;

	/* the fast-path counters - see struct hg_pstat above. Summed by
	 * hg_used()/hg_fragments(); never read directly. */
	struct hg_pstat pstat[HG_STAT_SLOTS];
	unsigned int thread_slot_next; /* next thread slot, counts down */
	unsigned long size;			   /* total arena size */

	/* span_end is the address extent of the grid, committed_bytes what is
	 * backed; they differ once a page is punched */
	unsigned long span_end;
	unsigned long committed_bytes;
	volatile unsigned long hoff;
	enum hg_mem_tier tier;
	unsigned long locked_mb;
	/* init could not mlock: grow unpinned too */
	unsigned int unpinned;
	/* MAP_SHARED or MAP_PRIVATE; a pkg delta costs delta x nproc */
	int shared;

	/* elastic arena: hcap is reserved and mapped before fork, growth only
	 * commits more of it; hcap == committed_bytes is a fixed arena */
	unsigned long committed_min; /* committed_bytes at init: shrink floor */
	unsigned long grow_granule;	 /* bytes per grow step, hps multiple */
	unsigned long shrink_step;	 /* max bytes released per drain tick */
	unsigned long grows;		 /* successful commits */
	unsigned long grow_bytes;	 /* their total */
	unsigned long grow_refused;	 /* refusals (cap or resource) */
	/* allocation failure log throttle (hg_oom_take()) */
	unsigned long oom_fails;
	unsigned long oom_fails_said;
	unsigned long oom_max_req;
	unsigned int oom_episode;
	unsigned long shrinks;		/* successful releases */
	unsigned long shrink_bytes; /* their total */
	/* interior release: punched pages and their re-pin by the carver */
	unsigned long punches; /* interior pages released */
	unsigned long punch_bytes;
	unsigned long punched_pages; /* gauge: currently punched */
	unsigned long repins;		 /* punched pages carved again */
	unsigned long repin_bytes;
	unsigned long repin_failed; /* ...and could not be re-pinned */
	unsigned int repin_said;	/* one warning per episode */
	unsigned long hole_regrows; /* grow events served from holes */
	/* consecutive quiet sweep ticks; any grow resets it */
	unsigned int shrink_quiet;
	/* quiet window reached: release one step per maintenance tick */
	unsigned int draining;
	/* release failed structurally: no more shrinking for this arena */
	unsigned int shrink_unsupported;
	/* one log line per refusal episode */
	unsigned int grow_refuse_said;
	/* what refused the last grow (enum hg_grow_refusal) */
	unsigned int grow_refuse_why;
	unsigned int grow_refuse_mlock;
	/* the maintenance process is alive and owns the proactive ticks */
	unsigned int maint_active;
	unsigned int grow_inflight; /* 0/1 */
	/* bumped by every grow and hole re-grow that published pages */
	unsigned long grow_gen;
	unsigned long grow_waits;		  /* workers that waited for a grow */
	unsigned long grow_wait_timeouts; /* ...and gave up (refused) */
	unsigned long lk_worst_ns;		  /* longest hold ever, and whose */
	int lk_worst_reason;
	int lk_worst_proc;
	unsigned long lk_stalls;		/* holds over hg_lock_stall_us */
	unsigned long lk_last_stall_ns; /* most recent stall, for the */
	int lk_last_stall_reason;		/* deferred warning */
	int lk_last_stall_proc;
	unsigned int lk_stall_event_due;
	/* growth attribution: the tick, or an exhausted request */
	unsigned long grows_proactive;
	unsigned long grows_exhaustion;
	/* deferred collapse (tier 3): pages a worker's grow published on 4 KB
	 * for the maintenance process to collapse - see hg_collapse_deferred() */
	unsigned int collapse_pending;
	unsigned long collapse_wanting;		/* pages still flagged */
	unsigned long collapse_scan;		/* next page to look at */
	unsigned long collapse_deferred;	/* pages handed over */
	unsigned long collapsed_late;		/* pages it collapsed */
	unsigned long collapse_retries;		/* failed tries, retried */
	unsigned long collapse_late_failed; /* pages left on 4 KB */
	unsigned int collapse_fail_said;	/* first failure logged */
	/* grow-blocked: host-side refusals only, latched when refused again after
	 * a full GC pass; cleared by a grow or by recovery above the floor */
	unsigned int grow_blocked;
	unsigned long grow_blocked_mark;
	/* grow_refused at arming: the sweep latches when nothing is reclaimable */
	unsigned long grow_blocked_refuse0;
	unsigned int grow_event_due;
	/* bytes per achieved backing tier (enum hg_mem_tier) */
	unsigned long tier_bytes[HG_MEM_4K + 1];

	/* page grid: page 0 at pbase (hbase rounded up to hps), hps probed */
	unsigned long hps;		  /* huge page size, probed at reserve time */
	unsigned int hps_shift;	  /* log2(hps), so page-of is a shift */
	char *pbase;			  /* page 0 - hbase rounded up to hps */
	unsigned long npages;	  /* whole pages from pbase to the span end */
	unsigned long npages_cap; /* whole pages to the cap, all described */

	/* buddy state, written under hb->lock only */
	struct hg_page *pages;						  /* npages descriptors */
	struct hg_free_blk *bfree[HG_MAX_ORDERS + 1]; /* free list per order */
	unsigned long nfree[HG_MAX_ORDERS + 1];		  /* its length, per order */
	/* whole-free pages, one bit each; nfree[top] is its population count */
	unsigned long *wfree;
	unsigned long wfree_words;
	unsigned long buddy_free_leaves; /* free leaves, whole arena */
	unsigned int buddy_top;			 /* whole-page order, cached */
	unsigned int buddy_ready;		 /* 0 until hg_buddy_init() */

	/* GC queue: blocks with every cell in the shared pool, per class */
	struct hg_chunk *drained[HG_NCLASSES];
	/* the oldest drained block, O(1) */
	struct hg_chunk *drained_tail[HG_NCLASSES];
	unsigned int ndrained[HG_NCLASSES];
	unsigned long gc_blocks_returned; /* lifetime, for stats */
	unsigned long gc_passes;
	/* drained blocks kept per class, from its carves per cycle, halved when
	 * idle, capped in bytes and HG_GC_KEEP_MAX */
	unsigned int gc_keep[HG_NCLASSES];
	unsigned int carves_cycle[HG_NCLASSES];
	unsigned int gc_next_class; /* tick's round-robin start */
	/* who returned blocks: the maintenance tick or a trigger site */
	unsigned long gc_tick_blocks;
	unsigned long gc_inline_blocks;
	unsigned long sweeps_skipped;	/* clean, not dispatched */
	unsigned long cache_flushes;	/* sweeps run */
	unsigned long cells_flushed;	/* cells recovered from TLS */
	unsigned long pressure_flushes; /* self-flushes at carve exhaustion */
	unsigned long pressure_cells;	/* cells those handed back */
	/* corruption detections per kind */
	unsigned long corrupt[HG_CORRUPT_KINDS];
	/* frees routed to another arena than the caller named, by every
	 * process (shm arena only, atomic) */
	unsigned long xarena_frees;
	unsigned long buddy_splits;	 /* blocks split down an order */
	unsigned long buddy_merges;	 /* blocks merged with a buddy, after init */
	unsigned long blocks_carved; /* class blocks cut, lifetime */
	/* set while a flush walks a cache chain: pushing a cell can reclaim its
	 * block, and the next cell on the chain may live in that same block */
	unsigned int gc_deferred;
	/* free leaves below which a sweep is triggered (1/16 of the grid) */
	unsigned long reserve_floor;
	unsigned int below_floor;  /* hysteresis: already reported */
	unsigned int floor_warned; /* this crossing was a WARN */
	unsigned long floor_crossings;

	unsigned char size2class[(HG_CELL_MAX / HG_ROUNDTO) + 1];
} __attribute__((aligned(HG_ROUNDTO)));

/* address to page/leaf, verified by pages_init() at every arena init */

/* how many leaves tile one huge page */
static inline unsigned long hg_leaves_per_page(const struct hg_block *hb)
{
	return 1UL << (hb->hps_shift - HG_LEAF_SHIFT);
}

/* is @p page-addressable? The header region before pbase is not */
static inline int hg_in_pages(const struct hg_block *hb, const void *p)
{
	return (const char *)p >= hb->pbase
		   && (const char *)p < hb->pbase + (hb->npages << hb->hps_shift);
}

/* page index of @p; only meaningful when hg_in_pages() */
static inline unsigned long hg_page_of(const struct hg_block *hb, const void *p)
{
	return (unsigned long)((const char *)p - hb->pbase) >> hb->hps_shift;
}

/* first byte of the page holding @p */
static inline char *hg_page_base(const struct hg_block *hb, const void *p)
{
	return hb->pbase + (hg_page_of(hb, p) << hb->hps_shift);
}

/* leaf index of @p within its own page */
static inline unsigned long hg_leaf_of(const struct hg_block *hb, const void *p)
{
	unsigned long off = (unsigned long)((const char *)p - hb->pbase);

	return (off & ((1UL << hb->hps_shift) - 1)) >> HG_LEAF_SHIFT;
}

#endif /* hg_block_h */
