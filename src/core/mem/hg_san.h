/*
 * slab allocator with elastic arenas - sanitizer hooks
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

/* tell ASan / Valgrind which bytes of a private-arena cell a caller may touch
 * (shared arenas rely on the debug poison instead); TSan learns hb->lock */

#ifndef hg_san_h
#define hg_san_h

#if defined(__SANITIZE_ADDRESS__)
#define HG_SAN_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define HG_SAN_ASAN 1
#endif
#endif

#if defined(__SANITIZE_THREAD__)
#define HG_SAN_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define HG_SAN_TSAN 1
#endif
#endif

#if defined(HG_SAN_ASAN)
#include <sanitizer/asan_interface.h>
#define HG_SAN_ON 1
#define HG_SAN_CLOSE(p, n) ASAN_POISON_MEMORY_REGION((p), (n))
#define HG_SAN_OPEN(p, n) ASAN_UNPOISON_MEMORY_REGION((p), (n))
#elif defined(HG_VALGRIND)
#include <valgrind/memcheck.h>
#define HG_SAN_ON 1
#define HG_SAN_CLOSE(p, n) ((void)VALGRIND_MAKE_MEM_NOACCESS((p), (n)))
#define HG_SAN_OPEN(p, n) ((void)VALGRIND_MAKE_MEM_DEFINED((p), (n)))
#else
#define HG_SAN_ON 0
#define HG_SAN_CLOSE(p, n) ((void)0)
#define HG_SAN_OPEN(p, n) ((void)0)
#endif

#ifdef HG_SAN_TSAN
void __tsan_acquire(void *addr);
void __tsan_release(void *addr);
#define HG_TSAN_ACQUIRE(a) __tsan_acquire((void *)(a))
#define HG_TSAN_RELEASE(a) __tsan_release((void *)(a))
#else
#define HG_TSAN_ACQUIRE(a) ((void)0)
#define HG_TSAN_RELEASE(a) ((void)0)
#endif

/* the free-list link a free cell keeps at its payload's start */
#define HG_SAN_LINK (sizeof(void *))

#define HG_SAN_PRIVATE(hb) (HG_SAN_ON && !(hb)->shared)

/* @cs is a cell start holding @cap payload bytes */
#define HG_SAN_PAY(cs) ((char *)(cs) + HG_CELL_HDR)

/* handed out: @size bytes open, the rest closed */
#define hg_san_give(hb, cs, cap, size)           \
	do {                                         \
		if(HG_SAN_PRIVATE(hb)) {                 \
			HG_SAN_CLOSE(HG_SAN_PAY(cs), (cap)); \
			HG_SAN_OPEN(HG_SAN_PAY(cs), (size)); \
		}                                        \
	} while(0)

/* with the allocator: closed but for the link */
#define hg_san_take(hb, cs, cap)                                \
	do {                                                        \
		if(HG_SAN_PRIVATE(hb)) {                                \
			HG_SAN_CLOSE(HG_SAN_PAY(cs), (cap));                \
			HG_SAN_OPEN(HG_SAN_PAY(cs),                         \
					(cap) < HG_SAN_LINK ? (cap) : HG_SAN_LINK); \
		}                                                       \
	} while(0)

/* the allocator about to read or write the whole payload */
#define hg_san_open(hb, cs, cap)                \
	do {                                        \
		if(HG_SAN_PRIVATE(hb))                  \
			HG_SAN_OPEN(HG_SAN_PAY(cs), (cap)); \
	} while(0)

/* in quarantine: closed entirely */
#define hg_san_close(hb, cs, cap)                \
	do {                                         \
		if(HG_SAN_PRIVATE(hb))                   \
			HG_SAN_CLOSE(HG_SAN_PAY(cs), (cap)); \
	} while(0)

/* @len bytes at @p leaving the cell tiers, back to the buddy */
#define hg_san_release(hb, p, len)   \
	do {                             \
		if(HG_SAN_PRIVATE(hb))       \
			HG_SAN_OPEN((p), (len)); \
	} while(0)

#endif /* hg_san_h */
