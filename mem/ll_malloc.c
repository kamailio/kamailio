/*
 * shared memory, multi-process safe, pool based, mostly lockless version of 
 *  f_malloc
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

#ifdef LL_MALLOC

#include <string.h>
#include <stdlib.h>

#include "ll_malloc.h"
#include "../dprint.h"
#include "../globals.h"
#include "memdbg.h"
#include "../cfg/cfg.h" /* memlog */

#define MAX_POOL_FRAGS 10000 /* max fragments per pool hash bucket */
#define MIN_POOL_FRAGS 10    /* min fragments per pool hash bucket */

/*useful macros*/

#define FRAG_NEXT(f) \
	((struct sfm_frag*)((char*)(f)+sizeof(struct sfm_frag)+(f)->size ))


/* SF_ROUNDTO= 2^k so the following works */
#define ROUNDTO_MASK	(~((unsigned long)SF_ROUNDTO-1))
#define ROUNDUP(s)		(((s)+(SF_ROUNDTO-1))&ROUNDTO_MASK)
#define ROUNDDOWN(s)	((s)&ROUNDTO_MASK)

#define FRAG_OVERHEAD	(sizeof(struct sfm_frag))
#define INIT_OVERHEAD	\
	(ROUNDUP(sizeof(struct sfm_block))+sizeof(struct sfm_frag))



/* finds hash if s <=SF_MALLOC_OPTIMIZE */
#define GET_SMALL_HASH(s) (unsigned long)(s)/SF_ROUNDTO
/* finds hash if s > SF_MALLOC_OPTIMIZE */
#define GET_BIG_HASH(s) \
	(SF_MALLOC_OPTIMIZE/SF_ROUNDTO+big_hash_idx((s))-SF_MALLOC_OPTIMIZE_FACTOR+1)

/* finds the hash value for s, s=SF_ROUNDTO multiple*/
#define GET_HASH(s)   ( ((unsigned long)(s)<=SF_MALLOC_OPTIMIZE)?\
							GET_SMALL_HASH(s): GET_BIG_HASH(s) )


#define UN_HASH_SMALL(h) ((unsigned long)(h)*SF_ROUNDTO)
#define UN_HASH_BIG(h) (1UL<<((unsigned long)(h)-SF_MALLOC_OPTIMIZE/SF_ROUNDTO+\
							SF_MALLOC_OPTIMIZE_FACTOR-1))

#define UN_HASH(h)	( ((unsigned long)(h)<=(SF_MALLOC_OPTIMIZE/SF_ROUNDTO))?\
						UN_HASH_SMALL(h): UN_HASH_BIG(h) )

#define BITMAP_BITS (sizeof(((struct sfm_block*)0)->bitmap)*8)
#define BITMAP_BLOCK_SIZE ((SF_MALLOC_OPTIMIZE/SF_ROUNDTO)/ BITMAP_BITS)
/* only for "small" hashes (up to HASH(SF_MALLOC_OPTIMIZE) */
#define HASH_BIT_POS(h) (((unsigned long)(h))/BITMAP_BLOCK_SIZE)
#define HASH_TO_BITMAP(h) (1UL<<HASH_BIT_POS(h))
#define BIT_TO_HASH(b) ((b)*BITMAP_BLOCK_SIZE)



/* mark/test used/unused frags */
#define FRAG_MARK_USED(f)
#define FRAG_CLEAR_USED(f)
#define FRAG_WAS_USED(f)   (1)

/* other frag related defines:
 * MEM_COALESCE_FRAGS 
 * MEM_FRAG_AVOIDANCE
 */
#define MEM_FRAG_AVOIDANCE


#define SFM_REALLOC_REMALLOC

/* computes hash number for big buckets*/
inline static unsigned long big_hash_idx(unsigned long s)
{
	unsigned long idx;
	/* s is rounded => s = k*2^n (SF_ROUNDTO=2^n) 
	 * index= i such that 2^i > s >= 2^(i-1)
	 *
	 * => index = number of the first non null bit in s*/
	idx=sizeof(long)*8-1;
	for (; !(s&(1UL<<(sizeof(long)*8-1))) ; s<<=1, idx--);
	return idx;
}


#ifdef DBG_F_MALLOC
#define ST_CHECK_PATTERN   0xf0f0f0f0
#define END_CHECK_PATTERN1 0xc0c0c0c0
#define END_CHECK_PATTERN2 0xabcdefed
#endif


#ifdef SFM_ONE_LOCK

#define SFM_MAIN_HASH_LOCK(qm, hash) lock_get(&(qm)->lock)
#define SFM_MAIN_HASH_UNLOCK(qm, hash) lock_release(&(qm)->lock)
#define SFM_POOL_LOCK(p, hash) lock_get(&(p)->lock)
#define SFM_POOL_UNLOCK(p, hash) lock_release(&(p)->lock)

#warn "degraded performance, only one lock"

#elif defined SFM_LOCK_PER_BUCKET

#define SFM_MAIN_HASH_LOCK(qm, hash) \
	lock_get(&(qm)->free_hash[(hash)].lock)
#define SFM_MAIN_HASH_UNLOCK(qm, hash)  \
	lock_release(&(qm)->free_hash[(hash)].lock)
#define SFM_POOL_LOCK(p, hash) lock_get(&(p)->pool_hash[(hash)].lock)
#define SFM_POOL_UNLOCK(p, hash) lock_release(&(p)->pool_hash[(hash)].lock)
#else
#error no locks defined
#endif /* SFM_ONE_LOCK/SFM_LOCK_PER_BUCKET */

#define SFM_BIG_GET_AND_SPLIT_LOCK(qm)   lock_get(&(qm)->get_and_split)
#define SFM_BIG_GET_AND_SPLIT_UNLOCK(qm) lock_release(&(qm)->get_and_split)

static unsigned long sfm_max_hash=0; /* maximum hash value (no point in
										 searching further) */
static unsigned long pool_id=(unsigned long)-1;


/* call for each child */
int sfm_pool_reset()
{
	pool_id=(unsigned long)-1;
	return 0;
}


#define sfm_fix_pool_id(qm) \
	do{ \
		if (unlikely(pool_id>=SFM_POOLS_NO)) \
			pool_id=((unsigned)atomic_add(&(qm)->crt_id, 1))%SFM_POOLS_NO; \
	}while(0)



static inline void frag_push(struct sfm_frag** head, struct sfm_frag* frag)
{
	register struct sfm_frag* old;
	register struct sfm_frag* crt;
	
	crt=(void*)atomic_get_long(head);
	do{
		frag->u.nxt_free=crt;
		old=crt;
		membar_write_atomic_op();
		crt=(void*)atomic_cmpxchg_long((void*)head, (long)old, (long)frag);
	}while(crt!=old);
}


static inline struct sfm_frag* frag_pop(struct sfm_frag** head)
{
	register struct sfm_frag* old;
	register struct sfm_frag* crt;
	register struct sfm_frag* nxt;
	
	crt=(void*)atomic_get_long(head);
	do{
		/* if circular list, test not needed */
		nxt=crt?crt->u.nxt_free:0;
		old=crt;
		membar_read_atomic_op();
		crt=(void*)atomic_cmpxchg_long((void*)head, (long)old, (long)nxt);
	}while(crt!=old);
	return crt;
}


static inline void sfm_pool_insert (struct sfm_pool* pool, int hash,
								struct sfm_frag* frag)
{
	unsigned long hash_bit;

	frag_push(&pool->pool_hash[hash].first, frag);
	atomic_inc_long((long*)&pool->pool_hash[hash].no);
	/* set it only if not already set (avoids an expensive
	 * cache trashing atomic write op) */
	hash_bit=HASH_TO_BITMAP(hash);
	if  (!(atomic_get_long((long*)&pool->bitmap) & hash_bit))
		atomic_or_long((long*)&pool->bitmap, hash_bit);
}



/* returns 1 if it's ok to add a fragm. to pool p_id @ hash, 0 otherwise */
static inline int sfm_check_pool(struct sfm_block* qm, unsigned long p_id,
									int hash, int split)
{
	/* TODO: come up with something better
	 * if fragment is some  split/rest from an allocation, that is
	 *  >= requested size, accept it, else
	 *  look at misses and current fragments and decide based on them */
	return (p_id<SFM_POOLS_NO) && (split ||
			( (qm->pool[p_id].pool_hash[hash].no < MIN_POOL_FRAGS) ||
			  ((qm->pool[p_id].pool_hash[hash].misses > 
				 qm->pool[p_id].pool_hash[hash].no) &&
				(qm->pool[p_id].pool_hash[hash].no<MAX_POOL_FRAGS) ) ) );
}


/* choose on which pool to add a free'd packet
 * return - pool idx or -1 if it should be added to main*/
static inline unsigned long  sfm_choose_pool(struct sfm_block* qm,
												struct sfm_frag* frag,
												int hash, int split)
{
	/* check original pool first */
	if (sfm_check_pool(qm, frag->id, hash, split))
		return frag->id;
	else{
		/* check if our pool is properly set */
		sfm_fix_pool_id(qm);
		/* check if my pool needs some frags */
		if ((pool_id!=frag->id) && (sfm_check_pool(qm,  pool_id, hash, 0))){
			frag->id=pool_id;
			return pool_id;
		}
	}
	/* else add it back to main */
	frag->id=(unsigned long)(-1);
	return frag->id;
}


static inline void sfm_insert_free(struct sfm_block* qm, struct sfm_frag* frag,
									int split)
{
	struct sfm_frag** f;
	unsigned long p_id;
	int hash;
	unsigned long hash_bit;
	
	if (likely(frag->size<=SF_POOL_MAX_SIZE)){
		hash=GET_SMALL_HASH(frag->size);
		if (unlikely((p_id=sfm_choose_pool(qm, frag, hash, split))==
					(unsigned long)-1)){
			/* add it back to the "main" hash */
				frag->id=(unsigned long)(-1); /* main hash marker */
				/*insert it here*/
				frag_push(&(qm->free_hash[hash].first), frag);
				atomic_inc_long((long*)&qm->free_hash[hash].no);
				/* set it only if not already set (avoids an expensive
		 		* cache trashing atomic write op) */
				hash_bit=HASH_TO_BITMAP(hash);
				if  (!(atomic_get_long((long*)&qm->bitmap) & hash_bit))
					atomic_or_long((long*)&qm->bitmap, hash_bit);
		}else{
			/* add it to one of the pools pool */
			sfm_pool_insert(&qm->pool[p_id], hash, frag);
		}
	}else{
		hash=GET_BIG_HASH(frag->size);
		SFM_MAIN_HASH_LOCK(qm, hash);
			f=&(qm->free_hash[hash].first);
			for(; *f; f=&((*f)->u.nxt_free))
				if (frag->size <= (*f)->size) break;
			frag->id=(unsigned long)(-1); /* main hash marker */
			/*insert it here*/
			frag->u.nxt_free=*f;
			*f=frag;
			qm->free_hash[hash].no++;
			/* inc. big hash free size ? */
		SFM_MAIN_HASH_UNLOCK(qm, hash);
	}
	
}



 /* size should be already rounded-up */
static inline
#ifdef DBG_F_MALLOC 
void sfm_split_frag(struct sfm_block* qm, struct sfm_frag* frag,
					unsigned long size,
					const char* file, const char* func, unsigned int line)
#else
void sfm_split_frag(struct sfm_block* qm, struct sfm_frag* frag,
					unsigned long size)
#endif
{
	unsigned long rest;
	struct sfm_frag* n;
	int bigger_rest;
	
	rest=frag->size-size;
#ifdef MEM_FRAG_AVOIDANCE
	if ((rest> (FRAG_OVERHEAD+SF_MALLOC_OPTIMIZE))||
		(rest>=(FRAG_OVERHEAD+size))){ /* the residue fragm. is big enough*/
		bigger_rest=1;
#else
	if (rest>(FRAG_OVERHEAD+SF_MIN_FRAG_SIZE)){
		bigger_rest=rest>=(size+FRAG_OVERHEAD);
#endif
		frag->size=size;
		/*split the fragment*/
		n=FRAG_NEXT(frag);
		n->size=rest-FRAG_OVERHEAD;
		n->id=pool_id;
		FRAG_CLEAR_USED(n); /* never used */
#ifdef DBG_F_MALLOC
		/* frag created by malloc, mark it*/
		n->file=file;
		n->func="frag. from sfm_malloc";
		n->line=line;
		n->check=ST_CHECK_PATTERN;
#endif
		/* reinsert n in free list*/
		sfm_insert_free(qm, n, bigger_rest);
	}else{
		/* we cannot split this fragment any more => alloc all of it*/
	}
}



/* init malloc and return a sfm_block*/
struct sfm_block* sfm_malloc_init(char* address, unsigned long size, int type)
{
	char* start;
	char* end;
	struct sfm_block* qm;
	unsigned long init_overhead;
	int r;
#ifdef SFM_LOCK_PER_BUCKET
	int i;
#endif
	
	/* make address and size multiple of 8*/
	start=(char*)ROUNDUP((unsigned long) address);
	DBG("sfm_malloc_init: SF_OPTIMIZE=%lu, /SF_ROUNDTO=%lu\n",
			SF_MALLOC_OPTIMIZE, SF_MALLOC_OPTIMIZE/SF_ROUNDTO);
	DBG("sfm_malloc_init: SF_HASH_SIZE=%lu, sfm_block size=%lu\n",
			SF_HASH_SIZE, (long)sizeof(struct sfm_block));
	DBG("sfm_malloc_init(%p, %lu), start=%p\n", address, size, start);

	if (size<start-address) return 0;
	size-=(start-address);
	if (size <(SF_MIN_FRAG_SIZE+FRAG_OVERHEAD)) return 0;
	size=ROUNDDOWN(size);

	init_overhead=INIT_OVERHEAD;
	
	
	if (size < init_overhead)
	{
		/* not enough mem to create our control structures !!!*/
		return 0;
	}
	end=start+size;
	qm=(struct sfm_block*)start;
	memset(qm, 0, sizeof(struct sfm_block));
	qm->size=size;
	qm->type = type;
	size-=init_overhead;
	
	qm->first_frag=(struct sfm_frag*)(start+ROUNDUP(sizeof(struct sfm_block)));
	qm->last_frag=(struct sfm_frag*)(end-sizeof(struct sfm_frag));
	/* init initial fragment*/
	qm->first_frag->size=size;
	qm->first_frag->id=(unsigned long)-1; /* not in a pool */
	qm->last_frag->size=0;
	
#ifdef DBG_F_MALLOC
	qm->first_frag->check=ST_CHECK_PATTERN;
	qm->last_frag->check=END_CHECK_PATTERN1;
#endif
	
	/* link initial fragment into the free list*/
	
	sfm_insert_free(qm, qm->first_frag, 0);
	sfm_max_hash=GET_HASH(size);
	
	/* init locks */
	if (lock_init(&qm->get_and_split)==0)
		goto error;
#ifdef SFM_ONE_LOCK
	if (lock_init(&qm->lock)==0){
		lock_destroy(&qm->get_and_split);
		goto error;
	}
	for (r=0; r<SFM_POOLS_NO; r++){
		if (lock_init(&qm->pool[r].lock)==0){
			for (;r>0; r--) lock_destroy(&qm->pool[r-1].lock);
			lock_destroy(&qm->lock);
			lock_destroy(&qm->get_and_split);
			goto error;
		}
	}
#elif defined(SFM_LOCK_PER_BUCKET)
	for (r=0; r<SF_HASH_SIZE; r++)
		if (lock_init(&qm->free_hash[r].lock)==0){
			for(;r>0; r--) lock_destroy(&qm->free_hash[r-1].lock);
			lock_destroy(&qm->get_and_split);
			goto error;
		}
	for (i=0; i<SFM_POOLS_NO; i++){
		for (r=0; r<SF_HASH_POOL_SIZE; r++)
			if (lock_init(&qm->pool[i].pool_hash[r].lock)==0){
				for(;r>0; r--) lock_destroy(&qm->pool[i].poo_hash[r].lock);
				for(; i>0; i--){
					for (r=0; r<SF_HASH_POOL_SIZE; r++)
						lock_destroy(&qm->pool[i].pool_hash[r].lock);
				}
				for (r=0; r<SF_HASH_SIZE; r++)
					lock_destroy(&qm->free_hash[r].lock);
				lock_destroy(&qm->get_and_split);
				goto error;
			}
	}
#endif
	qm->is_init=1;
	return qm;
error:
	return 0;
}



/* cleanup */
void sfm_malloc_destroy(struct sfm_block* qm)
{
	int r, i;
	/* destroy all the locks */
	if (!qm || !qm->is_init)
		return; /* nothing to do */
	lock_destroy(&qm->get_and_split);
#ifdef SFM_ONE_LOCK
	lock_destroy(&qm->lock);
	for (r=0; r<SFM_POOLS_NO; r++){
		lock_destroy(&qm->pool[r].lock);
	}
#elif defined(SFM_LOCK_PER_BUCKET)
	for (r=0; r<SF_HASH_SIZE; r++)
		lock_destroy(&qm->free_hash[r].lock);
	for (i=0; i<SFM_POOLS_NO; i++){
		for (r=0; r<SF_HASH_POOL_SIZE; r++)
			lock_destroy(&qm->pool[i].pool_hash[r].lock);
	}
#endif
	qm->is_init=0;

}


/* returns next set bit in bitmap, starts at b
 * if b is set, returns b
 * if not found returns BITMAP_BITS */
static inline unsigned long _next_set_bit(unsigned long b,
											unsigned long* bitmap)
{
	for (; !((1UL<<b)& *bitmap) && b<BITMAP_BITS; b++);
	return b;
}

/* returns start of block b and sets *end
 * (handles also the "rest" block at the end ) */
static inline unsigned long _hash_range(unsigned long b, unsigned long* end)
{
	unsigned long s;
	
	if ((unlikely(b>=BITMAP_BITS))){
		s=BIT_TO_HASH(BITMAP_BITS);
		*end=SF_HASH_POOL_SIZE; /* last, possible rest block */
	}else{
		s=BIT_TO_HASH(b);
		*end=s+BITMAP_BLOCK_SIZE;
	}
	return s;
}


#ifdef DBG_F_MALLOC
static inline struct sfm_frag* pool_get_frag(struct sfm_block* qm,
						struct sfm_pool*  pool, int hash, unisgned long size,
						const char* file, const char* func, unsigned int line)
#else
static inline struct sfm_frag* pool_get_frag(struct sfm_block* qm,
											struct sfm_pool*  pool,
											int hash, unsigned long size)
#endif
{
	int r;
	int next_block;
	struct sfm_frag* volatile* f;
	struct sfm_frag* frag;
	unsigned long b;
	unsigned long eob;

	/* special case for r=hash */
	r=hash;
	f=&pool->pool_hash[r].first;

	/* detach it from the free list */
	if ((frag=frag_pop((struct sfm_frag**)f))==0)
		goto not_found;
found:
	atomic_dec_long((long*)&pool->pool_hash[r].no);
	frag->u.nxt_free=0; /* mark it as 'taken' */
	frag->id=pool_id;
#ifdef DBG_F_MALLOC
	sfm_split_frag(qm, frag, size, file, func, line);
#else
	sfm_split_frag(qm, frag, size);
#endif
	if (&qm->pool[pool_id]==pool)
		atomic_inc_long((long*)&pool->hits);
	return frag;
	
not_found:
	atomic_inc_long((long*)&pool->pool_hash[r].misses);
	r++;
	b=HASH_BIT_POS(r);
	
	while(r<SF_HASH_POOL_SIZE){
		b=_next_set_bit(b, &pool->bitmap);
		next_block=_hash_range(b, &eob);
		r=(r<next_block)?next_block:r;
		for (; r<eob; r++){
			f=&pool->pool_hash[r].first;
			if ((frag=frag_pop((struct sfm_frag**)f))!=0)
				goto found;
			atomic_inc_long((long*)&pool->pool_hash[r].misses);
		}
		b++;
	}
	atomic_inc_long((long*)&pool->missed);
	return 0;
}



#ifdef DBG_F_MALLOC
static inline struct sfm_frag* main_get_frag(struct sfm_block* qm, int hash,
						unsigned long size,
						const char* file, const char* func, unsigned int line)
#else
static inline struct sfm_frag* main_get_frag(struct sfm_block* qm, int hash,
												unsigned long size)
#endif
{
	int r;
	int next_block;
	struct sfm_frag* volatile* f;
	struct sfm_frag* frag;
	unsigned long b;
	unsigned long eob;

	r=hash;
	b=HASH_BIT_POS(r);
	while(r<=SF_MALLOC_OPTIMIZE/SF_ROUNDTO){
			b=_next_set_bit(b, &qm->bitmap);
			next_block=_hash_range(b, &eob);
			r=(r<next_block)?next_block:r;
			for (; r<eob; r++){
				f=&qm->free_hash[r].first;
				if ((frag=frag_pop((struct sfm_frag**)f))!=0){
					atomic_dec_long((long*)&qm->free_hash[r].no);
					frag->u.nxt_free=0; /* mark it as 'taken' */
					frag->id=pool_id;
#ifdef DBG_F_MALLOC
					sfm_split_frag(qm, frag, size, file, func, line);
#else
					sfm_split_frag(qm, frag, size);
#endif
					return frag;
				}
			}
			b++;
	}
	/* big fragments */
	SFM_BIG_GET_AND_SPLIT_LOCK(qm);
	for (; r<= sfm_max_hash ; r++){
		f=&qm->free_hash[r].first;
		if (*f){
			SFM_MAIN_HASH_LOCK(qm, r);
			if (unlikely((*f)==0)){
				/* not found */
				SFM_MAIN_HASH_UNLOCK(qm, r);
				continue; 
			}
			for(;(*f); f=&((*f)->u.nxt_free))
				if ((*f)->size>=size){
					/* found, detach it from the free list*/
					frag=*f;
					*f=frag->u.nxt_free;
					frag->u.nxt_free=0; /* mark it as 'taken' */
					qm->free_hash[r].no--;
					SFM_MAIN_HASH_UNLOCK(qm, r);
					frag->id=pool_id;
#ifdef DBG_F_MALLOC
					sfm_split_frag(qm, frag, size, file, func, line);
#else
					sfm_split_frag(qm, frag, size);
#endif
					SFM_BIG_GET_AND_SPLIT_UNLOCK(qm);
					return frag;
				};
			SFM_MAIN_HASH_UNLOCK(qm, r);
			/* try in a bigger bucket */
		}
	}
	SFM_BIG_GET_AND_SPLIT_UNLOCK(qm);
	return 0;
}



#ifdef DBG_F_MALLOC
void* sfm_malloc(struct sfm_block* qm, unsigned long size,
					const char* file, const char* func, unsigned int line)
#else
void* sfm_malloc(struct sfm_block* qm, unsigned long size)
#endif
{
	struct sfm_frag* frag;
	int hash;
	unsigned int i;
	
#ifdef DBG_F_MALLOC
	MDBG("sfm_malloc(%p, %lu) called from %s: %s(%d)\n", qm, size, file, func,
			line);
#endif
	/*size must be a multiple of 8*/
	size=ROUNDUP(size);
/*	if (size>(qm->size-qm->real_used)) return 0; */

	/* check if our pool id is set */
	sfm_fix_pool_id(qm);
	
	/*search for a suitable free frag*/
	if (likely(size<=SF_POOL_MAX_SIZE)){
		hash=GET_SMALL_HASH(size);
		/* try first in our pool */
#ifdef DBG_F_MALLOC
		if (likely((frag=pool_get_frag(qm, &qm->pool[pool_id], hash, size,
										file, func, line))!=0))
			goto found;
		/* try in the "main" free hash, go through all the hash */
		if (likely((frag=main_get_frag(qm, hash, size, file, func, line))!=0))
			goto found;
		/* really low mem , try in other pools */
		for (i=(pool_id+1); i< (pool_id+SFM_POOLS_NO); i++){
			if ((frag=pool_get_frag(qm, &qm->pool[i%SFM_POOLS_NO], hash, size,
										file, func, line))!=0)
				goto found;
		}
#else
		if (likely((frag=pool_get_frag(qm, &qm->pool[pool_id], hash, size))
					!=0 ))
			goto found;
		/* try in the "main" free hash, go through all the hash */
		if (likely((frag=main_get_frag(qm, hash, size))!=0))
			goto found;
		/* really low mem , try in other pools */
		for (i=(pool_id+1); i< (pool_id+SFM_POOLS_NO); i++){
			if ((frag=pool_get_frag(qm, &qm->pool[i%SFM_POOLS_NO], hash, size))
					!=0 )
				goto found;
		}
#endif
		/* not found, bad! */
		return 0;
	}else{
		hash=GET_BIG_HASH(size);
#ifdef DBG_F_MALLOC
		if ((frag=main_get_frag(qm, hash, size, file, func, line))==0)
			return 0; /* not found, bad! */
#else
		if ((frag=main_get_frag(qm, hash, size))==0)
			return 0; /* not found, bad! */
#endif
	}

found:
	/* we found it!*/
#ifdef DBG_F_MALLOC
	frag->file=file;
	frag->func=func;
	frag->line=line;
	frag->check=ST_CHECK_PATTERN;
	MDBG("sfm_malloc(%p, %lu) returns address %p \n", qm, size,
		(char*)frag+sizeof(struct sfm_frag));
#endif
	FRAG_MARK_USED(frag); /* mark it as used */
	return (char*)frag+sizeof(struct sfm_frag);
}



#ifdef DBG_F_MALLOC
void sfm_free(struct sfm_block* qm, void* p, const char* file,
				const char* func, unsigned int line)
#else
void sfm_free(struct sfm_block* qm, void* p)
#endif
{
	struct sfm_frag* f;

#ifdef DBG_F_MALLOC
	MDBG("sfm_free(%p, %p), called from %s: %s(%d)\n", qm, p, file, func,
				line);
	if (p>(void*)qm->last_frag || p<(void*)qm->first_frag){
		LOG(L_CRIT, "BUG: sfm_free: bad pointer %p (out of memory block!) - "
				"aborting\n", p);
		abort();
	}
#endif
	if (unlikely(p==0)) {
		LOG(L_WARN, "WARNING: sfm_free: free(0) called\n");
		return;
	}
	f=(struct sfm_frag*) ((char*)p-sizeof(struct sfm_frag));
#ifdef DBG_F_MALLOC
	MDBG("sfm_free: freeing block alloc'ed from %s: %s(%ld)\n",
			f->file, f->func, f->line);
#endif
#ifdef DBG_F_MALLOC
	f->file=file;
	f->func=func;
	f->line=line;
#endif
	sfm_insert_free(qm, f, 0);
}


#ifdef DBG_F_MALLOC
void* sfm_realloc(struct sfm_block* qm, void* p, unsigned long size,
					const char* file, const char* func, unsigned int line)
#else
void* sfm_realloc(struct sfm_block* qm, void* p, unsigned long size)
#endif
{
	struct sfm_frag *f;
	unsigned long orig_size;
	void *ptr;
#ifndef SFM_REALLOC_REMALLOC
	struct sfm_frag *n;
	struct sfm_frag **pf;
	unsigned long diff;
	unsigned long p_id;
	int hash;
	unsigned long n_size;
	struct sfm_pool * pool;
#endif
	
#ifdef DBG_F_MALLOC
	MDBG("sfm_realloc(%p, %p, %lu) called from %s: %s(%d)\n", qm, p, size,
			file, func, line);
	if ((p)&&(p>(void*)qm->last_frag || p<(void*)qm->first_frag)){
		LOG(L_CRIT, "BUG: sfm_free: bad pointer %p (out of memory block!) - "
				"aborting\n", p);
		abort();
	}
#endif
	if (size==0) {
		if (p)
#ifdef DBG_F_MALLOC
			sfm_free(qm, p, file, func, line);
#else
			sfm_free(qm, p);
#endif
		return 0;
	}
	if (p==0)
#ifdef DBG_F_MALLOC
		return sfm_malloc(qm, size, file, func, line);
#else
		return sfm_malloc(qm, size);
#endif
	f=(struct sfm_frag*) ((char*)p-sizeof(struct sfm_frag));
#ifdef DBG_F_MALLOC
	MDBG("sfm_realloc: realloc'ing frag %p alloc'ed from %s: %s(%ld)\n",
			f, f->file, f->func, f->line);
#endif
	size=ROUNDUP(size);
	orig_size=f->size;
	if (f->size > size){
		/* shrink */
#ifdef DBG_F_MALLOC
		MDBG("sfm_realloc: shrinking from %lu to %lu\n", f->size, size);
		sfm_split_frag(qm, f, size, file, "frag. from sfm_realloc", line);
#else
		sfm_split_frag(qm, f, size);
#endif
	}else if (f->size<size){
		/* grow */
#ifdef DBG_F_MALLOC
		MDBG("sfm_realloc: growing from %lu to %lu\n", f->size, size);
#endif
#ifndef SFM_REALLOC_REMALLOC
/* should set a magic value in list head and in push/pop if magic value =>
 * lock and wait */
#error LL_MALLOC realloc not finished yet
		diff=size-f->size;
		n=FRAG_NEXT(f);
		if (((char*)n < (char*)qm->last_frag) && 
				(n->u.nxt_free)&&((n->size+FRAG_OVERHEAD)>=diff)){
			/* join  */
			/* detach n from the free list */
try_again:
			p_id=n->id;
			n_size=n->size;
			if ((unlikely(p_id >=SFM_POOLS_NO))){
				hash=GET_HASH(n_size);
				SFM_MAIN_HASH_LOCK(qm, hash);
				if (unlikely((n->u.nxt_free==0) ||
							((n->size+FRAG_OVERHEAD)<diff))){ 
					SFM_MAIN_HASH_UNLOCK(qm, hash);
					goto not_found;
				}
				if (unlikely((n->id!=p_id) || (n->size!=n_size))){
					/* fragment still free, but changed, either 
					 * moved to another pool or has a diff. size */
					SFM_MAIN_HASH_UNLOCK(qm, hash);
					goto try_again;
				}
				pf=&(qm->free_hash[hash].first);
				/* find it */
				for(;(*pf)&&(*pf!=n); pf=&((*pf)->u.nxt_free));/*FIXME slow */
				if (*pf==0){
					SFM_MAIN_HASH_UNLOCK(qm, hash);
					/* not found, bad! */
					LOG(L_WARN, "WARNING: sfm_realloc: could not find %p in "
							    "free " "list (hash=%d)\n", n, hash);
					/* somebody is in the process of changing it ? */
					goto not_found;
				}
				/* detach */
				*pf=n->u.nxt_free;
				n->u.nxt_free=0; /* mark it immediately as detached */
				qm->free_hash[hash].no--;
				SFM_MAIN_HASH_UNLOCK(qm, hash);
				/* join */
				f->size+=n->size+FRAG_OVERHEAD;
				/* split it if necessary */
				if (f->size > size){
			#ifdef DBG_F_MALLOC
					sfm_split_frag(qm, f, size, file, "fragm. from "
									"sfm_realloc", line);
			#else
					sfm_split_frag(qm, f, size);
			#endif
				}
			}else{ /* p_id < SFM_POOLS_NO (=> in a pool )*/
				hash=GET_SMALL_HASH(n_size);
				pool=&qm->pool[p_id];
				SFM_POOL_LOCK(pool, hash);
				if (unlikely((n->u.nxt_free==0) ||
							((n->size+FRAG_OVERHEAD)<diff))){
					SFM_POOL_UNLOCK(pool, hash);
					goto not_found;
				}
				if (unlikely((n->id!=p_id) || (n->size!=n_size))){
					/* fragment still free, but changed, either 
					 * moved to another pool or has a diff. size */
					SFM_POOL_UNLOCK(pool, hash);
					goto try_again;
				}
				pf=&(pool->pool_hash[hash].first);
				/* find it */
				for(;(*pf)&&(*pf!=n); pf=&((*pf)->u.nxt_free));/*FIXME slow */
				if (*pf==0){
					SFM_POOL_UNLOCK(pool, hash);
					/* not found, bad! */
					LOG(L_WARN, "WARNING: sfm_realloc: could not find %p in "
							    "free " "list (hash=%d)\n", n, hash);
					/* somebody is in the process of changing it ? */
					goto not_found;
				}
				/* detach */
				*pf=n->u.nxt_free;
				n->u.nxt_free=0; /* mark it immediately as detached */
				pool->pool_hash[hash].no--;
				SFM_POOL_UNLOCK(pool, hash);
				/* join */
				f->size+=n->size+FRAG_OVERHEAD;
				/* split it if necessary */
				if (f->size > size){
			#ifdef DBG_F_MALLOC
					sfm_split_frag(qm, f, size, file, "fragm. from "
									"sfm_realloc", line);
			#else
					sfm_split_frag(qm, f, size);
			#endif
				}
			}
		}else{
not_found:
			/* could not join => realloc */
#else/* SFM_REALLOC_REMALLOC */ 
		{
#endif /* SFM_REALLOC_REMALLOC */
	#ifdef DBG_F_MALLOC
			ptr=sfm_malloc(qm, size, file, func, line);
	#else
			ptr=sfm_malloc(qm, size);
	#endif
			if (ptr){
				/* copy, need by libssl */
				memcpy(ptr, p, orig_size);
	#ifdef DBG_F_MALLOC
				sfm_free(qm, p, file, func, line);
	#else
				sfm_free(qm, p);
	#endif
			}
			p=ptr;
		}
	}else{
		/* do nothing */
#ifdef DBG_F_MALLOC
		MDBG("sfm_realloc: doing nothing, same size: %lu - %lu\n", 
				f->size, size);
#endif
	}
#ifdef DBG_F_MALLOC
	MDBG("sfm_realloc: returning %p\n", p);
#endif
	return p;
}



void sfm_status(struct sfm_block* qm)
{
	struct sfm_frag* f;
	int i,j;
	int h;
	int unused;
	unsigned long size;
	int k;
	int memlog;
	int mem_summary;

#warning "ll_status doesn't work (might crash if used)"

	memlog=cfg_get(core, core_cfg, memlog);
	mem_summary=cfg_get(core, core_cfg, mem_summary);
	LOG(memlog, "sfm_status (%p):\n", qm);
	if (!qm) return;

	LOG(memlog, " heap size= %ld\n", qm->size);

	if (mem_summary & 16) return;

	LOG(memlog, "dumping free list:\n");
	for(h=0,i=0,size=0;h<=sfm_max_hash;h++){
		SFM_MAIN_HASH_LOCK(qm, h);
		unused=0;
		for (f=qm->free_hash[h].first,j=0; f;
				size+=f->size,f=f->u.nxt_free,i++,j++){
			if (!FRAG_WAS_USED(f)){
				unused++;
#ifdef DBG_F_MALLOC
				LOG(memlog, "unused fragm.: hash = %3d, fragment %p,"
							" address %p size %lu, created from %s: %s(%ld)\n",
						    h, f, (char*)f+sizeof(struct sfm_frag), f->size,
							f->file, f->func, f->line);
#endif
			};
		}
		if (j) LOG(memlog, "hash = %3d fragments no.: %5d, unused: %5d\n\t\t"
							" bucket size: %9lu - %9lu (first %9lu)\n",
							h, j, unused, UN_HASH(h),
						((h<=SF_MALLOC_OPTIMIZE/SF_ROUNDTO)?1:2)* UN_HASH(h),
							qm->free_hash[h].first->size
				);
		if (j!=qm->free_hash[h].no){
			LOG(L_CRIT, "BUG: sfm_status: different free frag. count: %d!=%ld"
					" for hash %3d\n", j, qm->free_hash[h].no, h);
		}
		SFM_MAIN_HASH_UNLOCK(qm, h);
	}
	for (k=0; k<SFM_POOLS_NO; k++){
		for(h=0;h<SF_HASH_POOL_SIZE;h++){
			SFM_POOL_LOCK(&qm->pool[k], h);
			unused=0;
			for (f=qm->pool[k].pool_hash[h].first,j=0; f;
					size+=f->size,f=f->u.nxt_free,i++,j++){
				if (!FRAG_WAS_USED(f)){
					unused++;
#ifdef DBG_F_MALLOC
					LOG(memlog, "[%2d] unused fragm.: hash = %3d, fragment %p,"
								" address %p size %lu, created from %s: "
								"%s(%ld)\n", k
								h, f, (char*)f+sizeof(struct sfm_frag),
								f->size, f->file, f->func, f->line);
#endif
				};
			}
			if (j) LOG(memlog, "[%2d] hash = %3d fragments no.: %5d, unused: "
								"%5d\n\t\t bucket size: %9lu - %9lu "
								"(first %9lu)\n",
								k, h, j, unused, UN_HASH(h),
							((h<=SF_MALLOC_OPTIMIZE/SF_ROUNDTO)?1:2) *
								UN_HASH(h),
								qm->pool[k].pool_hash[h].first->size
					);
			if (j!=qm->pool[k].pool_hash[h].no){
				LOG(L_CRIT, "BUG: sfm_status: [%d] different free frag."
							" count: %d!=%ld for hash %3d\n",
							k, j, qm->pool[k].pool_hash[h].no, h);
			}
			SFM_POOL_UNLOCK(&qm->pool[k], h);
		}
	}
	LOG(memlog, "TOTAL: %6d free fragments = %6lu free bytes\n", i, size);
	LOG(memlog, "-----------------------------\n");
}



/* fills a malloc info structure with info about the block
 * if a parameter is not supported, it will be filled with 0 */
void sfm_info(struct sfm_block* qm, struct mem_info* info)
{
	int r, k;
	unsigned long total_frags;
	struct sfm_frag* f;
	
	memset(info,0, sizeof(*info));
	total_frags=0;
	info->total_size=qm->size;
	info->min_frag=SF_MIN_FRAG_SIZE;
	/* we'll have to compute it all */
	for (r=0; r<=SF_MALLOC_OPTIMIZE/SF_ROUNDTO; r++){
		info->free+=qm->free_hash[r].no*UN_HASH(r);
		total_frags+=qm->free_hash[r].no;
	}
	for(;r<=sfm_max_hash; r++){
		total_frags+=qm->free_hash[r].no;
		SFM_MAIN_HASH_LOCK(qm, r);
		for(f=qm->free_hash[r].first;f;f=f->u.nxt_free){
			info->free+=f->size;
		}
		SFM_MAIN_HASH_UNLOCK(qm, r);
	}
	for (k=0; k<SFM_POOLS_NO; k++){
		for (r=0; r<SF_HASH_POOL_SIZE; r++){
			info->free+=qm->pool[k].pool_hash[r].no*UN_HASH(r);
			total_frags+=qm->pool[k].pool_hash[r].no;
		}
	}
	info->real_used=info->total_size-info->free;
	info->used=info->real_used-total_frags*FRAG_OVERHEAD-INIT_OVERHEAD
				-FRAG_OVERHEAD;
	info->max_used=0; /* we don't really know */
	info->total_frags=total_frags;
}



/* returns how much free memory is available
 * on error (not compiled with bookkeeping code) returns (unsigned long)(-1) */
unsigned long sfm_available(struct sfm_block* qm)
{
	/* we don't know how much free memory we have and it's to expensive
	 * to compute it */
	return ((unsigned long)-1);
}

#endif
