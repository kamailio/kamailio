/*
 * shared memory, multi-process safe, pool based version of f_malloc
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Copyright (C) 2007 iptelorg GmbH
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


#if !defined(sf_malloc_h)  
#define sf_malloc_h


#include "meminfo.h"

#include "../lock_ops.h"
#include "../atomic_ops.h"
#include "../compiler_opt.h"
/* defs*/


#ifdef GEN_LOCK_T_UNLIMITED
#define SFM_LOCK_PER_BUCKET
#else
#define SFM_ONE_LOCK
#endif

#ifdef DBG_SF_MALLOC
#if defined(__CPU_sparc64) || defined(__CPU_sparc)
/* tricky, on sun in 32 bits mode long long must be 64 bits aligned
 * but long can be 32 bits aligned => malloc should return long long
 * aligned memory */
	#define SF_ROUNDTO	sizeof(long long)
#else
	#define SF_ROUNDTO	sizeof(void*) /* size we round to, must be = 2^n, and
                      sizeof(sfm_frag) must be multiple of SF_ROUNDTO !*/
#endif
#else /* DBG_SF_MALLOC */
	#define SF_ROUNDTO 8UL
#endif
#define SF_MIN_FRAG_SIZE	SF_ROUNDTO

#define SFM_POOLS_NO 4U /* the more the better, but higher initial
                            mem. consumption */

#define SF_MALLOC_OPTIMIZE_FACTOR 14UL /*used below */
#define SF_MALLOC_OPTIMIZE  (1UL<<SF_MALLOC_OPTIMIZE_FACTOR)
								/* size to optimize for,
									(most allocs <= this size),
									must be 2^k */

#define SF_HASH_POOL_SIZE	(SF_MALLOC_OPTIMIZE/SF_ROUNDTO + 1)
#define SF_POOL_MAX_SIZE	SF_MALLOC_OPTIMIZE

#define SF_HASH_SIZE (SF_MALLOC_OPTIMIZE/SF_ROUNDTO + \
		(sizeof(long)*8-SF_MALLOC_OPTIMIZE_FACTOR)+1)

/* hash structure:
 * 0 .... SF_MALLOC_OPTIMIZE/SF_ROUNDTO  - small buckets, size increases with
 *                            SF_ROUNDTO from bucket to bucket
 * +1 .... end -  size = 2^k, big buckets */

struct sfm_frag{
	union{
		struct sfm_frag* nxt_free;
		long reserved;
	}u;
	unsigned long size;
	unsigned long id; /* TODO better optimize the size */
	/* pad to SF_ROUNDTO multiple */
	char _pad[((3*sizeof(long)+SF_ROUNDTO-1)&~(SF_ROUNDTO-1))-3*sizeof(long)];
#ifdef DBG_SF_MALLOC
	const char* file;
	const char* func;
	unsigned long line;
	unsigned long check;
#endif
};

struct sfm_frag_lnk{
	struct sfm_frag* first;
#ifdef SFM_LOCK_PER_BUCKET
	gen_lock_t lock;
#endif
	unsigned long no;
};

struct sfm_pool_head{
	struct sfm_frag* first;
#ifdef SFM_LOCK_PER_BUCKET
	gen_lock_t lock;
#endif
	unsigned long no;
	unsigned long misses;
};

struct sfm_pool{
#ifdef SFM_ONE_LOCK
	gen_lock_t lock;
#endif
	unsigned long missed;
	unsigned long hits; /* debugging only TODO: remove */
	unsigned long bitmap;
	struct sfm_pool_head pool_hash[SF_HASH_POOL_SIZE];
};

struct sfm_block{
#ifdef SFM_ONE_LOCK
	gen_lock_t lock;
#endif
	atomic_t crt_id; /* current pool */
	int type;
	unsigned long size; /* total size */
	/* stats are kept now per bucket */
	struct sfm_frag* first_frag;
	struct sfm_frag* last_frag;
	unsigned long bitmap; /* only up to SF_MALLOC_OPTIMIZE */
	struct sfm_frag_lnk free_hash[SF_HASH_SIZE];
	struct sfm_pool pool[SFM_POOLS_NO];
	int is_init;
	gen_lock_t get_and_split;
	char _pad[256];
};



struct sfm_block* sfm_malloc_init(char* address, unsigned long size, int type);
void sfm_malloc_destroy(struct sfm_block* qm);
int sfm_pool_reset();

#ifdef DBG_SF_MALLOC
void* sfm_malloc(struct sfm_block*, unsigned long size,
					const char* file, const char* func, unsigned int line);
#else
void* sfm_malloc(struct sfm_block*, unsigned long size);
#endif

#ifdef DBG_SF_MALLOC
void  sfm_free(struct sfm_block*, void* p, const char* file, const char* func, 
				unsigned int line);
#else
void  sfm_free(struct sfm_block*, void* p);
#endif

#ifdef DBG_SF_MALLOC
void*  sfm_realloc(struct sfm_block*, void* p, unsigned long size, 
					const char* file, const char* func, unsigned int line);
#else
void*  sfm_realloc(struct sfm_block*, void* p, unsigned long size);
#endif

void  sfm_status(struct sfm_block*);
void  sfm_info(struct sfm_block*, struct mem_info*);

unsigned long sfm_available(struct sfm_block*);

#endif
