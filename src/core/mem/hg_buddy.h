/*
 * slab allocator with elastic arenas - buddy tree
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

/* one buddy tree per huge page, leaf HG_LEAF_SIZE; no cross-page merging.
 * All of it runs under hb->lock, never on the cell fast path */

#ifndef HG_BUDDY_H
#define HG_BUDDY_H

#include "hg_malloc.h"

/* a free block keeps its list links in its own first bytes; double frees
 * and wild pointers are caught by the bitmap and the hg_buddy_free() checks */
struct hg_free_blk
{
	struct hg_free_blk *next;
	struct hg_free_blk *prev;
};

/* per-page descriptor, in the metadata region, never inside its own page */
/* deferred-collapse attempts per page before it is left on 4 KB pages */
#define HG_COLLAPSE_TRIES 16

struct hg_page
{
	char *base;				  /* first byte of this page */
	unsigned char *leaforder; /* per leaf: order of the block holding it */
	unsigned long *bitmap;	  /* per tree node: 1 = free, whole, not split */

	unsigned int idx;		  /* page index within the arena */
	unsigned int free_leaves; /* leaves not currently allocated */
	/* >0 on the first page of a multi-page run: its length; the other pages
	 * of the run carry HG_RUN_MEMBER (hg_buddy_alloc_run()) */
	unsigned int run_len;
	/* the backing tier this page was committed with (enum hg_mem_tier) */
	unsigned char tier;
	/* backing punched out (MADV_REMOVE) while whole-free; the tree is not
	 * touched, the next carve must re-commit the page first */
	unsigned char punched;
	/* a trim or punch is between list removal and bookkeeping, lock dropped;
	 * bitmap scans must skip the page */
	unsigned char releasing;
	/* MADV_COLLAPSE attempts left, worked off by the maintenance process */
	unsigned char want_collapse;
};

/* run_len marker for a page that belongs to a run but does not head it */
#define HG_RUN_MEMBER 0xffffffffu
/* no per-page per-order free counts: they would triple the descriptor */

/* leaforder[] value for a leaf in no buddy block: a page not yet published,
 * or one of a multi-page run */
#define HG_LEAF_NONE 0xff

/* carve per-page metadata from the arena front, mark it and the header
 * used, publish the rest as free. 0 or -1 */
/* lay out the page grid and verify its address arithmetic; 0 or -1 */
int hg_pages_init(struct hg_block *hb);
int hg_buddy_init(struct hg_block *hb);

/* one block of exactly HG_LEAF_SIZE << @order bytes, naturally aligned;
 * hb->lock held, NULL when no page can serve it */
void *hg_buddy_alloc(struct hg_block *hb, unsigned int order);

/* commit @need rounded up to whole granules (at most the room left) and
 * publish it; hb->lock held. 0 grew, -1 cannot */
/* *_unlocks and the ticks drop hb->lock inside and retake it: nothing read
 * before the call is valid after it */
int hg_buddy_grow_unlocks(
		struct hg_block *hb, unsigned long need, enum hg_grow_why why);
/* the always-on headroom rule on its own (the maintenance process ticks it
 * every second); hb->lock held; 1 = grew */
int hg_grow_headroom_tick(struct hg_block *hb);

/* end a grow-blocked episode; @how finishes "GROW-BLOCKED cleared - ".
 * hb->lock held */
void hg_grow_unblock(struct hg_block *hb, const char *how);

/* promote or disarm a grow-blocked episode each sweep; hb->lock held */
void hg_grow_blocked_tick(struct hg_block *hb);

/* after HG_SHRINK_QUIET_TICKS quiet ticks, start a drain of whole-free
 * pages, never below the initial size; hb->lock held */
void hg_shrink_tick(struct hg_block *hb);
/* one drain step (trim + punch) if the quiet window is open; every
 * maintenance tick, hb->lock held */
void hg_drain_tick(struct hg_block *hb);

/* the headroom rule for an arena the maintenance process does not tick;
 * hb->lock held */
void hg_grow_tick(struct hg_block *hb);

/* return a block from hg_buddy_alloc(), merging it up as far as it goes;
 * hb->lock held */
void hg_buddy_free(struct hg_block *hb, void *p, unsigned int order);

/* the block containing @p, by shifts and a table byte, no search;
 * NULL outside the grid or inside a multi-page run */
static inline void *hg_buddy_block_of(const struct hg_block *hb, const void *p)
{
	const struct hg_page *pg;
	unsigned long leaf;
	unsigned char o;

	if(!hb->buddy_ready || !hg_in_pages(hb, p))
		return NULL;
	pg = &hb->pages[hg_page_of(hb, p)];
	/* a punched page is unbacked and a releasing one about to be: neither
	 * holds a block, whatever its tree says */
	if(pg->punched || pg->releasing)
		return NULL;
	leaf = hg_leaf_of(hb, p);
	o = pg->leaforder[leaf];
	if(o == HG_LEAF_NONE)
		return NULL;
	return pg->base + ((leaf & ~((1UL << o) - 1)) << HG_LEAF_SHIFT);
}

/* may the header at @p be read? Before the grid everything is committed;
 * a grid page is unless punched or being released; the span beyond the
 * grid is reserved only, and on hugetlb a touch there is SIGBUS */
static inline int hg_backed(const struct hg_block *hb, const void *p)
{
	const struct hg_page *pg;

	if(!hb->buddy_ready)
		return (const char *)p < hb->hbase + hb->span_end;
	if((const char *)p < hb->pbase)
		return 1;
	if(!hg_in_pages(hb, p))
		return 0;
	pg = &hb->pages[hg_page_of(hb, p)];
	return !pg->punched && !pg->releasing;
}

/* @npages contiguous whole pages, for allocations above a huge page;
 * a linear scan of page descriptors. hb->lock held, NULL on failure */
void *hg_buddy_alloc_run(struct hg_block *hb, unsigned long npages);

/* release a run from hg_buddy_alloc_run(); hb->lock held */
void hg_buddy_free_run(struct hg_block *hb, void *p);

/* length in pages of the run starting at @p, or 0 if @p heads no run */
unsigned long hg_buddy_run_len(const struct hg_block *hb, const void *p);

/* highest order this arena can serve: the whole-page order */
static inline unsigned int hg_buddy_top_order(const struct hg_block *hb)
{
	return hb->hps_shift - HG_LEAF_SHIFT;
}

/* smallest order whose block holds @bytes; -1 above a whole page, which
 * the caller routes to the large tier */
static inline int hg_buddy_order_for(
		const struct hg_block *hb, unsigned long bytes)
{
	unsigned int o = 0;

	while((HG_LEAF_SIZE << o) < bytes) {
		if(o >= hg_buddy_top_order(hb))
			return -1;
		o++;
	}
	return (int)o;
}

#endif /* HG_BUDDY_H */
