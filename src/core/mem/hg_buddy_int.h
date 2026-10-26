/*
 * slab allocator with elastic arenas - buddy tree helpers
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

#ifndef hg_buddy_int_h
#define hg_buddy_int_h

#include <string.h>

#include "hg_malloc.h"
#include "hg_buddy.h"

/* leaforder[leaf]: order of the block holding the leaf (HG_LEAF_NONE in a
 * run); bitmap[node]: 1 iff the node is a whole free block */

/* node id in the per-page tree: level 0 is the page, level top the leaves
 * (511 nodes, 64 bytes, for 2 MB pages and 8 KB leaves) */
static inline unsigned long node_id(
		unsigned int top, unsigned int order, unsigned long leaf)
{
	unsigned int level = top - order;

	return (1UL << level) - 1 + (leaf >> order);
}

static inline unsigned long nodes_per_page(unsigned int top)
{
	return (1UL << (top + 1)) - 1;
}

static inline int bit_test(const unsigned long *bm, unsigned long n)
{
	return (bm[n / (sizeof(long) * 8)] >> (n % (sizeof(long) * 8))) & 1UL;
}

static inline void bit_set(unsigned long *bm, unsigned long n)
{
	bm[n / (sizeof(long) * 8)] |= 1UL << (n % (sizeof(long) * 8));
}

static inline void bit_clear(unsigned long *bm, unsigned long n)
{
	bm[n / (sizeof(long) * 8)] &= ~(1UL << (n % (sizeof(long) * 8)));
}

/* whole-free-page bitmap (hb->wfree): one bit per page, find-first-set
 * serves the lowest page so the top drains for shrink */
#define HG_WF_BITS (sizeof(long) * 8)

static inline int wf_test(const struct hg_block *hb, unsigned long idx)
{
	return (hb->wfree[idx / HG_WF_BITS] >> (idx % HG_WF_BITS)) & 1UL;
}

static inline void wf_set(struct hg_block *hb, unsigned long idx)
{
	if(wf_test(hb, idx)) {
		LM_CRIT("%s: page %lu published as whole-free twice\n", hb->name, idx);
		hg_corrupt(hb, HG_C_INTERNAL);
		return;
	}
	hb->wfree[idx / HG_WF_BITS] |= 1UL << (idx % HG_WF_BITS);
	hb->nfree[hb->buddy_top]++;
}

static inline void wf_clear(struct hg_block *hb, unsigned long idx)
{
	if(!wf_test(hb, idx)) {
		LM_CRIT("%s: page %lu taken off offer but was not on it\n", hb->name,
				idx);
		hg_corrupt(hb, HG_C_INTERNAL);
		return;
	}
	hb->wfree[idx / HG_WF_BITS] &= ~(1UL << (idx % HG_WF_BITS));
	hb->nfree[hb->buddy_top]--;
}

/* the lowest whole-free page on offer, or -1 */
static inline long wf_first(const struct hg_block *hb)
{
	unsigned long w;

	for(w = 0; w < hb->wfree_words; w++)
		if(hb->wfree[w])
			return (long)(w * HG_WF_BITS + __builtin_ctzl(hb->wfree[w]));
	return -1;
}

/* --- free lists ------------------------------------------------------- */

static inline void fl_push(struct hg_block *hb, void *p, unsigned int order)
{
	struct hg_free_blk *b = (struct hg_free_blk *)p;

	/* top order in the bitmap; lower orders stay LIFO lists */
	if(order == hb->buddy_top) {
		wf_set(hb, hg_page_of(hb, p));
		return;
	}
	b->prev = NULL;
	b->next = hb->bfree[order];
	if(b->next)
		b->next->prev = b;
	hb->bfree[order] = b;
	hb->nfree[order]++;
}

static inline void fl_unlink(struct hg_block *hb, void *p, unsigned int order)
{
	struct hg_free_blk *b = (struct hg_free_blk *)p;

	if(order == hb->buddy_top) {
		wf_clear(hb, hg_page_of(hb, p));
		return;
	}
	if(b->prev)
		b->prev->next = b->next;
	else
		hb->bfree[order] = b->next;
	if(b->next)
		b->next->prev = b->prev;
	hb->nfree[order]--;
}


/* --- geometry --------------------------------------------------------- */

static inline struct hg_page *page_of(struct hg_block *hb, const void *p)
{
	return &hb->pages[hg_page_of(hb, p)];
}

/* publish an untouched page as one top-order free block */
static inline void page_publish_whole(struct hg_block *hb, struct hg_page *pg)
{
	unsigned int top = hb->buddy_top;

	memset(pg->leaforder, (int)top, (size_t)hg_leaves_per_page(hb));
	bit_set(pg->bitmap, node_id(top, top, 0));
	fl_push(hb, pg->base, top);
	pg->free_leaves = (unsigned int)hg_leaves_per_page(hb);
	hb->buddy_free_leaves += pg->free_leaves;
}

/* is this whole page free and unsplit, i.e. available to a run? */
static inline int page_is_whole_free(
		const struct hg_block *hb, const struct hg_page *pg)
{
	unsigned int top = hb->buddy_top;

	return pg->run_len == 0 && bit_test(pg->bitmap, node_id(top, top, 0))
		   && pg->leaforder[0] == top;
}

#endif /* hg_buddy_int_h */
