/*
 * slab allocator with elastic arenas - page backing internals
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

#ifndef hg_backing_h
#define hg_backing_h

#include "hg_malloc.h"

/* Linux values only: the same bits mean other things elsewhere (0x40000 is
 * MAP_PREFAULT_READ on FreeBSD); other systems get a plain anonymous mapping */
#ifdef __OS_linux
#ifndef MAP_HUGETLB
#define MAP_HUGETLB 0x40000
#endif
#ifndef MADV_HUGEPAGE
#define MADV_HUGEPAGE 14
#endif
#ifndef MADV_COLLAPSE
#define MADV_COLLAPSE 25
#endif
#ifndef MADV_POPULATE_WRITE
#define MADV_POPULATE_WRITE 23
#endif
#endif /* __OS_linux */

/* default huge page size from /proc/meminfo (2 MB on x86_64, up to 512 MB on
 * arm64); 2 MB only if it cannot be read */
unsigned long hg_hps(void);

#define HG_HPS (hg_hps())

/* small-arena mode: an arena smaller than one huge page runs its grid on
 * 256 KB pages, enough for the largest slab chunk and early large objects */
#define HG_SMALL_PAGE_SHIFT 18
#define HG_SMALL_PAGE (1UL << HG_SMALL_PAGE_SHIFT)

/* the page unit for this init/cap; INIT under a huge page with a larger
 * cap is settled by the tier in hg_malloc_init() */
static inline unsigned long hg_arena_page(
		unsigned long init_b, unsigned long cap_b)
{
	unsigned long m = cap_b > init_b ? cap_b : init_b;

	return (m && m < HG_HPS) ? HG_SMALL_PAGE : HG_HPS;
}

static inline unsigned long hg_page_round(unsigned long v, unsigned long pg)
{
	return (v + pg - 1) & ~(pg - 1);
}

/* the smallest arena that can function in this page unit */
static inline unsigned long hg_min_viable(unsigned long apage)
{
	return apage < HG_HPS ? 2 * apage : apage;
}

/* "512 KB" vs "48 MB" in one printable pair, for the error messages */
#define HG_SZ_VAL(b) ((b) < (1UL << 20) ? (b) >> 10 : (b) >> 20)
#define HG_SZ_UNIT(b) ((b) < (1UL << 20) ? "KB" : "MB")

/* HG_ROUNDTO=2^k so the following works (same trick as f_malloc.c) */
#define ROUNDTO_MASK (~((unsigned long)HG_ROUNDTO - 1))
#define ROUNDUP_TO(s) (((s) + (HG_ROUNDTO - 1)) & ROUNDTO_MASK)

/* hg_backing.c */
long hg_meminfo_kb(const char *key);
void hg_meminfo_kb2(const char *k1, long *v1, const char *k2, long *v2);
unsigned long hg_memlock_limit(void);
const char *hg_4k_reason(int shared);
void *hg_mem_reserve(unsigned long size, unsigned long *cap,
		enum hg_mem_tier *tier, unsigned long *locked_b, int shared,
		unsigned long apage);

/* hg_hostmem.c */
int hg_init_fits(
		const char *what, unsigned long init, unsigned long cap, int shared);

#endif /* hg_backing_h */
