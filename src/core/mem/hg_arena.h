/*
 * slab allocator with elastic arenas - arena internals
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

/* slab tier: cells in fixed-size blocks carved from the buddy, a block
 * bound to one class while in use and returned when all its cells are free */

#ifndef hg_arena_h
#define hg_arena_h

#include "hg_malloc.h"

/* class lookup table and bump offset past the @hdr_size block header */
int hg_arena_init(struct hg_block *hb, unsigned long hdr_size);

/* bump-carve @size bytes from hb's reservation, 64-aligned; NULL if spent */
void *hg_chunk_backing(struct hg_block *hb, unsigned long size);

/* flush this thread's cached cells in every arena; owner thread only */
void hg_cache_flush_self(void);
/* the same with nothing kept: shared arenas before fork(), every arena at
 * thread exit */
void hg_cache_fork_prepare(void);
void hg_cache_drain_self(void);
void hg_private_caps_retune(struct hg_block *hb);
int hg_proc_cache_dirty(int proc);

/* drained-block GC, hb->lock held: hg_gc_cycle() sets each class's keep,
 * hg_gc_tick() returns up to @budget blocks above it */
#define HG_GC_KEEP_MAX 8
#define HG_GC_KEEP_BUDGET_SHIFT 8 /* 1/256 of the arena per class */
#define HG_GC_EMERGENCY 32		  /* drained beyond keep: donate path GCs */
#define HG_GC_TICK_BUDGET 32	  /* blocks per maintenance tick */

void hg_gc_cycle(struct hg_block *hb);
unsigned int hg_gc_tick(struct hg_block *hb, unsigned int budget);

/* sweep generation, for threads without a sweep point of their own */
extern volatile unsigned long hg_sweep_gen;

/* reserve floor check on every grid alloc and free; hb->lock held */
void hg_reserve_floor_check(struct hg_block *hb);

/* flush the calling thread's caches if a sweep was published since its
 * last flush; for the TCP reactor pool threads */
void hg_cache_flush_if_due(void);

/* a cell of at least @size bytes, NULL above HG_CELL_MAX or when spent;
 * the debug variant stamps the caller into the header on each call */
#ifdef DBG_HG_MALLOC
void *hg_cell_alloc(struct hg_block *hb, unsigned long size, const char *file,
		const char *func, unsigned int line, const char *mname);
void hg_cell_free(struct hg_block *hb, void *p, const char *file,
		const char *func, unsigned int line, const char *mname);
#else
void *hg_cell_alloc(struct hg_block *hb, unsigned long size);
void hg_cell_free(struct hg_block *hb, void *p);
#endif

/* free straight to the shared pool, for arenas the caller has no cache for */
void hg_cell_free_global(struct hg_block *hb, void *cell);

/* after fork(): drop the calling thread's slot for @hb, see
 * hg_malloc_child_init_all() */
void hg_arena_child_init(struct hg_block *hb);

/* a FREE cell onto the calling thread's private list of @hb - for hg_dbg.c,
 * whose quarantine hands cells back this way */
void hg_cell_push_private(struct hg_block *hb, struct hg_palloc *pl,
		unsigned int c, char *cell_start);

void hg_arena_destroy(struct hg_block *hb);

void hg_arena_stats(
		struct hg_block *hb, unsigned int *nchunks, unsigned long *bytes);

/* @cb(payload, ctx) for every live small cell; exact only before fork */
void hg_arena_walk_live(
		struct hg_block *hb, void (*cb)(void *payload, void *ctx), void *ctx);

#endif /* hg_arena_h */
