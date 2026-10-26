/*
 * slab allocator with elastic arenas - cell header and chunk layout
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

#ifndef hg_cell_h
#define hg_cell_h

#include <stdio.h>
#include <time.h>
#include <stddef.h> /* offsetof, for the header-layout pins */
#include <string.h> /* memcpy/memset, for the tail guard */
#include <stdint.h> /* uint64_t: 64-bit free-map words on every ABI */
#include "meminfo.h"
#include "hg_kam.h"
#include "hg_san.h"
/* process_no, for hg_pstat_mine() */
#include "../globals.h"

#undef HG_ROUNDTO

#if defined(__CPU_sparc64) || defined(__CPU_sparc)
#define HG_ROUNDTO sizeof(long long)
#else
#define HG_ROUNDTO sizeof(void *)
#endif

#define HG_NCLASSES 21
#define HG_CELL_MAX 65536 /* largest cell; bigger requests use the large tier */

/* the four-tier huge-page ladder, best first */
enum hg_mem_tier
{
	HG_MEM_HUGETLB = 1,	 /* mmap MAP_HUGETLB */
	HG_MEM_THP_ADVISE,	 /* shmem THP via MADV_HUGEPAGE, huge at fault */
	HG_MEM_THP_COLLAPSE, /* shmem THP via MADV_COLLAPSE, post-fill */
	HG_MEM_4K,			 /* plain pages - always works */
};

const char *hg_mem_tier_str(enum hg_mem_tier tier);

/* cell header before every returned pointer: the class word, plus file, func,
 * line and module under DBG_HG_MALLOC; a free cell's payload holds the link */
#ifdef DBG_HG_MALLOC
#define HG_CELL_HDR_DBG (HG_ROUNDTO * 4) /* file, func, line, module */
#else
#define HG_CELL_HDR_DBG 0
#endif

/* payloads 8-aligned for 64-bit fields and atomics, also on 32-bit ARM
 * (HG_ROUNDTO 4); the padding goes at the end of the header */
#define HG_PAYLOAD_ALIGN 8

/* where a guarded cell keeps its size (mem_guard): the spare half of the
 * first word when HG_ROUNDTO is 8, else the header's tail padding */
#if defined(__CPU_sparc64) || defined(__CPU_sparc) || defined(__LP64__) \
		|| defined(_LP64)                                               \
		|| (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
#define HG_REQ_SLOT0 1
#else
#define HG_REQ_SLOT0 0
#endif

/* the header layout, pinned by the _Static_asserts below */
struct hg_cell_hdr
{
	/* class, state and checksum in the first HG_ROUNDTO bytes (hg_hdr_set()) */
	union
	{
		struct
		{
			unsigned char cls;	  /* class id, or HG_LARGE_MARKER */
			unsigned char state;  /* HG_ST_FRESH / _FREE / _LIVE[_G] */
			unsigned short check; /* hg_hdr_make() */
#if HG_REQ_SLOT0
			unsigned int req; /* LIVE_G: keyed size (hg_guard_set()) */
#endif
		};
		unsigned int hw; /* class, state and checksum as one word */
		unsigned char _slot0[HG_ROUNDTO];
	};
#ifdef DBG_HG_MALLOC
	const char *file;	/* HG_ROUNDTO */
	const char *func;	/* HG_ROUNDTO * 2 */
	unsigned long line; /* HG_ROUNDTO * 3 */
	const char *mname;	/* HG_ROUNDTO * 4 */
#endif
#if !HG_REQ_SLOT0
	unsigned int req; /* LIVE_G: keyed size (hg_guard_set()) */
#endif
};

#define HG_CELL_HDR_RAW sizeof(struct hg_cell_hdr)
#define HG_CELL_HDR                                                \
	(((HG_CELL_HDR_RAW + HG_PAYLOAD_ALIGN - 1) / HG_PAYLOAD_ALIGN) \
			* HG_PAYLOAD_ALIGN)

#define HG_STATIC_ASSERT _Static_assert

/* the pins: the documented offsets, and the raw size the arithmetic gave */
HG_STATIC_ASSERT(HG_CELL_HDR_RAW
						 == HG_ROUNDTO + HG_CELL_HDR_DBG
									+ (HG_REQ_SLOT0 ? 0 : sizeof(unsigned int)),
		"struct hg_cell_hdr is not the documented header layout");
HG_STATIC_ASSERT(
		HG_REQ_SLOT0 ? HG_ROUNDTO == 8 && offsetof(struct hg_cell_hdr, req) == 4
					 : HG_CELL_HDR == HG_CELL_HDR_RAW,
		"the guarded size must fit in a word the header leaves spare");
HG_STATIC_ASSERT(offsetof(struct hg_cell_hdr, cls) == 0,
		"the class byte must be the first byte of the header");
HG_STATIC_ASSERT(offsetof(struct hg_cell_hdr, state) == 1
						 && offsetof(struct hg_cell_hdr, check) == 2,
		"the state byte and the checksum must follow the class byte");
HG_STATIC_ASSERT(sizeof(unsigned int) == 4 && HG_ROUNDTO >= 4,
		"class, state and checksum must make up one 32-bit word");
#ifdef DBG_HG_MALLOC
HG_STATIC_ASSERT(
		offsetof(struct hg_cell_hdr, file) == HG_ROUNDTO
				&& offsetof(struct hg_cell_hdr, func) == HG_ROUNDTO * 2
				&& offsetof(struct hg_cell_hdr, line) == HG_ROUNDTO * 3
				&& offsetof(struct hg_cell_hdr, mname) == HG_ROUNDTO * 4,
		"the DBG_HG_MALLOC fields moved");
#endif

#define HG_HDR(p) ((char *)(p) - HG_CELL_HDR)
/* the header of a payload pointer / of a cell-start pointer */
#define HG_CELL(p) ((struct hg_cell_hdr *)(void *)HG_HDR(p))
#define HG_CELL_AT(cs) ((struct hg_cell_hdr *)(void *)(cs))
#define HG_CLASS(p) (HG_CELL(p)->cls)

/* cell state: FRESH when carved, LIVE when handed out, FREE when freed (by CAS,
 * or locked for large frags); LIVE_G is LIVE with a mem_guard tail guard */
#define HG_ST_FRESH 0x3c
#define HG_ST_FREE 0x5a
#define HG_ST_LIVE 0xa5
#define HG_ST_LIVE_G 0x96

static inline int hg_st_live(int st)
{
	return st == HG_ST_LIVE || st == HG_ST_LIVE_G;
}

/* per-run secret mixed into every header checksum, set before the first
 * arena is created (so it is the same in every process) */
extern unsigned long long hg_hdr_secret;

/* 16-bit check over address, class and state: base(addr, class) XOR
 * key(state), one multiply; catches overflows and misdirected pointers */
#define HG_ST_KEY(st) ((unsigned short)((st) * 0x0101u))

static inline unsigned short hg_hdr_base(const void *cs, unsigned int cls)
{
	/* the multiply spreads every input bit into the top 16 */
	unsigned long long x =
			((unsigned long long)(unsigned long)cs ^ hg_hdr_secret ^ cls)
			* 0x9e3779b97f4a7c15ULL;

	return (unsigned short)(x >> 48);
}

/* the header's first word, as the fields struct hg_cell_hdr lays out */
union hg_hdr_w
{
	struct
	{
		unsigned char cls, state;
		unsigned short check;
	} f;
	unsigned int w;
};

HG_STATIC_ASSERT(sizeof(union hg_hdr_w) == 4,
		"the header word must be exactly class, state and checksum");

/* the header word of a cell of class @cls in state @st, from its @base */
static inline unsigned int hg_hdr_make(
		unsigned int cls, unsigned int st, unsigned short base)
{
	union hg_hdr_w h;

	h.f.cls = (unsigned char)cls;
	h.f.state = (unsigned char)st;
	h.f.check = (unsigned short)(base ^ HG_ST_KEY(st));
	return h.w;
}

/* one store, so a racing hg_hdr_claim_free() never sees half a header */
static inline void hg_hdr_set_base(
		void *cs, unsigned int cls, unsigned int st, unsigned short base)
{
	__atomic_store_n(
			&HG_CELL_AT(cs)->hw, hg_hdr_make(cls, st, base), __ATOMIC_RELAXED);
}

static inline void hg_hdr_set(void *cs, unsigned int cls, unsigned int st)
{
	hg_hdr_set_base(cs, cls, st, hg_hdr_base(cs, cls));
}

/* read the header word once and check it: the state, or -1 */
static inline int hg_hdr_read(
		const void *cs, union hg_hdr_w *w, unsigned short *base)
{
	w->w = __atomic_load_n(
			&((const struct hg_cell_hdr *)cs)->hw, __ATOMIC_RELAXED);
	*base = hg_hdr_base(cs, w->f.cls);
	if(w->f.check != (unsigned short)(*base ^ HG_ST_KEY(w->f.state)))
		return -1;
	return w->f.state;
}

static inline int hg_hdr_state(const void *cs)
{
	union hg_hdr_w w;
	unsigned short base;

	return hg_hdr_read(cs, &w, &base);
}

/* LIVE -> FREE in one relaxed CAS: 1 if this free won, 0 if another did;
 * cells reach other threads only via gpool_push() under hb->lock */
static inline int hg_hdr_claim_free(
		void *cs, unsigned int live, unsigned int cls, unsigned short base)
{
	return __atomic_compare_exchange_n(&HG_CELL_AT(cs)->hw, &live,
			hg_hdr_make(cls, HG_ST_FREE, base), 0, __ATOMIC_RELAXED,
			__ATOMIC_RELAXED);
}

/* mem_guard tail guard: req keeps the size asked for, keyed by the header's
 * base; up to HG_GUARD_LEN bytes of HG_GUARD_BYTE follow it, never none */
#define HG_GUARD_BYTE 0xcc
#define HG_GUARD_LEN 8
#define HG_GUARD_WORD 0xccccccccccccccccULL
#define HG_GUARD_ROOM 1

static inline unsigned int hg_guard_key(unsigned short base)
{
	return (unsigned int)base * 0x10001u;
}

/* @size + HG_GUARD_ROOM <= @cap */
static inline void hg_guard_set(
		void *cs, unsigned short base, unsigned long cap, unsigned long size)
{
	unsigned char *g = (unsigned char *)cs + HG_CELL_HDR + size;
	unsigned long long w = HG_GUARD_WORD;

	HG_CELL_AT(cs)->req = (unsigned int)size ^ hg_guard_key(base);
	if(cap - size >= HG_GUARD_LEN)
		memcpy(g, &w, HG_GUARD_LEN);
	else
		memset(g, HG_GUARD_BYTE, cap - size);
}

/* 1 if the guard is intact; @size is the size kept, @cap or more when that
 * was overwritten */
static inline int hg_guard_ok(
		void *cs, unsigned short base, unsigned long cap, unsigned long *size)
{
	const unsigned char *g;
	unsigned long long w;
	unsigned long i;

	*size = HG_CELL_AT(cs)->req ^ hg_guard_key(base);
	if(*size >= cap)
		return 0;
	g = (const unsigned char *)cs + HG_CELL_HDR + *size;
	if(cap - *size >= HG_GUARD_LEN) {
		memcpy(&w, g, HG_GUARD_LEN);
		return w == HG_GUARD_WORD;
	}
	for(i = *size; i < cap; i++, g++)
		if(*g != HG_GUARD_BYTE)
			return 0;
	return 1;
}

/* class byte value of a large-tier cell: first value past the classes */
#define HG_LARGE_MARKER HG_NCLASSES


struct hg_large_chunk; /* opaque here, defined in hg_large.c */

/* the large tier's boundary-tag fragment header (hg_large.c) */
struct hg_lfrag
{
	unsigned long size;		   /* total frag size: hdr + tag + payload */
	struct hg_lfrag *nxt_free; /* NULL if allocated */
	struct hg_lfrag **prev;	   /* NULL if allocated; free-list back-link */
	struct hg_lfrag *pf;	   /* physical previous frag, NULL if first */
} __attribute__((aligned(HG_ROUNDTO))); /* == f_malloc's ROUNDTO everywhere */

#define HG_LFRAG_HDR_SIZE (sizeof(struct hg_lfrag))

/* a chunk is one buddy block of one class, its header in the block's
 * first bytes; the cell header carries the class */
/* fullness grades per class */
#define HG_GRADES 8
#define HG_GRADE_NONE 0xffffffffu	 /* no free cells, on no list */
#define HG_GRADE_DRAINED 0xfffffffeu /* every cell free, on drained[] */

#define HG_CHUNK_MAP_WORDS 2 /* 128 cells per block, at most */
#define HG_CHUNK_MAP_BITS 64 /* bits per map word, every ABI */
#define HG_CHUNK_MAX_CELLS (HG_CHUNK_MAP_WORDS * HG_CHUNK_MAP_BITS)

struct hg_chunk
{
	/* registry, doubly linked so a reclaimed block can leave it */
	struct hg_chunk *next;
	struct hg_chunk *prev;
	unsigned int cls;		/* immutable */
	unsigned int cell_size; /* total slot size, header included */
	unsigned int cells;

	/* cells of this block in the shared pool; == cells means drained, which
	 * is what the GC acts on (privately cached cells still count as live) */
	unsigned int in_gpool;
	unsigned int order; /* buddy order, so the GC can hand it back */

	/* this block's own free cells, so a refill takes them from one block,
	 * the fullest partial */
	void *free_head;
	unsigned int grade; /* HG_GRADE_NONE / _DRAINED, or 0..HG_GRADES-1 */

	/* the drained queue link */
	struct hg_chunk *fnext;
	struct hg_chunk *fprev;

	/* one bit per cell in the shared pool, set at push, cleared at pop: exact
	 * double-free detection and link checks */
	uint64_t gfree[HG_CHUNK_MAP_WORDS];
} __attribute__((aligned(64)));

/* two cache lines on LP64 with the map in the spare bytes; smaller on ILP32,
 * where the pointers shrink - never more */
HG_STATIC_ASSERT(sizeof(struct hg_chunk) <= 128,
		"struct hg_chunk must not exceed two cache lines");

/* buddy geometry: 8 KB leaf; leaves per page follow the probed huge page */
#define HG_LEAF_SHIFT 13
#define HG_LEAF_SIZE (1UL << HG_LEAF_SHIFT)

/* ceiling on buddy orders (the real count is hps_shift - HG_LEAF_SHIFT) */
#define HG_MAX_ORDERS 24

struct hg_page;		/* hg_buddy.h */
struct hg_free_blk; /* hg_buddy.h */

#endif /* hg_cell_h */
