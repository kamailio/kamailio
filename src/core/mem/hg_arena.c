/*
 * slab allocator with elastic arenas - slab tier
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

#include <stdlib.h> /* abort */
#include <string.h>
#include <unistd.h>	 /* sysconf, for the IO-pool consumer count */
#include <pthread.h> /* the thread-exit key */

#include "hg_version.h" /* HG_MALLOC_NAME */
#include "hg_malloc.h"
#include "hg_arena.h"
#include "hg_arena_int.h"
#include "hg_large.h"
#include "hg_dbg.h"
#include "../dprint.h"

/* ~x1.5 ladder of total slot sizes (header + payload), multiples of 32 */
static const unsigned int cell_sizes[HG_NCLASSES] = {64, 96, 128, 192, 256, 384,
		512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576,
		32768, 49152, 65536};

#define HG_CHUNK_SMALL (256 * 1024) /* ceiling on chunk_max, see below */
#define HG_CHUNK_MIN (8 * 1024)		/* floor, however tiny the arena */
/* target cells per block; a block is the reclaim unit, so this trades
 * fragmentation against carves */
#define HG_CELLS_PER_BLOCK 32
/* GC: the maintenance tick returns drained blocks beyond keep, a budget at
 * a time; the donate path only past HG_GC_EMERGENCY */
#define HG_REFILL_BATCH 32 /* cells pulled from the global pool */

/* private cache budget in bytes: HG_PRIVATE_PCT of the arena over all
 * threads, split over HG_PRIVATE_CONSUMERS, at most HG_PRIVATE_CAP cells */
#define HG_PRIVATE_PCT 25		/* of the arena, all threads and classes */
#define HG_PRIVATE_CONSUMERS 16 /* fallback when the count is unknown     */
/* divisor: TCP reactor pool threads or online CPUs, clamped */
#define HG_PRIVATE_CONSUMERS_MIN 4
#define HG_PRIVATE_CONSUMERS_MAX 64
#define HG_PRIVATE_CAP 256 /* cells per class and thread, at most */

/* per-thread private free lists, one slot per live arena: __thread, since
 * the TCP reactor pool threads allocate concurrently in one process */
static __thread struct hg_palloc palloc_slots[HG_MAX_INSTANCES];

/* last slot this thread used: the fast path is one TLS load and compare;
 * slots that get reset also clear it */
static __thread struct hg_palloc *hg_last;

static void slot_reset(struct hg_palloc *pl);

/* a free routed to another arena than the caller named: counted in the shm
 * arena, so the figure covers every process (not counted without hg shm) */
static void hg_xarena_count(void)
{
	struct hg_block *s = (struct hg_block *)hg_shm_block;

	if(s)
		__atomic_fetch_add(&s->xarena_frees, 1, __ATOMIC_RELAXED);
}

/* a thread that exits would strand what it caches: a key armed at the
 * first slot claim runs the drain from its destructor */
static pthread_key_t hg_exit_key;
static pthread_once_t hg_exit_once = PTHREAD_ONCE_INIT;
static int hg_exit_key_ok;

static void hg_thread_exit(void *arg)
{
	int i;

	(void)arg;
	hg_cache_drain_self();
	/* a free from a later destructor claims a slot and re-arms the key */
	for(i = 0; i < HG_MAX_INSTANCES; i++) {
		hg_quar_unmap(palloc_slots[i].quar);
		memset(&palloc_slots[i], 0, sizeof palloc_slots[i]);
	}
	hg_last = NULL;
}

static void hg_exit_key_init(void)
{
	hg_exit_key_ok = pthread_key_create(&hg_exit_key, hg_thread_exit) == 0;
	if(!hg_exit_key_ok)
		LM_WARN("pthread_key_create() failed - an exiting thread keeps "
				"its cached cells\n");
}

static struct hg_palloc *hg_palloc_lookup(struct hg_block *hb)
{
	int i, free_slot = -1;

	for(i = 0; i < HG_MAX_INSTANCES; i++) {
		if(palloc_slots[i].owner == hb) {
			hg_last = &palloc_slots[i];
			return &palloc_slots[i];
		}
		if(free_slot < 0 && !palloc_slots[i].owner)
			free_slot = i;
	}

	if(free_slot < 0) {
		LM_CRIT("more than %d live HG_MALLOC instances in one process "
				"- raise HG_MAX_INSTANCES in hg_arena.c\n",
				HG_MAX_INSTANCES);
		return NULL;
	}

	slot_reset(&palloc_slots[free_slot]);
	palloc_slots[free_slot].owner = hb;
	hg_last = &palloc_slots[free_slot];
	pthread_once(&hg_exit_once, hg_exit_key_init);
	if(hg_exit_key_ok)
		pthread_setspecific(hg_exit_key, palloc_slots);
	return &palloc_slots[free_slot];
}

static inline struct hg_palloc *hg_get_palloc(struct hg_block *hb)
{
	struct hg_palloc *pl = hg_last;

	if(pl && pl->owner == hb)
		return pl;
	return hg_palloc_lookup(hb);
}

/* a freed cell onto this thread's list; donate half once over the bound */
static inline void cell_push_private(struct hg_block *hb, struct hg_palloc *pl,
		unsigned int c, char *cell_start)
{
	unsigned int i;
	void *d;

	cell_set_next(cell_start, pl->cls[c].free_head);
	pl->cls[c].free_head = cell_start;
	pl->cls[c].nfree++;

	/* over the class bound: donate half (priv_max 0 never caches) */
	if(pl->cls[c].nfree > hb->priv_max[c]) {
		unsigned int claimed = pl->cls[c].nfree;
		unsigned int donate = hb->priv_donate[c];

		hg_lock_enter(hb, HG_LK_RETURN);
		for(i = 0; i < donate; i++) {
			d = pl->cls[c].free_head;
			/* an early NULL means nfree drifted above the chain */
			if(!d)
				break;
			if(!cell_block(hb, c, d)) {
				/* an overwritten link - the chain ends here, the
				 * rest leaks; the resync below records the truth */
				pl->cls[c].free_head = NULL;
				pl->cls[c].nfree = i;
				break;
			}
			pl->cls[c].free_head = cell_next(d);
			pl->cls[c].nfree--;
			if(hg_gpool_push(hb, c, d) < 0) {
				/* refused: the chain is not ours to follow any further */
				pl->cls[c].free_head = NULL;
				pl->cls[c].nfree = i + 1;
				break;
			}
		}
		/* the chain ran dry: nfree is 0, resync it */
		if(i < donate)
			pl->cls[c].nfree = 0;
		hg_lock_leave(hb);

		if(i < donate) {
			LM_CRIT("%s: class %u free list ran dry after %u cells "
					"but nfree claimed %u - counter resynced to 0, "
					"pool accounting drifted\n",
					hb->name, c, i, claimed);
			hg_corrupt(hb, HG_C_NFREE_UNDERFLOW);
		}
	}
}

/* hg_dbg.c's quarantine releases a cell through this; the free path here
 * keeps the inlined copy */
void hg_cell_push_private(struct hg_block *hb, struct hg_palloc *pl,
		unsigned int c, char *cell_start)
{
	cell_push_private(hb, pl, c, cell_start);
}

/* zero a private slot, keep its emptied quarantine mapping */
static void slot_reset(struct hg_palloc *pl)
{
	struct hg_quar *q = pl->quar;

	memset(pl, 0, sizeof *pl);
	if(q) {
		hg_quar_reset(q);
		pl->quar = q;
	}
}

/* bump carve from the arena's own reservation; NULL when exhausted */
void *hg_chunk_backing(struct hg_block *hb, unsigned long size)
{
	unsigned long asz = (size + 63) & ~63UL; /* keep 64-aligned */
	unsigned long off;

	/* after hg_buddy_init() a bump would hand out buddy-owned memory: refuse */
	if(hb->buddy_ready) {
		LM_CRIT("%s: bump carve of %lu bytes after the buddy owns the "
				"arena - refusing, this would double-allocate\n",
				hb->name, size);
		hg_corrupt(hb, HG_C_INTERNAL);
		return NULL;
	}

	off = __atomic_fetch_add(&hb->hoff, asz, __ATOMIC_RELAXED);

	if(off + asz <= hb->span_end)
		return hb->hbase + off;

	/* exhausted: hoff stays past the end, later bumps fail too */
	return NULL;
}

/* chunk size scales with the arena: a fixed 256 KB per class would eat a
 * small pkg arena before it serves anything */
static inline unsigned int chunk_size_for(struct hg_block *hb, int c)
{
	unsigned int want, least = sizeof(struct hg_chunk) + cell_sizes[c] * 2;

	/* HG_CELLS_PER_BLOCK cells, at least one leaf */
	want = cell_sizes[c] * HG_CELLS_PER_BLOCK;
	if(want < HG_LEAF_SIZE)
		want = HG_LEAF_SIZE;

	if(want > hb->chunk_max)
		want = hb->chunk_max;
	/* ...but always enough for the header plus a couple of cells, or the
	 * class could never be served at all */
	if(want < least)
		want = least;
	return want;
}

/* carve a chunk for class @c, hb->lock held; cell class bytes stamped here */
static int carve_chunk(struct hg_block *hb, int c, struct hg_palloc *pl)
{
	struct hg_chunk *ch;
	unsigned int size = chunk_size_for(hb, c), i;
	char *cells;
	int ord;

	/* chunks are buddy blocks, the size rounds up to an order */
	ord = hg_buddy_order_for(hb, size);
	if(ord < 0) {
		LM_ERR("%s: class %d wants a %u byte chunk, larger than the %lu byte "
			   "page the buddy tops out at\n",
				hb->name, c, size, (unsigned long)hb->hps);
		return -1;
	}
	size = (unsigned int)(HG_LEAF_SIZE << ord);
	ch = hg_buddy_alloc(hb, (unsigned int)ord);
	/* no block of this order: flush this thread's caches and GC before
	 * growing; retry only if a block came back */
	if(!ch) {
		unsigned long returned = hb->gc_blocks_returned;
		unsigned int n = hg_cache_flush_locked(hb, pl);

		hb->pressure_flushes++;
		hb->pressure_cells += n;
		if(hb->gc_blocks_returned != returned)
			ch = hg_buddy_alloc(hb, (unsigned int)ord);
	}
	/* exhaustion grows the arena, or waits for another grow; then one retry */
	if(!ch && hg_buddy_grow_unlocks(hb, size, HG_GROW_EXHAUSTION) == 0)
		ch = hg_buddy_alloc(hb, (unsigned int)ord);

	/* the floor is checked in the buddy layer (hg_reserve_floor_check()) */
	if(!ch) {
		if(hg_oom_first(hb, size))
			LM_ERR("%s: no more HG_MALLOC arena memory for a %u byte "
				   "chunk (class %d, order %d) - increase the arena size; "
				   "further failures are summarised every 30 s\n",
					hb->name, size, c, ord);
		return -1;
	}

	/* the block holds stale free-list bytes: set every field */
	ch->in_gpool = 0;
	ch->order = (unsigned int)ord;
	ch->free_head = NULL;
	ch->grade = HG_GRADE_NONE;
	ch->fnext = NULL;
	ch->fprev = NULL;
	ch->cls = c;
	ch->cell_size = cell_sizes[c];
	ch->cells = (size - sizeof(struct hg_chunk)) / cell_sizes[c];
	if(ch->cells > HG_CHUNK_MAX_CELLS) {
		/* cannot happen with this class table; cap rather than overrun */
		LM_WARN("%s: class %d block would hold %u cells, capping at %lu - "
				"the per-block map is %lu bits\n",
				hb->name, c, ch->cells, (unsigned long)HG_CHUNK_MAX_CELLS,
				(unsigned long)HG_CHUNK_MAX_CELLS);
		ch->cells = HG_CHUNK_MAX_CELLS;
	}
	memset(ch->gfree, 0, sizeof ch->gfree);

	cells = (char *)ch + sizeof(struct hg_chunk);
	for(i = 0; i < ch->cells; i++) {
		struct hg_cell_hdr *h =
				HG_CELL_AT(cells + (unsigned long)i * cell_sizes[c]);

		hg_hdr_set(h, c, HG_ST_FRESH);
#ifdef DBG_HG_MALLOC
		/* recycled memory: clear file/func for the debug walk */
		h->file = NULL;
		h->func = NULL;
		h->line = 0;
		h->mname = NULL;
#endif
		hg_san_take(hb, h, cell_sizes[c] - HG_CELL_HDR);
	}

	ch->next = hb->chunks;
	ch->prev = NULL;
	if(ch->next)
		ch->next->prev = ch;
	hb->chunks = ch;
	hb->nchunks++;
	hb->slab_capacity += (unsigned long)ch->cells * ch->cell_size;
	hb->blocks_carved++;
	hb->carves_cycle[c]++; /* this cycle's demand */
	hb->real_used += size;
	if(hb->real_used > hb->max_real_used)
		hb->max_real_used = hb->real_used;

	/* the whole chunk belongs to the carving process */
	pl->cls[c].bump = cells;
	pl->cls[c].left = ch->cells;

	LM_DBG("%s class %d: new %u byte chunk, %u cells of %u\n", hb->name, c,
			size, ch->cells, cell_sizes[c]);
	return 0;
}

/* threads the private-cache share is split over: the TCP reactor pool or
 * online CPUs; re-run by hg_private_caps_retune() after the config */
static unsigned int hg_io_consumers(void)
{
	int n = hg_io_threads();

	if(n <= 0) {
		long cpus = sysconf(_SC_NPROCESSORS_ONLN);

		n = (cpus > 0) ? (int)cpus : HG_PRIVATE_CONSUMERS;
	}
	if(n < HG_PRIVATE_CONSUMERS_MIN)
		n = HG_PRIVATE_CONSUMERS_MIN;
	if(n > HG_PRIVATE_CONSUMERS_MAX)
		n = HG_PRIVATE_CONSUMERS_MAX;
	return (unsigned int)n;
}

/* per-class cell caps from the byte budget, once at init; a class larger
 * than one thread's share gets 0. priv_donate is half the cap */
static void private_caps_init(struct hg_block *hb)
{
	unsigned int consumers = hg_io_consumers();
	unsigned long per_thread =
			(unsigned long)hb->size / 100 * HG_PRIVATE_PCT / consumers;
	unsigned long per_class = per_thread / HG_NCLASSES;
	unsigned int c;

	for(c = 0; c < HG_NCLASSES; c++) {
		unsigned long n = per_class / cell_sizes[c];

		if(n > HG_PRIVATE_CAP)
			n = HG_PRIVATE_CAP;
		hb->priv_max[c] = (unsigned int)n;
		hb->priv_donate[c] = (unsigned int)(n / 2);
		if(hb->priv_donate[c] == 0)
			hb->priv_donate[c] = 1;
	}

	LM_DBG("%s: private cache budget %lu B/thread over %u consumers (%lu "
		   "B/class): "
		   "class %u caches %u cells, class %u caches %u\n",
			hb->name, per_thread, consumers, per_class, cell_sizes[0],
			hb->priv_max[0], cell_sizes[HG_NCLASSES - 1],
			hb->priv_max[HG_NCLASSES - 1]);
}

/* re-derive the private caps once the reactor pool size is known, before
 * fork; the last write to them */
void hg_private_caps_retune(struct hg_block *hb)
{
	if(hb)
		private_caps_init(hb);
}

/* slow-path access to this thread's slots, for the other arena files */
struct hg_palloc *hg_palloc_slot(int i)
{
	return &palloc_slots[i];
}

struct hg_palloc *hg_palloc_mine(struct hg_block *hb)
{
	return hg_get_palloc(hb);
}

unsigned int hg_chunk_size_for(struct hg_block *hb, int c)
{
	return chunk_size_for(hb, c);
}

int hg_arena_init(struct hg_block *hb, unsigned long hdr_size)
{
	unsigned int idx, c, needed;

	if(hg_pages_init(hb) < 0)
		return -1;

	/* the first carve starts on a leaf boundary after the block header, so
	 * every buddy block is naturally aligned */
	hb->hoff = (hdr_size + HG_LEAF_SIZE - 1) & ~(HG_LEAF_SIZE - 1);
	if(hb->pbase != hb->hbase) {
		/* the grid does not start at hbase, so the first carve must clear
		 * the discarded head too */
		unsigned long skip = (unsigned long)(hb->pbase - hb->hbase);

		if(hb->hoff < skip)
			hb->hoff = skip;
	}

	/* chunk_max: /64 of the arena (see chunk_size_for()) */
	hb->thread_slot_next = HG_STAT_SLOTS - 1; /* threads count down */
	hb->chunk_max = hb->size / 64;
	if(hb->chunk_max > HG_CHUNK_SMALL)
		hb->chunk_max = HG_CHUNK_SMALL;
	if(hb->chunk_max < HG_CHUNK_MIN)
		hb->chunk_max = HG_CHUNK_MIN;

	private_caps_init(hb);

	/* size -> class LUT: needed = requested payload + hidden header */
	for(idx = 0; idx <= HG_CELL_MAX / HG_ROUNDTO; idx++) {
		needed = idx * HG_ROUNDTO + HG_CELL_HDR;
		for(c = 0; c < HG_NCLASSES; c++)
			if(cell_sizes[c] >= needed)
				break;
		hb->size2class[idx] = (unsigned char)c; /* HG_NCLASSES = oversize */
	}

	LM_DBG("%s arena ready: %d classes, %u B to %u B cells (header=%zu B)\n",
			hb->name, HG_NCLASSES, cell_sizes[0], cell_sizes[HG_NCLASSES - 1],
			HG_CELL_HDR);

	/* last: it bump-carves its metadata, after that bumps are refused */
	if(hg_buddy_init(hb) < 0)
		return -1;
	return 0;
}

void hg_arena_destroy(struct hg_block *hb)
{
	int i;

	/* the reservation is munmapped whole; only drop this process's slot */
	for(i = 0; i < HG_MAX_INSTANCES; i++)
		if(palloc_slots[i].owner == hb)
			slot_reset(&palloc_slots[i]);
	hg_last = NULL;

	hg_large_destroy(hb);
}

void hg_arena_child_init(struct hg_block *hb)
{
	int i;

	/* after fork, discard the inherited private state for @hb: keeping or
	 * donating it would hand the parent's cells out twice */
	for(i = 0; i < HG_MAX_INSTANCES; i++)
		if(palloc_slots[i].owner == hb)
			slot_reset(&palloc_slots[i]);
	hg_last = NULL;
}

unsigned int hg_cell_total_size(unsigned char cls)
{
	if(cls >= HG_NCLASSES)
		return 0;
	return cell_sizes[cls];
}

/* the private list head must be a cell of this arena, else drop the list */
static inline int private_head_ok(
		struct hg_block *hb, struct hg_palloc *pl, int c)
{
	char *cs = pl->cls[c].free_head;

	if(hg_owns(hb, cs))
		return 1;
	LM_CRIT("%s: class %d private list leads to %p, outside the arena - "
			"dropping the list\n",
			hb->name, c, cs);
	hg_corrupt(hb, HG_C_FOREIGN_PTR);
	pl->cls[c].free_head = NULL;
	pl->cls[c].nfree = 0;
	return 0;
}

#ifdef DBG_HG_MALLOC
void *hg_cell_alloc(struct hg_block *hb, unsigned long size, const char *file,
		const char *func, unsigned int line, const char *mname)
#else
void *hg_cell_alloc(struct hg_block *hb, unsigned long size)
#endif
{
	struct hg_palloc *pl;
	char *cell_start, *payload;
	unsigned int got;
	unsigned long want = size;
	/* read once: the setting may change between two allocations */
	int guard = hg_guard_on(hb);
	int c;

	if(hb->dbg)
		want += HG_DBG_TAIL; /* room for the trailer, see HG_DBG_TAIL */
	else if(guard)
		want += HG_GUARD_ROOM; /* at least one guard byte */
	if(want + HG_CELL_HDR > HG_CELL_MAX) {
#ifdef DBG_HG_MALLOC
		return hg_large_alloc(hb, size, file, func, line, mname);
#else
		return hg_large_alloc(hb, size);
#endif
	}
	c = hb->size2class[(want + HG_ROUNDTO - 1) / HG_ROUNDTO];
	if(c >= HG_NCLASSES)
		return NULL;

	pl = hg_get_palloc(hb);
	if(!pl)
		return NULL;

again:
	/* fast paths: no locks, no shared lines */
	cell_start = pl->cls[c].free_head;
	if(cell_start) {
		if(!private_head_ok(hb, pl, c))
			goto again;
		pl->cls[c].free_head = cell_next(cell_start);
		pl->cls[c].nfree--;
		goto found;
	}
	if(pl->cls[c].left) {
		cell_start = pl->cls[c].bump;
		pl->cls[c].bump += cell_sizes[c];
		pl->cls[c].left--;
		goto found;
	}

	/* slow path: refill from the global pool, else carve a chunk */
	hg_lock_enter(hb, HG_LK_REFILL);
	for(got = 0; got < HG_REFILL_BATCH; got++) {
		cell_start = hg_gpool_pop(hb, c);
		if(!cell_start)
			break;
		cell_set_next(cell_start, pl->cls[c].free_head);
		pl->cls[c].free_head = cell_start;
		pl->cls[c].nfree++;
	}
	/* no flush here: carve_chunk() does it when the buddy is out */
	if(!got && carve_chunk(hb, c, pl) < 0) {
		hg_lock_leave(hb);
		/* the quarantine never fails an allocation: let it go and look
		 * again - once, as the drain leaves it empty */
		if(hb->dbg && hg_quar_drain(hb, pl)) {
			hg_pstat_mine(hb)->dirty = 1;
			goto again;
		}
		return NULL;
	}
	hg_lock_leave(hb);
	/* the rest of the batch stays cached: mark it for the sweep, as a free
	 * does */
	if(got)
		hg_pstat_mine(hb)->dirty = 1;

	cell_start = pl->cls[c].free_head;
	if(cell_start) {
		if(!private_head_ok(hb, pl, c))
			goto again;
		pl->cls[c].free_head = cell_next(cell_start);
		pl->cls[c].nfree--;
		goto found;
	}
	cell_start = pl->cls[c].bump;
	pl->cls[c].bump += cell_sizes[c];
	pl->cls[c].left--;

found:
	/* hand out only FRESH or FREE cells with intact headers; drop others */
	{
		union hg_hdr_w w;
		unsigned short base;
		int st = hg_hdr_read(cell_start, &w, &base);

		if((st != HG_ST_FRESH && st != HG_ST_FREE) || w.f.cls != c) {
			LM_CRIT("%s: class %d cell %p about to be handed out is %s - "
					"dropping it\n",
					hb->name, c, cell_start,
					st < 0			 ? "damaged"
					: hg_st_live(st) ? "still in use"
									 : "of another class");
			hg_corrupt(hb, st < 0			? HG_C_HEADER
						   : hg_st_live(st) ? HG_C_DOUBLE_FREE
											: HG_C_CLASS_MISMATCH);
			goto again;
		}
		if(guard) {
			hg_san_open(hb, cell_start, cell_sizes[c] - HG_CELL_HDR);
			hg_guard_set(cell_start, base, cell_sizes[c] - HG_CELL_HDR, size);
		}
		hg_hdr_set_base(cell_start, c, guard ? HG_ST_LIVE_G : HG_ST_LIVE, base);
	}
	payload = cell_start + HG_CELL_HDR;
	pl->cls[c].touched = 1;
	if(hb->dbg) {
		hg_san_open(hb, cell_start, cell_sizes[c] - HG_CELL_HDR);
		hg_dbg_tail_set(cell_start, cell_sizes[c] - HG_CELL_HDR, size);
	}
	hg_san_give(hb, cell_start, cell_sizes[c] - HG_CELL_HDR, size);
#ifdef DBG_HG_MALLOC
	HG_CELL_AT(cell_start)->file = file;
	HG_CELL_AT(cell_start)->func = func;
	HG_CELL_AT(cell_start)->line = line;
	HG_CELL_AT(cell_start)->mname = mname;
#endif
	/* per-process slot: no lock, no shared cache line (see hg_pstat) */
	{
		struct hg_pstat *ps = hg_pstat_mine(hb);
		ps->used += cell_sizes[c] - HG_CELL_HDR;
		ps->fragments++;
		ps->cell_live += cell_sizes[c];
	}
	return payload;
}

static const char *const hg_corrupt_str[HG_CORRUPT_KINDS] = {
		[HG_C_CLASS_MISMATCH] = "class mismatch",
		[HG_C_DOUBLE_FREE] = "double free",
		[HG_C_NFREE_UNDERFLOW] = "free count underflow",
		[HG_C_BAD_CLASS] = "bad class",
		[HG_C_FOREIGN_PTR] = "foreign pointer",
		[HG_C_BUDDY_BAD_FREE] = "bad buddy free",
		[HG_C_INTERNAL] = "internal inconsistency",
		[HG_C_HEADER] = "damaged header",
		[HG_C_USE_AFTER_FREE] = "use after free",
		[HG_C_OVERFLOW] = "overflow"};

void hg_corrupt_abort(struct hg_block *hb, enum hg_corrupt_kind k)
{
	LM_CRIT("%s: aborting on the %s above (mem_safety = 0)\n",
			hb ? hb->name : HG_MALLOC_NAME, hg_corrupt_str[k]);
	abort();
}

void hg_guard_bad(
		struct hg_block *hb, void *cs, unsigned long cap, unsigned long size)
{
	const unsigned char *pay = (const unsigned char *)cs + HG_CELL_HDR;
	unsigned long i = size;

	if(size < cap) {
		while(i < cap && pay[i] == HG_GUARD_BYTE)
			i++;
		LM_CRIT("%s: %p (%lu bytes) was written past the %lu bytes asked "
				"for (byte %lu)\n",
				hb->name, (const void *)pay, cap, size, i);
	} else {
		LM_CRIT("%s: %p (%lu bytes) was written past its size (the size "
				"field is lost)\n",
				hb->name, (const void *)pay, cap);
	}
	hg_corrupt(hb, HG_C_OVERFLOW);
}

/* a pointer into the arena but at memory it has given back: refuse it
 * before the header read, which would touch the unbacked page */
static inline int free_backed(struct hg_block *hb, char *cell_start, void *p)
{
	if(hg_backed(hb, cell_start))
		return 1;
	LM_CRIT("%s: %p points into memory the arena has released - not a "
			"live allocation; refusing to free it\n",
			hb->name, p);
	hg_corrupt(hb, HG_C_FOREIGN_PTR);
	return 0;
}

/* header intact and LIVE or LIVE_G: the class, or -1 (counted, leaked) */
static inline int free_check(struct hg_block *hb, char *cell_start, void *p,
		union hg_hdr_w *w, unsigned short *base)
{
	int st = hg_hdr_read(cell_start, w, base);

	if(hg_st_live(st)) {
		unsigned int c = w->f.cls;
		unsigned long cap = 0, asked;

		/* debug or guard: report a write past the request and go on */
		if(hb->dbg || HG_SAN_PRIVATE(hb) || st == HG_ST_LIVE_G) {
			if(c == HG_LARGE_MARKER)
				cap = ((struct hg_lfrag *)(void *)(cell_start - HG_LFRAG_HDR))
							  ->size
					  - HG_CELL_HDR;
			else if(c < HG_NCLASSES)
				cap = cell_sizes[c] - HG_CELL_HDR;
		}
		if(cap) {
			hg_san_open(hb, cell_start, cap);
			if(hb->dbg)
				hg_dbg_tail_check(hb, cell_start, cap);
			else if(st == HG_ST_LIVE_G
					&& !hg_guard_ok(cell_start, *base, cap, &asked))
				hg_guard_bad(hb, cell_start, cap, asked);
		}
		return c;
	}
	if(st < 0) {
		LM_CRIT("%s: %p has a damaged header - overwritten by a "
				"neighbour's overflow, or not a pointer this arena handed "
				"out - leaking it\n",
				hb->name, p);
		hg_corrupt(hb, HG_C_HEADER);
	} else {
		LM_CRIT("%s: %p is already free - double free, ignoring the "
				"second free\n",
				hb->name, p);
		hg_corrupt(hb, HG_C_DOUBLE_FREE);
	}
	return -1;
}

/* LIVE -> FREE; 0 if another free of the cell won */
static inline int free_claim(struct hg_block *hb, char *cell_start,
		unsigned int c, void *p, union hg_hdr_w *w, unsigned short base)
{
	if(hg_hdr_claim_free(cell_start, w->w, c, base))
		return 1;
	LM_CRIT("%s: %p was freed twice at the same moment - ignoring the "
			"second free\n",
			hb->name, p);
	hg_corrupt(hb, HG_C_DOUBLE_FREE);
	return 0;
}

#ifdef DBG_HG_MALLOC
void hg_cell_free(struct hg_block *hb, void *p, const char *file,
		const char *func, unsigned int line, const char *mname)
#else
void hg_cell_free(struct hg_block *hb, void *p)
#endif
{
	struct hg_palloc *pl;
	char *cell_start;
	unsigned int c;
	int ci;
	union hg_hdr_w hw;
	unsigned short hbase;

	if(!p)
		return;

	cell_start = HG_HDR(p);

	/* check ownership before reading the class byte */
	if(!hg_owns(hb, cell_start)) {
		struct hg_block *owner = hg_owner(cell_start);

		if(!owner) {
			LM_CRIT("%s: %p belongs to no live arena - refusing to "
					"free it\n",
					hb->name, p);
			hg_corrupt(hb, HG_C_FOREIGN_PTR);
			return;
		}

		/* another hg arena owns it (normal after fork): free straight to its
		 * shared pool, this process never allocates there again */
		hg_xarena_count();
		hg_cell_free_global(owner, p);
		return;
	}

	/* no private slot: the shared pool takes it, checks and all - looked
	 * up first, so the checks below run once per free */
	pl = hg_get_palloc(hb);
	if(!pl) {
		hg_cell_free_global(hb, p);
		return;
	}

	if(!free_backed(hb, cell_start, p))
		return;
	ci = free_check(hb, cell_start, p, &hw, &hbase);
	if(ci < 0)
		return;
	c = (unsigned int)ci;

	if(c == HG_LARGE_MARKER) {
		hg_large_free(
				hb, (struct hg_lfrag *)(void *)(cell_start - HG_LFRAG_HDR));
		return;
	}

	if(c >= HG_NCLASSES) {
		/* an intact header naming no class: leak it and report */
		LM_CRIT("%s: cell %p carries invalid class %u - leaking it\n", hb->name,
				p, c);
		hg_corrupt(hb, HG_C_BAD_CLASS);
		return;
	}

	if(!free_claim(hb, cell_start, c, p, &hw, hbase))
		return;

	{
		struct hg_pstat *ps = hg_pstat_mine(hb);
		ps->used -= cell_sizes[c] - HG_CELL_HDR;
		ps->fragments--;
		ps->cell_live -= cell_sizes[c];
		ps->dirty = 1; /* this cache now holds something to sweep */
	}

#ifdef DBG_HG_MALLOC
	if(hb->dbg) {
		/* the free's site replaces the allocation's: a write after the
		 * free is reported against it */
		HG_CELL_AT(cell_start)->file = file;
		HG_CELL_AT(cell_start)->func = func;
		HG_CELL_AT(cell_start)->line = line;
		HG_CELL_AT(cell_start)->mname = mname;
	}
#endif
	if(hb->dbg) {
		hg_quar_put(hb, pl, c, cell_start);
	} else {
		hg_san_take(hb, cell_start, cell_sizes[c] - HG_CELL_HDR);
		cell_push_private(hb, pl, c, cell_start);
	}
}

void hg_cell_free_global(struct hg_block *hb, void *p)
{
	char *cell_start;
	unsigned int c;
	int pushed, ci;
	union hg_hdr_w hw;
	unsigned short hbase;

	if(!p)
		return;

	cell_start = HG_HDR(p);
	if(!hg_owns(hb, cell_start)) {
		struct hg_block *owner = hg_owner(cell_start);

		/* only the resolved owner arrives here, so the redirect cannot loop */
		if(!owner) {
			LM_CRIT("%s: %p belongs to no live arena - refusing to "
					"free it\n",
					hb->name, p);
			hg_corrupt(hb, HG_C_FOREIGN_PTR);
			return;
		}
		if(owner != hb) {
			hg_xarena_count();
			hb = owner;
		}
	}
	if(!free_backed(hb, cell_start, p))
		return;
	ci = free_check(hb, cell_start, p, &hw, &hbase);
	if(ci < 0)
		return;
	c = (unsigned int)ci;
	if(c == HG_LARGE_MARKER) {
		hg_large_free(
				hb, (struct hg_lfrag *)(void *)(cell_start - HG_LFRAG_HDR));
		return;
	}
	if(c >= HG_NCLASSES) {
		LM_CRIT("%s: cell %p carries invalid class %u - leaking it\n", hb->name,
				p, c);
		hg_corrupt(hb, HG_C_BAD_CLASS);
		return;
	}
	/* the counters move here, and not for a refused push */
	if(!free_claim(hb, cell_start, c, p, &hw, hbase))
		return;
	hg_san_take(hb, cell_start, cell_sizes[c] - HG_CELL_HDR);
	hg_lock_enter(hb, HG_LK_RETURN);
	pushed = hg_gpool_push(hb, c, cell_start);
	hg_lock_leave(hb);

	if(pushed == 0) {
		struct hg_pstat *ps = hg_pstat_mine(hb);
		ps->used -= cell_sizes[c] - HG_CELL_HDR;
		ps->fragments--;
		ps->cell_live -= cell_sizes[c];
	}
}

#endif /* HG_MALLOC */
