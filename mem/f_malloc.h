/*
 * Copyright (C) 2001-2003 FhG Fokus
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


/**
 * Use a bitmap to quickly find free fragments, should speed up
 * especially startup (non-warmed-up malloc) 
 */
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



#define F_MALLOC_OPTIMIZE_FACTOR 14UL /* used below */
/** Size to optimize for, (most allocs <= this size), must be 2^k */
#define F_MALLOC_OPTIMIZE  (1UL<<F_MALLOC_OPTIMIZE_FACTOR)


#define F_HASH_SIZE (F_MALLOC_OPTIMIZE/ROUNDTO + \
		(sizeof(long)*8-F_MALLOC_OPTIMIZE_FACTOR)+1)

#ifdef F_MALLOC_HASH_BITMAP
typedef unsigned long fm_hash_bitmap_t;
#define FM_HASH_BMP_BITS  (sizeof(fm_hash_bitmap_t)*8)
#define FM_HASH_BMP_SIZE  \
	((F_HASH_SIZE+FM_HASH_BMP_BITS-1)/FM_HASH_BMP_BITS)
#endif

/**
 * \name Hash structure
 * - 0 .... F_MALLOC_OPTIMIZE/ROUNDTO  - small buckets, size increases with
 * ROUNDTO from bucket to bucket
 * - +1 .... end -  size = 2^k, big buckets
 */
struct fm_frag{
	unsigned long size;
	union{
		struct fm_frag* nxt_free;
		long reserved;
	}u;
	struct fm_frag** prv_free;
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
	int type;
	unsigned long size; /** total size */
	unsigned long used; /** allocated size*/
	unsigned long real_used; /** used + malloc overhead */
	unsigned long max_real_used;
	unsigned long ffrags;
	
	struct fm_frag* first_frag;
	struct fm_frag* last_frag;
#ifdef F_MALLOC_HASH_BITMAP
	fm_hash_bitmap_t free_bitmap[FM_HASH_BMP_SIZE];
#endif
	struct fm_frag_lnk free_hash[F_HASH_SIZE];
};


/**
 * \brief Initialize memory manager malloc
 * \param address start address for memory block
 * \param size Size of allocation
 * \return return the fm_block
 */
struct fm_block* fm_malloc_init(char* address, unsigned long size, int type);


/**
 * \brief Main memory manager allocation function
 * \param qm memory block
 * \param size memory allocation size
 * \return address of allocated memory
 */
#ifdef DBG_F_MALLOC
void* fm_malloc(struct fm_block* qm, unsigned long size,
					const char* file, const char* func, unsigned int line);
#else
void* fm_malloc(struct fm_block* qm, unsigned long size);
#endif


/**
 * \brief Main memory manager free function
 * 
 * Main memory manager free function, provide functionality necessary for pkg_free
 * \param qm memory block
 * \param p freed memory
 */
#ifdef DBG_F_MALLOC
void  fm_free(struct fm_block* qm, void* p, const char* file, const char* func, 
				unsigned int line);
#else
void  fm_free(struct fm_block* qm, void* p);
#endif


/**
 * \brief Main memory manager realloc function
 * 
 * Main memory manager realloc function, provide functionality for pkg_realloc
 * \param qm memory block
 * \param p reallocated memory block
 * \param size
 * \return reallocated memory block
 */
#ifdef DBG_F_MALLOC
void*  fm_realloc(struct fm_block* qm, void* p, unsigned long size, 
					const char* file, const char* func, unsigned int line);
#else
void*  fm_realloc(struct fm_block* qm, void* p, unsigned long size);
#endif


/**
 * \brief Report internal memory manager status
 * \param qm memory block
 */
void fm_status(struct fm_block* qm);


/**
 * \brief Fills a malloc info structure with info about the block
 *
 * Fills a malloc info structure with info about the block, if a
 * parameter is not supported, it will be filled with 0
 * \param qm memory block
 * \param info memory information
 */
void fm_info(struct fm_block* qm, struct mem_info* info);


/**
 * \brief Helper function for available memory report
 * \param qm memory block
 * \return Returns how much free memory is available, on error (not compiled
 * with bookkeeping code) returns (unsigned long)(-1)
 */
unsigned long fm_available(struct fm_block* qm);


/**
 * \brief Debugging helper, summary and logs all allocated memory blocks
 * \param qm memory block
 */
#ifdef DBG_F_MALLOC
void fm_sums(struct fm_block* qm);
#else
#define fm_sums(qm) do{}while(0)
#endif /* DBG_F_MALLOC */

#endif
