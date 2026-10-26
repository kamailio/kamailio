/*
 * slab allocator with elastic arenas - shared pool and block GC
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
#include "hg_arena.h"
#include "hg_arena_int.h"
#include "../dprint.h"

/* --- shared free cells, held per block and graded by fullness ---------- */

/* fullness grade: 0 is the fullest partial block, which refills drain first */
static inline unsigned int grade_of(const struct hg_chunk *ch)
{
	unsigned int g;

	if(ch->in_gpool == 0)
		return HG_GRADE_NONE;
	if(ch->in_gpool >= ch->cells)
		return HG_GRADE_DRAINED;
	g = (ch->in_gpool * HG_GRADES) / ch->cells;
	if(g >= HG_GRADES)
		g = HG_GRADES - 1;
	return g;
}

static inline void list_unlink(struct hg_block *hb, int c, struct hg_chunk *ch)
{
	struct hg_chunk **head;

	if(ch->grade == HG_GRADE_NONE)
		return;
	head = (ch->grade == HG_GRADE_DRAINED) ? &hb->drained[c]
										   : &hb->bucket[c][ch->grade];
	if(ch->fprev)
		ch->fprev->fnext = ch->fnext;
	else
		*head = ch->fnext;
	if(ch->fnext)
		ch->fnext->fprev = ch->fprev;
	if(ch->grade == HG_GRADE_DRAINED) {
		if(hb->drained_tail[c] == ch)
			hb->drained_tail[c] = ch->fprev;
		hb->ndrained[c]--;
	}
	ch->fnext = ch->fprev = NULL;
	ch->grade = HG_GRADE_NONE;
}

static inline void list_link(
		struct hg_block *hb, int c, struct hg_chunk *ch, unsigned int g)
{
	struct hg_chunk **head;

	if(g == HG_GRADE_NONE) {
		ch->grade = HG_GRADE_NONE;
		return;
	}
	head = (g == HG_GRADE_DRAINED) ? &hb->drained[c] : &hb->bucket[c][g];
	ch->fprev = NULL;
	ch->fnext = *head;
	if(ch->fnext)
		ch->fnext->fprev = ch;
	*head = ch;
	ch->grade = g;
	if(g == HG_GRADE_DRAINED) {
		if(!ch->fnext)
			hb->drained_tail[c] = ch; /* was empty */
		hb->ndrained[c]++;
	}
}

/* move @ch to the list for its grade; usually a no-op */
static inline void block_regrade(
		struct hg_block *hb, int c, struct hg_chunk *ch)
{
	unsigned int g = grade_of(ch);

	if(g == ch->grade)
		return;
	list_unlink(hb, c, ch);
	list_link(hb, c, ch, g);
}

/* index of the cell in @ch, or -1 when @cell_start is not a cell boundary */
static inline long cell_index(const struct hg_chunk *ch, const void *cell_start)
{
	unsigned long off =
			(unsigned long)((const char *)cell_start
							- ((const char *)ch + sizeof(struct hg_chunk)));

	if((const char *)cell_start < (const char *)ch + sizeof(struct hg_chunk)
			|| off % ch->cell_size || off / ch->cell_size >= ch->cells)
		return -1;
	return (long)(off / ch->cell_size);
}

/* a link about to be followed must be a free cell of the same block;
 * NULL ends the list only when the count agrees */
static inline int pool_link_ok(struct hg_block *hb, struct hg_chunk *ch,
		void *next, unsigned int remaining)
{
	long i;

	if(!next)
		return remaining == 0;
	i = cell_index(ch, next);
	if(i < 0 || !map_test(ch, i)) {
		LM_CRIT("%s: block %p (class %u) free chain points at %p, which is "
				"%s - the link was overwritten after the free; dropping the "
				"rest of the chain (%u cells leak)\n",
				hb->name, (void *)ch, ch->cls, next,
				i < 0 ? "not one of its cells" : "not free by the map",
				remaining);
		hg_corrupt(hb, HG_C_NFREE_UNDERFLOW);
		return 0;
	}
	return 1;
}

/* return drained blocks of @c to the buddy, down to @keep and at most
 * @limit per call; hb->lock held */
unsigned int hg_gc_class(
		struct hg_block *hb, int c, unsigned int keep, unsigned int limit)
{
	struct hg_chunk *ch;
	unsigned int freed = 0;

	/* down to @keep blocks, at most @limit per call */
	while(hb->ndrained[c] > keep && freed < limit) {
		unsigned int ord;
		unsigned long sz;

		/* oldest drained block from the tail, O(1) */
		ch = hb->drained_tail[c];
#ifdef HG_PARANOIA
		{
			/* HG_PARANOIA builds re-walk the list to check the tail */
			struct hg_chunk *w = hb->drained[c];

			while(w && w->fnext)
				w = w->fnext;
			if(w != ch) {
				LM_CRIT("%s: class %d drained tail %p disagrees with "
						"the walked tail %p\n",
						hb->name, c, (void *)ch, (void *)w);
				hg_corrupt(hb, HG_C_INTERNAL);
				ch = w;
			}
		}
#endif
		if(!ch) {
			/* ndrained says one, the list has none: reset the count */
			LM_CRIT("%s: class %d claims %u drained blocks but the list "
					"is empty - resetting the count\n",
					hb->name, c, hb->ndrained[c]);
			hg_corrupt(hb, HG_C_INTERNAL);
			hb->ndrained[c] = 0;
			break;
		}

		list_unlink(hb, c, ch);
		ch->in_gpool = 0;
		ch->free_head = NULL;

		/* out of the registry, or the capacity count and walkers see it */
		if(ch->prev)
			ch->prev->next = ch->next;
		else
			hb->chunks = ch->next;
		if(ch->next)
			ch->next->prev = ch->prev;
		hb->nchunks--;
		hb->slab_capacity -= (unsigned long)ch->cells * ch->cell_size;

		ord = ch->order;
		sz = HG_LEAF_SIZE << ord;
		hb->real_used -= sz;

		/* every read of ch must precede this: the buddy immediately reuses
		 * the block's first bytes for its own free-list linkage */
		hg_san_release(hb, ch, sz);
		hg_buddy_free(hb, ch, ord);
		freed++;
	}

	if(freed) {
		hb->gc_blocks_returned += freed;
		hb->gc_passes++;
		LM_DBG("%s gc class %d: returned %u blocks, %u drained kept\n",
				hb->name, c, freed, hb->ndrained[c]);
	}
	return freed;
}

/* a cell becomes shared: onto its own block's free list */
int hg_gpool_push(struct hg_block *hb, int c, void *cell_start)
{
	struct hg_chunk *ch = cell_block(hb, c, cell_start);
	long i;

	if(!ch)
		return -1; /* refused above, with a CRIT */
	i = cell_index(ch, cell_start);
	if(i < 0) {
		LM_CRIT("%s: %p is inside block %p (class %u) but not on a cell "
				"boundary - refusing to pool it\n",
				hb->name, cell_start, (void *)ch, ch->cls);
		hg_corrupt(hb, HG_C_FOREIGN_PTR);
		return -1;
	}
	if(map_test(ch, i)) {
		/* exact: the cell is already in the pool (the block-level test below
		 * fires only after the damage) */
		LM_CRIT("%s: cell %p (block %p, class %u, index %ld) is already in "
				"the global pool - double free, refusing the second copy\n",
				hb->name, cell_start, (void *)ch, ch->cls, i);
		hg_corrupt(hb, HG_C_DOUBLE_FREE);
		return -1;
	}
	if(ch->in_gpool >= ch->cells) {
		LM_CRIT("%s: block %p already has all %u cells free, refusing to "
				"add another - this is a double free\n",
				hb->name, (void *)ch, ch->cells);
		hg_corrupt(hb, HG_C_DOUBLE_FREE);
		return -1;
	}
	map_set(ch, i);
	cell_set_next(cell_start, ch->free_head);
	ch->free_head = cell_start;
	ch->in_gpool++;
	block_regrade(hb, c, ch);

	/* deferred during a cache flush: returning a block mid-walk would let the
	 * buddy overwrite cells the walk still follows */
	/* returned here only when far past keep between maintenance ticks */
	if(!hb->gc_deferred && hb->ndrained[c] > hb->gc_keep[c] + HG_GC_EMERGENCY)
		hb->gc_inline_blocks +=
				hg_gc_class(hb, c, hb->gc_keep[c], HG_GC_EMERGENCY);
	return 0;
}

/* take a shared cell from the fullest partial block */
void *hg_gpool_pop(struct hg_block *hb, int c)
{
	struct hg_chunk *ch = NULL;
	void *cell_start;
	unsigned int g;
	long i;

	for(g = 0; g < HG_GRADES; g++)
		if(hb->bucket[c][g]) {
			ch = hb->bucket[c][g];
			break;
		}
	/* nothing partial - reuse a drained block rather than carve a fresh
	 * one; it is already ours and already the right class */
	if(!ch)
		ch = hb->drained[c];
	if(!ch)
		return NULL;

	cell_start = ch->free_head;
	if(!cell_start) {
		LM_CRIT("%s: block %p claims %u free cells but its list is empty - "
				"dropping it from the pool\n",
				hb->name, (void *)ch, ch->in_gpool);
		hg_corrupt(hb, HG_C_NFREE_UNDERFLOW);
		ch->in_gpool = 0;
		memset(ch->gfree, 0, sizeof ch->gfree);
		block_regrade(hb, c, ch);
		return NULL;
	}
	/* the head must be a free cell of this block and its link sound */
	i = cell_index(ch, cell_start);
	if(i < 0 || !map_test(ch, i)
			|| !pool_link_ok(hb, ch, cell_next(cell_start), ch->in_gpool - 1)) {
		if(i >= 0 && map_test(ch, i)) {
			/* bad link: serve the head, leak the rest, never a cell twice */
			map_clear(ch, i);
			ch->in_gpool = 0;
			ch->free_head = NULL;
			memset(ch->gfree, 0, sizeof ch->gfree);
			block_regrade(hb, c, ch);
			return cell_start;
		}
		LM_CRIT("%s: block %p (class %u) free head %p is not a free cell "
				"of the block - dropping the block's chain (%u cells leak)\n",
				hb->name, (void *)ch, ch->cls, cell_start, ch->in_gpool);
		hg_corrupt(hb, HG_C_NFREE_UNDERFLOW);
		ch->in_gpool = 0;
		ch->free_head = NULL;
		memset(ch->gfree, 0, sizeof ch->gfree);
		block_regrade(hb, c, ch);
		return NULL;
	}
	map_clear(ch, i);
	ch->free_head = cell_next(cell_start);
	ch->in_gpool--;
	block_regrade(hb, c, ch);
	return cell_start;
}

#endif /* HG_MALLOC */
