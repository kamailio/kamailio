/* $Id$
 *
 */

#if !defined(q_malloc) && !(defined VQ_MALLOC) && !(defined F_MALLOC)
#define q_malloc

#include <stdlib.h>
#include <string.h>

#include "q_malloc.h"
#include "../dprint.h"
#include "../globals.h"


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

/*
#define ROUNDUP(s)		(((s)%ROUNDTO)?((s)+ROUNDTO)/ROUNDTO*ROUNDTO:(s))
#define ROUNDDOWN(s)	(((s)%ROUNDTO)?((s)-ROUNDTO)/ROUNDTO*ROUNDTO:(s))
*/



	/* finds the hash value for s, s=ROUNDTO multiple*/
#define GET_HASH(s)   ( ((s)<QM_MALLOC_OPTIMIZE)?(s)/ROUNDTO: \
						QM_MALLOC_OPTIMIZE/ROUNDTO+big_hash_idx((s))- \
							QM_MALLOC_OPTIMIZE_FACTOR+1 )


/* computes hash number for big buckets*/
inline static int big_hash_idx(int s)
{
	int idx;
	/* s is rounded => s = k*2^n (ROUNDTO=2^n) 
	 * index= i such that 2^i > s >= 2^(i-1)
	 *
	 * => index = number of the first non null bit in s*/
	for (idx=31; !(s&0x80000000) ; s<<=1, idx--);
	return idx;
}


#ifdef DBG_QM_MALLOC
#define ST_CHECK_PATTERN   0xf0f0f0f0
#define END_CHECK_PATTERN1 0xc0c0c0c0
#define END_CHECK_PATTERN2 0xabcdefed


static  void qm_debug_frag(struct qm_block* qm, struct qm_frag* f)
{
	if (f->check!=ST_CHECK_PATTERN){
		LOG(L_CRIT, "BUG: qm_*: fragm. %p beginning overwritten(%x)!\n",
				f, f->check);
		qm_status(qm);
		abort();
	};
	if ((FRAG_END(f)->check1!=END_CHECK_PATTERN1)||
		(FRAG_END(f)->check2!=END_CHECK_PATTERN2)){
		LOG(L_CRIT, "BUG: qm_*: fragm. %p end overwritten(%x, %x)!\n",
				f, FRAG_END(f)->check1, FRAG_END(f)->check2);
		qm_status(qm);
		abort();
	}
	if ((f>qm->first_frag)&&
			((PREV_FRAG_END(f)->check1!=END_CHECK_PATTERN1) ||
				(PREV_FRAG_END(f)->check2!=END_CHECK_PATTERN2) ) ){
		LOG(L_CRIT, "BUG: qm_*: prev. fragm. tail overwritten(%x, %x)[%p]!\n",
				PREV_FRAG_END(f)->check1, PREV_FRAG_END(f)->check2, f);
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
}



/* init malloc and return a qm_block*/
struct qm_block* qm_malloc_init(char* address, unsigned int size)
{
	char* start;
	char* end;
	struct qm_block* qm;
	unsigned int init_overhead;
	int h;
	
	/* make address and size multiple of 8*/
	start=(char*)ROUNDUP((unsigned int) address);
	DBG("qm_malloc_init: QM_OPTIMIZE=%d, /ROUNDTO=%d\n",
			QM_MALLOC_OPTIMIZE, QM_MALLOC_OPTIMIZE/ROUNDTO);
	DBG("qm_malloc_init: QM_HASH_SIZE=%d, qm_block size=%d\n",
			QM_HASH_SIZE, sizeof(struct qm_block));
	DBG("qm_malloc_init(%p, %d), start=%p\n", address, size, start);
	if (size<start-address) return 0;
	size-=(start-address);
	if (size <(MIN_FRAG_SIZE+FRAG_OVERHEAD)) return 0;
	size=ROUNDDOWN(size);
	
	init_overhead=ROUNDUP(sizeof(struct qm_block))+sizeof(struct qm_frag)+
		sizeof(struct qm_frag_end);
	DBG("qm_malloc_init: size= %d, init_overhead=%d\n", size, init_overhead);
	
	if (size < init_overhead)
	{
		/* not enough mem to create our control structures !!!*/
		return 0;
	}
	end=start+size;
	qm=(struct qm_block*)start;
	memset(qm, 0, sizeof(struct qm_block));
	size-=init_overhead;
	qm->size=size;
	qm->real_used=init_overhead;
	qm->max_real_used=qm->real_used;
	
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



static inline struct qm_frag* qm_find_free(struct qm_block* qm, 
										unsigned int size)
{
	int hash;
	struct qm_frag* f;

	for (hash=GET_HASH(size); hash<QM_HASH_SIZE; hash++){
		for (f=qm->free_hash[hash].head.u.nxt_free; 
					f!=&(qm->free_hash[hash].head); f=f->u.nxt_free){
			if (f->size>=size) return f;
		}
	/*try in a bigger bucket*/
	}
	/* not found */
	return 0;
}



#ifdef DBG_QM_MALLOC
void* qm_malloc(struct qm_block* qm, unsigned int size, char* file, char* func,
					unsigned int line)
#else
void* qm_malloc(struct qm_block* qm, unsigned int size)
#endif
{
	struct qm_frag* f;
	struct qm_frag_end* end;
	struct qm_frag* n;
	unsigned int rest;
	
#ifdef DBG_QM_MALLOC
	unsigned int list_cntr;

	list_cntr = 0;
	DBG("qm_malloc(%p, %d) called from %s: %s(%d)\n", qm, size, file, func,
			line);
#endif
	/*size must be a multiple of 8*/
	size=ROUNDUP(size);
	if (size>(qm->size-qm->real_used)) return 0;

	/*search for a suitable free frag*/
	if ((f=qm_find_free(qm, size))!=0){
		/* we found it!*/
		/*detach it from the free list*/
#ifdef DBG_QM_MALLOC
			qm_debug_frag(qm, f);
#endif
		qm_detach_free(qm, f);
		/*mark it as "busy"*/
		f->u.is_free=0;
		
		/*see if we'll use full frag, or we'll split it in 2*/
		rest=f->size-size;
		if (rest>(FRAG_OVERHEAD+MIN_FRAG_SIZE)){
			f->size=size;
			/*split the fragment*/
			end=FRAG_END(f);
			end->size=size;
			n=(struct qm_frag*)((char*)end+sizeof(struct qm_frag_end));
			n->size=rest-FRAG_OVERHEAD;
			FRAG_END(n)->size=n->size;
			qm->real_used+=FRAG_OVERHEAD;
#ifdef DBG_QM_MALLOC
			end->check1=END_CHECK_PATTERN1;
			end->check2=END_CHECK_PATTERN2;
			/* frag created by malloc, mark it*/
			n->file=file;
			n->func="frag. from qm_malloc";
			n->line=line;
			n->check=ST_CHECK_PATTERN;
/*			FRAG_END(n)->check1=END_CHECK_PATTERN1;
			FRAG_END(n)->check2=END_CHECK_PATTERN2; */
#endif
			/* reinsert n in free list*/
			qm_insert_free(qm, n);
		}else{
			/* we cannot split this fragment any more => alloc all of it*/
		}
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
		DBG("qm_malloc(%p, %d) returns address %p on %d -th hit\n", qm, size,
			(char*)f+sizeof(struct qm_frag), list_cntr );
#endif
		return (char*)f+sizeof(struct qm_frag);
	}
	return 0;
}



#ifdef DBG_QM_MALLOC
void qm_free(struct qm_block* qm, void* p, char* file, char* func, 
				unsigned int line)
#else
void qm_free(struct qm_block* qm, void* p)
#endif
{
	struct qm_frag* f;
	struct qm_frag* prev;
	struct qm_frag* next;
	unsigned int size;

#ifdef DBG_QM_MALLOC
	DBG("qm_free(%p, %p), called from %s: %s(%d)\n", qm, p, file, func, line);
	if (p>(void*)qm->last_frag_end || p<(void*)qm->first_frag){
		LOG(L_CRIT, "BUG: qm_free: bad pointer %p (out of memory block!) - "
				"aborting\n", p);
		abort();
	}
#endif
	if (p==0) {
		LOG(L_WARN, "WARNING:qm_free: free(0) called\n");
		return;
	}
	prev=next=0;
	f=(struct qm_frag*) ((char*)p-sizeof(struct qm_frag));
#ifdef DBG_QM_MALLOC
	qm_debug_frag(qm, f);
	if (f->u.is_free){
		LOG(L_CRIT, "BUG: qm_free: freeing already freed pointer,"
				" first free: %s: %s(%d) - aborting\n",
				f->file, f->func, f->line);
		abort();
	}
	DBG("qm_free: freeing block alloc'ed from %s: %s(%d)\n", f->file, f->func,
			f->line);
#endif
	size=f->size;
	qm->used-=size;
	qm->real_used-=size;
#ifdef DBG_QM_MALLOC
	qm_debug_frag(qm, f);
#endif

#ifdef QM_JOIN_FREE
	/* join packets if possible*/
	next=FRAG_NEXT(f);
	if (((char*)next < (char*)qm->last_frag_end) &&( next->u.is_free)){
		/* join */
		qm_detach_free(qm, next);
		size+=next->size+FRAG_OVERHEAD;
		qm->real_used-=FRAG_OVERHEAD;
	}
	
	if (f > qm->first_frag){
		prev=FRAG_PREV(f);
		/*	(struct qm_frag*)((char*)f - (struct qm_frag_end*)((char*)f-
								sizeof(struct qm_frag_end))->size);*/
#ifdef DBG_QM_MALLOC
		qm_debug_frag(qm, f);
#endif
		if (prev->u.is_free){
			/*join*/
			qm_detach_free(qm, prev);
			size+=prev->size+FRAG_OVERHEAD;
			qm->real_used-=FRAG_OVERHEAD;
			f=prev;
		}
	}
	f->size=size;
	FRAG_END(f)->size=f->size;
#endif /* QM_JOIN_FREE*/
#ifdef DBG_QM_MALLOC
	f->file=file;
	f->func=func;
	f->line=line;
#endif
	qm_insert_free(qm, f);
}



void qm_status(struct qm_block* qm)
{
	struct qm_frag* f;
	int i,j;
	int h;

	LOG(memlog, "qm_status (%p):\n", qm);
	if (!qm) return;

	LOG(memlog, " heap size= %d\n", qm->size);
	LOG(memlog, " used= %d, used+overhead=%d, free=%d\n",
			qm->used, qm->real_used, qm->size-qm->real_used);
	LOG(memlog, " max used (+overhead)= %d\n", qm->max_real_used);
	
	LOG(memlog, "dumping all allocked. fragments:\n");
	for (f=qm->first_frag, i=0;(char*)f<(char*)qm->last_frag_end;f=FRAG_NEXT(f)
			,i++){
		if (! f->u.is_free){
			LOG(memlog, "    %3d. %c  address=%p  size=%d\n", i, 
				(f->u.is_free)?'a':'N',
				(char*)f+sizeof(struct qm_frag), f->size);
#ifdef DBG_QM_MALLOC
			LOG(memlog, "            %s from %s: %s(%d)\n",
				(f->u.is_free)?"freed":"alloc'd", f->file, f->func, f->line);
			LOG(memlog, "        start check=%x, end check= %x, %x\n",
				f->check, FRAG_END(f)->check1, FRAG_END(f)->check2);
#endif
		}
	}
	LOG(memlog, "dumping free list stats :\n");
	for(h=0,i=0;h<QM_HASH_SIZE;h++){
		
		for (f=qm->free_hash[h].head.u.nxt_free,j=0; 
				f!=&(qm->free_hash[h].head); f=f->u.nxt_free, i++, j++);
			if (j) LOG(memlog, "hash= %3d. fragments no.: %5d\n", h, j);
	}
	LOG(memlog, "-----------------------------\n");
}




#endif
