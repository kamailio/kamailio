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


#if defined(Q_MALLOC)

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

#include "pkg.h"

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


#define qm_debug_frag(qm, f, file, line)	\
			qm_debug_check_frag((qm), (f), (file), (line), __FILE__, __LINE__)
static  void qm_debug_check_frag(struct qm_block* qm, struct qm_frag* f,
		const char* file, unsigned int line,
		const char* efile, unsigned int eline)
{
	if (f->check!=ST_CHECK_PATTERN){
		LM_CRIT("BUG: qm: fragm. %p (address %p) "
				"beginning overwritten (%lx)! Memory allocator was called "
				"from %s:%u. Fragment marked by %s:%lu. Exec from %s:%u.\n",
				f, (char*)f+sizeof(struct qm_frag),
				f->check, file, line, f->file, f->line, efile, eline);
		qm_status(qm);
		abort();
	};
	if ((FRAG_END(f)->check1!=END_CHECK_PATTERN1)||
			(FRAG_END(f)->check2!=END_CHECK_PATTERN2)){
		LM_CRIT("BUG: qm: fragm. %p (address %p) "
				"end overwritten (%lx, %lx)! Memory allocator was called "
				"from %s:%u. Fragment marked by %s:%lu. Exec from %s:%u.\n",
				f, (char*)f+sizeof(struct qm_frag),
				FRAG_END(f)->check1, FRAG_END(f)->check2,
				file, line, f->file, f->line, efile, eline);
		qm_status(qm);
		abort();
	}
	if ((f>qm->first_frag)&&
			((PREV_FRAG_END(f)->check1!=END_CHECK_PATTERN1) ||
				(PREV_FRAG_END(f)->check2!=END_CHECK_PATTERN2) ) ){
		LM_CRIT("BUG: qm: prev. fragm. tail overwritten(%lx, %lx)[%p:%p]! "
				"Memory allocator was called from %s:%u. Fragment marked by "
				"%s:%lu. Exec from %s:%u.\n",
				PREV_FRAG_END(f)->check1, PREV_FRAG_END(f)->check2, f,
				(char*)f+sizeof(struct qm_frag), file, line, f->file, f->line,
				efile, eline);
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
	LM_DBG("QM_OPTIMIZE=%lu, /ROUNDTO=%lu\n",
			QM_MALLOC_OPTIMIZE, QM_MALLOC_OPTIMIZE/ROUNDTO);
	LM_DBG("QM_HASH_SIZE=%lu, qm_block size=%lu\n",
			QM_HASH_SIZE, (unsigned long)sizeof(struct qm_block));
	LM_DBG("qm_malloc_init(%p, %lu), start=%p\n", address,
			(unsigned long)size, start);
	if (size<start-address) return 0;
	size-=(start-address);
	if (size <(MIN_FRAG_SIZE+FRAG_OVERHEAD)) return 0;
	size=ROUNDDOWN(size);

	init_overhead=ROUNDUP(sizeof(struct qm_block))+sizeof(struct qm_frag)+
		sizeof(struct qm_frag_end);
	LM_DBG("size= %lu, init_overhead=%lu\n",
			(unsigned long)size, init_overhead);

	if (size < init_overhead)
	{
		/* not enough mem to create our control structures !!!*/
		LM_ERR("qm_malloc_init(%lu); No memory left to create control structures\n",
				(unsigned long)size);
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
	 * qm->last_frag_end->prev_free=&(qm->free_lst);
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
				size_t size, int *h, unsigned int *count)
#else
static inline struct qm_frag* qm_find_free(struct qm_block* qm,
				size_t size, int* h)
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
	LM_ERR("qm_find_free(%p, %lu); Free fragment not found!\n",
			qm, (unsigned long)size);
	return 0;
}


/* returns 0 on success, -1 on error;
 * new_size < size & rounded-up already!*/
static inline
#ifdef DBG_QM_MALLOC
int split_frag(struct qm_block* qm, struct qm_frag* f, size_t new_size,
				const char* file, const char* func, unsigned int line, const char *mname)
#else
int split_frag(struct qm_block* qm, struct qm_frag* f, size_t new_size)
#endif
{
	size_t rest;
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
		n->mname=mname;
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
void* qm_malloc(void* qmp, size_t size,
			const char* file, const char* func, unsigned int line,
			const char *mname)
#else
void* qm_malloc(void* qmp, size_t size)
#endif
{
	struct qm_block* qm;
	struct qm_frag* f;
	int hash;
#ifdef DBG_QM_MALLOC
	unsigned int list_cntr;
#endif

	qm = (struct qm_block*)qmp;

#ifdef DBG_QM_MALLOC
	list_cntr = 0;
	MDBG("qm_malloc(%p, %lu) called from %s: %s(%d)\n",
			qm, (unsigned long)size, file, func, line);
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
			qm_debug_frag(qm, f, file, line);
#endif
		qm_detach_free(qm, f);
		/*mark it as "busy"*/
		f->u.is_free=0;
		qm->free_hash[hash].no--;
		qm->ffrags--;
		/* we ignore split return */
#ifdef DBG_QM_MALLOC
		split_frag(qm, f, size, file, "fragm. from qm_malloc", line, mname);
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
		f->mname=mname;
		f->line=line;
		f->check=ST_CHECK_PATTERN;
		/*  FRAG_END(f)->check1=END_CHECK_PATTERN1;
			FRAG_END(f)->check2=END_CHECK_PATTERN2;*/
		MDBG("qm_malloc(%p, %lu) returns address %p frag. %p (size=%lu) on %d"
				" -th hit\n",
				qm, (unsigned long)size, (char*)f+sizeof(struct qm_frag), f,
				f->size, list_cntr);
#endif
#ifdef MALLOC_STATS
		if(qm->type==MEM_TYPE_PKG) {
			sr_event_exec(SREV_PKG_UPDATE_STATS, 0);
		}
#endif
		return (char*)f+sizeof(struct qm_frag);
	}

#ifdef DBG_QM_MALLOC
	LM_ERR("qm_malloc(%p, %lu) called from %s: %s(%d), module: %s;"
			" Free fragment not found!\n",
			qm, (unsigned long)size, file, func, line, mname);
#else
	LM_ERR("qm_malloc(%p, %lu); Free fragment not found!\n",
			qm, (unsigned long)size);
#endif

	return 0;
}


#ifdef DBG_QM_MALLOC
void* qm_mallocxz(void* qmp, size_t size,
			const char* file, const char* func, unsigned int line,
			const char *mname)
#else
void* qm_mallocxz(void* qmp, size_t size)
#endif
{
	void *p;

#ifdef DBG_QM_MALLOC
	p = qm_malloc(qmp, size, file, func, line, mname);
#else
	p = qm_malloc(qmp, size);
#endif

	if(p) memset(p, 0, size);

	return p;
}


#ifdef DBG_QM_MALLOC
void qm_free(void* qmp, void* p, const char* file, const char* func,
			unsigned int line, const char *mname)
#else
void qm_free(void* qmp, void* p)
#endif
{
	struct qm_block* qm;
	struct qm_frag* f;
	size_t size;
#ifdef MEM_JOIN_FREE
	struct qm_frag* next;
	struct qm_frag* prev;
#endif /* MEM_JOIN_FREE*/

	qm = (struct qm_block*)qmp;

#ifdef DBG_QM_MALLOC
	MDBG("qm_free(%p, %p), called from %s: %s(%d)\n", qm, p, file, func, line);
#endif

	if (p==0) {
#ifdef DBG_QM_MALLOC
		LM_WARN("WARNING: free(0) called from %s: %s(%d)\n", file, func, line);
#else
		LM_WARN("WARNING: free(0) called\n");
#endif
		return;
	}

#ifdef DBG_QM_MALLOC
	if (p>(void*)qm->last_frag_end || p<(void*)qm->first_frag){
		LM_CRIT("BUG: bad pointer %p (out of memory block!)"
				" called from %s: %s(%d) - aborting\n", p, file, func, line);
		if(likely(cfg_get(core, core_cfg, mem_safety)==0))
			abort();
		else return;
	}
#endif

	f=(struct qm_frag*) ((char*)p-sizeof(struct qm_frag));

#ifdef DBG_QM_MALLOC
	qm_debug_frag(qm, f, file, line);
	if (f->u.is_free){
		LM_CRIT("BUG: freeing already freed pointer (%p),"
				" called from %s: %s(%d), first free %s: %s(%ld) - aborting\n",
				p, file, func, line, f->file, f->func, f->line);
		if(likely(cfg_get(core, core_cfg, mem_safety)==0))
			abort();
		else return;
	}
	MDBG("freeing frag. %p alloc'ed from %s: %s(%ld)\n",
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
		 * to give us an extra chance of detecting a double free call (if the joined
		 * fragment has not yet been reused) */
		f->u.nxt_free=(void*)0x1L; /* bogus value, just to mark it as free */
		/* join packets if possible*/
		next=FRAG_NEXT(f);
		if (((char*)next < (char*)qm->last_frag_end) && (next->u.is_free)){
			/* join next packet */
#ifdef DBG_QM_MALLOC
			qm_debug_frag(qm, next, file, line);
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
				qm_debug_frag(qm, prev, file, line);
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
	f->mname=mname;
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
void* qm_realloc(void* qmp, void* p, size_t size,
					const char* file, const char* func, unsigned int line,
					const char *mname)
#else
void* qm_realloc(void* qmp, void* p, size_t size)
#endif
{
	struct qm_block* qm;
	struct qm_frag* f;
	size_t diff;
	size_t orig_size;
	struct qm_frag* n;
	void* ptr;

	qm = (struct qm_block*)qmp;

#ifdef DBG_QM_MALLOC
	MDBG("qm_realloc(%p, %p, %lu) called from %s: %s(%d)\n",
			qm, p, (unsigned long)size,
			file, func, line);
	if ((p)&&(p>(void*)qm->last_frag_end || p<(void*)qm->first_frag)) {
		LM_CRIT("BUG: bad pointer %p (out of memory block!) - "
				"aborting\n", p);
		abort();
	}
#endif

	if (size==0) {
		if (p) {
#ifdef DBG_QM_MALLOC
			qm_free(qm, p, file, func, line, mname);
#else
			qm_free(qm, p);
#endif
		}
		return 0;
	}
	if (p==0)
#ifdef DBG_QM_MALLOC
		return qm_malloc(qm, size, file, func, line, mname);
#else
		return qm_malloc(qm, size);
#endif
	f=(struct qm_frag*) ((char*)p-sizeof(struct qm_frag));
#ifdef DBG_QM_MALLOC
	qm_debug_frag(qm, f, file, line);
	MDBG("realloc'ing frag %p alloc'ed from %s: %s(%ld)\n",
			f, f->file, f->func, f->line);
	if (f->u.is_free){
		LM_CRIT("BUG: trying to realloc an already freed "
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
		MDBG("shrinking from %lu to %lu\n",
				f->size, (unsigned long)size);
		if(split_frag(qm, f, size, file, "fragm. from qm_realloc", line, mname)!=0){
		MDBG("shrinked successful\n");
#else
		if(split_frag(qm, f, size)!=0){
#endif
			/* update used sizes: freed the splited frag */
			/* split frag already adds FRAG_OVERHEAD for the newly created
			 * free frag, so here we only need orig_size-f->size for real used
			 */
			qm->real_used-=(orig_size-f->size);
			qm->used-=(orig_size-f->size);
		}

	}else if (f->size < size){
		/* grow */
#ifdef DBG_QM_MALLOC
		MDBG("growing from %lu to %lu\n",
				f->size, (unsigned long)size);
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
										line, mname);
	#else
					split_frag(qm, f, size);
	#endif
				}
				qm->real_used+=(f->size-orig_size);
				qm->used+=(f->size-orig_size);
			}else{
				/* could not join => realloc */
	#ifdef DBG_QM_MALLOC
				ptr=qm_malloc(qm, size, file, func, line, mname);
	#else
				ptr=qm_malloc(qm, size);
	#endif
				if (ptr){
					/* copy old content */
					memcpy(ptr, p, orig_size);
					/* free old pointer */
		#ifdef DBG_QM_MALLOC
					qm_free(qm, p, file, func, line, mname);
		#else
					qm_free(qm, p);
		#endif
				} else {
#ifdef DBG_QM_MALLOC
					LM_ERR("qm_realloc(%p, %lu) called from %s: %s(%d),"
							" module: %s; qm_malloc() failed!\n",
							qm, (unsigned long)size, file, func, line, mname);
#else
					LM_ERR("qm_realloc(%p, %lu); qm_malloc() failed!\n",
							qm, (unsigned long)size);
#endif
				}
				p=ptr;
			}
	}else{
		/* do nothing */
#ifdef DBG_QM_MALLOC
		MDBG("doing nothing, same size: %lu - %lu\n",
				f->size, (unsigned long)size);
#endif
	}
#ifdef DBG_QM_MALLOC
	MDBG("returning pointer address: %p\n", p);
#endif
#ifdef MALLOC_STATS
	if(qm->type==MEM_TYPE_PKG) {
		sr_event_exec(SREV_PKG_UPDATE_STATS, 0);
	}
#endif
	return p;
}


#ifdef DBG_QM_MALLOC
void* qm_reallocxf(void* qmp, void* p, size_t size,
					const char* file, const char* func, unsigned int line,
					const char *mname)
#else
void* qm_reallocxf(void* qmp, void* p, size_t size)
#endif
{
	void *r;

#ifdef DBG_QM_MALLOC
	r = qm_realloc(qmp, p, size, file, func, line, mname);
#else
	r = qm_realloc(qmp, p, size);
#endif

	if(!r && p) {
	#ifdef DBG_QM_MALLOC
		qm_free(qmp, p, file, func, line, mname);
	#else
		qm_free(qmp, p);
	#endif

	}

	return r;
}


void qm_check(struct qm_block* qm)
{
	struct qm_frag* f;
	long fcount = 0;
	int memlog;

	memlog=cfg_get(core, core_cfg, memlog);
	LOG(memlog, "executing qm_check()\n");
	f = qm->first_frag;
	while ((char*)f < (char*)qm->last_frag_end) {
		fcount++;
		/* check struct qm_frag */
#ifdef DBG_QM_MALLOC
		if (f->check!=ST_CHECK_PATTERN){
			LM_CRIT("BUG: qm: fragm. %p (address %p) "
					"beginning overwritten(%lx)!\n",
					f, (char*)f + sizeof(struct qm_frag),
					f->check);
			qm_status(qm);
			abort();
		};
#endif
		if ((char*)f + sizeof(struct qm_frag) + f->size
				+ sizeof(struct qm_frag_end) > (char*)qm->first_frag + qm->size) {
			LM_CRIT("BUG: qm: fragm. %p (address %p) "
				"bad size: %lu (frag end: %p > end of block: %p)\n",
				f, (char*)f + sizeof(struct qm_frag), f->size,
				(char*)f + sizeof(struct qm_frag) + f->size
					+ sizeof(struct qm_frag_end),
				(char*)qm->first_frag + qm->size);
			qm_status(qm);
			abort();
		}
		/* check struct qm_frag_end */
		if (FRAG_END(f)->size != f->size) {
			LM_CRIT("BUG: qm: fragm. %p (address %p) "
					"size in qm_frag and qm_frag_end does not match:"
					" frag->size=%lu, frag_end->size=%lu)\n",
				f, (char*)f + sizeof(struct qm_frag),
				f->size, FRAG_END(f)->size);
			qm_status(qm);
			abort();
		}
#ifdef DBG_QM_MALLOC
		if ((FRAG_END(f)->check1 != END_CHECK_PATTERN1) ||
			(FRAG_END(f)->check2 != END_CHECK_PATTERN2)) {
			LM_CRIT("BUG: qm: fragm. %p (address %p)"
						" end overwritten(%lx, %lx)!\n",
					f, (char*)f + sizeof(struct qm_frag),
					FRAG_END(f)->check1, FRAG_END(f)->check2);
			qm_status(qm);
			abort();
		}
#endif
		f = FRAG_NEXT(f);
	}

	LOG(memlog, "summary of qm_check: %lu fragments OK\n", fcount);
}

void qm_status(void* qmp)
{
	struct qm_block* qm;
	struct qm_frag* f;
	int i,j;
	int h;
	int unused;
	int memlog;
	int mem_summary;

	qm = (struct qm_block*)qmp;

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
		if ((! f->u.is_free) || (cfg_get(core, core_cfg, mem_status_mode)!=0)){
			LOG_(DEFAULT_FACILITY, memlog, "qm_status: ",
					"   %3d. %c  address=%p frag=%p size=%lu used=%d\n",
				i,
				(f->u.is_free)?'A':'N',
				(char*)f+sizeof(struct qm_frag), f, f->size, FRAG_WAS_USED(f));
#ifdef DBG_QM_MALLOC
			LOG_(DEFAULT_FACILITY, memlog, "qm_status: ",
					"          %s from %s: %s(%ld)\n",
				(f->u.is_free)?"freed":"alloc'd", f->file, f->func, f->line);
			LOG_(DEFAULT_FACILITY, memlog, "qm_status: ",
					"         start check=%lx, end check= %lx, %lx\n",
				f->check, FRAG_END(f)->check1, FRAG_END(f)->check2);
			if (f->check!=ST_CHECK_PATTERN){
				LOG_(DEFAULT_FACILITY, memlog, "qm_status: ",
						"         * beginning overwritten(%lx)!\n",
						f->check);
			}
			if ((FRAG_END(f)->check1 != END_CHECK_PATTERN1)
					|| (FRAG_END(f)->check2 != END_CHECK_PATTERN2)) {
				LOG_(DEFAULT_FACILITY, memlog, "qm_status: ",
						"         * end overwritten(%lx, %lx)!\n",
						FRAG_END(f)->check1, FRAG_END(f)->check2);
			}

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
void qm_info(void* qmp, struct mem_info* info)
{
	struct qm_block* qm;

	qm = (struct qm_block*)qmp;

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
unsigned long qm_available(void* qmp)
{
	struct qm_block* qm;

	qm = (struct qm_block*)qmp;

	return qm->size-qm->real_used;
}



#ifdef DBG_QM_MALLOC


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
	x->mname= f->mname;
	x->line = f->line;
	x->count = 0;
	x->size = 0;
	x->next = *root;
	*root = x;
	return x;
}



void qm_sums(void* qmp)
{
	struct qm_block* qm;
	struct qm_frag* f;
	int i;
	mem_counter *root, *x;
	int memlog;

	qm = (struct qm_block*)qmp;

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

void qm_mod_get_stats(void *qmp, void **qm_rootp)
{
	if (!qm_rootp) {
		return;
	}

	LM_DBG("get qm memory statistics\n");

	struct qm_block *qm = (struct qm_block *) qmp;
	mem_counter **qm_root = (mem_counter **) qm_rootp;
	struct qm_frag* f;
	int i;
	mem_counter *x;

	if (!qm) return ;

	/* update fragment detail list */
	for (f=qm->first_frag, i=0; (char*)f<(char*)qm->last_frag_end;
		f=FRAG_NEXT(f), i++){
		if (! f->u.is_free){
			x = get_mem_counter(qm_root,f);
			x->count++;
			x->size+=f->size;
		}
	}

	return ;
}

void qm_mod_free_stats(void *qm_rootp)
{
	if (!qm_rootp) {
		return;
	}

	LM_DBG("free qm memory statistics\n");

	mem_counter *root = (mem_counter *) qm_rootp;
	mem_counter *new, *old;
	new = root;
	old = root;

	while (new) {
		old = new;
		new = new->next;
		free(old);
	}
}
#else

void qm_sums(void *qmp)
{
	return;
}

void qm_mod_get_stats(void *qmp, void **qm_rootp)
{
	LM_WARN("Enable DBG_QM_MALLOC for getting statistics\n");
	return;
}

void qm_mod_free_stats(void *qm_rootp)
{
	LM_WARN("Enable DBG_QM_MALLOC for freeing statistics\n");
	return;
}
#endif /* DBG_QM_MALLOC */


/*memory manager core api*/
static char *_qm_mem_name = "q_malloc";

/* PKG - private memory API*/
static char *_qm_pkg_pool = 0;
static struct qm_block *_qm_pkg_block = 0;

/**
 * \brief Destroy memory pool
 */
void qm_malloc_destroy_pkg_manager(void)
{
	if (_qm_pkg_pool) {
		free(_qm_pkg_pool);
		_qm_pkg_pool = 0;
	}
	_qm_pkg_block = 0;
}

/**
 * \brief Init memory pool
 */
int qm_malloc_init_pkg_manager(void)
{
	sr_pkg_api_t ma;
	_qm_pkg_pool = malloc(pkg_mem_size);
	if (_qm_pkg_pool)
		_qm_pkg_block=qm_malloc_init(_qm_pkg_pool, pkg_mem_size, MEM_TYPE_PKG);
	if (_qm_pkg_block==0){
		LM_CRIT("could not initialize qm memory pool\n");
		fprintf(stderr, "Too much qm pkg memory demanded: %ld bytes\n",
						pkg_mem_size);
		return -1;
	}

	memset(&ma, 0, sizeof(sr_pkg_api_t));
	ma.mname = _qm_mem_name;
	ma.mem_pool = _qm_pkg_pool;
	ma.mem_block = _qm_pkg_block;
	ma.xmalloc = qm_malloc;
	ma.xmallocxz = qm_mallocxz;
	ma.xfree = qm_free;
	ma.xrealloc = qm_realloc;
	ma.xreallocxf = qm_reallocxf;
	ma.xstatus = qm_status;
	ma.xinfo = qm_info;
	ma.xavailable = qm_available;
	ma.xsums = qm_sums;
	ma.xdestroy = qm_malloc_destroy_pkg_manager;
	ma.xmodstats = qm_mod_get_stats;
	ma.xfmodstats = qm_mod_free_stats;

	return pkg_init_api(&ma);
}


/* SHM - shared memory API*/
static void *_qm_shm_pool = 0;
static struct qm_block *_qm_shm_block = 0;
static gen_lock_t* _qm_shm_lock = 0;

#define qm_shm_lock()    lock_get(_qm_shm_lock)
#define qm_shm_unlock()  lock_release(_qm_shm_lock)

/**
 *
 */
void qm_shm_glock(void* qmp)
{
	lock_get(_qm_shm_lock);
}

/**
 *
 */
void qm_shm_gunlock(void* qmp)
{
	lock_release(_qm_shm_lock);
}

/**
 *
 */
void qm_shm_lock_destroy(void)
{
	if (_qm_shm_lock){
		DBG("destroying the shared memory lock\n");
		lock_destroy(_qm_shm_lock); /* we don't need to dealloc it*/
	}
}

/**
 * init the core lock
 */
int qm_shm_lock_init(void)
{
	if (_qm_shm_lock) {
		LM_DBG("shared memory lock initialized\n");
		return 0;
	}

#ifdef DBG_QM_MALLOC
	_qm_shm_lock = qm_malloc(_qm_shm_block, sizeof(gen_lock_t),
					_SRC_LOC_, _SRC_FUNCTION_, _SRC_LINE_, _SRC_MODULE_);
#else
	_qm_shm_lock = qm_malloc(_qm_shm_block, sizeof(gen_lock_t));
#endif

	if (_qm_shm_lock==0){
		LOG(L_CRIT, "could not allocate lock\n");
		return -1;
	}
	if (lock_init(_qm_shm_lock)==0){
		LOG(L_CRIT, "could not initialize lock\n");
		return -1;
	}
	return 0;
}

/*SHM wrappers to sync the access to memory block*/
#ifdef DBG_QM_MALLOC
void* qm_shm_malloc(void* qmp, size_t size,
		const char* file, const char* func, unsigned int line,
		const char* mname)
{
	void *r;
	qm_shm_lock();
	r = qm_malloc(qmp, size, file, func, line, mname);
	qm_shm_unlock();
	return r;
}
void* qm_shm_mallocxz(void* qmp, size_t size,
		const char* file, const char* func, unsigned int line,
		const char* mname)
{
	void *r;
	qm_shm_lock();
	r = qm_mallocxz(qmp, size, file, func, line, mname);
	qm_shm_unlock();
	return r;
}
void* qm_shm_realloc(void* qmp, void* p, size_t size,
		const char* file, const char* func, unsigned int line,
		const char* mname)
{
	void *r;
	qm_shm_lock();
	r = qm_realloc(qmp, p, size, file, func, line, mname);
	qm_shm_unlock();
	return r;
}
void* qm_shm_reallocxf(void* qmp, void* p, size_t size,
		const char* file, const char* func, unsigned int line,
		const char* mname)
{
	void *r;
	qm_shm_lock();
	r = qm_reallocxf(qmp, p, size, file, func, line, mname);
	qm_shm_unlock();
	return r;
}
void* qm_shm_resize(void* qmp, void* p, size_t size,
		const char* file, const char* func, unsigned int line,
		const char* mname)
{
	void *r;
	qm_shm_lock();
	if(p) qm_free(qmp, p, file, func, line, mname);
	r = qm_malloc(qmp, size, file, func, line, mname);
	qm_shm_unlock();
	return r;
}
void qm_shm_free(void* qmp, void* p, const char* file, const char* func,
		unsigned int line, const char* mname)
{
	qm_shm_lock();
	qm_free(qmp, p, file, func, line, mname);
	qm_shm_unlock();
}
#else
void* qm_shm_malloc(void* qmp, size_t size)
{
	void *r;
	qm_shm_lock();
	r = qm_malloc(qmp, size);
	qm_shm_unlock();
	return r;
}
void* qm_shm_mallocxz(void* qmp, size_t size)
{
	void *r;
	qm_shm_lock();
	r = qm_mallocxz(qmp, size);
	qm_shm_unlock();
	return r;
}
void* qm_shm_realloc(void* qmp, void* p, size_t size)
{
	void *r;
	qm_shm_lock();
	r = qm_realloc(qmp, p, size);
	qm_shm_unlock();
	return r;
}
void* qm_shm_reallocxf(void* qmp, void* p, size_t size)
{
	void *r;
	qm_shm_lock();
	r = qm_reallocxf(qmp, p, size);
	qm_shm_unlock();
	return r;
}
void* qm_shm_resize(void* qmp, void* p, size_t size)
{
	void *r;
	qm_shm_lock();
	if(p) qm_free(qmp, p);
	r = qm_malloc(qmp, size);
	qm_shm_unlock();
	return r;
}
void qm_shm_free(void* qmp, void* p)
{
	qm_shm_lock();
	qm_free(qmp, p);
	qm_shm_unlock();
}
#endif
void qm_shm_status(void* qmp)
{
	qm_shm_lock();
	qm_status(qmp);
	qm_shm_unlock();
}
void qm_shm_info(void* qmp, struct mem_info* info)
{
	qm_shm_lock();
	qm_info(qmp, info);
	qm_shm_unlock();
}
unsigned long qm_shm_available(void* qmp)
{
	unsigned long r;
	qm_shm_lock();
	r = qm_available(qmp);
	qm_shm_unlock();
	return r;
}
void qm_shm_sums(void* qmp)
{
	qm_shm_lock();
	qm_sums(qmp);
	qm_shm_unlock();
}


/**
 * \brief Destroy memory pool
 */
void qm_malloc_destroy_shm_manager(void)
{
	qm_shm_lock_destroy();
	/*shm pool from core - nothing to do*/
	_qm_shm_pool = 0;
	_qm_shm_block = 0;
}

/**
 * \brief Init memory pool
 */
int qm_malloc_init_shm_manager(void)
{
	sr_shm_api_t ma;
	_qm_shm_pool = shm_core_get_pool();
	if (_qm_shm_pool)
		_qm_shm_block=qm_malloc_init(_qm_shm_pool, shm_mem_size, MEM_TYPE_SHM);
	if (_qm_shm_block==0){
		LM_CRIT("could not initialize qm shm memory pool\n");
		fprintf(stderr, "Too much qm shm memory demanded: %ld bytes\n",
						shm_mem_size);
		return -1;
	}

	memset(&ma, 0, sizeof(sr_shm_api_t));
	ma.mname          = _qm_mem_name;
	ma.mem_pool       = _qm_shm_pool;
	ma.mem_block      = _qm_shm_block;
	ma.xmalloc        = qm_shm_malloc;
	ma.xmallocxz      = qm_shm_mallocxz;
	ma.xmalloc_unsafe = qm_malloc;
	ma.xfree          = qm_shm_free;
	ma.xfree_unsafe   = qm_free;
	ma.xrealloc       = qm_shm_realloc;
	ma.xreallocxf     = qm_shm_reallocxf;
	ma.xresize        = qm_shm_resize;
	ma.xstatus        = qm_shm_status;
	ma.xinfo          = qm_shm_info;
	ma.xavailable     = qm_shm_available;
	ma.xsums          = qm_shm_sums;
	ma.xdestroy       = qm_malloc_destroy_shm_manager;
	ma.xmodstats      = qm_mod_get_stats;
	ma.xfmodstats     = qm_mod_free_stats;
	ma.xglock         = qm_shm_glock;
	ma.xgunlock       = qm_shm_gunlock;

	if(shm_init_api(&ma)<0) {
		LM_ERR("cannot initialize the core shm api\n");
		return -1;
	}
	if(qm_shm_lock_init()<0) {
		LM_ERR("cannot initialize the core shm lock\n");
		return -1;
	}
	return 0;
}

#endif
