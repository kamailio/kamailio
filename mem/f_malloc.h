/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of sip-router, a free SIP server.
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

/*
 * History:
 * --------
 *  2003-05-21  on sparc64 roundto 8 even in debugging mode (so malloc'ed
 *               long longs will be 64 bit aligned) (andrei)
 *  2004-07-19  support for 64 bit (2^64 mem. block) and more info
 *               for the future de-fragmentation support (andrei)
 *  2004-11-10  support for > 4Gb mem., switched to long (andrei)
 *  2007-06-23  added hash bitmap (andrei)
 */

/**
 * \file
 * \brief Simple, very fast, malloc library
 * \ingroup mem
 */


#if !defined(f_malloc_h)
#define f_malloc_h

#ifdef DBG_QM_MALLOC
#ifndef DBG_F_MALLOC
	#define DBG_F_MALLOC
#endif /* DBG_F_MALLOC */
#endif /* DBG_QM_MALLOC */

#include "meminfo.h"

/* defs*/

/* use a bitmap to quickly find free fragments, should speed up
 * especially startup (non-warmed-up malloc) */
#define F_MALLOC_HASH_BITMAP

#ifdef DBG_F_MALLOC
#if defined(__CPU_sparc64) || defined(__CPU_sparc)
/* tricky, on sun in 32 bits mode long long must be 64 bits aligned
 * but long can be 32 bits aligned => malloc should return long long
 * aligned memory */
	#define ROUNDTO		sizeof(long long)
#else
	#define ROUNDTO		sizeof(void*) /* size we round to, must be = 2^n, and
                      sizeof(fm_frag) must be multiple of ROUNDTO !*/
#endif
#else /* DBG_F_MALLOC */
	#define ROUNDTO 8UL
#endif
#define MIN_FRAG_SIZE	ROUNDTO



#define F_MALLOC_OPTIMIZE_FACTOR 14UL /*used below */
#define F_MALLOC_OPTIMIZE  (1UL<<F_MALLOC_OPTIMIZE_FACTOR)
								/* size to optimize for,
									(most allocs <= this size),
									must be 2^k */

#define F_HASH_SIZE (F_MALLOC_OPTIMIZE/ROUNDTO + \
		(sizeof(long)*8-F_MALLOC_OPTIMIZE_FACTOR)+1)

#ifdef F_MALLOC_HASH_BITMAP
typedef unsigned long fm_hash_bitmap_t;
#define FM_HASH_BMP_BITS  (sizeof(fm_hash_bitmap_t)*8)
#define FM_HASH_BMP_SIZE  \
	((F_HASH_SIZE+FM_HASH_BMP_BITS-1)/FM_HASH_BMP_BITS)
#endif

/* hash structure:
 * 0 .... F_MALLOC_OPTIMIZE/ROUNDTO  - small buckets, size increases with
 *                            ROUNDTO from bucket to bucket
 * +1 .... end -  size = 2^k, big buckets */

struct fm_frag{
	unsigned long size;
	union{
		struct fm_frag* nxt_free;
		long reserved;
	}u;
#ifdef DBG_F_MALLOC
	const char* file;
	const char* func;
	unsigned long line;
	unsigned long check;
#endif
};

struct fm_frag_lnk{
	struct fm_frag* first;
	unsigned long no;
};

/**
 * \brief Block of memory for F_MALLOC memory manager
 * \see mem_info
 */
struct fm_block{
	unsigned long size; /* total size */
#if defined(DBG_F_MALLOC) || defined(MALLOC_STATS)
	unsigned long used; /* alloc'ed size*/
	unsigned long real_used; /* used+malloc overhead*/
	unsigned long max_real_used;
#endif
	
	struct fm_frag* first_frag;
	struct fm_frag* last_frag;
#ifdef F_MALLOC_HASH_BITMAP
	fm_hash_bitmap_t free_bitmap[FM_HASH_BMP_SIZE];
#endif
	struct fm_frag_lnk free_hash[F_HASH_SIZE];
};



struct fm_block* fm_malloc_init(char* address, unsigned long size);

#ifdef DBG_F_MALLOC
void* fm_malloc(struct fm_block*, unsigned long size,
					const char* file, const char* func, unsigned int line);
#else
void* fm_malloc(struct fm_block*, unsigned long size);
#endif

#ifdef DBG_F_MALLOC
void  fm_free(struct fm_block*, void* p, const char* file, const char* func, 
				unsigned int line);
#else
void  fm_free(struct fm_block*, void* p);
#endif

#ifdef DBG_F_MALLOC
void*  fm_realloc(struct fm_block*, void* p, unsigned long size, 
					const char* file, const char* func, unsigned int line);
#else
void*  fm_realloc(struct fm_block*, void* p, unsigned long size);
#endif

void  fm_status(struct fm_block*);
void  fm_info(struct fm_block*, struct mem_info*);

unsigned long fm_available(struct fm_block*);

#ifdef DBG_F_MALLOC
void fm_sums(struct fm_block*);
#else
#define fm_sums(v) do{}while(0)
#endif /* DBG_F_MALLOC */

#endif
