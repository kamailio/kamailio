/*
 * slab allocator with elastic arenas - slab tier internals
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

#ifndef hg_arena_int_h
#define hg_arena_int_h

#include "hg_malloc.h"
#include "hg_buddy.h"

#define HG_MAX_INSTANCES 8 /* arenas one thread can cache in */

/* free-list link lives in the payload, after the header, for private and
 * shared lists alike */
static inline void *cell_next(void *cell_start)
{
	/* via void *: the link is pointer-aligned, this keeps -Wcast-align quiet */
	return *(void **)(void *)((char *)cell_start + HG_CELL_HDR);
}

static inline void cell_set_next(void *cell_start, void *next)
{
	*(void **)(void *)((char *)cell_start + HG_CELL_HDR) = next;
}

/* the block of a cell of class @c, NULL for anything else (a large chunk
 * must not be written as a class block) */
static inline struct hg_chunk *cell_block(struct hg_block *hb, int c, void *p)
{
	struct hg_chunk *ch = hg_buddy_block_of(hb, p);

	if(!ch) {
		LM_CRIT("%s: cell %p (class %d) is not on the arena's page grid - "
				"not touching it\n",
				hb->name, p, c);
		hg_corrupt(hb, HG_C_FOREIGN_PTR);
		return NULL;
	}
	if(ch->cls != (unsigned int)c) {
		LM_CRIT("%s: cell %p resolves to block %p of class %u, expected "
				"class %d - not touching it\n",
				hb->name, p, (void *)ch, ch->cls, c);
		hg_corrupt(hb, HG_C_CLASS_MISMATCH);
		return NULL;
	}
	return ch;
}

static inline int map_test(const struct hg_chunk *ch, long i)
{
	return (int)((ch->gfree[i / HG_CHUNK_MAP_BITS] >> (i % HG_CHUNK_MAP_BITS))
				 & 1);
}

static inline void map_set(struct hg_chunk *ch, long i)
{
	ch->gfree[i / HG_CHUNK_MAP_BITS] |= (uint64_t)1 << (i % HG_CHUNK_MAP_BITS);
}

static inline void map_clear(struct hg_chunk *ch, long i)
{
	ch->gfree[i / HG_CHUNK_MAP_BITS] &=
			~((uint64_t)1 << (i % HG_CHUNK_MAP_BITS));
}

/* hg_pool.c, hb->lock held */
int hg_gpool_push(struct hg_block *hb, int c, void *cell_start);
void *hg_gpool_pop(struct hg_block *hb, int c);
unsigned int hg_gc_class(
		struct hg_block *hb, int c, unsigned int keep, unsigned int limit);

/* hg_cache.c */
unsigned int hg_cache_flush_locked(struct hg_block *hb, struct hg_palloc *pl);

/* hg_arena.c */
struct hg_palloc *hg_palloc_slot(int i);
struct hg_palloc *hg_palloc_mine(struct hg_block *hb);
unsigned int hg_chunk_size_for(struct hg_block *hb, int c);

#endif /* hg_arena_int_h */
