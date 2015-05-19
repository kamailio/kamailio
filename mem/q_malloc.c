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
 * \brief Simple & fast malloc library
 * \ingroup mem
 */


#if !defined(q_malloc) && !(defined F_MALLOC) && !defined(TLSF_MALLOC)
#define q_malloc

#include <stdlib.h>
#include <string.h>

#include "q_malloc.h"
#include "../dprint.h"
#include "../globals.h"
#include "memdbg.h"
#include "../cfg/cfg.h" /* memlog */
#ifdef MALLOC_STATS
#include "../events.h"
#endif


/*useful macros*/
#define FRAG_END(f)  \
	((struct qm_frag_end*)((char*)(f)+sizeof(struct qm_frag)+ \
	   (f)->size))

#define FRAG_NEXT(f) \
	((struct qm_frag*)((char*)(f)+sizeof(struct qm_frag)+(f)->size+ \
	   sizeof(struct qm_frag_end)))
			
#define FRAG_PREV(f) \
	( (struct qm_frag*) ( ((char*)(f)-sizeof(struct qm_frag_end))- \
	((struct qm_frag_end*)((char*)(f)-sizeof(struct qm_frag_end)))->size- \
	   sizeof(struct qm_frag) ) )

#define PREV_FRAG_END(f) \
	((struct qm_frag_end*)((char*)(f)-sizeof(struct qm_frag_end)))


#define FRAG_OVERHEAD	(sizeof(struct qm_frag)+sizeof(struct qm_frag_end))


#define ROUNDTO_MASK	(~((unsigned long)ROUNDTO-1))
#define ROUNDUP(s)		(((s)+(ROUNDTO-1))&ROUNDTO_MASK)
#define ROUNDDOWN(s)	((s)&ROUNDTO_MASK)



	/* finds the hash value for s, s=ROUNDTO multiple*/
#define GET_HASH(s)   ( ((unsigned long)(s)<=QM_MALLOC_OPTIMIZE)?\
							(unsigned long)(s)/ROUNDTO: \
							QM_MALLOC_OPTIMIZE/ROUNDTO+big_hash_idx((s))- \
								QM_MALLOC_OPTIMIZE_FACTOR+1 )

#define UN_HASH(h)	( ((unsigned long)(h)<=(QM_MALLOC_OPTIMIZE/ROUNDTO))?\
							(unsigned long)(h)*ROUNDTO: \
							1UL<<((h)-QM_MALLOC_OPTIMIZE/ROUNDTO+\
								QM_MALLOC_OPTIMIZE_FACTOR-1)\
					)


/* mark/test used/unused frags */
#define FRAG_MARK_USED(f)
#define FRAG_CLEAR_USED(f)
#define FRAG_WAS_USED(f)   (1)

/* other frag related defines:
 * MEM_COALESCE_FRAGS 
 * MEM_FRAG_AVOIDANCE
 */

#define MEM_FRAG_AVOIDANCE


/* computes hash number for big buckets*/
inline static unsigned long big_hash_idx(unsigned long s)
{
	int idx;
	/* s is rounded => s = k*2^n (ROUNDTO=2^n) 
	 * index= i such that 2^i > s >= 2^(i-1)
	 *
	 * => index = number of the first non null bit in s*/
	idx=sizeof(long)*8-1;
	for (; !(s&(1UL<<(sizeof(long)*8-1))) ; s<<=1, idx--);
	return idx;
}


#ifdef DBG_QM_MALLOC
#define ST_CHECK_PATTERN   0xf0f0f0f0
#define END_CHECK_PATTERN1 0xc0c0c0c0
#define END_CHECK_PATTERN2 0xabcdefed


static  void qm_debug_frag(struct qm_block* qm, struct qm_frag* f)
{
	if (f->check!=ST_CHECK_PATTERN){
		LOG(L_CRIT, "BUG: qm_*: fragm. %p (address %p) "
				"beginning overwritten(%lx)!\n",
				f, (char*)f+sizeof(struct qm_frag),
				f->check);
		qm_status(qm);
		abort();
	};
	if ((FRAG_END(f)->check1!=END_CHECK_PATTERN1)||
		(FRAG_END(f)->check2!=END_CHECK_PATTERN2)){
		LOG(L_CRIT, "BUG: qm_*: fragm. %p (address %p)"
					" end overwritten(%lx, %lx)!\n",
				f, (char*)f+sizeof(struct qm_frag), 
				FRAG_END(f)->check1, FRAG_END(f)->check2);
		qm_status(qm);
		abort();
	}
	if ((f>qm->first_frag)&&
			((PREV_FRAG_END(f)->check1!=END_CHECK_PATTERN1) ||
				(PREV_FRAG_END(f)->check2!=END_CHECK_PATTERN2) ) ){
		LOG(L_CRIT, "BUG: qm_*: prev. fragm. tail overwritten(%lx, %lx)[%p:%p]!"
					"\n",
				PREV_FRAG_END(f)->check1, PREV_FRAG_END(f)->check2, f,
				(char*)f+sizeof(struct qm_frag));
		qm_status(qm);
		abort();
	}
}
#endif



static inline void qm_insert_free(struct qm_block* qm, struct qm_frag* frag)
{
	struct qm_frag* f;
	struct qm_frag* prev;
	int hash;
	
	hash=GET_HASH(frag->size);
	for(f=qm->free_hash[hash].head.u.nxt_free; f!=&(qm->free_hash[hash].head);
			f=f->u.nxt_free){
		if (frag->size <= f->size) break;
	}
	/*insert it here*/
	prev=FRAG_END(f)->prev_free;
	prev->u.nxt_free=frag;
	FRAG_END(frag)->prev_free=prev;
	frag->u.nxt_free=f;
	FRAG_END(f)->prev_free=frag;
	qm->free_hash[hash].no++;
	qm->ffrags++;
}



/* init malloc and return a qm_block*/
struct qm_block* qm_malloc_init(char* address, unsigned long size, int type)
{
	char* start;
	char* end;
	struct qm_block* qm;
	unsigned long init_overhead;
	int h;
	
	/* make address and size multiple of 8*/
	start=(char*)ROUNDUP((unsigned long) address);
	DBG("qm_malloc_init: QM_OPTIMIZE=%lu, /ROUNDTO=%lu\n",
			QM_MALLOC_OPTIMIZE, QM_MALLOC_OPTIMIZE/ROUNDTO);
	DBG("qm_malloc_init: QM_HASH_SIZE=%lu, qm_block size=%lu\n",
			QM_HASH_SIZE, (long)sizeof(struct qm_block));
	DBG("qm_malloc_init(%p, %lu), start=%p\n", address, size, start);
	if (size<start-address) return 0;
	size-=(start-address);
	if (size <(MIN_FRAG_SIZE+FRAG_OVERHEAD)) return 0;
	size=ROUNDDOWN(size);
	
	init_overhead=ROUNDUP(sizeof(struct qm_block))+sizeof(struct qm_frag)+
		sizeof(struct qm_frag_end);
	DBG("qm_malloc_init: size= %lu, init_overhead=%lu\n", size, init_overhead);
	
	if (size < init_overhead)
	{
		/* not enough mem to create our control structures !!!*/
		return 0;
	}
	end=start+size;
	qm=(struct qm_block*)start;
	memset(qm, 0, sizeof(struct qm_block));
	qm->size=size;
	qm->real_used=init_overhead;
	qm->max_real_used=qm->real_used;
	qm->type=type;
	size-=init_overhead;
	
	qm->first_frag=(struct qm_frag*)(start+ROUNDUP(sizeof(struct qm_block)));
	qm->last_frag_end=(struct qm_frag_end*)(end-sizeof(struct qm_frag_end));
	/* init initial fragment*/
	qm->first_frag->size=size;
	qm->last_frag_end->size=size;
	
#ifdef DBG_QM_MALLOC
	qm->first_frag->check=ST_CHECK_PATTERN;
	qm->last_frag_end->check1=END_CHECK_PATTERN1;
	qm->last_frag_end->check2=END_CHECK_PATTERN2;
#endif
	/* init free_hash* */
	for (h=0; h<QM_HASH_SIZE;h++){
		qm->free_hash[h].head.u.nxt_free=&(qm->free_hash[h].head);
		qm->free_hash[h].tail.prev_free=&(qm->free_hash[h].head);
		qm->free_hash[h].head.size=0;
		qm->free_hash[h].tail.size=0;
	}
	
	/* link initial fragment into the free list*/
	
	qm_insert_free(qm, qm->first_frag);
	
	/*qm->first_frag->u.nxt_free=&(qm->free_lst);
	  qm->last_frag_end->prev_free=&(qm->free_lst);
	*/
	
	
	return qm;
}



static inline void qm_detach_free(struct qm_block* qm, struct qm_frag* frag)
{
	struct qm_frag *prev;
	struct qm_frag *next;
	
	prev=FRAG_END(frag)->prev_free;
	next=frag->u.nxt_free;
	prev->u.nxt_free=next;
	FRAG_END(next)->prev_free=prev;
	
}



#ifdef DBG_QM_MALLOC
static inline struct qm_frag* qm_find_free(struct qm_block* qm, 
											unsigned long size,
											int *h,
											unsigned int *count)
#else
static inline struct qm_frag* qm_find_free(struct qm_block* qm, 
											unsigned long size,
											int* h)
#endif
{
	int hash;
	struct qm_frag* f;

	for (hash=GET_HASH(size); hash<QM_HASH_SIZE; hash++){
		for (f=qm->free_hash[hash].head.u.nxt_free; 
					f!=&(qm->free_hash[hash].head); f=f->u.nxt_free){
#ifdef DBG_QM_MALLOC
			*count+=1; /* *count++ generates a warning with gcc 2.9* -Wall */
#endif
			if (f->size>=size){ *h=hash; return f; }
		}
	/*try in a bigger bucket*/
	}
	/* not found */
	return 0;
}


/* returns 0 on success, -1 on error;
 * new_size < size & rounded-up already!*/
static inline
#ifdef DBG_QM_MALLOC
int split_frag(struct qm_block* qm, struct qm_frag* f, unsigned long new_size,
				const char* file, const char* func, unsigned int line)
#else
int split_frag(struct qm_block* qm, struct qm_frag* f, unsigned long new_size)
#endif
{
	unsigned long rest;
	struct qm_frag* n;
	struct qm_frag_end* end;
	
	rest=f->size-new_size;
#ifdef MEM_FRAG_AVOIDANCE
	if ((rest> (FRAG_OVERHEAD+QM_MALLOC_OPTIMIZE))||
		(rest>=(FRAG_OVERHEAD+new_size))){/* the residue fragm. is big enough*/
#else
	if (rest>(FRAG_OVERHEAD+MIN_FRAG_SIZE)){
#endif
		f->size=new_size;
		/*split the fragment*/
		end=FRAG_END(f);
		end->size=new_size;
		n=(struct qm_frag*)((char*)end+sizeof(struct qm_frag_end));
		n->size=rest-FRAG_OVERHEAD;
		FRAG_END(n)->size=n->size;
		FRAG_CLEAR_USED(n); /* never used */
		qm->real_used+=FRAG_OVERHEAD;
#ifdef DBG_QM_MALLOC
		end->check1=END_CHECK_PATTERN1;
		end->check2=END_CHECK_PATTERN2;
		/* frag created by malloc, mark it*/
		n->file=file;
		n->func=func;
		n->line=line;
		n->check=ST_CHECK_PATTERN;
#endif
		/* reinsert n in free list*/
		qm_insert_free(qm, n);
		return 0;
	}else{
			/* we cannot split this fragment any more */
		return -1;
	}
}



#ifdef DBG_QM_MALLOC
void* qm_malloc(struct qm_block* qm, unsigned long size,
					const char* file, const char* func, unsigned int line)
#else
void* qm_malloc(struct qm_block* qm, unsigned long size)
#endif
{
	struct qm_frag* f;
	int hash;
	
#ifdef DBG_QM_MALLOC
	unsigned int list_cntr;

	list_cntr = 0;
	MDBG("qm_malloc(%p, %lu) called from %s: %s(%d)\n", qm, size, file, func,
			line);
#endif
	/*malloc(0) should return a valid pointer according to specs*/
	if(unlikely(size==0)) size=4;
	/*size must be a multiple of 8*/
	size=ROUNDUP(size);
	if (size>(qm->size-qm->real_used)) return 0;

	/*search for a suitable free frag*/
#ifdef DBG_QM_MALLOC
	if ((f=qm_find_free(qm, size, &hash, &list_cntr))!=0){
#else
	if ((f=qm_find_free(qm, size, &hash))!=0){
#endif
		/* we found it!*/
		/*detach it from the free list*/
#ifdef DBG_QM_MALLOC
			qm_debug_frag(qm, f);
#endif
		qm_detach_free(qm, f);
		/*mark it as "busy"*/
		f->u.is_free=0;
		qm->free_hash[hash].no--;
		qm->ffrags--;
		/* we ignore split return */
#ifdef DBG_QM_MALLOC
		split_frag(qm, f, size, file, "fragm. from qm_malloc", line);
#else
		split_frag(qm, f, size);
#endif
		qm->real_used+=f->size;
		qm->used+=f->size;
		if (qm->max_real_used<qm->real_used)
			qm->max_real_used=qm->real_used;
#ifdef DBG_QM_MALLOC
		f->file=file;
		f->func=func;
		f->line=line;
		f->check=ST_CHECK_PATTERN;
		/*  FRAG_END(f)->check1=END_CHECK_PATTERN1;
			FRAG_END(f)->check2=END_CHECK_PATTERN2;*/
		MDBG("qm_malloc(%p, %lu) returns address %p frag. %p (size=%lu) on %d"
				" -th hit\n",
			 qm, size, (char*)f+sizeof(struct qm_frag), f, f->size, list_cntr );
#endif
#ifdef MALLOC_STATS
		if(qm->type==MEM_TYPE_PKG) {
			sr_event_exec(SREV_PKG_UPDATE_STATS, 0);
		}
#endif
		return (char*)f+sizeof(struct qm_frag);
	}
	return 0;
}



#ifdef DBG_QM_MALLOC
void qm_free(struct qm_block* qm, void* p, const char* file, const char* func, 
				unsigned int line)
#else
void qm_free(struct qm_block* qm, void* p)
#endif
{
	struct qm_frag* f;
	unsigned long size;
#ifdef MEM_JOIN_FREE
	struct qm_frag* next;
	struct qm_frag* prev;
#endif /* MEM_JOIN_FREE*/

#ifdef DBG_QM_MALLOC
	MDBG("qm_free(%p, %p), called from %s: %s(%d)\n", qm, p, file, func, line);
#endif

	if (p==0) {
#ifdef DBG_QM_MALLOC
		LOG(L_WARN, "WARNING:qm_free: free(0) called from %s: %s(%d)\n", file, func, line);
#else
		LOG(L_WARN, "WARNING:qm_free: free(0) called\n");
#endif
		return;
	}

#ifdef DBG_QM_MALLOC
	if (p>(void*)qm->last_frag_end || p<(void*)qm->first_frag){
		LOG(L_CRIT, "BUG: qm_free: bad pointer %p (out of memory block!)"
				" called from %s: %s(%d) - aborting\n", p, file, func, line);
		if(likely(cfg_get(core, core_cfg, mem_safety)==0))
			abort();
		else return;
	}
#endif

	f=(struct qm_frag*) ((char*)p-sizeof(struct qm_frag));

#ifdef DBG_QM_MALLOC
	qm_debug_frag(qm, f);
	if (f->u.is_free){
		LOG(L_CRIT, "BUG: qm_free: freeing already freed pointer (%p),"
				" called from %s: %s(%d), first free %s: %s(%ld) - aborting\n",
				p, file, func, line, f->file, f->func, f->line);
		if(likely(cfg_get(core, core_cfg, mem_safety)==0))
			abort();
		else return;
	}
	MDBG("qm_free: freeing frag. %p alloc'ed from %s: %s(%ld)\n",
			f, f->file, f->func, f->line);
#endif
	if (unlikely(f->u.is_free)){
		LM_INFO("freeing a free fragment (%p/%p) - ignore\n",
				f, p);
		return;
	}

	size=f->size;
	qm->used-=size;
	qm->real_used-=size;

#ifdef MEM_JOIN_FREE
	if(unlikely(cfg_get(core, core_cfg, mem_join)!=0)) {
		next=prev=0;
		/* mark this fragment as used (might fall into the middle of joined frags)
		  to give us an extra chance of detecting a double free call (if the joined
		  fragment has not yet been reused) */
		f->u.nxt_free=(void*)0x1L; /* bogus value, just to mark it as free */
		/* join packets if possible*/
		next=FRAG_NEXT(f);
		if (((char*)next < (char*)qm->last_frag_end) && (next->u.is_free)){
			/* join next packet */
#ifdef DBG_QM_MALLOC
			qm_debug_frag(qm, next);
#endif
			qm_detach_free(qm, next);
			size+=next->size+FRAG_OVERHEAD;
			qm->real_used-=FRAG_OVERHEAD;
			qm->free_hash[GET_HASH(next->size)].no--; /* FIXME slow */
			qm->ffrags--;
		}
	
		if (f > qm->first_frag){
			prev=FRAG_PREV(f);
			/*	(struct qm_frag*)((char*)f - (struct qm_frag_end*)((char*)f-
								sizeof(struct qm_frag_end))->size);*/
			if (prev->u.is_free){
				/* join prev packet */
#ifdef DBG_QM_MALLOC
				qm_debug_frag(qm, prev);
#endif
				qm_detach_free(qm, prev);
				size+=prev->size+FRAG_OVERHEAD;
				qm->real_used-=FRAG_OVERHEAD;
				qm->free_hash[GET_HASH(prev->size)].no--; /* FIXME slow */
				qm->ffrags--;
				f=prev;
			}
		}
		f->size=size;
		FRAG_END(f)->size=f->size;
	} /* if cfg_core->mem_join */
#endif /* MEM_JOIN_FREE*/
#ifdef DBG_QM_MALLOC
	f->file=file;
	f->func=func;
	f->line=line;
#endif
	qm_insert_free(qm, f);
#ifdef MALLOC_STATS
	if(qm->type==MEM_TYPE_PKG) {
		sr_event_exec(SREV_PKG_UPDATE_STATS, 0);
	}
#endif
}



#ifdef DBG_QM_MALLOC
void* qm_realloc(struct qm_block* qm, void* p, unsigned long size,
					const char* file, const char* func, unsigned int line)
#else
void* qm_realloc(struct qm_block* qm, void* p, unsigned long size)
#endif
{
	struct qm_frag* f;
	unsigned long diff;
	unsigned long orig_size;
	struct qm_frag* n;
	void* ptr;
	
	
#ifdef DBG_QM_MALLOC
	MDBG("qm_realloc(%p, %p, %lu) called from %s: %s(%d)\n", qm, p, size,
			file, func, line);
	if ((p)&&(p>(void*)qm->last_frag_end || p<(void*)qm->first_frag)){
		LOG(L_CRIT, "BUG: qm_free: bad pointer %p (out of memory block!) - "
				"aborting\n", p);
		abort();
	}
#endif
	
	if (size==0) {
		if (p)
#ifdef DBG_QM_MALLOC
			qm_free(qm, p, file, func, line);
#else
			qm_free(qm, p);
#endif
		return 0;
	}
	if (p==0)
#ifdef DBG_QM_MALLOC
		return qm_malloc(qm, size, file, func, line);
#else
		return qm_malloc(qm, size);
#endif
	f=(struct qm_frag*) ((char*)p-sizeof(struct qm_frag));
#ifdef DBG_QM_MALLOC
	qm_debug_frag(qm, f);
	MDBG("qm_realloc: realloc'ing frag %p alloc'ed from %s: %s(%ld)\n",
			f, f->file, f->func, f->line);
	if (f->u.is_free){
		LOG(L_CRIT, "BUG:qm_realloc: trying to realloc an already freed "
				"pointer %p , fragment %p -- aborting\n", p, f);
		abort();
	}
#endif
	/* find first acceptable size */
	size=ROUNDUP(size);
	if (f->size > size){
		orig_size=f->size;
		/* shrink */
#ifdef DBG_QM_MALLOC
		MDBG("qm_realloc: shrinking from %lu to %lu\n", f->size, size);
		if(split_frag(qm, f, size, file, "fragm. from qm_realloc", line)!=0){
		MDBG("qm_realloc : shrinked successful\n");
#else
		if(split_frag(qm, f, size)!=0){
#endif
			/* update used sizes: freed the splited frag */
			/* split frag already adds FRAG_OVERHEAD for the newly created
			   free frag, so here we only need orig_size-f->size for real used
			 */
			qm->real_used-=(orig_size-f->size);
			qm->used-=(orig_size-f->size);
		}
		
	}else if (f->size < size){
		/* grow */
#ifdef DBG_QM_MALLOC
		MDBG("qm_realloc: growing from %lu to %lu\n", f->size, size);
#endif
			orig_size=f->size;
			diff=size-f->size;
			n=FRAG_NEXT(f);
			if (((char*)n < (char*)qm->last_frag_end) && 
					(n->u.is_free)&&((n->size+FRAG_OVERHEAD)>=diff)){
				/* join  */
				qm_detach_free(qm, n);
				qm->free_hash[GET_HASH(n->size)].no--; /*FIXME: slow*/
				qm->ffrags--;
				f->size+=n->size+FRAG_OVERHEAD;
				qm->real_used-=FRAG_OVERHEAD;
				FRAG_END(f)->size=f->size;
				/* end checks should be ok */
				/* split it if necessary */
				if (f->size > size ){
	#ifdef DBG_QM_MALLOC
					split_frag(qm, f, size, file, "fragm. from qm_realloc",
										line);
	#else
					split_frag(qm, f, size);
	#endif
				}
				qm->real_used+=(f->size-orig_size);
				qm->used+=(f->size-orig_size);
			}else{
				/* could not join => realloc */
	#ifdef DBG_QM_MALLOC
				ptr=qm_malloc(qm, size, file, func, line);
	#else
				ptr=qm_malloc(qm, size);
	#endif
				if (ptr){
					/* copy, need by libssl */
					memcpy(ptr, p, orig_size);
				}
	#ifdef DBG_QM_MALLOC
				qm_free(qm, p, file, func, line);
	#else
				qm_free(qm, p);
	#endif
				p=ptr;
			}
	}else{
		/* do nothing */
#ifdef DBG_QM_MALLOC
		MDBG("qm_realloc: doing nothing, same size: %lu - %lu\n",
				f->size, size);
#endif
	}
#ifdef DBG_QM_MALLOC
	MDBG("qm_realloc: returning %p\n", p);
#endif
#ifdef MALLOC_STATS
	if(qm->type==MEM_TYPE_PKG) {
		sr_event_exec(SREV_PKG_UPDATE_STATS, 0);
	}
#endif
	return p;
}


void qm_check(struct qm_block* qm)
{
	struct qm_frag* f;
	long fcount = 0;
	int memlog;
	
	memlog=cfg_get(core, core_cfg, memlog);
	LOG(memlog, "DEBUG: qm_check()\n");
	f = qm->first_frag;
	while ((char*)f < (char*)qm->last_frag_end) {
		fcount++;
		/* check struct qm_frag */
#ifdef DBG_QM_MALLOC
		if (f->check!=ST_CHECK_PATTERN){
			LOG(L_CRIT, "BUG: qm_*: fragm. %p (address %p) "
					"beginning overwritten(%lx)!\n",
					f, (char*)f + sizeof(struct qm_frag),
					f->check);
			qm_status(qm);
			abort();
		};
#endif
		if (f + sizeof(struct qm_frag) + f->size + sizeof(struct qm_frag_end) > qm->first_frag + qm->size) {
			LOG(L_CRIT, "BUG: qm_*: fragm. %p (address %p) "
				"bad size: %lu (frag end: %p > end of block: %p)\n",
				f, (char*)f + sizeof(struct qm_frag) + sizeof(struct qm_frag_end), f->size,
				f + sizeof(struct qm_frag) + f->size, qm->first_frag + qm->size);
			qm_status(qm);
			abort();
		}
		/* check struct qm_frag_end */
		if (FRAG_END(f)->size != f->size) {
			LOG(L_CRIT, "BUG: qm_*: fragm. %p (address %p) "
				"size in qm_frag and qm_frag_end does not match: frag->size=%lu, frag_end->size=%lu)\n",
				f, (char*)f + sizeof(struct qm_frag),
				f->size, FRAG_END(f)->size);
			qm_status(qm);
			abort();
		}
#ifdef DBG_QM_MALLOC
		if ((FRAG_END(f)->check1 != END_CHECK_PATTERN1) ||
			(FRAG_END(f)->check2 != END_CHECK_PATTERN2)) {
			LOG(L_CRIT, "BUG: qm_*: fragm. %p (address %p)"
						" end overwritten(%lx, %lx)!\n",
					f, (char*)f + sizeof(struct qm_frag), 
					FRAG_END(f)->check1, FRAG_END(f)->check2);
			qm_status(qm);
			abort();
		}
#endif
		f = FRAG_NEXT(f);
	}

	LOG(memlog, "DEBUG: qm_check: %lu fragments OK\n", fcount);
}

void qm_status(struct qm_block* qm)
{
	struct qm_frag* f;
	int i,j;
	int h;
	int unused;
	int memlog;
	int mem_summary;

	memlog=cfg_get(core, core_cfg, memlog);
	mem_summary=cfg_get(core, core_cfg, mem_summary);
	LOG_(DEFAULT_FACILITY, memlog, "qm_status: ", "(%p):\n", qm);
	if (!qm) return;

	LOG_(DEFAULT_FACILITY, memlog, "qm_status: ", "heap size= %lu\n",
			qm->size);
	LOG_(DEFAULT_FACILITY, memlog, "qm_status: ",
			"used= %lu, used+overhead=%lu, free=%lu\n",
			qm->used, qm->real_used, qm->size-qm->real_used);
	LOG_(DEFAULT_FACILITY, memlog, "qm_status: ",
			"max used (+overhead)= %lu\n", qm->max_real_used);
	
	if (mem_summary & 16) return;

	LOG_(DEFAULT_FACILITY, memlog, "qm_status: ",
			"dumping all alloc'ed. fragments:\n");
	for (f=qm->first_frag, i=0;(char*)f<(char*)qm->last_frag_end;f=FRAG_NEXT(f)
			,i++){
		if (! f->u.is_free){
			LOG_(DEFAULT_FACILITY, memlog, "qm_status: ",
					"   %3d. %c  address=%p frag=%p size=%lu used=%d\n",
				i,
				(f->u.is_free)?'a':'N',
				(char*)f+sizeof(struct qm_frag), f, f->size, FRAG_WAS_USED(f));
#ifdef DBG_QM_MALLOC
			LOG_(DEFAULT_FACILITY, memlog, "qm_status: ",
					"          %s from %s: %s(%ld)\n",
				(f->u.is_free)?"freed":"alloc'd", f->file, f->func, f->line);
			LOG_(DEFAULT_FACILITY, memlog, "qm_status: ",
					"         start check=%lx, end check= %lx, %lx\n",
				f->check, FRAG_END(f)->check1, FRAG_END(f)->check2);
#endif
		}
	}
	LOG_(DEFAULT_FACILITY, memlog, "qm_status: ",
			"dumping free list stats :\n");
	for(h=0,i=0;h<QM_HASH_SIZE;h++){
		unused=0;
		for (f=qm->free_hash[h].head.u.nxt_free,j=0; 
				f!=&(qm->free_hash[h].head); f=f->u.nxt_free, i++, j++){
				if (!FRAG_WAS_USED(f)){
					unused++;
#ifdef DBG_QM_MALLOC
					LOG_(DEFAULT_FACILITY, memlog, "qm_status: ",
						"unused fragm.: hash = %3d, fragment %p,"
						" address %p size %lu, created from %s: %s(%lu)\n",
					    h, f, (char*)f+sizeof(struct qm_frag), f->size,
						f->file, f->func, f->line);
#endif
				}
		}

		if (j) LOG_(DEFAULT_FACILITY, memlog, "qm_status: ",
				"hash= %3d. fragments no.: %5d, unused: %5d\n"
					"\t\t bucket size: %9lu - %9ld (first %9lu)\n",
					h, j, unused, UN_HASH(h),
					((h<=QM_MALLOC_OPTIMIZE/ROUNDTO)?1:2)*UN_HASH(h),
					qm->free_hash[h].head.u.nxt_free->size
				);
		if (j!=qm->free_hash[h].no){
			LOG(L_CRIT, "BUG: qm_status: different free frag. count: %d!=%lu"
				" for hash %3d\n", j, qm->free_hash[h].no, h);
		}

	}
	LOG_(DEFAULT_FACILITY, memlog, "qm_status: ",
			"-----------------------------\n");
}


/* fills a malloc info structure with info about the block
 * if a parameter is not supported, it will be filled with 0 */
void qm_info(struct qm_block* qm, struct mem_info* info)
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


/* returns how much free memory is available
 * it never returns an error (unlike fm_available) */
unsigned long qm_available(struct qm_block* qm)
{
	return qm->size-qm->real_used;
}



#ifdef DBG_QM_MALLOC

typedef struct _mem_counter{
	const char *file;
	const char *func;
	unsigned long line;
	
	unsigned long size;
	int count;
	
	struct _mem_counter *next;
} mem_counter;

static mem_counter* get_mem_counter(mem_counter **root, struct qm_frag* f)
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



void qm_sums(struct qm_block* qm)
{
	struct qm_frag* f;
	int i;
	mem_counter *root, *x;
	int memlog;
	
	root=0;
	if (!qm) return;
	
	memlog=cfg_get(core, core_cfg, memlog);
	LOG_(DEFAULT_FACILITY, memlog, "qm_sums: ",
			"summarizing all alloc'ed. fragments:\n");
	
	for (f=qm->first_frag, i=0;(char*)f<(char*)qm->last_frag_end;
			f=FRAG_NEXT(f),i++){
		if (! f->u.is_free){
			x = get_mem_counter(&root,f);
			x->count++;
			x->size+=f->size;
		}
	}
	x = root;
	while(x){
		LOG_(DEFAULT_FACILITY, memlog, "qm_sums: ",
				" count=%6d size=%10lu bytes from %s: %s(%ld)\n",
			x->count,x->size,
			x->file, x->func, x->line
			);
		root = x->next;
		free(x);
		x = root;
	}
	LOG_(DEFAULT_FACILITY, memlog, "qm_sums: ",
			"-----------------------------\n");
}
#endif /* DBG_QM_MALLOC */


#endif
