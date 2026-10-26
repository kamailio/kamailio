/*
 * hg_malloc - size classes with per-thread caches
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

/* cells of 21 size classes from a page bitmap; a thread allocates and frees
 * on its own lists and meets other threads only in the pool, under the lock */

#ifdef HG_MALLOC

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <pthread.h>

#include "memcore.h"
#include "memapi.h"
#include "meminfo.h"
#include "pkg.h"
#include "shm.h"
#include "../dprint.h"
#include "../globals.h"
#include "../events.h"
#include "../locking.h"
#include "../compiler_opt.h"
#include "../cfg/cfg.h"
#include "../cfg_core.h"
#include "../pt.h"

/* the memory debugging signature is accepted and ignored: no per-site
 * accounting is kept, so the summaries say "not available" */
#ifdef DBG_SR_MEMORY
#define HG_DBG_PARAMS \
	, const char *file, const char *func, unsigned int line, const char *mname
#define HG_DBG_ARGS , file, func, line, mname
#else
#define HG_DBG_PARAMS
#define HG_DBG_ARGS
#endif

#define HG_NCLASSES 21
#define HG_CELL_MAX 65472 /* largest cell, header included; above: page runs */
#define HG_PAGE_SHIFT 12
#define HG_PAGE (1UL << HG_PAGE_SHIFT)
#define HG_ROUNDTO 8			  /* payload alignment, size table step */
#define HG_LINE 64				  /* cache line */
#define HG_REF_SHIFT 6			  /* a cell names its block in 2^this units */
#define HG_MAX_ORDER 6			  /* class blocks are 2^k pages, k <= this */
#define HG_CELLS_PER_BLOCK 32	  /* target, see chunk_order_for() */
#define HG_CHUNK_MIN (8 * 1024)	  /* smallest block */
#define HG_CHUNK_BIG (256 * 1024) /* largest block, if the arena allows */
#define HG_BLOCK_SLACK 32  /* unused at a block's end, out of a short overrun */
#define HG_REFILL_BATCH 32 /* cells a refill takes from the pool */
#define HG_PRIVATE_PCT 25  /* of the arena, for all caches */
#define HG_PRIVATE_CAP 256 /* cells per class and thread, at most */
#define HG_CONSUMERS_MIN 4 /* CPUs the shm cache share is split over */
#define HG_CONSUMERS_MAX 64
#define HG_KEEP_MIN 2		 /* drained blocks a class keeps at least */
#define HG_OOM_PERIOD 30	 /* seconds between failure reports */
#define HG_STATUS_TRIES 1000 /* lock attempts before a status dump gives up */
#define HG_DIV_SHIFT 40		 /* exact for offsets x sizes below 2^40 */
#define HG_TAG 0x68676368u /* a block header's tag, with its class and place */
#define HG_BITS (sizeof(unsigned long) * 8)
#define HG_ALIGN_UP(x, al) (((x) + (al) - 1) & ~((unsigned long)(al) - 1))

/* class and state in one word, claimed LIVE to FREE by one compare-and-swap;
 * the cell's block, or the run's length, in the other */
struct hg_hdr
{
	unsigned int w; /* class | state << 8 */
	union
	{
		unsigned int block; /* a cell's block offset >> HG_REF_SHIFT */
		unsigned int pages; /* a page run's page count */
	} u;
};

/* every allocation: the header, then the payload, whose first word links a
 * free cell */
struct hg_cell
{
	struct hg_hdr h;
	struct hg_cell *next;
};

#define HG_HDR offsetof(struct hg_cell, next)
#define HG_ST_FREE 0x5a
#define HG_ST_LIVE 0xa5
#define HG_LARGE 0xff /* class byte of a page run */
#define HG_WORD(cls, st) ((unsigned int)(cls) | ((unsigned int)(st) << 8))
#define HG_CLS(w) ((w) & 0xffu)
#define HG_ST(w) ((w) >> 8)

/* a block of one class: this header, then its cells */
struct hg_chunk
{
	struct hg_chunk *fnext; /* the partial or drained list of its class */
	struct hg_chunk *fprev;
	struct hg_cell *free_head; /* cells in the shared pool */
	unsigned int cls;
	unsigned int tag;	   /* HG_TAG ^ class ^ place: an overwrite shows */
	unsigned int in_gpool; /* cells on free_head */
	unsigned int list;	   /* HG_L_* */
	char area[] __attribute__((aligned(HG_LINE)));
};

#define HG_CHUNK_HDR offsetof(struct hg_chunk, area)
#define HG_L_NONE 0	   /* no cell in the pool */
#define HG_L_PARTIAL 1 /* some cells in the pool */
#define HG_L_DRAINED 2 /* every cell in the pool: the block can go back */

/* one thread's private cells of one arena */
struct hg_tcache
{
	int armed;		   /* the thread-exit destructor is set */
	unsigned long sec; /* the second of the last idle-block return */
	struct
	{
		struct hg_cell *free_head;
		unsigned int nfree;
		unsigned int left;	/* cells of the own block not handed out yet */
		char *bump;			/* the next of them */
		unsigned int block; /* the own block, as its cells name it */
		unsigned int used;	/* a cell of it was handed out since sec */
	} cls[HG_NCLASSES];
};

struct hg_arena
{
	/* read by the fast paths, written at init (caps: at the first fork) */
	char *name;
	char *pbase; /* page 0 */
	char *pend;	 /* pbase + npages * HG_PAGE */
	unsigned long npages;
	int slot;
	int shared;
	unsigned int chunk_max;				   /* largest class block, bytes */
	unsigned long consumers;			   /* the caches' share is split over */
	unsigned int priv_max[HG_NCLASSES];	   /* cells a thread keeps per class */
	unsigned int priv_donate[HG_NCLASSES]; /* and hands over at once */
	unsigned char blk_order[HG_NCLASSES];  /* a class's blocks: 2^this pages */
	unsigned int blk_cells[HG_NCLASSES];   /* holding this many cells */
	unsigned long long cell_div[HG_NCLASSES]; /* 2^HG_DIV_SHIFT / size, up */
	unsigned char size2class[HG_CELL_MAX / HG_ROUNDTO + 1];

	/* the rest is written under the lock, on lines of its own */
	gen_lock_t lock __attribute__((aligned(HG_LINE)));
	unsigned long bm_words;
	unsigned long *starts; /* a bit per page that starts a block or run */
	/* no aligned free run of order k starts below hint[k]; every page at or
	 * above top is in use (page runs are placed from the top down) */
	unsigned long hint[HG_MAX_ORDER + 1];
	unsigned long top;
	unsigned long free_pages;
	struct hg_chunk *partial[HG_NCLASSES];
	struct hg_chunk *drained[HG_NCLASSES];
	unsigned int ndrained[HG_NCLASSES];
	/* drained blocks a class keeps: what it took back into use in the last
	 * second, at least HG_KEEP_MIN */
	unsigned int keep[HG_NCLASSES];
	unsigned int demand[HG_NCLASSES]; /* carves and drained-block reuses */
	unsigned long sec;				  /* the second demand[] counts */
	unsigned long carved;			  /* bytes in blocks and page runs */
	unsigned long carved_max;
	unsigned long pool_free; /* bytes of the cells in the shared pool */
	unsigned long nchunks, nlarge;
	unsigned long carves, releases;	   /* blocks cut and given back, lifetime */
	unsigned long oom_fails, oom_last; /* since the last report, and when */
	unsigned long refused; /* requests refused for lack of memory, lifetime */
	unsigned long bad;	   /* refused frees and dropped lists */
	unsigned long bm[];	   /* a bit per page in use, then starts[] */
};

_Static_assert(HG_HDR == sizeof(struct hg_hdr) && HG_HDR % HG_ROUNDTO == 0,
		"the payload follows the header, HG_ROUNDTO aligned");
_Static_assert(HG_CHUNK_HDR == HG_LINE, "a block's cells start one line in");

/* ~x1.5 ladder of cell sizes, header included, multiples of 32; the powers
 * of two from 8 KB are 64 B less, so they fill their blocks */
static const unsigned int cell_sizes[HG_NCLASSES] = {64, 96, 128, 192, 256, 384,
		512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8128, 12288, 16320, 24576,
		32704, 49152, 65472};

#define HG_SLOT_SHM 0
#define HG_SLOT_PKG 1
#define HG_SLOTS 2
static struct hg_arena *hg_arenas[HG_SLOTS];
static __thread struct hg_tcache hg_tc[HG_SLOTS];
static pthread_key_t hg_exit_key;
static int hg_exit_key_ok;
static char *_hg_pkg_pool;

/* the page bitmaps, under the lock */

enum
{
	BM_CLEAR,
	BM_SET,
	BM_ALL_CLEAR,
	BM_ALL_SET
};

/* sets or clears @n bits from @from, or tells if all are clear or all set */
static int bm_op(unsigned long *bm, unsigned long from, unsigned long n, int op)
{
	unsigned long w, b, k, m;

	for(; n; from += k, n -= k) {
		w = from / HG_BITS;
		b = from % HG_BITS;
		k = HG_BITS - b < n ? HG_BITS - b : n;
		m = (k == HG_BITS) ? ~0UL : ((1UL << k) - 1) << b;
		if(op == BM_SET)
			bm[w] |= m;
		else if(op == BM_CLEAR)
			bm[w] &= ~m;
		else if((op == BM_ALL_SET ? ~bm[w] : bm[w]) & m)
			return 0;
	}
	return 1;
}

/* the arena struct at the start of the pool, its two bitmaps after it, the
 * page grid from the next page boundary; @size is the pool from @a */
static int pages_init(struct hg_arena *a, unsigned long size)
{
	unsigned long words = ((size >> HG_PAGE_SHIFT) + HG_BITS - 1) / HG_BITS;
	unsigned long end = (unsigned long)a + size, pbase, npages;

	pbase = HG_ALIGN_UP((unsigned long)&a->bm[2 * words], HG_PAGE);
	if(size < sizeof *a + 2 * words * sizeof(unsigned long)
			|| pbase + 4 * HG_PAGE > end) {
		LM_ERR("%s: %lu bytes are too few for an arena\n", a->name, size);
		return -1;
	}
	npages = (end - pbase) >> HG_PAGE_SHIFT;
	if(npages > (unsigned long)(((1ULL << 32) - 1)
								>> (HG_PAGE_SHIFT - HG_REF_SHIFT))) {
		LM_ERR("%s: an arena holds at most %lu GB\n", a->name,
				(unsigned long)((1ULL << (32 + HG_REF_SHIFT)) >> 30));
		return -1;
	}
	a->bm_words = (npages + HG_BITS - 1) / HG_BITS;
	a->starts = &a->bm[a->bm_words];
	memset(a->bm, 0, 2 * a->bm_words * sizeof(unsigned long));
	/* bits past the last page stay set, so no scan runs into them */
	if(npages % HG_BITS)
		a->bm[a->bm_words - 1] = ~0UL << (npages % HG_BITS);
	a->pbase = (char *)pbase;
	a->pend = a->pbase + (npages << HG_PAGE_SHIFT);
	a->npages = a->free_pages = a->top = npages;
	return 0;
}

static void pages_take(struct hg_arena *a, unsigned long s, unsigned long n)
{
	bm_op(a->bm, s, n, BM_SET);
	bm_op(a->starts, s, 1, BM_SET);
	a->free_pages -= n;
}

/* 2^k pages at the lowest free 2^k-aligned index (buddy placement without
 * its bookkeeping); -1 if none */
static long pages_alloc_block(struct hg_arena *a, unsigned int k)
{
	unsigned long n = 1UL << k, s;

	for(s = HG_ALIGN_UP(a->hint[k], n);
			n <= a->free_pages && s + n <= a->npages; s += n) {
		if(n < HG_BITS && a->bm[s / HG_BITS] == ~0UL)
			s = (s | (HG_BITS - 1)) + 1 - n; /* a word in use: the next */
		else if(bm_op(a->bm, s, n, BM_ALL_CLEAR)) {
			pages_take(a, s, n);
			a->hint[k] = s + n; /* nothing of this order is free below */
			return (long)s;
		}
	}
	return -1;
}

/* @n pages from the top down, so runs never fragment the blocks; -1 if none */
static long pages_alloc_run(struct hg_arena *a, unsigned long n)
{
	unsigned long p = a->top, len = 0, w;

	if(n == 0 || n > a->free_pages)
		return -1;
	while(len < n && p > 0) {
		w = a->bm[(p - 1) / HG_BITS];
		if(p % HG_BITS == 0 && (w == 0 || w == ~0UL)) {
			p -= HG_BITS; /* a whole word free, or in use */
			len = w ? 0 : len + HG_BITS;
		} else {
			p--;
			len = ((w >> (p % HG_BITS)) & 1) ? 0 : len + 1;
		}
	}
	if(len < n)
		return -1;
	p += len - n;
	pages_take(a, p, n);
	if(p + n == a->top)
		a->top = p;
	return (long)p;
}

/* @n pages from @idx in use, no block or run starting inside them, and one
 * starting at @idx exactly if @start */
static int pages_ok(
		struct hg_arena *a, unsigned long idx, unsigned long n, int start)
{
	return n > 0 && idx < a->npages && n <= a->npages - idx
		   && bm_op(a->starts, idx, 1, BM_ALL_SET) == start
		   && (n == 1 || bm_op(a->starts, idx + 1, n - 1, BM_ALL_CLEAR))
		   && bm_op(a->bm, idx, n, BM_ALL_SET);
}

/* a block or run from its first page (@start), or a run's tail pages */
static void pages_free(
		struct hg_arena *a, unsigned long idx, unsigned long n, int start)
{
	unsigned int k;

	if(!pages_ok(a, idx, n, start)) {
		LM_CRIT("%s: pages %lu+%lu are not a %s in use - not freed\n", a->name,
				idx, n, start ? "block or run" : "run's tail");
		return;
	}
	if(start)
		bm_op(a->starts, idx, 1, BM_CLEAR);
	bm_op(a->bm, idx, n, BM_CLEAR);
	a->free_pages += n;
	/* an aligned run of any order this completes starts at or after idx
	 * rounded down to its alignment */
	for(k = 0; k <= HG_MAX_ORDER; k++)
		if(a->hint[k] > (idx & ~((1UL << k) - 1)))
			a->hint[k] = idx & ~((1UL << k) - 1);
	if(idx + n > a->top)
		a->top = idx + n;
}

/* cells and blocks */

static inline void *cell_payload(struct hg_cell *cs)
{
	return &cs->next;
}

static inline struct hg_cell *cell_of(void *p)
{
	return (struct hg_cell *)((char *)p - offsetof(struct hg_cell, next));
}

static inline int hg_in_pages(const struct hg_arena *a, const void *p)
{
	return (const char *)p >= a->pbase && (const char *)p < a->pend;
}

/* may @cs be a header of @a: in its pages and aligned as one */
static inline int cell_ptr_ok(const struct hg_arena *a, const void *cs)
{
	return hg_in_pages(a, cs) && ((unsigned long)cs & (HG_ROUNDTO - 1)) == 0;
}

static inline unsigned int hdr_load(const struct hg_cell *cs)
{
	return __atomic_load_n(&cs->h.w, __ATOMIC_RELAXED);
}

static inline void hdr_store(struct hg_cell *cs, unsigned int w)
{
	__atomic_store_n(&cs->h.w, w, __ATOMIC_RELAXED);
}

/* LIVE -> FREE in one compare-and-swap: 1 if this free won */
static inline int hdr_claim(
		struct hg_cell *cs, unsigned int live, unsigned int cls)
{
	return __atomic_compare_exchange_n(&cs->h.w, &live,
			HG_WORD(cls, HG_ST_FREE), 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

static inline unsigned long page_of(const struct hg_arena *a, const void *p)
{
	return (unsigned long)((const char *)p - a->pbase) >> HG_PAGE_SHIFT;
}

static inline struct hg_chunk *chunk_of(
		const struct hg_arena *a, unsigned int block)
{
	return (struct hg_chunk *)(a->pbase
							   + ((unsigned long)block << HG_REF_SHIFT));
}

static inline unsigned int chunk_ref(
		const struct hg_arena *a, const struct hg_chunk *ch)
{
	return (unsigned int)(((const char *)ch - a->pbase) >> HG_REF_SHIFT);
}

/* a block header in its place, its tag matching its class and place */
static int chunk_ok(const struct hg_arena *a, const struct hg_chunk *ch)
{
	return hg_in_pages(a, ch) && !((unsigned long)ch & (HG_PAGE - 1))
		   && ch->cls < HG_NCLASSES
		   && ch->tag == (HG_TAG ^ ch->cls ^ chunk_ref(a, ch))
		   && ch->in_gpool <= a->blk_cells[ch->cls];
}

/* the block a class-@c cell names, if it holds the cell there by the
 * arena's layout (its header is not read); else NULL */
static struct hg_chunk *cell_block(
		const struct hg_arena *a, unsigned int c, const struct hg_cell *cs)
{
	struct hg_chunk *ch = chunk_of(a, cs->h.u.block);
	unsigned long off, i;

	if(!hg_in_pages(a, ch) || ((unsigned long)ch & (HG_PAGE - 1))
			|| (const char *)cs < ch->area)
		return NULL;
	off = (unsigned long)((const char *)cs - ch->area);
	if(off >= (HG_PAGE << a->blk_order[c]))
		return NULL;
	i = (unsigned long)(((unsigned long long)off * a->cell_div[c])
						>> HG_DIV_SHIFT);
	return i < a->blk_cells[c] && i * cell_sizes[c] == off ? ch : NULL;
}

/* a caller's mistake was caught: counted, fatal with mem_safety 0 */
static void hg_bad_count(struct hg_arena *a)
{
	__atomic_fetch_add(&a->bad, 1, __ATOMIC_RELAXED);
	if(cfg_get(core, core_cfg, mem_safety) == 0) {
		LM_CRIT("%s: aborting on the memory error above (mem_safety = 0)\n",
				a->name);
		abort();
	}
}

#define hg_bad(a, fmt, args...)                 \
	do {                                        \
		LM_CRIT("%s: " fmt, (a)->name, ##args); \
		hg_bad_count(a);                        \
	} while(0)

/* the lock is held */
static void hg_oom(struct hg_arena *a, unsigned long size)
{
	unsigned long now = (unsigned long)time(NULL);

	a->oom_fails++;
	a->refused++;
	if(now - a->oom_last >= HG_OOM_PERIOD) {
		LM_ERR("%s: out of memory - a %lu byte request was refused, %lu "
			   "failures since the last report, %lu of %lu pages free; "
			   "raise -%c\n",
				a->name, size, a->oom_fails, a->free_pages, a->npages,
				a->shared ? 'm' : 'M');
		a->oom_last = now;
		a->oom_fails = 0;
	}
}

/* the shared pool, under the lock */

static inline struct hg_chunk **list_head(
		struct hg_arena *a, unsigned int c, unsigned int l)
{
	return (l == HG_L_DRAINED) ? &a->drained[c] : &a->partial[c];
}

/* a broken link (a block header overwritten): the class's list is dropped,
 * its blocks stay in use unlisted, as far as the links can be followed */
static void list_drop(struct hg_arena *a, unsigned int c, unsigned int l)
{
	struct hg_chunk *ch, *nxt;
	unsigned long n = 0;

	hg_bad(a, "class %u %s list has a broken link - dropping the list\n", c,
			l == HG_L_DRAINED ? "drained" : "partial");
	for(ch = *list_head(a, c, l); ch && n < a->nchunks && chunk_ok(a, ch)
								  && ch->cls == c && ch->list == l;
			ch = nxt, n++) {
		nxt = ch->fnext;
		ch->fnext = ch->fprev = NULL;
		ch->list = HG_L_NONE;
	}
	*list_head(a, c, l) = NULL;
	if(l == HG_L_DRAINED)
		a->ndrained[c] = 0;
}

static void list_unlink(struct hg_arena *a, struct hg_chunk *ch)
{
	struct hg_chunk **head = list_head(a, ch->cls, ch->list);

	if(ch->list == HG_L_NONE)
		return;
	/* both neighbours must point back at the block */
	if((ch->fprev ? !chunk_ok(a, ch->fprev) || ch->fprev->fnext != ch
				  : *head != ch)
			|| (ch->fnext
					&& (!chunk_ok(a, ch->fnext) || ch->fnext->fprev != ch)))
		list_drop(a, ch->cls, ch->list);
	else {
		if(ch->fprev)
			ch->fprev->fnext = ch->fnext;
		else
			*head = ch->fnext;
		if(ch->fnext)
			ch->fnext->fprev = ch->fprev;
		if(ch->list == HG_L_DRAINED)
			a->ndrained[ch->cls]--;
	}
	ch->fnext = ch->fprev = NULL;
	ch->list = HG_L_NONE;
}

/* the block onto the list its pool count calls for; usually a no-op */
static void chunk_regrade(struct hg_arena *a, struct hg_chunk *ch)
{
	unsigned int l = ch->in_gpool == 0						 ? HG_L_NONE
					 : ch->in_gpool >= a->blk_cells[ch->cls] ? HG_L_DRAINED
															 : HG_L_PARTIAL;
	struct hg_chunk **head;

	if(l == ch->list)
		return;
	list_unlink(a, ch);
	if(l == HG_L_NONE)
		return;
	head = list_head(a, ch->cls, l);
	ch->fnext = *head;
	if(ch->fnext)
		ch->fnext->fprev = ch;
	*head = ch;
	ch->list = l;
	if(l == HG_L_DRAINED)
		a->ndrained[ch->cls]++;
}

/* a drained block goes back to the pages */
static void chunk_release(struct hg_arena *a, struct hg_chunk *ch)
{
	list_unlink(a, ch);
	a->nchunks--;
	a->releases++;
	a->carved -= HG_PAGE << a->blk_order[ch->cls];
	a->pool_free -= (unsigned long)a->blk_cells[ch->cls] * cell_sizes[ch->cls];
	ch->tag = 0;
	pages_free(a, page_of(a, ch), 1UL << a->blk_order[ch->cls], 1);
}

/* drained blocks of class @c back to the pages, down to @keep */
static unsigned int release_drained(
		struct hg_arena *a, unsigned int c, unsigned int keep)
{
	struct hg_chunk *ch;
	unsigned int n = 0;

	for(; a->ndrained[c] > keep && (ch = a->drained[c]) != NULL; n++) {
		if(!chunk_ok(a, ch) || ch->cls != c || ch->list != HG_L_DRAINED) {
			list_drop(a, c, HG_L_DRAINED);
			break;
		}
		chunk_release(a, ch);
	}
	return n;
}

/* before a request is refused: every class's drained blocks go back */
static unsigned int reclaim_drained(struct hg_arena *a)
{
	unsigned int c, n = 0;

	for(c = 0; c < HG_NCLASSES; c++)
		n += release_drained(a, c, 0);
	return n;
}

/* a cell becomes shared: onto its own block's list; -1 if refused */
static int gpool_push(struct hg_arena *a, unsigned int c, struct hg_cell *cs)
{
	struct hg_chunk *ch = cell_block(a, c, cs);

	if(!ch || !chunk_ok(a, ch) || ch->cls != c) {
		hg_bad(a,
				"cell %p (class %u) names block %p, which does not hold it "
				"- header overwritten; the cell leaks\n",
				(void *)cs, c, (void *)chunk_of(a, cs->h.u.block));
		return -1;
	}
	/* a link rewritten after a free can lead to a live cell */
	if(hdr_load(cs) != HG_WORD(c, HG_ST_FREE)) {
		hg_bad(a,
				"cell %p (class %u) on a free list is not free - header or "
				"link overwritten; not pooled\n",
				(void *)cs, c);
		return -1;
	}
	if(ch->in_gpool >= a->blk_cells[c]) {
		hg_bad(a,
				"block %p already has all its cells free - %p is freed "
				"twice; ignored\n",
				(void *)ch, (void *)cs);
		return -1;
	}
	cs->next = ch->free_head;
	ch->free_head = cs;
	ch->in_gpool++;
	a->pool_free += cell_sizes[c];
	chunk_regrade(a, ch);
	if(ch->list == HG_L_DRAINED && a->ndrained[c] > a->keep[c])
		chunk_release(a, ch);
	return 0;
}

/* a shared cell of class @c, from a partial block first */
static struct hg_cell *gpool_pop(struct hg_arena *a, unsigned int c)
{
	unsigned int l = a->partial[c] ? HG_L_PARTIAL : HG_L_DRAINED;
	struct hg_chunk *ch = *list_head(a, c, l);
	struct hg_cell *cs;

	if(!ch)
		return NULL;
	if(l == HG_L_DRAINED)
		a->demand[c]++; /* a drained block goes back into use */
	if(!chunk_ok(a, ch) || ch->cls != c || ch->list != l) {
		list_drop(a, c, l);
		return NULL;
	}
	cs = ch->free_head;
	/* the head must be a cell of this block, or the chain is not followed */
	if(!cs || !cell_ptr_ok(a, cs) || cell_block(a, c, cs) != ch) {
		hg_bad(a,
				"block %p (class %u) claims %u free cells but its chain "
				"leads to %p - dropping the chain, those cells leak\n",
				(void *)ch, c, ch->in_gpool, (void *)cs);
		a->pool_free -= (unsigned long)ch->in_gpool * cell_sizes[c];
		ch->in_gpool = 0;
		ch->free_head = NULL;
		chunk_regrade(a, ch);
		return NULL;
	}
	ch->free_head = cs->next;
	ch->in_gpool--;
	a->pool_free -= cell_sizes[c];
	chunk_regrade(a, ch);
	return cs;
}

/* a class keeps the drained blocks it took back into use in the last second;
 * returning them at once churned the bitmap thousands of times a second */
static void keep_tick(struct hg_arena *a, unsigned long now)
{
	unsigned int c;

	a->sec = now;
	for(c = 0; c < HG_NCLASSES; c++) {
		a->keep[c] = a->demand[c] > HG_KEEP_MIN ? a->demand[c] : HG_KEEP_MIN;
		a->demand[c] = 0;
		release_drained(a, c, a->keep[c]);
	}
}

/* a new block of class @c, all of it this thread's to hand out */
static int carve_chunk(struct hg_arena *a, unsigned int c, struct hg_tcache *tc)
{
	unsigned long size = HG_PAGE << a->blk_order[c];
	struct hg_chunk *ch;
	long idx;

	a->demand[c]++;
	idx = pages_alloc_block(a, a->blk_order[c]);
	if(idx < 0 && reclaim_drained(a))
		idx = pages_alloc_block(a, a->blk_order[c]);
	if(idx < 0) {
		hg_oom(a, size);
		return -1;
	}
	ch = (struct hg_chunk *)(a->pbase + ((unsigned long)idx << HG_PAGE_SHIFT));
	memset(ch, 0, sizeof *ch);
	ch->cls = c;
	ch->tag = HG_TAG ^ c ^ chunk_ref(a, ch);
	a->nchunks++;
	a->carves++;
	a->carved += size;
	if(a->carved > a->carved_max)
		a->carved_max = a->carved;
	tc->cls[c].bump = ch->area;
	tc->cls[c].left = a->blk_cells[c];
	tc->cls[c].block = chunk_ref(a, ch);
	return 0;
}

/* per-thread caches */

static inline struct hg_tcache *tc_of(const struct hg_arena *a)
{
	return &hg_tc[a->slot];
}

/* a cell is dropped from a private list, and so is the rest of the list,
 * which the cell's link would have led to */
static void private_drop(struct hg_arena *a, struct hg_tcache *tc,
		unsigned int c, struct hg_cell *cs, const char *why)
{
	hg_bad(a,
			"cell %p on the class %u private list is %s - dropping the "
			"list, its cells leak\n",
			(void *)cs, c, why);
	tc->cls[c].free_head = NULL;
	tc->cls[c].nfree = 0;
}

/* up to @n cells of a private list to the pool; the lock is held */
static void private_give(struct hg_arena *a, struct hg_tcache *tc,
		unsigned int c, unsigned int n)
{
	struct hg_cell *d;

	while(n-- && (d = tc->cls[c].free_head) != NULL) {
		if(!cell_ptr_ok(a, d)) {
			private_drop(a, tc, c, d, "not a cell of the arena");
			return;
		}
		tc->cls[c].free_head = d->next;
		tc->cls[c].nfree--;
		if(gpool_push(a, c, d) < 0) {
			tc->cls[c].free_head = NULL; /* its link is not trusted either */
			break;
		}
	}
	if(!tc->cls[c].free_head)
		tc->cls[c].nfree = 0;
}

/* the own block's cells of class @c not handed out yet go to the pool, like
 * freed ones; the lock is held */
static void bump_return(
		struct hg_arena *a, struct hg_tcache *tc, unsigned int c)
{
	struct hg_cell *cs;

	for(; tc->cls[c].left; tc->cls[c].left--) {
		cs = (struct hg_cell *)tc->cls[c].bump;
		tc->cls[c].bump += cell_sizes[c];
		cs->h.u.block = tc->cls[c].block;
		hdr_store(cs, HG_WORD(c, HG_ST_FREE));
		if(gpool_push(a, c, cs) < 0)
			tc->cls[c].left = 1;
	}
}

/* the lock, and the once-a-second work: the arena's keep counts, and the
 * thread's own blocks of classes idle since, so they can drain */
static void arena_lock(struct hg_arena *a, struct hg_tcache *tc)
{
	unsigned long now;
	unsigned int c;

	lock_get(&a->lock);
	now = (unsigned long)time(NULL);
	if(now != a->sec)
		keep_tick(a, now);
	if(!tc || now == tc->sec)
		return;
	tc->sec = now;
	for(c = 0; c < HG_NCLASSES; c++) {
		if(tc->cls[c].left && !tc->cls[c].used)
			bump_return(a, tc, c);
		tc->cls[c].used = 0;
	}
}

/* the thread-exit destructor, armed on a thread's first cached cell */
static void tc_arm(struct hg_tcache *tc)
{
	tc->armed = 1;
	if(hg_exit_key_ok)
		pthread_setspecific(hg_exit_key, tc);
}

/* everything this thread caches in @a back to the pool: before a fork of a
 * shared arena, and when a thread exits */
static void cache_flush(struct hg_arena *a)
{
	struct hg_tcache *tc = tc_of(a);
	unsigned int c;

	arena_lock(a, NULL);
	for(c = 0; c < HG_NCLASSES; c++) {
		private_give(a, tc, c, (unsigned int)-1);
		bump_return(a, tc, c);
	}
	lock_release(&a->lock);
}

static void hg_thread_exit(void *arg)
{
	int i;

	for(i = 0; i < HG_SLOTS; i++)
		if(hg_arenas[i])
			cache_flush(hg_arenas[i]);
}

static void private_caps_set(struct hg_arena *a, unsigned long consumers);

/* the parent flushes its shm caches before a fork and the child drops its
 * copy; pkg is copied with its caches, and its lock is held over the fork */
static void hg_fork_prepare(void)
{
	struct hg_arena *shm = hg_arenas[HG_SLOT_SHM];

	/* every process shares shm: its caches are sized from their count once
	 * the attendant knows it */
	if(shm && pt && process_no == 0
			&& (unsigned long)get_max_procs() != shm->consumers) {
		private_caps_set(shm, (unsigned long)get_max_procs());
		LM_INFO("shm hg_malloc caches: up to %u cells per class and process, "
				"%lu processes\n",
				shm->priv_max[0], shm->consumers);
	}
	if(shm)
		cache_flush(shm);
	if(hg_arenas[HG_SLOT_PKG])
		lock_get(&hg_arenas[HG_SLOT_PKG]->lock);
}

static void hg_fork_parent(void)
{
	if(hg_arenas[HG_SLOT_PKG])
		lock_release(&hg_arenas[HG_SLOT_PKG]->lock);
}

static void hg_fork_child(void)
{
	memset(&hg_tc[HG_SLOT_SHM], 0, sizeof hg_tc[HG_SLOT_SHM]);
	hg_fork_parent();
}

/* page runs: requests over the cell limit */

static void *hg_large_alloc(struct hg_arena *a, unsigned long size)
{
	/* refused before any arithmetic that could wrap */
	unsigned long n = size > (a->npages << HG_PAGE_SHIFT) - HG_HDR
							  ? 0
							  : (size + HG_HDR + HG_PAGE - 1) >> HG_PAGE_SHIFT;
	struct hg_cell *h;
	long idx = -1;

	lock_get(&a->lock);
	if(n && (idx = pages_alloc_run(a, n)) < 0 && reclaim_drained(a))
		idx = pages_alloc_run(a, n);
	if(idx >= 0) {
		a->nlarge++;
		a->carved += n << HG_PAGE_SHIFT;
		if(a->carved > a->carved_max)
			a->carved_max = a->carved;
	} else {
		hg_oom(a, size);
	}
	lock_release(&a->lock);
	if(idx < 0)
		return NULL;
	h = (struct hg_cell *)(a->pbase + ((unsigned long)idx << HG_PAGE_SHIFT));
	h->h.u.pages = (unsigned int)n;
	hdr_store(h, HG_WORD(HG_LARGE, HG_ST_LIVE));
	return cell_payload(h);
}

/* is @cs the header of a run of the pages it names; the lock is held */
static int run_ok(struct hg_arena *a, const struct hg_cell *cs)
{
	return ((unsigned long)cs & (HG_PAGE - 1)) == 0
		   && pages_ok(a, page_of(a, cs), cs->h.u.pages, 1);
}

/* @cs is a LIVE run header */
static void hg_large_free(
		struct hg_arena *a, struct hg_cell *cs, unsigned int w)
{
	const char *why = NULL;

	lock_get(&a->lock);
	if(hdr_load(cs) != w)
		why = "was freed twice at the same moment";
	else if(!run_ok(a, cs))
		why = "is not a page run of the arena (header overwritten)";
	else if(!hdr_claim(cs, w, HG_LARGE))
		why = "was freed twice at the same moment";
	else {
		pages_free(a, page_of(a, cs), cs->h.u.pages, 1);
		a->nlarge--;
		a->carved -= (unsigned long)cs->h.u.pages << HG_PAGE_SHIFT;
	}
	lock_release(&a->lock);
	if(why)
		hg_bad(a, "%p %s - not freed\n", cell_payload(cs), why);
}

/* the request path */

static void *hg_cell_alloc(struct hg_arena *a, unsigned long size)
{
	struct hg_tcache *tc;
	struct hg_cell *cs;
	unsigned int c, got;

	if(size > HG_CELL_MAX - HG_HDR)
		return hg_large_alloc(a, size);
	c = a->size2class[(size + HG_ROUNDTO - 1) / HG_ROUNDTO];
	tc = tc_of(a);
	for(;;) {
		cs = tc->cls[c].free_head;
		if(likely(cs != NULL)) {
			/* the range, then the header, then the link - the same line */
			if(unlikely(!cell_ptr_ok(a, cs)))
				private_drop(a, tc, c, cs, "not a cell of the arena");
			else if(unlikely(hdr_load(cs) != HG_WORD(c, HG_ST_FREE)))
				private_drop(a, tc, c, cs, "not a free cell of the class");
			else {
				tc->cls[c].free_head = cs->next;
				tc->cls[c].nfree--;
				hdr_store(cs, HG_WORD(c, HG_ST_LIVE));
				return cell_payload(cs);
			}
			continue;
		}
		if(tc->cls[c].left) {
			cs = (struct hg_cell *)tc->cls[c].bump;
			tc->cls[c].bump += cell_sizes[c];
			tc->cls[c].left--;
			tc->cls[c].used = 1;
			cs->h.u.block = tc->cls[c].block;
			hdr_store(cs, HG_WORD(c, HG_ST_LIVE));
			return cell_payload(cs);
		}
		/* the slow path: a batch from the pool, at most the cap, else a new
		 * block */
		if(unlikely(!tc->armed))
			tc_arm(tc);
		arena_lock(a, tc);
		for(got = 0; got < a->priv_max[c] && got < HG_REFILL_BATCH
					 && (cs = gpool_pop(a, c)) != NULL;
				got++) {
			cs->next = tc->cls[c].free_head;
			tc->cls[c].free_head = cs;
			tc->cls[c].nfree++;
		}
		if(!got && carve_chunk(a, c, tc) < 0) {
			lock_release(&a->lock);
			return NULL;
		}
		lock_release(&a->lock);
		/* pkg.stats refresh once per pool batch, after the lock: the
		 * handler reads them through hg_info() */
		if(!a->shared)
			sr_event_exec(SREV_PKG_UPDATE_STATS, 0);
	}
}

static void hg_cell_free(struct hg_arena *a, void *p)
{
	struct hg_cell *cs = cell_of(p);
	struct hg_tcache *tc;
	unsigned int w, c;
	int ok;

	if(unlikely(!hg_in_pages(a, cs))) {
		/* a caller mixed shm and pkg: the free goes where it came from */
		struct hg_arena *o =
				hg_arenas[a->slot == HG_SLOT_SHM ? HG_SLOT_PKG : HG_SLOT_SHM];

		if(!o || !hg_in_pages(o, cs)) {
			hg_bad(a, "%p was not allocated by hg_malloc - not freed\n", p);
			return;
		}
		hg_bad(a, "%p belongs to the %s arena - freed there\n", p, o->name);
		a = o;
	}
	ok = cell_ptr_ok(a, cs);
	w = ok ? hdr_load(cs) : 0;
	c = HG_CLS(w);
	if(unlikely(!ok || HG_ST(w) != HG_ST_LIVE
				|| (c != HG_LARGE
						&& (c >= HG_NCLASSES || !cell_block(a, c, cs))))) {
		hg_bad(a, "%p is %s - not freed\n", p,
				!ok						 ? "not aligned as an allocation"
				: HG_ST(w) == HG_ST_FREE ? "already free (double free)"
				: HG_ST(w) != HG_ST_LIVE
						? "not a live allocation (header overwritten, or "
						  "never allocated)"
						: "not the start of an allocation of its class");
		return;
	}
	if(c == HG_LARGE) {
		hg_large_free(a, cs, w);
		return;
	}
	if(unlikely(!hdr_claim(cs, w, c))) {
		hg_bad(a, "%p was freed twice at the same moment - ignored\n", p);
		return;
	}
	tc = tc_of(a);
	if(unlikely(!tc->armed))
		tc_arm(tc);
	cs->next = tc->cls[c].free_head;
	tc->cls[c].free_head = cs;
	if(unlikely(++tc->cls[c].nfree > a->priv_max[c])) {
		/* over the bound: part of the list goes to the pool */
		arena_lock(a, tc);
		private_give(a, tc, c, a->priv_donate[c]);
		lock_release(&a->lock);
	}
}

/* the memapi entry points */

static void *hg_malloc(void *ap, size_t size HG_DBG_PARAMS)
{
	return hg_cell_alloc((struct hg_arena *)ap, size);
}

static void *hg_mallocxz(void *ap, size_t size HG_DBG_PARAMS)
{
	void *p = hg_cell_alloc((struct hg_arena *)ap, size);

	if(p)
		memset(p, 0, size);
	return p;
}

static void *hg_mallocxn(void *ap, size_t size HG_DBG_PARAMS)
{
	void *p = hg_cell_alloc((struct hg_arena *)ap, size);

	if(p && size > 0)
		((char *)p)[size - 1] = 0;
	return p;
}

static void hg_free(void *ap, void *p HG_DBG_PARAMS)
{
	if(p)
		hg_cell_free((struct hg_arena *)ap, p);
}

/* the cell or run is kept while the new size fits it; else a new one */
static void *hg_realloc(void *ap, void *p, size_t size HG_DBG_PARAMS)
{
	struct hg_arena *a = (struct hg_arena *)ap;
	struct hg_cell *cs = p ? cell_of(p) : NULL;
	unsigned long cap = 0, have = 0, need;
	unsigned int w, c;
	void *np;

	if(!p || size == 0) {
		if(p)
			hg_cell_free(a, p);
		return size ? hg_cell_alloc(a, size) : NULL;
	}
	w = cell_ptr_ok(a, cs) ? hdr_load(cs) : 0;
	c = HG_CLS(w);
	if(HG_ST(w) == HG_ST_LIVE && c < HG_NCLASSES && cell_block(a, c, cs))
		cap = cell_sizes[c] - HG_HDR;
	else if(HG_ST(w) == HG_ST_LIVE && c == HG_LARGE) {
		lock_get(&a->lock);
		if(run_ok(a, cs)) {
			have = cs->h.u.pages;
			cap = (have << HG_PAGE_SHIFT) - HG_HDR;
		}
		lock_release(&a->lock);
	}
	if(!cap) {
		hg_bad(a,
				"realloc of %p, which is not a live allocation of the "
				"arena - refused\n",
				p);
		return NULL;
	}
	if(size <= cap) {
		/* a page run gives back the pages it no longer needs */
		need = (size + HG_HDR + HG_PAGE - 1) >> HG_PAGE_SHIFT;
		if(need < have) {
			lock_get(&a->lock);
			cs->h.u.pages = (unsigned int)need;
			pages_free(a, page_of(a, cs) + need, have - need, 0);
			a->carved -= (have - need) << HG_PAGE_SHIFT;
			lock_release(&a->lock);
		}
		return p;
	}
	np = hg_cell_alloc(a, size);
	if(np) {
		memcpy(np, p, cap);
		hg_cell_free(a, p);
	}
	return np;
}

static void *hg_reallocxf(void *ap, void *p, size_t size HG_DBG_PARAMS)
{
	void *r = hg_realloc(ap, p, size HG_DBG_ARGS);

	if(!r && p)
		hg_cell_free((struct hg_arena *)ap, p);
	return r;
}

/* free, then allocate: the contents are not kept */
static void *hg_resize(void *ap, void *p, size_t size HG_DBG_PARAMS)
{
	if(p)
		hg_cell_free((struct hg_arena *)ap, p);
	return hg_cell_alloc((struct hg_arena *)ap, size);
}

/* figures: block-granular, so cells in private caches count as used */

static void hg_info(void *ap, struct mem_info *info)
{
	struct hg_arena *a = (struct hg_arena *)ap;

	memset(info, 0, sizeof *info);
	lock_get(&a->lock);
	info->total_size = a->npages << HG_PAGE_SHIFT;
	info->free_size = a->free_pages << HG_PAGE_SHIFT;
	info->real_used = a->carved;
	info->used_size = a->carved - a->pool_free;
	info->max_used = a->carved_max;
	info->min_frag = cell_sizes[0];
	info->total_frags = a->nchunks + a->nlarge;
	lock_release(&a->lock);
}

static void hg_report(void *ap, mem_report_t *mrep)
{
	struct mem_info mi;

	hg_info(ap, &mi);
	memset(mrep, 0, sizeof *mrep);
	mrep->total_size = mi.total_size;
	mrep->free_size_s = mi.free_size;
	mrep->used_size_s = mi.used_size;
	mrep->real_used_s = mi.real_used;
	mrep->max_used_s = mi.max_used;
	mrep->total_frags = mrep->used_frags = mi.total_frags;
}

static unsigned long hg_available(void *ap)
{
	return ((struct hg_arena *)ap)->free_pages << HG_PAGE_SHIFT;
}

#define HG_MEMLOG(lev, fmt, args...) \
	LOG_FP(DEFAULT_FACILITY, lev, "hg_status: ", fmt, ##args)

static void hg_status(void *ap)
{
	struct hg_arena *a = (struct hg_arena *)ap;
	struct hg_chunk *ch;
	unsigned int c, l, n[2], cells, i;
	int memlog = cfg_get(core, core_cfg, memlog);

	HG_MEMLOG(memlog, "(%p) %s:\n", ap, a ? a->name : "");
	if(!a)
		return;
	/* a signal handler may have interrupted a locked section, or a process
	 * died holding the lock: give up rather than wait */
	for(i = 0; i < HG_STATUS_TRIES && lock_try(&a->lock) != 0; i++)
		sched_yield();
	if(i == HG_STATUS_TRIES) {
		HG_MEMLOG(memlog, " no figures - the arena lock stays taken\n");
		return;
	}
	HG_MEMLOG(memlog, " heap size= %lu, carved= %lu (peak %lu), free= %lu\n",
			a->npages << HG_PAGE_SHIFT, a->carved, a->carved_max,
			a->free_pages << HG_PAGE_SHIFT);
	HG_MEMLOG(memlog,
			" blocks= %lu (cut %lu, given back %lu), page runs= %lu, "
			"cells in the pool= %lu bytes, errors= %lu, refused= %lu\n",
			a->nchunks, a->carves, a->releases, a->nlarge, a->pool_free, a->bad,
			a->refused);
	for(c = 0; c < HG_NCLASSES; c++) {
		for(l = 0, cells = 0; l < 2; l++)
			for(ch = *list_head(a, c, l ? HG_L_DRAINED : HG_L_PARTIAL),
			n[l] = 0;
					ch && n[l] < a->nchunks && chunk_ok(a, ch);
					ch = ch->fnext, n[l]++)
				cells += ch->in_gpool;
		if(n[0] || n[1])
			HG_MEMLOG(memlog,
					" class %2u (%5u B): %u partial, %u drained blocks "
					"(keeps %u), %u cells in the pool\n",
					c, cell_sizes[c], n[0], n[1], a->keep[c], cells);
	}
	lock_release(&a->lock);
}

static void hg_sums(void *ap)
{
	LOG_FP(DEFAULT_FACILITY, cfg_get(core, core_cfg, memlog), "hg_sums: ",
			"not available - hg_malloc keeps no per-site accounting (%p)\n",
			ap);
}

static void hg_mod_get_stats(void *ap, void **rootp)
{
	LM_WARN("hg_malloc keeps no per-site accounting - no module "
			"statistics\n");
}

static void hg_mod_free_stats(void *rootp)
{
}

static __thread int hg_glocked;

/* hg locks internally; tm brackets its frees with shm_global_lock() */
static void hg_glock(void *ap)
{
	hg_glocked++;
}

/* an unlock with no lock before it is cleanup()'s, once every child exited:
 * a child inside the arena lock at SIGTERM exits holding it */
static void hg_gunlock(void *ap)
{
	if(hg_glocked > 0)
		hg_glocked--;
	else if(ap)
		lock_release(&((struct hg_arena *)ap)->lock);
}

/* arenas */

/* the private cache budget: HG_PRIVATE_PCT of the arena over its sharers (0:
 * the CPUs); at least a cell per class, so alloc-free pairs never lock */
static void private_caps_set(struct hg_arena *a, unsigned long consumers)
{
	long cpus = sysconf(_SC_NPROCESSORS_ONLN);
	unsigned long per_class, n;
	unsigned int c;

	if(!consumers)
		consumers = cpus < HG_CONSUMERS_MIN	  ? HG_CONSUMERS_MIN
					: cpus > HG_CONSUMERS_MAX ? HG_CONSUMERS_MAX
											  : (unsigned long)cpus;
	per_class = (a->npages << HG_PAGE_SHIFT) / 100 * HG_PRIVATE_PCT / consumers
				/ HG_NCLASSES;
	for(c = 0; c < HG_NCLASSES; c++) {
		n = per_class / cell_sizes[c];
		n = n > HG_PRIVATE_CAP ? HG_PRIVATE_CAP : n ? n : 1;
		a->priv_max[c] = (unsigned int)n;
		a->priv_donate[c] = n / 2 ? (unsigned int)(n / 2) : 1;
	}
	a->consumers = consumers;
}

/* the block order of class @c: HG_CELLS_PER_BLOCK cells within the bounds,
 * rounded up to a power of two of pages, which the cells fill anyway */
static unsigned int chunk_order_for(const struct hg_arena *a, unsigned int c)
{
	unsigned long want = (unsigned long)cell_sizes[c] * HG_CELLS_PER_BLOCK;
	unsigned long least = HG_CHUNK_HDR + HG_BLOCK_SLACK + 2UL * cell_sizes[c];
	unsigned int k = 0;

	want = want < HG_CHUNK_MIN ? HG_CHUNK_MIN : want;
	want = want > a->chunk_max ? a->chunk_max : want;
	while((HG_PAGE << k) < (want < least ? least : want))
		k++;
	return k;
}

static struct hg_arena *hg_arena_init(
		char *pool, unsigned long size, char *name, int shared, int slot)
{
	struct hg_arena *a =
			(struct hg_arena *)HG_ALIGN_UP((unsigned long)pool, HG_LINE);
	unsigned long skip = (unsigned long)((char *)a - pool), idx;
	unsigned int c;

	if(size <= skip + sizeof *a) {
		LM_ERR("%s: %lu bytes are too few for an arena\n", name, size);
		return NULL;
	}
	memset(a, 0, sizeof *a);
	a->name = name;
	a->shared = shared;
	a->slot = slot;
	if(pages_init(a, size - skip) < 0)
		return NULL;
	if(lock_init(&a->lock) == 0) {
		LM_ERR("%s: could not initialize the arena lock\n", name);
		return NULL;
	}
	/* blocks: a 64th of the arena, within the bounds */
	a->chunk_max = (unsigned int)((a->npages << HG_PAGE_SHIFT) / 64);
	a->chunk_max = a->chunk_max > HG_CHUNK_BIG	 ? HG_CHUNK_BIG
				   : a->chunk_max < HG_CHUNK_MIN ? HG_CHUNK_MIN
												 : a->chunk_max;
	for(c = 0; c < HG_NCLASSES; c++) {
		a->blk_order[c] = (unsigned char)chunk_order_for(a, c);
		a->blk_cells[c] =
				(unsigned int)(((HG_PAGE << a->blk_order[c]) - HG_CHUNK_HDR
									   - HG_BLOCK_SLACK)
							   / cell_sizes[c]);
		a->cell_div[c] =
				((1ULL << HG_DIV_SHIFT) + cell_sizes[c] - 1) / cell_sizes[c];
		a->keep[c] = HG_KEEP_MIN;
	}
	private_caps_set(a, shared ? 0 : 1); /* pkg has one owner */
	a->sec = (unsigned long)time(NULL);
	/* payload size -> class: the cell must hold the header too */
	for(idx = 0, c = 0; idx <= HG_CELL_MAX / HG_ROUNDTO; idx++) {
		while(c < HG_NCLASSES && cell_sizes[c] < idx * HG_ROUNDTO + HG_HDR)
			c++;
		a->size2class[idx] = (unsigned char)c;
	}
	if(!hg_arenas[HG_SLOT_SHM] && !hg_arenas[HG_SLOT_PKG]) {
		hg_exit_key_ok = pthread_key_create(&hg_exit_key, hg_thread_exit) == 0;
		if(pthread_atfork(hg_fork_prepare, hg_fork_parent, hg_fork_child) != 0
				|| !hg_exit_key_ok)
			LM_WARN("pthread_key_create() or pthread_atfork() failed - exiting "
					"threads or forked children may keep stale caches\n");
	}
	hg_arenas[slot] = a;
	LM_NOTICE("%s hg_malloc arena: %lu MB in %lu pages, blocks up to %u KB\n",
			name, (a->npages << HG_PAGE_SHIFT) >> 20, a->npages,
			a->chunk_max >> 10);
	return a;
}

/* the memapi table fields both managers share */
#define HG_API_FILL(ma, a, pool, destroy)    \
	do {                                     \
		memset(&(ma), 0, sizeof(ma));        \
		(ma).mname = "hg_malloc";            \
		(ma).mem_pool = (pool);              \
		(ma).mem_block = (a);                \
		(ma).xmalloc = hg_malloc;            \
		(ma).xmallocxz = hg_mallocxz;        \
		(ma).xmallocxn = hg_mallocxn;        \
		(ma).xfree = hg_free;                \
		(ma).xrealloc = hg_realloc;          \
		(ma).xreallocxf = hg_reallocxf;      \
		(ma).xstatus = hg_status;            \
		(ma).xinfo = hg_info;                \
		(ma).xreport = hg_report;            \
		(ma).xavailable = hg_available;      \
		(ma).xsums = hg_sums;                \
		(ma).xdestroy = (destroy);           \
		(ma).xmodstats = hg_mod_get_stats;   \
		(ma).xfmodstats = hg_mod_free_stats; \
	} while(0)

static void hg_pkg_destroy(void)
{
	if(hg_arenas[HG_SLOT_PKG])
		lock_destroy(&hg_arenas[HG_SLOT_PKG]->lock);
	hg_arenas[HG_SLOT_PKG] = NULL;
	free(_hg_pkg_pool);
	_hg_pkg_pool = NULL;
}

/* pkg: the process's own pool, from malloc() as for f_malloc */
int hg_malloc_init_pkg_manager(void)
{
	sr_pkg_api_t ma;
	struct hg_arena *a = NULL;

	_hg_pkg_pool = malloc(pkg_mem_size);
	if(_hg_pkg_pool)
		a = hg_arena_init(_hg_pkg_pool, pkg_mem_size, "pkg", 0, HG_SLOT_PKG);
	if(!a) {
		LM_CRIT("could not initialize the hg pkg memory pool\n");
		fprintf(stderr, "Too much hg pkg memory demanded: %ld bytes\n",
				pkg_mem_size);
		hg_pkg_destroy();
		return -1;
	}
	HG_API_FILL(ma, a, _hg_pkg_pool, hg_pkg_destroy);
	return pkg_init_api(&ma);
}

static void hg_shm_destroy(void)
{
	if(hg_arenas[HG_SLOT_SHM])
		lock_destroy(&hg_arenas[HG_SLOT_SHM]->lock);
	hg_arenas[HG_SLOT_SHM] = NULL;
}

/* shm: the core's pool */
int hg_malloc_init_shm_manager(void)
{
	sr_shm_api_t ma;
	struct hg_arena *a = NULL;
	void *pool;

	/* one arena that every process shares: -z blocks would split it */
	if(shm_core_get_pool_count() != 1) {
		LM_CRIT("hg_malloc manages one shm block - start without -z or with "
				"-z 1\n");
		return -1;
	}
	pool = shm_core_get_pool(0);
	if(pool)
		a = hg_arena_init(
				pool, shm_core_get_pool_size(0), "shm", 1, HG_SLOT_SHM);
	if(!a) {
		LM_CRIT("could not initialize the hg shm memory pool\n");
		fprintf(stderr, "Too much hg shm memory demanded: %ld bytes\n",
				shm_mem_size);
		return -1;
	}
	HG_API_FILL(ma, a, pool, hg_shm_destroy);
	ma.xmalloc_unsafe = hg_malloc;
	ma.xfree_unsafe = hg_free;
	ma.xresize = hg_resize;
	ma.xglock = hg_glock;
	ma.xgunlock = hg_gunlock;
	ma.xlocked = 1;
	return shm_init_api(&ma, 1);
}

#endif /* HG_MALLOC */
