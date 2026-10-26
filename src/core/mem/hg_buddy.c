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

#ifdef HG_MALLOC

#include <string.h>

#include "hg_malloc.h"
#include "hg_buddy_int.h"
#include "hg_arena.h"
#include "../dprint.h"

/* --- init ------------------------------------------------------------- */

/* release leaves [from, to) of the page the header and metadata share,
 * leaf by leaf through the free path so merges follow the normal rules */
static void page_release_range(struct hg_block *hb, struct hg_page *pg,
		unsigned long from, unsigned long to)
{
	unsigned long leaf;

	for(leaf = from; leaf < to; leaf++) {
		/* hand it to the free path as a legitimately allocated leaf */
		pg->leaforder[leaf] = 0;
		hg_buddy_free(hb, pg->base + (leaf << HG_LEAF_SHIFT), 0);
	}
}

int hg_buddy_init(struct hg_block *hb)
{
	unsigned long i, lpp, bmwords, meta, consumed_leaves, wfwords;
	unsigned int top;
	char *meta_base, *cur;
	struct hg_page *pg;

	if(hb->npages == 0) {
		LM_INFO("%s: no whole pages, buddy reclaim inactive\n", hb->name);
		return 0;
	}

	top = hg_buddy_top_order(hb);
	if(top > HG_MAX_ORDERS) {
		LM_ERR("%s: %u buddy orders exceeds the %d the free-list array "
			   "holds\n",
				hb->name, top, HG_MAX_ORDERS);
		return -1;
	}
	hb->buddy_top = top;

	lpp = hg_leaves_per_page(hb);
	bmwords = (nodes_per_page(top) + sizeof(long) * 8 - 1) / (sizeof(long) * 8);

	/* one metadata carve from the arena front, sized for npages_cap so a
	 * later grow never needs room for metadata in a full arena */
	wfwords = (hb->npages_cap + HG_WF_BITS - 1) / HG_WF_BITS;
	meta = hb->npages_cap
				   * (sizeof(struct hg_page) + lpp + bmwords * sizeof(long))
		   + wfwords * sizeof(long);
	meta_base = hg_chunk_backing(hb, meta);
	if(!meta_base) {
		LM_ERR("%s: cannot carve %lu bytes of buddy metadata for %lu "
			   "pages (%lu committed)\n",
				hb->name, meta, hb->npages_cap, hb->npages);
		return -1;
	}
	memset(meta_base, 0, meta);

	hb->pages = (struct hg_page *)(void *)meta_base;
	cur = meta_base + hb->npages_cap * sizeof(struct hg_page);
	/* the whole-free-page bitmap, ahead of the per-page arrays */
	hb->wfree = (unsigned long *)(void *)cur;
	hb->wfree_words = wfwords;
	cur += wfwords * sizeof(long);
	for(i = 0; i < hb->npages_cap; i++) {
		pg = &hb->pages[i];
		pg->idx = (unsigned int)i;
		pg->base = hb->pbase + (i << hb->hps_shift);
		pg->leaforder = (unsigned char *)cur;
		cur += lpp;
		pg->bitmap = (unsigned long *)(void *)cur;
		cur += bmwords * sizeof(long);
		memset(pg->leaforder, HG_LEAF_NONE, lpp);
		/* the committed pages carry init's achieved tier; pages beyond
		 * get theirs stamped by the grow that commits them */
		pg->tier = (unsigned char)hb->tier;
		/* starts wholly reserved; pages < npages are published below,
		 * pages beyond wait for hg_buddy_grow_unlocks() */
	}

	/* everything below hoff is header and metadata; round up to a leaf */
	consumed_leaves = 0;
	{
		unsigned long hoff = hb->hoff;
		const char *cend =
				hb->hbase + ((hoff + HG_LEAF_SIZE - 1) & ~(HG_LEAF_SIZE - 1));

		if(cend > hb->pbase)
			consumed_leaves =
					(unsigned long)(cend - hb->pbase) >> HG_LEAF_SHIFT;
	}

	for(i = 0; i < hb->npages; i++) {
		unsigned long first = i * lpp, last = first + lpp;

		pg = &hb->pages[i];
		if(consumed_leaves >= last)
			continue; /* wholly consumed */
		if(consumed_leaves <= first) {
			page_publish_whole(hb, pg); /* wholly free */
			continue;
		}
		/* the single straddling page */
		page_release_range(hb, pg, consumed_leaves - first, lpp);
	}

	/* restart the merge counter: building the tree used the free path */
	hb->buddy_merges = 0;
	hb->buddy_splits = 0; /* init allocates nothing; make that explicit */

	hb->buddy_ready = 1;
	/* reserve floor: 1/16 of the grid */
	hb->reserve_floor = (hb->npages * hg_leaves_per_page(hb)) / 16;
	LM_DBG("%s buddy: %lu pages, orders 0..%u (%lu B..%lu B), %lu B metadata "
		   "(%lu B/page, %.3f%%), %lu of %lu leaves free after reserving %lu\n",
			hb->name, hb->npages, top, HG_LEAF_SIZE, HG_LEAF_SIZE << top, meta,
			meta / hb->npages,
			100.0 * (double)meta / (double)hb->committed_bytes,
			hb->buddy_free_leaves, hb->npages * lpp, consumed_leaves);
	return 0;
}

/* rebuild one order's free list from the bitmaps, skipping punched pages;
 * corruption path only */
static void fl_rebuild(struct hg_block *hb, unsigned int o)
{
	unsigned int top = hb->buddy_top;
	unsigned long i, leaf, lpp = hg_leaves_per_page(hb), step = 1UL << o;
	unsigned long before = hb->nfree[o];

	if(o == top)
		memset(hb->wfree, 0, hb->wfree_words * sizeof(long));
	else
		hb->bfree[o] = NULL;
	hb->nfree[o] = 0;
	for(i = 0; i < hb->npages; i++) {
		struct hg_page *pg = &hb->pages[i];

		/* a page being released is off every free list on purpose and
		 * its memory may be going away right now */
		if(pg->punched || pg->releasing)
			continue;
		for(leaf = 0; leaf < lpp; leaf += step)
			if(bit_test(pg->bitmap, node_id(top, o, leaf)))
				fl_push(hb, pg->base + (leaf << HG_LEAF_SHIFT), o);
	}
	LM_WARN("%s: free list order %u rebuilt from the bitmaps: %lu blocks "
			"before, %lu now, %lu page%s punched\n",
			hb->name, o, before, hb->nfree[o], hb->punched_pages,
			hb->punched_pages == 1 ? "" : "s");
}

/* --- allocate --------------------------------------------------------- */

void *hg_buddy_alloc(struct hg_block *hb, unsigned int order)
{
	unsigned int o, top = hb->buddy_top;
	struct hg_page *pg;
	unsigned long leaf;
	char *blk;

	if(!hb->buddy_ready || order > top)
		return NULL;

	/* smallest free block that fits, scanning up from @order */
	for(o = order; o <= top; o++) {
		if(o == top) {
			long idx = wf_first(hb);

			if(idx < 0)
				continue;
			pg = &hb->pages[idx];
		} else {
			if(!hb->bfree[o])
				continue;
			pg = page_of(hb, hb->bfree[o]);
		}
		if(!pg->punched)
			break;
		/* list head on a punched page: refuse before the unlink touches it,
		 * rebuild this order from the bitmaps */
		LM_CRIT("%s: free list order %u heads at punched page %u - the "
				"list and the page descriptor disagree; rebuilding the list "
				"from the bitmaps\n",
				hb->name, o, pg->idx);
		hg_corrupt(hb, HG_C_INTERNAL);
		fl_rebuild(hb, o);
		if(o == top ? wf_first(hb) >= 0 : hb->bfree[o] != NULL)
			break;
	}
	if(o > top)
		return NULL;

	if(o == top) {
		pg = &hb->pages[wf_first(hb)];
		blk = pg->base;
	} else {
		blk = (char *)hb->bfree[o];
		pg = page_of(hb, blk);
	}
	fl_unlink(hb, blk, o);
	leaf = hg_leaf_of(hb, blk);
	bit_clear(pg->bitmap, node_id(top, o, leaf));

	/* split down, publishing the upper half at each step. The lower half
	 * stays in hand, so the returned address never moves. */
	while(o > order) {
		unsigned long bleaf;
		char *buddy;

		o--;
		bleaf = leaf + (1UL << o);
		buddy = pg->base + (bleaf << HG_LEAF_SHIFT);
		memset(pg->leaforder + bleaf, (int)o, (size_t)1UL << o);
		bit_set(pg->bitmap, node_id(top, o, bleaf));
		fl_push(hb, buddy, o);
		hb->buddy_splits++;
	}

	memset(pg->leaforder + leaf, (int)order, (size_t)1UL << order);
	pg->free_leaves -= 1U << order;
	hb->buddy_free_leaves -= 1UL << order;
	hg_reserve_floor_check(hb);
	return blk;
}

/* --- free ------------------------------------------------------------- */

void hg_buddy_free(struct hg_block *hb, void *p, unsigned int order)
{
	unsigned int o = order, top = hb->buddy_top;
	struct hg_page *pg;
	unsigned long leaf;
	char *blk = p;

	if(!hg_in_pages(hb, p)) {
		LM_CRIT("%s: buddy free of %p, which is outside the page grid - "
				"ignoring\n",
				hb->name, p);
		hg_corrupt(hb, HG_C_BUDDY_BAD_FREE);
		return;
	}
	pg = page_of(hb, p);
	leaf = hg_leaf_of(hb, p);

	if(((unsigned long)p & ((HG_LEAF_SIZE << order) - 1))
			!= ((unsigned long)pg->base & ((HG_LEAF_SIZE << order) - 1))) {
		LM_CRIT("%s: buddy free of %p at order %u, which is not aligned to "
				"its own size - ignoring\n",
				hb->name, p, order);
		hg_corrupt(hb, HG_C_BUDDY_BAD_FREE);
		return;
	}
	if(leaf & ((1UL << order) - 1)) {
		LM_CRIT("%s: buddy free of %p as order %u, but leaf %lu does not "
				"start a block of that order - ignoring\n",
				hb->name, p, order, leaf);
		hg_corrupt(hb, HG_C_BUDDY_BAD_FREE);
		return;
	}
	if(pg->leaforder[leaf] != order) {
		LM_CRIT("%s: buddy free of %p as order %u, but leaf %lu records "
				"order %u - ignoring\n",
				hb->name, p, order, leaf, pg->leaforder[leaf]);
		hg_corrupt(hb, HG_C_BUDDY_BAD_FREE);
		return;
	}
	/* double free: the bitmap, not leaforder, says already free and entire */
	if(bit_test(pg->bitmap, node_id(top, order, leaf))) {
		LM_CRIT("%s: double buddy free of %p at order %u - ignoring\n",
				hb->name, p, order);
		hg_corrupt(hb, HG_C_DOUBLE_FREE);
		return;
	}

	pg->free_leaves += 1U << order;
	hb->buddy_free_leaves += 1UL << order;

	/* merge up while the buddy (one bit flipped) is free and entire; stops at
	 * the page */
	while(o < top) {
		unsigned long bleaf = leaf ^ (1UL << o);
		char *buddy = pg->base + (bleaf << HG_LEAF_SHIFT);

		if(!bit_test(pg->bitmap, node_id(top, o, bleaf)))
			break; /* allocated, or split */
		if(pg->leaforder[bleaf] != o)
			break; /* free but not at this order */

		fl_unlink(hb, buddy, o);
		bit_clear(pg->bitmap, node_id(top, o, bleaf));
		hb->buddy_merges++;
		pg->leaforder[bleaf] = HG_LEAF_NONE;

		if(bleaf < leaf) { /* this block is the upper half - move down */
			leaf = bleaf;
			blk = buddy;
		}
		o++;
	}

	memset(pg->leaforder + leaf, (int)o, (size_t)1UL << o);
	bit_set(pg->bitmap, node_id(top, o, leaf));
	fl_push(hb, blk, o);
	hg_reserve_floor_check(hb);
}

/* --- multi-page runs -------------------------------------------------- */

void *hg_buddy_alloc_run(struct hg_block *hb, unsigned long npages)
{
	unsigned int top = hb->buddy_top;
	unsigned long i, start, run = 0;

	if(!hb->buddy_ready || npages == 0)
		return NULL;
	if(npages == 1)
		return hg_buddy_alloc(hb, top);

	for(i = 0, start = 0; i < hb->npages; i++) {
		/* punched pages are off the list and unbacked: a run must skip them */
		if(hb->pages[i].punched || hb->pages[i].releasing
				|| !page_is_whole_free(hb, &hb->pages[i])) {
			run = 0;
			start = i + 1;
			continue;
		}
		if(++run == npages)
			break;
	}
	if(run < npages)
		return NULL;

	for(i = start; i < start + npages; i++) {
		struct hg_page *pg = &hb->pages[i];

		fl_unlink(hb, pg->base, top);
		bit_clear(pg->bitmap, node_id(top, top, 0));
		memset(pg->leaforder, HG_LEAF_NONE, (size_t)hg_leaves_per_page(hb));
		pg->free_leaves = 0;
		pg->run_len = (i == start) ? (unsigned int)npages : HG_RUN_MEMBER;
		hb->buddy_free_leaves -= hg_leaves_per_page(hb);
	}
	hg_reserve_floor_check(hb);
	LM_DBG("%s: run of %lu pages at page %lu\n", hb->name, npages, start);
	return hb->pages[start].base;
}

unsigned long hg_buddy_run_len(const struct hg_block *hb, const void *p)
{
	const struct hg_page *pg;

	if(!hb->buddy_ready || !hg_in_pages(hb, p))
		return 0;
	pg = &hb->pages[hg_page_of(hb, p)];
	if(pg->base != p || pg->run_len == 0 || pg->run_len == HG_RUN_MEMBER)
		return 0;
	return pg->run_len;
}

void hg_buddy_free_run(struct hg_block *hb, void *p)
{
	unsigned long n, i, start;
	struct hg_page *pg;

	n = hg_buddy_run_len(hb, p);
	if(n == 0) {
		LM_CRIT("%s: run free of %p, which heads no run - ignoring\n", hb->name,
				p);
		hg_corrupt(hb, HG_C_BUDDY_BAD_FREE);
		return;
	}
	start = hg_page_of(hb, p);
	for(i = start; i < start + n; i++) {
		pg = &hb->pages[i];
		pg->run_len = 0;
		page_publish_whole(hb, pg);
	}
	hg_reserve_floor_check(hb);
}

/* lay out the page grid and check the address arithmetic on real
 * addresses; 0 usable, -1 not */
int hg_pages_init(struct hg_block *hb)
{
	unsigned long hps = hb->hps, off;
	unsigned int shift = 0;
	const char *end;
	int i;

	if(!hps || (hps & (hps - 1))) {
		LM_ERR("%s: huge page size %lu is not a power of two - cannot "
			   "build the page grid\n",
				hb->name, hps);
		return -1;
	}
	while((1UL << shift) < hps)
		shift++;
	hb->hps_shift = shift;

	/* the shift must match the probed page size */
	if((1UL << shift) != hps) {
		LM_ERR("%s: page shift %u describes %lu bytes, but the probed huge "
			   "page is %lu\n",
				hb->name, shift, 1UL << shift, hps);
		return -1;
	}

	if(shift <= HG_LEAF_SHIFT) {
		LM_ERR("%s: huge page size %lu is not larger than the %lu byte "
			   "buddy leaf\n",
				hb->name, hps, HG_LEAF_SIZE);
		return -1;
	}

	/* page 0 starts at the first aligned address at or after hbase */
	hb->pbase = (char *)(((unsigned long)hb->hbase + hps - 1) & ~(hps - 1));
	end = hb->hbase + hb->span_end;
	hb->npages = (unsigned long)(end - hb->pbase) >> shift;
	/* descriptors for every page up to the cap, so a grow only publishes */
	hb->npages_cap = (unsigned long)(hb->hbase + hb->hcap - hb->pbase) >> shift;
	if(hb->npages_cap < hb->npages) /* hcap==span_end or a tiny arena */
		hb->npages_cap = hb->npages;

	if(hb->pbase != hb->hbase)
		LM_INFO("%s: reservation base %p is not %lu-aligned, losing %lu "
				"bytes of head to align the page grid\n",
				hb->name, hb->hbase, hps,
				(unsigned long)(hb->pbase - hb->hbase));

	if(hb->npages == 0) {
		/* an arena under one huge page has no buddy page; not fatal */
		LM_INFO("%s: arena of %lu bytes holds no whole %lu byte page - "
				"buddy reclaim will be inactive\n",
				hb->name, hb->committed_bytes, hps);
		return 0;
	}

	/* the grid must not describe memory past the reservation */
	if(hb->pbase + (hb->npages << shift) > end) {
		LM_ERR("%s: page grid of %lu pages ends at %p, past the %lu byte "
			   "reservation ending at %p\n",
				hb->name, hb->npages, hb->pbase + (hb->npages << shift),
				hb->span_end, end);
		return -1;
	}

	/* self-check on real addresses over the whole grid */
	for(i = 0; i < 5; i++) {
		unsigned long pg, leaf, want_pg;
		const char *probe, *pbase_of;

		switch(i) {
			case 0:
				want_pg = 0;
				off = 0;
				break;
			case 1:
				want_pg = 0;
				off = HG_LEAF_SIZE;
				break;
			case 2:
				want_pg = hb->npages / 2;
				off = hps / 2;
				break;
			case 3:
				want_pg = hb->npages - 1;
				off = 0;
				break;
			default:
				want_pg = hb->npages - 1;
				off = hps - 1;
				break;
		}
		probe = hb->pbase + (want_pg << shift) + off;

		if(!hg_in_pages(hb, probe)) {
			LM_ERR("%s: addressing self-test %d: %p should be inside the "
				   "%lu-page grid at %p but is not\n",
					hb->name, i, probe, hb->npages, hb->pbase);
			return -1;
		}
		pg = hg_page_of(hb, probe);
		if(pg != want_pg) {
			LM_ERR("%s: addressing self-test %d: %p resolved to page %lu, "
				   "expected %lu\n",
					hb->name, i, probe, pg, want_pg);
			return -1;
		}
		pbase_of = hg_page_base(hb, probe);
		if((unsigned long)pbase_of & (hps - 1)) {
			LM_ERR("%s: addressing self-test %d: page base %p is not "
				   "%lu-aligned\n",
					hb->name, i, pbase_of, hps);
			return -1;
		}
		if(probe - pbase_of != (long)off) {
			LM_ERR("%s: addressing self-test %d: %p is %ld bytes into its "
				   "page, expected %lu\n",
					hb->name, i, probe, (long)(probe - pbase_of), off);
			return -1;
		}
		leaf = hg_leaf_of(hb, probe);
		if(leaf != off >> HG_LEAF_SHIFT || leaf >= hg_leaves_per_page(hb)) {
			LM_ERR("%s: addressing self-test %d: %p is leaf %lu, expected "
				   "%lu of %lu\n",
					hb->name, i, probe, leaf, off >> HG_LEAF_SHIFT,
					hg_leaves_per_page(hb));
			return -1;
		}
	}

	/* the header region and the byte past the end are not page memory */
	if(hb->pbase != hb->hbase && hg_in_pages(hb, hb->hbase)) {
		LM_ERR("%s: addressing self-test: unaligned head %p classifies as "
			   "page memory\n",
				hb->name, hb->hbase);
		return -1;
	}
	if(hg_in_pages(hb, hb->pbase + (hb->npages << shift))) {
		LM_ERR("%s: addressing self-test: the byte past the last page "
			   "classifies as page memory\n",
				hb->name);
		return -1;
	}

	LM_DBG("%s page grid: %lu pages of %lu B at %p (shift %u), %lu leaves "
		   "of %lu B per page; addressing verified\n",
			hb->name, hb->npages, hps, hb->pbase, shift, hg_leaves_per_page(hb),
			HG_LEAF_SIZE);
	return 0;
}

#endif /* HG_MALLOC */
