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


#if !defined(q_malloc)  && (defined F_MALLOC)

#include <string.h>
#include <stdlib.h>

#include "f_malloc.h"
#include "../dprint.h"
#include "../globals.h"
#include "../compiler_opt.h"
#include "memdbg.h"
#include "../bit_scan.h"
#include "../cfg/cfg.h" /* memlog */
#ifdef MALLOC_STATS
#include "../events.h"
#endif


/* useful macros */

#define FRAG_NEXT(f) \
	((struct fm_frag*)((char*)(f)+sizeof(struct fm_frag)+(f)->size ))

#define FRAG_OVERHEAD	(sizeof(struct fm_frag))
#define INIT_OVERHEAD	\
	(ROUNDUP(sizeof(struct fm_block))+2*sizeof(struct fm_frag))



/** ROUNDTO= 2^k so the following works */
#define ROUNDTO_MASK	(~((unsigned long)ROUNDTO-1))
#define ROUNDUP(s)		(((s)+(ROUNDTO-1))&ROUNDTO_MASK)
#define ROUNDDOWN(s)	((s)&ROUNDTO_MASK)



/** finds the hash value for s, s=ROUNDTO multiple */
#define GET_HASH(s)   ( ((unsigned long)(s)<=F_MALLOC_OPTIMIZE)?\
							(unsigned long)(s)/ROUNDTO: \
							F_MALLOC_OPTIMIZE/ROUNDTO+big_hash_idx((s))- \
								F_MALLOC_OPTIMIZE_FACTOR+1 )

#define UN_HASH(h)	( ((unsigned long)(h)<=(F_MALLOC_OPTIMIZE/ROUNDTO))?\
						(unsigned long)(h)*ROUNDTO: \
						1UL<<((unsigned long)(h)-F_MALLOC_OPTIMIZE/ROUNDTO+\
							F_MALLOC_OPTIMIZE_FACTOR-1)\
					)


#ifdef F_MALLOC_HASH_BITMAP

#define fm_bmp_set(qm, b) \
	do{ \
		(qm)->free_bitmap[(b)/FM_HASH_BMP_BITS] |= \
											1UL<<((b)%FM_HASH_BMP_BITS); \
	}while(0)

#define fm_bmp_reset(qm, b) \
	do{ \
		(qm)->free_bitmap[(b)/FM_HASH_BMP_BITS] &= \
											~(1UL<<((b)%FM_HASH_BMP_BITS)); \
	}while(0)

/** returns 0 if not set, !=0 if set */
#define fm_bmp_is_set(qm, b) \
	((qm)->free_bitmap[(b)/FM_HASH_BMP_BITS] & (1UL<<((b)%FM_HASH_BMP_BITS)))



/**
 * \brief Find the first free fragment in a memory block
 * 
 * Find the first free fragment in a memory block
 * \param qm searched memory block
 * \param start start value
 * \return index for free fragment
 */
inline static int fm_bmp_first_set(struct fm_block* qm, int start)
{
	int bmp_idx;
	int bit;
	int r;
	fm_hash_bitmap_t test_val;
	fm_hash_bitmap_t v;
	
	bmp_idx=start/FM_HASH_BMP_BITS;
	bit=start%FM_HASH_BMP_BITS;
	test_val=1UL <<((unsigned long)bit);
	if (qm->free_bitmap[bmp_idx] & test_val)
		return start;
	else if (qm->free_bitmap[bmp_idx] & ~(test_val-1)){
#if 0
		test_val<<=1;
		for (r=bit+1; r<FM_HASH_BMP_BITS; r++, test_val<<=1){
			if (qm->free_bitmap[bmp_idx] & test_val)
				return (start-bit+r);
		}
#endif
		v=qm->free_bitmap[bmp_idx]>>(bit+1);
		return start+1+bit_scan_forward((unsigned long)v);
	}
	for (r=bmp_idx+1;r<FM_HASH_BMP_SIZE; r++){
		if (qm->free_bitmap[r]){
			/* find first set bit */
			return r*FM_HASH_BMP_BITS+
						bit_scan_forward((unsigned long)qm->free_bitmap[r]);
		}
	}
	/* not found, nothing free */
	return -1;
}
#endif /* F_MALLOC_HASH_BITMAP */



/* mark/test used/unused frags */
#define FRAG_MARK_USED(f)
#define FRAG_CLEAR_USED(f)
#define FRAG_WAS_USED(f)   (1)

/* other frag related defines:
 * MEM_COALESCE_FRAGS 
 * MEM_FRAG_AVOIDANCE
 */
#define MEM_FRAG_AVOIDANCE


/** computes hash number for big buckets */
#define big_hash_idx(s) ((unsigned long)bit_scan_reverse((unsigned long)(s)))


/**
 * \name Memory manager boundary check pattern
 */
/*@{ */
#ifdef DBG_F_MALLOC
#define ST_CHECK_PATTERN   0xf0f0f0f0 /** inserted at the beginning */
#define END_CHECK_PATTERN1 0xc0c0c0c0 /** inserted at the end       */
#define END_CHECK_PATTERN2 0xabcdefed /** inserted at the end       */
/*@} */
#endif


/**
 * \brief Extract memory fragment from free list
 * \param qm memory block
 * \param frag memory fragment
 */
static inline void fm_extract_free(struct fm_block* qm, struct fm_frag* frag)
{
	struct fm_frag** pf;
	int hash;

	pf = frag->prv_free;
	hash = GET_HASH(frag->size);

	if(unlikely(pf==0)) {
		/* try to discover previous fragment (safety review) */
		LM_WARN("missing prev info for fragment %p from %p [%d]\n",
					frag, qm, hash);
		if(likely(qm->free_hash[hash].first)) {
			if(likely(qm->free_hash[hash].first==frag)) {
				pf = &(qm->free_hash[hash].first);
			} else {
				for(pf=&(qm->free_hash[hash].first); (*pf); pf=&((*pf)->u.nxt_free)) {
					if((*pf)->u.nxt_free==frag) {
						break;
					}
				}
			}
		}
		if(unlikely(pf==0)) {
			LM_ALERT("attemting to extract inexistent fragment %p from %p [%d]\n",
					frag, qm, hash);
			return;
		}
		frag->prv_free = pf;
	}

	*pf=frag->u.nxt_free;

	if(frag->u.nxt_free) frag->u.nxt_free->prv_free = pf;

	qm->ffrags--;
	qm->free_hash[hash].no--;
#ifdef F_MALLOC_HASH_BITMAP
	if (qm->free_hash[hash].no==0)
		fm_bmp_reset(qm, hash);
#endif /* F_MALLOC_HASH_BITMAP */
	frag->prv_free = NULL;

	qm->real_used+=frag->size;
	qm->used+=frag->size;
}

/**
 * \brief Insert a memory fragment in a memory block
 * \param qm memory block
 * \param frag memory fragment
 */
static inline void fm_insert_free(struct fm_block* qm, struct fm_frag* frag)
{
	struct fm_frag** f;
	int hash;
	
	hash=GET_HASH(frag->size);
	f=&(qm->free_hash[hash].first);
	if (frag->size > F_MALLOC_OPTIMIZE){ /* because of '<=' in GET_HASH,
											(different from 0.8.1[24] on
											 purpose --andrei ) */
		for(; *f; f=&((*f)->u.nxt_free)){
			if (frag->size <= (*f)->size) break;
		}
	}
	
	/*insert it here*/
	frag->prv_free = f;
	frag->u.nxt_free=*f;
	if (*f) (*f)->prv_free = &(frag->u.nxt_free);
	*f=frag;
	qm->ffrags++;
	qm->free_hash[hash].no++;
#ifdef F_MALLOC_HASH_BITMAP
	fm_bmp_set(qm, hash);
#endif /* F_MALLOC_HASH_BITMAP */
	qm->used-=frag->size;
	qm->real_used-=frag->size;
}


/**
 *\brief Split a memory fragement
 *
 * Split a memory fragement, size should be already rounded-up
 * \param qm memory block
 * \param frag memory fragment
 * \param size fragement size
 */
static inline
#ifdef DBG_F_MALLOC 
void fm_split_frag(struct fm_block* qm, struct fm_frag* frag,
					unsigned long size,
					const char* file, const char* func, unsigned int line)
#else
void fm_split_frag(struct fm_block* qm, struct fm_frag* frag,
					unsigned long size)
#endif
{
	unsigned long rest;
	struct fm_frag* n;
	
	rest=frag->size-size;
#ifdef MEM_FRAG_AVOIDANCE
	if ((rest> (FRAG_OVERHEAD+F_MALLOC_OPTIMIZE))||
		(rest>=(FRAG_OVERHEAD+size))){ /* the residue fragm. is big enough*/
#else
	if (rest>(FRAG_OVERHEAD+MIN_FRAG_SIZE)){
#endif
		frag->size=size;
		/*split the fragment*/
		n=FRAG_NEXT(frag);
		n->size=rest-FRAG_OVERHEAD;
		FRAG_CLEAR_USED(n); /* never used */
#ifdef DBG_F_MALLOC
		/* frag created by malloc, mark it*/
		n->file=file;
		n->func="frag. from fm_split_frag";
		n->line=line;
		n->check=ST_CHECK_PATTERN;
#endif
		/* reinsert n in free list*/
		qm->used-=FRAG_OVERHEAD;
		fm_insert_free(qm, n);
	}else{
		/* we cannot split this fragment any more => alloc all of it*/
	}
}


/**
 * \brief Initialize memory manager malloc
 * \param address start address for memory block
 * \param size Size of allocation
 * \return return the fm_block
 */
struct fm_block* fm_malloc_init(char* address, unsigned long size, int type)
{
	char* start;
	char* end;
	struct fm_block* qm;
	unsigned long init_overhead;
	
	/* make address and size multiple of 8*/
	start=(char*)ROUNDUP((unsigned long) address);
	DBG("fm_malloc_init: F_OPTIMIZE=%lu, /ROUNDTO=%lu\n",
			F_MALLOC_OPTIMIZE, F_MALLOC_OPTIMIZE/ROUNDTO);
	DBG("fm_malloc_init: F_HASH_SIZE=%lu, fm_block size=%lu\n",
			F_HASH_SIZE, (long)sizeof(struct fm_block));
	DBG("fm_malloc_init(%p, %lu), start=%p\n", address, size, start);

	if (size<start-address) return 0;
	size-=(start-address);
	if (size <(MIN_FRAG_SIZE+FRAG_OVERHEAD)) return 0;
	size=ROUNDDOWN(size);

	init_overhead=INIT_OVERHEAD;
	
	
	if (size < init_overhead)
	{
		/* not enough mem to create our control structures !!!*/
		return 0;
	}
	end=start+size;
	qm=(struct fm_block*)start;
	memset(qm, 0, sizeof(struct fm_block));
	qm->size=size;
	qm->used = size - init_overhead;
	qm->real_used=size;
	qm->max_real_used=init_overhead;
	qm->type = type;
	size-=init_overhead;
	
	qm->first_frag=(struct fm_frag*)(start+ROUNDUP(sizeof(struct fm_block)));
	qm->last_frag=(struct fm_frag*)(end-sizeof(struct fm_frag));
	/* init first fragment*/
	qm->first_frag->size=size;
	qm->first_frag->prv_free=0;
	/* init last fragment*/
	qm->last_frag->size=0;
	qm->last_frag->prv_free=0;
	
#ifdef DBG_F_MALLOC
	qm->first_frag->check=ST_CHECK_PATTERN;
	qm->last_frag->check=END_CHECK_PATTERN1;
#endif
	
	/* link initial fragment into the free list*/
	
	fm_insert_free(qm, qm->first_frag);

	return qm;
}

/**
 * \brief Try merging free fragments to fit requested size
 * \param qm memory block
 * \param size memory allocation size
 * \return address of allocated memory
 */
struct fm_frag* fm_search_defrag(struct fm_block* qm, unsigned long size)
{
	struct fm_frag* frag;
	struct fm_frag* nxt;

	frag = qm->first_frag;
	while((char*)frag < (char*)qm->last_frag) {
		nxt = FRAG_NEXT(frag);

		if ( ((char*)nxt < (char*)qm->last_frag) && frag->prv_free
				&& nxt->prv_free) {
			/* join frag + nxt */
			fm_extract_free(qm, frag);
			do {
				fm_extract_free(qm, nxt);
				frag->size += nxt->size + FRAG_OVERHEAD;

				/* join - one frag less, add overhead to used */
				qm->used += FRAG_OVERHEAD;

				if( frag->size >size )
					return frag;

				nxt = FRAG_NEXT(frag);
			} while (((char*)nxt < (char*)qm->last_frag) && nxt->prv_free);

			fm_insert_free(qm, frag);
		}
		frag = nxt;
	}

	return 0;
}

/**
 * \brief Main memory manager allocation function
 * 
 * Main memory manager allocation function, provide functionality necessary for pkg_malloc
 * \param qm memory block
 * \param size memory allocation size
 * \return address of allocated memory
 */
#ifdef DBG_F_MALLOC
void* fm_malloc(struct fm_block* qm, unsigned long size,
					const char* file, const char* func, unsigned int line)
#else
void* fm_malloc(struct fm_block* qm, unsigned long size)
#endif
{
	struct fm_frag** f;
	struct fm_frag* frag;
	int hash;
	
#ifdef DBG_F_MALLOC
	MDBG("fm_malloc(%p, %lu) called from %s: %s(%d)\n", qm, size, file, func,
			line);
#endif
	/*malloc(0) should return a valid pointer according to specs*/
	if(unlikely(size==0)) size=4;
	/*size must be a multiple of 8*/
	size=ROUNDUP(size);
	
	/*search for a suitable free frag*/

#ifdef F_MALLOC_HASH_BITMAP
	hash=fm_bmp_first_set(qm, GET_HASH(size));
	if (likely(hash>=0)){
		if (likely(hash<=F_MALLOC_OPTIMIZE/ROUNDTO)) { /* return first match */
			f=&(qm->free_hash[hash].first);
			if(likely(*f)) goto found;
#ifdef DBG_F_MALLOC
			MDBG(" block %p hash %d empty but no. is %lu\n", qm,
					hash, qm->free_hash[hash].no);
#endif
			/* reset slot and try next hash */
			qm->free_hash[hash].no=0;
			fm_bmp_reset(qm, hash);
			hash++;
		}
		/* if we are here we are searching next hash slot or a "big" fragment
		   between F_MALLOC_OPTIMIZE/ROUNDTO+1
		   and F_MALLOC_OPTIMIZE/ROUNDTO + (32|64) - F_MALLOC_OPTIMIZE_FACTOR
		   => 18 hash buckets on 32 bits and 50 buckets on 64 bits
		   The free hash bitmap is used to jump directly to non-empty
		   hash buckets.
		*/
		do {
			for(f=&(qm->free_hash[hash].first);(*f); f=&((*f)->u.nxt_free))
				if ((*f)->size>=size) goto found;
			hash++; /* try in next hash cell */
		}while((hash < F_HASH_SIZE) &&
				((hash=fm_bmp_first_set(qm, hash)) >= 0));
	}
#else /* F_MALLOC_HASH_BITMAP */
	for(hash=GET_HASH(size);hash<F_HASH_SIZE;hash++){
		f=&(qm->free_hash[hash].first);
		for(;(*f); f=&((*f)->u.nxt_free))
			if ((*f)->size>=size) goto found;
		/* try in a bigger bucket */
	}
#endif /* F_MALLOC_HASH_BITMAP */
	/* not found, search by defrag */

	frag = fm_search_defrag(qm, size);

	if(frag) goto finish;

	return 0;

found:
	/* we found it!*/
	/* detach it from the free list*/
	frag=*f;
	fm_extract_free(qm, frag);

	/*see if use full frag or split it in two*/
#ifdef DBG_F_MALLOC
	fm_split_frag(qm, frag, size, file, func, line);
#else
	fm_split_frag(qm, frag, size);
#endif

finish:

#ifdef DBG_F_MALLOC
	frag->file=file;
	frag->func=func;
	frag->line=line;
	frag->check=ST_CHECK_PATTERN;
	MDBG("fm_malloc(%p, %lu) returns address %p \n", qm, size,
		(char*)frag+sizeof(struct fm_frag));
#endif

	if (qm->max_real_used<qm->real_used)
		qm->max_real_used=qm->real_used;
	FRAG_MARK_USED(frag); /* mark it as used */
	if(qm->type==MEM_TYPE_PKG) {
		sr_event_exec(SREV_PKG_UPDATE_STATS, 0);
	}
	return (char*)frag+sizeof(struct fm_frag);
}


#ifdef MEM_JOIN_FREE
/**
 * join fragment f with next one (if it is free)
 */
static void fm_join_frag(struct fm_block* qm, struct fm_frag* f)
{
	int hash;
	struct fm_frag **pf;
	struct fm_frag* n;

	n=FRAG_NEXT(f);
	/* check if valid and if in free list */
	if (((char*)n >= (char*)qm->last_frag) || (n->prv_free==NULL))
		return;

	/* detach n from the free list */
	hash=GET_HASH(n->size);
	pf=n->prv_free;
	if (*pf==0){
		/* not found, bad! */
		LM_WARN("could not find %p in free list (hash=%ld)\n", n, GET_HASH(n->size));
		return;
	}
	/* detach */
	*pf=n->u.nxt_free;
	if(n->u.nxt_free) n->u.nxt_free->prv_free = pf;
	qm->ffrags--;
	qm->free_hash[hash].no--;
#ifdef F_MALLOC_HASH_BITMAP
	if (qm->free_hash[hash].no==0)
		fm_bmp_reset(qm, hash);
#endif /* F_MALLOC_HASH_BITMAP */
	/* join */
	f->size+=n->size+FRAG_OVERHEAD;
	qm->real_used+=n->size;
	qm->used+=n->size + FRAG_OVERHEAD;
}
#endif /*MEM_JOIN_FREE*/

/**
 * \brief Main memory manager free function
 * 
 * Main memory manager free function, provide functionality necessary for pkg_free
 * \param qm memory block
 * \param p freed memory
 */
#ifdef DBG_F_MALLOC
void fm_free(struct fm_block* qm, void* p, const char* file, const char* func, 
				unsigned int line)
#else
void fm_free(struct fm_block* qm, void* p)
#endif
{
	struct fm_frag* f;

#ifdef DBG_F_MALLOC
	MDBG("fm_free(%p, %p), called from %s: %s(%d)\n", qm, p, file, func, line);
#endif
	if (p==0) {
		MDBG("WARNING:fm_free: free(0) called\n");
		return;
	}
#ifdef DBG_F_MALLOC
	if (p>(void*)qm->last_frag || p<(void*)qm->first_frag){
		LOG(L_CRIT, "BUG: fm_free: bad pointer %p (out of memory block!),"
				" called from %s: %s(%d) - aborting\n", p,
				file, func, line);
		if(likely(cfg_get(core, core_cfg, mem_safety)==0))
			abort();
		else return;
	}
#endif
	f=(struct fm_frag*) ((char*)p-sizeof(struct fm_frag));
#ifdef DBG_F_MALLOC
	MDBG("fm_free: freeing block alloc'ed from %s: %s(%ld)\n",
			f->file, f->func, f->line);
#endif
	if(unlikely(f->prv_free!=NULL)) {
		LM_INFO("freeing a free fragment (%p/%p) - ignore\n",
				f, p);
		return;
	}
	if(qm->type==MEM_TYPE_PKG) {
		sr_event_exec(SREV_PKG_UPDATE_STATS, 0);
	}
#ifdef DBG_F_MALLOC
	f->file=file;
	f->func=func;
	f->line=line;
#endif
#ifdef MEM_JOIN_FREE
	if(unlikely(cfg_get(core, core_cfg, mem_join)!=0))
		fm_join_frag(qm, f);
#endif /*MEM_JOIN_FREE*/
	fm_insert_free(qm, f);
}


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
void* fm_realloc(struct fm_block* qm, void* p, unsigned long size,
					const char* file, const char* func, unsigned int line)
#else
void* fm_realloc(struct fm_block* qm, void* p, unsigned long size)
#endif
{
	struct fm_frag *f;
	unsigned long diff;
	unsigned long orig_size;
	struct fm_frag *n;
	void *ptr;
	
#ifdef DBG_F_MALLOC
	MDBG("fm_realloc(%p, %p, %lu) called from %s: %s(%d)\n", qm, p, size,
			file, func, line);
	if ((p)&&(p>(void*)qm->last_frag || p<(void*)qm->first_frag)){
		LOG(L_CRIT, "BUG: fm_free: bad pointer %p (out of memory block!) - "
				"aborting\n", p);
		abort();
	}
#endif
	if (size==0) {
		if (p)
#ifdef DBG_F_MALLOC
			fm_free(qm, p, file, func, line);
#else
			fm_free(qm, p);
#endif
		return 0;
	}
	if (p==0)
#ifdef DBG_F_MALLOC
		return fm_malloc(qm, size, file, func, line);
#else
		return fm_malloc(qm, size);
#endif
	f=(struct fm_frag*) ((char*)p-sizeof(struct fm_frag));
#ifdef DBG_F_MALLOC
	MDBG("fm_realloc: realloc'ing frag %p alloc'ed from %s: %s(%ld)\n",
			f, f->file, f->func, f->line);
#endif
	size=ROUNDUP(size);
	orig_size=f->size;
	if (f->size > size){
		/* shrink */
#ifdef DBG_F_MALLOC
		MDBG("fm_realloc: shrinking from %lu to %lu\n", f->size, size);
		fm_split_frag(qm, f, size, file, "frag. from fm_realloc", line);
#else
		fm_split_frag(qm, f, size);
#endif
	}else if (f->size<size){
		/* grow */
#ifdef DBG_F_MALLOC
		MDBG("fm_realloc: growing from %lu to %lu\n", f->size, size);
#endif
		diff=size-f->size;
		n=FRAG_NEXT(f);
		if (((char*)n < (char*)qm->last_frag) && 
				(n->prv_free) && ((n->size+FRAG_OVERHEAD)>=diff)){
			/* detach n from the free list */
			fm_extract_free(qm, n);
			/* join  */
			f->size+=n->size+FRAG_OVERHEAD;
			qm->used+=FRAG_OVERHEAD;

			/* split it if necessary */
			if (f->size > size){
		#ifdef DBG_F_MALLOC
				fm_split_frag(qm, f, size, file, "fragm. from fm_realloc",
						line);
		#else
				fm_split_frag(qm, f, size);
		#endif
			}
		}else{
			/* could not join => realloc */
	#ifdef DBG_F_MALLOC
			ptr=fm_malloc(qm, size, file, func, line);
	#else
			ptr=fm_malloc(qm, size);
	#endif
			if (ptr){
				/* copy, need by libssl */
				memcpy(ptr, p, orig_size);
			}
	#ifdef DBG_F_MALLOC
			fm_free(qm, p, file, func, line);
	#else
			fm_free(qm, p);
	#endif
			p=ptr;
		}
	}else{
		/* do nothing */
#ifdef DBG_F_MALLOC
		MDBG("fm_realloc: doing nothing, same size: %lu - %lu\n", 
				f->size, size);
#endif
	}
#ifdef DBG_F_MALLOC
	MDBG("fm_realloc: returning %p\n", p);
#endif
	if(qm->type==MEM_TYPE_PKG) {
		sr_event_exec(SREV_PKG_UPDATE_STATS, 0);
	}
	return p;
}


/**
 * \brief Report internal memory manager status
 * \param qm memory block
 */
void fm_status(struct fm_block* qm)
{
	struct fm_frag* f;
	int i,j;
	int h;
	int unused;
	unsigned long size;
	int memlog;
	int mem_summary;

	memlog=cfg_get(core, core_cfg, memlog);
	mem_summary=cfg_get(core, core_cfg, mem_summary);
	LOG_(DEFAULT_FACILITY, memlog, "fm_status: ", "fm_status (%p):\n", qm);
	if (!qm) return;

	LOG_(DEFAULT_FACILITY, memlog, "fm_status: ", " heap size= %ld\n",
			qm->size);
#if defined(DBG_F_MALLOC) || defined(MALLOC_STATS)
	LOG_(DEFAULT_FACILITY, memlog, "fm_status: ",
			" used= %lu, used+overhead=%lu, free=%lu\n",
			qm->used, qm->real_used, qm->size-qm->real_used);
	LOG_(DEFAULT_FACILITY, memlog, "fm_status: ",
			" max used (+overhead)= %lu\n", qm->max_real_used);
#endif

	if (mem_summary & 16) return;

	/*
	LOG_(DEFAULT_FACILITY, memlog, "fm_status: ", "dumping all fragments:\n");
	for (f=qm->first_frag, i=0;((char*)f<(char*)qm->last_frag) && (i<10);
			f=FRAG_NEXT(f), i++){
		LOG_(DEFAULT_FACILITY, memlog, "fm_status: ",
				"    %3d. %c  address=%x  size=%d\n", i,
				(f->u.reserved)?'a':'N',
				(char*)f+sizeof(struct fm_frag), f->size);
#ifdef DBG_F_MALLOC
		LOG_(DEFAULT_FACILITY, memlog, "fm_status: ",
				"            %s from %s: %s(%d)\n",
				(f->u.is_free)?"freed":"alloc'd", f->file, f->func, f->line);
#endif
	}
*/
	LOG_(DEFAULT_FACILITY, memlog, "fm_status: ", "dumping free list:\n");
	for(h=0,i=0,size=0;h<F_HASH_SIZE;h++){
		unused=0;
		for (f=qm->free_hash[h].first,j=0; f;
				size+=f->size,f=f->u.nxt_free,i++,j++){
			if (!FRAG_WAS_USED(f)){
				unused++;
#ifdef DBG_F_MALLOC
				LOG_(DEFAULT_FACILITY, memlog, "fm_status: ",
							"unused fragm.: hash = %3d, fragment %p,"
							" address %p size %lu, created from %s: %s(%ld)\n",
						    h, f, (char*)f+sizeof(struct fm_frag), f->size,
							f->file, f->func, f->line);
#endif
			};
		}
		if (j) LOG_(DEFAULT_FACILITY, memlog, "fm_status: ",
							"hash = %3d fragments no.: %5d, unused: %5d\n\t\t"
							" bucket size: %9lu - %9lu (first %9lu)\n",
							h, j, unused, UN_HASH(h),
						((h<=F_MALLOC_OPTIMIZE/ROUNDTO)?1:2)* UN_HASH(h),
							qm->free_hash[h].first->size
				);
		if (j!=qm->free_hash[h].no){
			LOG(L_CRIT, "BUG: fm_status: different free frag. count: %d!=%ld"
					" for hash %3d\n", j, qm->free_hash[h].no, h);
		}
		/*
		{
			LOG_(DEFAULT_FACILITY, memlog, "fm_status: ",
					"   %5d.[%3d:%3d] %c  address=%x  size=%d(%x)\n",
					i, h, j,
					(f->u.reserved)?'a':'N',
					(char*)f+sizeof(struct fm_frag), f->size, f->size);
#ifdef DBG_F_MALLOC
			DBG("            %s from %s: %s(%d)\n", 
				(f->u.reserved)?"freed":"alloc'd", f->file, f->func, f->line);
#endif
		}
	*/
	}
	LOG_(DEFAULT_FACILITY, memlog, "fm_status: ",
			"TOTAL: %6d free fragments = %6lu free bytes\n", i, size);
	LOG_(DEFAULT_FACILITY, memlog, "fm_status: ",
			"-----------------------------\n");
}


/**
 * \brief Fills a malloc info structure with info about the block
 *
 * Fills a malloc info structure with info about the block, if a
 * parameter is not supported, it will be filled with 0
 * \param qm memory block
 * \param info memory information
 */
void fm_info(struct fm_block* qm, struct mem_info* info)
{
	memset(info,0, sizeof(*info));
	info->total_size=qm->size;
	info->min_frag=MIN_FRAG_SIZE;
	info->free=qm->size-qm->real_used;
	info->used=qm->used;
	info->real_used=qm->real_used;
	info->max_used=qm->max_real_used;
	info->total_frags=qm->ffrags;
}


/**
 * \brief Helper function for available memory report
 * \param qm memory block
 * \return Returns how much free memory is available, on error (not compiled
 * with bookkeeping code) returns (unsigned long)(-1)
 */
unsigned long fm_available(struct fm_block* qm)
{
	return qm->size-qm->real_used;
}


#ifdef DBG_F_MALLOC

typedef struct _mem_counter{
	const char *file;
	const char *func;
	unsigned long line;
	
	unsigned long size;
	int count;
	
	struct _mem_counter *next;
} mem_counter;

static mem_counter* get_mem_counter(mem_counter **root,struct fm_frag* f)
{
	mem_counter *x;
	
	if (!*root) goto make_new;
	for(x=*root;x;x=x->next)
		if (x->file == f->file && x->func == f->func && x->line == f->line)
			return x;
make_new:	
	x = malloc(sizeof(mem_counter));
	x->file = f->file;
	x->func = f->func;
	x->line = f->line;
	x->count = 0;
	x->size = 0;
	x->next = *root;
	*root = x;
	return x;
}


/**
 * \brief Debugging helper, summary and logs all allocated memory blocks
 * \param qm memory block
 */
void fm_sums(struct fm_block* qm)
{
	struct fm_frag* f;
	int i;
	int memlog;
	mem_counter *root,*x;
	
	root=0;
	if (!qm) return;

	memlog=cfg_get(core, core_cfg, memlog);
	LOG_(DEFAULT_FACILITY, memlog, "fm_status: ",
			"summarizing all alloc'ed. fragments:\n");
	
	for (f=qm->first_frag, i=0; (char*)f<(char*)qm->last_frag;
			f=FRAG_NEXT(f), i++){
		if (f->prv_free==0){
			x = get_mem_counter(&root,f);
			x->count++;
			x->size+=f->size;
		}
	}
	x = root;
	while(x){
		LOG_(DEFAULT_FACILITY, memlog, "fm_status: ",
				" count=%6d size=%10lu bytes from %s: %s(%ld)\n",
			x->count,x->size,
			x->file, x->func, x->line
			);
		root = x->next;
		free(x);
		x = root;
	}
	LOG_(DEFAULT_FACILITY, memlog, "fm_status: ",
			"-----------------------------\n");
}
#endif /* DBG_F_MALLOC */



#endif
