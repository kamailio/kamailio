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

/* large-object tier for requests above HG_CELL_MAX: boundary-tag fragments
 * in whole chunks, one first-fit free list, no coalescing across chunks */

#ifndef hg_large_h
#define hg_large_h

#include "hg_malloc.h"

/* struct hg_lfrag is in hg_cell.h, visible wherever a payload pointer is
 * turned into its frag header */
#define HG_LFRAG_HDR (sizeof(struct hg_lfrag))

/* @size is the usable payload; the headers are added internally */
#ifdef DBG_HG_MALLOC
void *hg_large_alloc(struct hg_block *hb, unsigned long size, const char *file,
		const char *func, unsigned int line, const char *mname);
#else
void *hg_large_alloc(struct hg_block *hb, unsigned long size);
#endif

/* @frag is the struct hg_lfrag* at the very start of the block (i.e.
 * HG_HDR(payload) - HG_LFRAG_HDR), not the payload pointer itself */
void hg_large_free(struct hg_block *hb, struct hg_lfrag *frag);

/* total frag size (hdr+tag+payload), for hg_frag_size()/stats */
unsigned long hg_large_frag_size(const struct hg_lfrag *frag);


void hg_large_destroy(struct hg_block *hb);

/* call @cb(payload, ctx) for every live large fragment (exact) */
void hg_large_walk_live(
		struct hg_block *hb, void (*cb)(void *payload, void *ctx), void *ctx);

#endif /* hg_large_h */
