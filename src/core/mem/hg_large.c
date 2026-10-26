/*
 * slab allocator with elastic arenas - large-object tier
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

#include "hg_malloc.h"
#include "hg_large.h"
#include "hg_dbg.h"
#include "hg_buddy.h"
#include "../dprint.h"

#define HG_LARGE_MIN_FRAG HG_ROUNDTO
#define HG_LARGE_DEFAULT_CHUNK (1UL << 20) /* 1 MB - amortizes future churn */
/* on a big arena large chunks are whole huge pages: a chunk_max-sized one
 * wastes about half of each buddy block on 256 KB requests */
#define HG_LARGE_BIG_ARENA (256UL << 20)

struct hg_large_chunk
{
	struct hg_large_chunk *next;
	unsigned long size;
	/* the buddy block or run under this chunk, header and slack included */
	unsigned long backing;
	/* how it was taken, so it can be given back the same way: a buddy
	 * block of this order, or -1 for a run of whole pages */
	int ord;
	struct hg_lfrag *first_frag;
	struct hg_lfrag *last_frag; /* sentinel (size 0): ends forward coalescing */
};

/* size after the hg_lfrag header, as fm_frag->size in f_malloc */
#define HG_LFRAG_NEXT(f) \
	((struct hg_lfrag *)(void *)((char *)(f) + HG_LFRAG_HDR + (f)->size))

static inline void lfrag_insert_free(struct hg_block *hb, struct hg_lfrag *frag)
{
	frag->prev = &hb->large_free;
	frag->nxt_free = hb->large_free;
	if(hb->large_free)
		hb->large_free->prev = &frag->nxt_free;
	hb->large_free = frag;
}

static inline void lfrag_remove_free(struct hg_lfrag *frag)
{
	*(frag->prev) = frag->nxt_free;
	if(frag->nxt_free)
		frag->nxt_free->prev = frag->prev;
	frag->prev = NULL;
}

#ifdef DBG_HG_MALLOC
void *hg_large_alloc(struct hg_block *hb, unsigned long size, const char *file,
		const char *func, unsigned int line, const char *mname)
#else
void *hg_large_alloc(struct hg_block *hb, unsigned long size)
#endif
{
	unsigned long need, chunk_size, rest, backing = 0;
	int chunk_ord = -1;
	struct hg_lfrag *f, *n;
	struct hg_large_chunk *ch;
	/* must start NULL: the grow-and-retry loop below reads it first */
	char *base = NULL, *tag;
	/* the guarded size is 32 bits: a larger request goes unguarded */
	int guard = hg_guard_on(hb) && size < 0xffffffffUL;

	/* round to HG_PAYLOAD_ALIGN: a frag's size places the next frag */
	need = (HG_CELL_HDR + size
				   + (hb->dbg	   ? HG_DBG_TAIL
						   : guard ? HG_GUARD_ROOM
								   : 0)
				   + HG_PAYLOAD_ALIGN - 1)
		   & ~(unsigned long)(HG_PAYLOAD_ALIGN - 1);

	hg_lock_enter(hb, HG_LK_LARGE);

	/* first fit over one unsorted free list; large allocations are rare */
	f = NULL;
	for(n = hb->large_free; n; n = n->nxt_free) {
		if(n->size >= need) {
			f = n;
			break;
		}
	}

	if(!f) {
		/* grow: carve a chunk for this request from the same arena, with a
		 * floor so small large-object churn does not carve a chunk each time */
		chunk_size = HG_LFRAG_HDR * 2 + need;
		/* the 1 MB floor only on a big arena (see chunk_size_for()) */
		{
			unsigned long floor = HG_LARGE_DEFAULT_CHUNK;
			if(hb->size >= HG_LARGE_BIG_ARENA) {
				/* one whole page, header included, so the buddy hands
				 * back exactly a page and not a two-page run */
				floor = hb->hps;
				if(chunk_size + sizeof(struct hg_large_chunk) < floor)
					chunk_size =
							(floor - sizeof(struct hg_large_chunk)) & ~63UL;
			} else {
				if(floor > hb->chunk_max)
					floor = hb->chunk_max;
				if(chunk_size < floor)
					chunk_size = floor;
			}
		}
		chunk_size = (chunk_size + 63) & ~63UL;

		/* chunks come from the buddy: one block up to a page, else a run of
		 * whole pages (a big hash table is one allocation of several MB) */
		{
			unsigned long total = chunk_size + sizeof(struct hg_large_chunk);
			int ord = hg_buddy_order_for(hb, total);
			int attempt;

			/* two passes: a miss grows the arena, the retry is then served from
			 * fresh pages; a second miss means the grow was refused */
			for(attempt = 0; attempt < 2 && !base; attempt++) {
				if(attempt
						&& hg_buddy_grow_unlocks(hb, total, HG_GROW_EXHAUSTION)
								   != 0)
					break;
				if(ord >= 0) {
					base = hg_buddy_alloc(hb, (unsigned int)ord);
					if(base) {
						backing = HG_LEAF_SIZE << ord;
						chunk_ord = ord;
						chunk_size = backing - sizeof(struct hg_large_chunk);
					}
				} else {
					unsigned long np = (total + hb->hps - 1) >> hb->hps_shift;

					base = hg_buddy_alloc_run(hb, np);
					if(base) {
						backing = np << hb->hps_shift;
						chunk_size = backing - sizeof(struct hg_large_chunk);
					}
				}
			}
			chunk_size &= ~63UL;
		}
		if(!base) {
			int first = hg_oom_first(hb, need);

			hg_lock_leave(hb);
			if(first)
				LM_ERR("%s: no more HG_MALLOC arena memory for a %lu byte "
					   "large chunk (need %lu bytes for this allocation) - "
					   "increase the arena size; further failures are "
					   "summarised every 30 s\n",
						hb->name, chunk_size, need);
			return NULL;
		}

		ch = (struct hg_large_chunk *)(void *)base;
		ch->size = chunk_size;
		ch->first_frag = (struct hg_lfrag *)(void *)(base + sizeof(*ch));
		ch->last_frag =
				(struct hg_lfrag *)(void *)((char *)ch->first_frag + chunk_size)
				- 1;

		ch->first_frag->size = chunk_size - HG_LFRAG_HDR - HG_LFRAG_HDR;
		ch->first_frag->pf = NULL;
		ch->first_frag->prev = NULL;
		ch->last_frag->size = 0;
		ch->last_frag->pf = ch->first_frag;
		ch->last_frag->prev = NULL;

		ch->next = hb->large_chunks;
		hb->large_chunks = ch;
		ch->backing = backing;
		ch->ord = chunk_ord;
		hb->large_chunks_carved++;

		/* charge the whole buddy block when it is taken, as carve_chunk() does,
		 * so real_used and free_to_carve stay true */
		hb->large_backing += backing;
		hb->real_used += backing;
		if(hb->real_used > hb->max_real_used)
			hb->max_real_used = hb->real_used;

		lfrag_insert_free(hb, ch->first_frag);
		f = ch->first_frag;
		/* f->size is guaranteed >= need: the chunk was sized for it */
	}

	lfrag_remove_free(f);

	/* split off the remainder if it's big enough to be worth keeping as
	 * its own free frag (mirrors f_malloc's fm_split_frag threshold) */
	rest = f->size - need;
	if(rest > HG_LFRAG_HDR + HG_LARGE_MIN_FRAG) {
		f->size = need;
		n = HG_LFRAG_NEXT(f);
		n->size = rest - HG_LFRAG_HDR;
		n->pf = f;
		HG_LFRAG_NEXT(n)->pf = n;
		n->prev = NULL;
		lfrag_insert_free(hb, n);
	}

	/* charge the frag's capacity, not @size: hg_large_free() knows only that */
	{
		struct hg_pstat *ps = hg_pstat_mine(hb);
		ps->used += f->size - HG_CELL_HDR;
		ps->fragments++;
	}
	/* how much of the charged backing is handed out, for hg_large_recycled() */
	hb->large_live += HG_LFRAG_HDR + f->size;

	hg_lock_leave(hb);

	tag = (char *)f + HG_LFRAG_HDR;
	if(guard) {
		unsigned short hsum = hg_hdr_base(tag, HG_LARGE_MARKER);

		hg_guard_set(tag, hsum, f->size - HG_CELL_HDR, size);
		hg_hdr_set_base(tag, HG_LARGE_MARKER, HG_ST_LIVE_G, hsum);
	} else {
		hg_hdr_set(tag, HG_LARGE_MARKER, HG_ST_LIVE);
	}
	if(hb->dbg)
		hg_dbg_tail_set(tag, f->size - HG_CELL_HDR, size);
	hg_san_give(hb, tag, f->size - HG_CELL_HDR, size);
#ifdef DBG_HG_MALLOC
	HG_CELL_AT(tag)->file = file;
	HG_CELL_AT(tag)->func = func;
	HG_CELL_AT(tag)->line = line;
	HG_CELL_AT(tag)->mname = mname;
#endif

	return tag + HG_CELL_HDR;
}

void hg_large_free(struct hg_block *hb, struct hg_lfrag *frag)
{
	struct hg_lfrag *neigh;

	hg_lock_enter(hb, HG_LK_LARGE);

	/* re-check the state under the lock: two racing frees both passed the
	 * unlocked check in hg_cell_free(); a LIVE tag with prev set is damage */
	if(!hg_st_live(hg_hdr_state((char *)frag + HG_LFRAG_HDR)) || frag->prev) {
		hg_lock_leave(hb);
		LM_CRIT("%s: large fragment %p is already free - refusing to "
				"free it again\n",
				hb->name, (void *)frag);
		hg_corrupt(hb, HG_C_DOUBLE_FREE);
		return;
	}
	hg_hdr_set((char *)frag + HG_LFRAG_HDR, HG_LARGE_MARKER, HG_ST_FREE);

	{
		struct hg_pstat *ps = hg_pstat_mine(hb);
		ps->used -= frag->size - HG_CELL_HDR;
		ps->fragments--;
	}
	hb->large_live -= HG_LFRAG_HDR + frag->size;

	/* forward coalesce: neigh->prev is NULL for allocated frags and for a
	 * chunk's sentinel, so this stops at the boundary */
	neigh = HG_LFRAG_NEXT(frag);
	if(neigh->prev) {
		lfrag_remove_free(neigh);
		frag->size += HG_LFRAG_HDR + neigh->size;
		HG_LFRAG_NEXT(neigh)->pf = frag;
	}

	/* backward coalesce */
	neigh = frag->pf;
	if(neigh && neigh->prev) {
		lfrag_remove_free(neigh);
		neigh->size += HG_LFRAG_HDR + frag->size;
		HG_LFRAG_NEXT(frag)->pf = neigh;
		frag = neigh;
	}

	/* first frag (pf NULL) followed by the sentinel = chunk empty; return it
	 * now (carve/return counters show any thrash) */
	if(!frag->pf && HG_LFRAG_NEXT(frag)->size == 0) {
		struct hg_large_chunk *ch = (struct hg_large_chunk *)(void
						*)((char *)frag - sizeof(struct hg_large_chunk));

		if(ch->first_frag == frag) {
			struct hg_large_chunk **pp;
			void *base = ch;
			unsigned long backing = ch->backing;
			int ord = ch->ord;

			for(pp = &hb->large_chunks; *pp; pp = &(*pp)->next)
				if(*pp == ch) {
					*pp = ch->next;
					break;
				}

			hb->large_backing -= backing;
			hb->real_used -= backing;
			hb->large_chunks_returned++;

			/* the buddy writes its links into it: no ch/frag below */
			if(ord >= 0)
				hg_buddy_free(hb, base, (unsigned int)ord);
			else
				hg_buddy_free_run(hb, base);

			hg_lock_leave(hb);
			return;
		}
	}

	lfrag_insert_free(hb, frag);

	hg_lock_leave(hb);
}

unsigned long hg_large_frag_size(const struct hg_lfrag *frag)
{
	return HG_LFRAG_HDR + frag->size;
}

/* the frag size at @frag, for hg_frag_size() in hg_malloc.h */
unsigned long hg_large_frag_size_at(const void *frag)
{
	return hg_large_frag_size((const struct hg_lfrag *)frag);
}

/* the frag header keeps every payload HG_PAYLOAD_ALIGN-aligned */
_Static_assert(HG_LFRAG_HDR_SIZE % HG_PAYLOAD_ALIGN == 0,
		"struct hg_lfrag must be a multiple of HG_PAYLOAD_ALIGN");

void hg_large_destroy(struct hg_block *hb)
{
	/* chunks go back with the reservation's munmap; only drop bookkeeping */
	hb->large_chunks = NULL;
	hb->large_free = NULL;
	hb->large_backing = 0;
	hb->large_live = 0;
}

void hg_large_walk_live(
		struct hg_block *hb, void (*cb)(void *payload, void *ctx), void *ctx)
{
	struct hg_large_chunk *ch;
	struct hg_lfrag *f;

	hg_lock_enter(hb, HG_LK_STATS);

	for(ch = hb->large_chunks; ch; ch = ch->next)
		for(f = ch->first_frag; f != ch->last_frag; f = HG_LFRAG_NEXT(f))
			if(!f->prev) /* live, exact - see hg_large.h */
				cb((char *)f + HG_LFRAG_HDR + HG_CELL_HDR, ctx);

	hg_lock_leave(hb);
}

#endif /* HG_MALLOC */
