/* $Id$
 *
 */

#if !defined(q_malloc) && !(defined VQ_MALLOC) 

#include "f_malloc.h"
#include "../dprint.h"


/*useful macros*/

#define FRAG_NEXT(f) \
	((struct fm_frag*)((char*)(f)+sizeof(struct fm_frag)+(f)->size ))

#define FRAG_OVERHEAD	(sizeof(struct fm_frag))


#define ROUNDUP(s)		(((s)%ROUNDTO)?((s)+ROUNDTO)/ROUNDTO*ROUNDTO:(s))
#define ROUNDDOWN(s)	(((s)%ROUNDTO)?((s)-ROUNDTO)/ROUNDTO*ROUNDTO:(s))



	/* finds the hash value for s, s=ROUNDTO multiple*/
#define GET_HASH(s)   ( ((s)<F_MALLOC_OPTIMIZE)?(s)/ROUNDTO: \
						F_MALLOC_OPTIMIZE/ROUNDTO+big_hash_idx((s))- \
							F_MALLOC_OPTIMIZE_FACTOR+1 )


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


#ifdef DBG_F_MALLOC
#define ST_CHECK_PATTERN   0xf0f0f0f0
#define END_CHECK_PATTERN1 0xc0c0c0c0
#define END_CHECK_PATTERN2 0xabcdefed
#endif



static inline void fm_insert_free(struct fm_block* qm, struct fm_frag* frag)
{
	struct fm_frag** f;
	int hash;
	
	hash=GET_HASH(frag->size);
	f=&(qm->free_hash[hash]);
	if (frag->size > F_MALLOC_OPTIMIZE){
		for(; *f; f=&((*f)->u.nxt_free)){
			if (frag->size <= (*f)->size) break;
		}
	}
	
	/*insert it here*/
	frag->u.nxt_free=*f;
	*f=frag;
}



/* init malloc and return a qm_block*/
struct fm_block* fm_malloc_init(char* address, unsigned int size)
{
	char* start;
	char* end;
	struct fm_block* qm;
	unsigned int init_overhead;
	int h;
	
	/* make address and size multiple of 8*/
	start=(char*)ROUNDUP((unsigned int) address);
	if (size<start-address) return 0;
	size-=(start-address);
	if (size <(MIN_FRAG_SIZE+FRAG_OVERHEAD)) return 0;
	size=ROUNDDOWN(size);

	init_overhead=(ROUNDUP(sizeof(struct fm_block))+sizeof(struct fm_frag));
	
	
	if (size < init_overhead)
	{
		/* not enough mem to create our control structures !!!*/
		return 0;
	}
	end=start+size;
	qm=(struct fm_block*)start;
	memset(qm, 0, sizeof(struct fm_block));
	size-=init_overhead;
	qm->size=size;
#ifdef DBG_F_MALLOC
	qm->real_used=init_overhead;
	qm->max_real_used=qm->real_used;
#endif
	
	qm->first_frag=(struct fm_frag*)(start+ROUNDUP(sizeof(struct fm_block)));
	qm->last_frag=(struct fm_frag*)(end-sizeof(struct fm_frag));
	/* init initial fragment*/
	qm->first_frag->size=size;
	qm->last_frag->size=0;
	
#ifdef DBG_F_MALLOC
	qm->first_frag->check=ST_CHECK_PATTERN;
	qm->last_frag->check=END_CHECK_PATTERN1;
#endif
	
	/* link initial fragment into the free list*/
	
	fm_insert_free(qm, qm->first_frag);
	
	
	return qm;
}



#ifdef DBG_F_MALLOC
void* fm_malloc(struct fm_block* qm, unsigned int size, char* file, char* func,
					unsigned int line)
#else
void* fm_malloc(struct fm_block* qm, unsigned int size)
#endif
{
	struct fm_frag** f;
	struct fm_frag* frag;
	struct fm_frag* n;
	unsigned int rest;
	int hash;
	
#ifdef DBG_F_MALLOC
	DBG("fm_malloc(%x, %d) called from %s: %s(%d)\n", qm, size, file, func,
			line);
#endif
	/*size must be a multiple of 8*/
	size=ROUNDUP(size);
/*	if (size>(qm->size-qm->real_used)) return 0; */

	
	/*search for a suitable free frag*/

	for(hash=GET_HASH(size);hash<F_HASH_SIZE;hash++){
		f=&(qm->free_hash[hash]);
		for(;(*f); f=&((*f)->u.nxt_free))
			if ((*f)->size>=size) goto found;
		/* try in a bigger bucket */
	}
	/* not found, bad! */
	return 0;

found:
	/* we found it!*/
	/* detach it from the free list*/
	frag=*f;
	*f=frag->u.nxt_free;
	
	/*see if we'll use full frag, or we'll split it in 2*/
	rest=frag->size-size;
	if (rest>(FRAG_OVERHEAD+MIN_FRAG_SIZE)){
		frag->size=size;
		/*split the fragment*/
		n=FRAG_NEXT(frag);
		n->size=rest-FRAG_OVERHEAD;
#ifdef DBG_F_MALLOC
		qm->real_used+=FRAG_OVERHEAD;
		/* frag created by malloc, mark it*/
		n->file=file;
		n->func="frag. from qm_malloc";
		n->line=line;
		n->check=ST_CHECK_PATTERN;
#endif
		/* reinsert n in free list*/
		fm_insert_free(qm, n);
	}else{
		/* we cannot split this fragment any more => alloc all of it*/
	}
#ifdef DBG_F_MALLOC
	qm->real_used+=frag->size;
	qm->used+=frag->size;

	if (qm->max_real_used<qm->real_used)
		qm->max_real_used=qm->real_used;

	frag->file=file;
	frag->func=func;
	frag->line=line;
	frag->check=ST_CHECK_PATTERN;
	DBG("fm_malloc(%x, %d) returns address %x \n", qm, size,
		(char*)frag+sizeof(struct fm_frag));
#endif
	return (char*)frag+sizeof(struct fm_frag);
}



#ifdef DBG_F_MALLOC
void fm_free(struct fm_block* qm, void* p, char* file, char* func, 
				unsigned int line)
#else
void fm_free(struct fm_block* qm, void* p)
#endif
{
	struct fm_frag* f;
	unsigned int size;

#ifdef DBG_F_MALLOC
	DBG("fm_free(%x, %x), called from %s: %s(%d)\n", qm, p, file, func, line);
	if (p>(void*)qm->last_frag || p<(void*)qm->first_frag){
		LOG(L_CRIT, "BUG: fm_free: bad pointer %x (out of memory block!) - "
				"aborting\n", p);
		abort();
	}
#endif
	if (p==0) {
		LOG(L_WARN, "WARNING:fm_free: free(0) called\n");
		return;
	}
	f=(struct fm_frag*) ((char*)p-sizeof(struct fm_frag));
#ifdef DBG_F_MALLOC
	DBG("fm_free: freeing block alloc'ed from %s: %s(%d)\n", f->file, f->func,
			f->line);
#endif
	size=f->size;

#ifdef DBG_F_MALLOC
	qm->used-=size;
	qm->real_used-=size;
	f->file=file;
	f->func=func;
	f->line=line;
#endif
	fm_insert_free(qm, f);
}



void fm_status(struct fm_block* qm)
{
	struct fm_frag* f;
	int i,j;
	int h;

	LOG(L_INFO, "fm_status (%x):\n", qm);
	if (!qm) return;

	LOG(L_INFO, " heap size= %d\n", qm->size);
#ifdef DBG_F_MALLOC
	LOG(L_INFO, " used= %d, used+overhead=%d, free=%d\n",
			qm->used, qm->real_used, qm->size-qm->real_used);
	LOG(L_INFO, " max used (+overhead)= %d\n", qm->max_real_used);
#endif
	/*
	LOG(L_INFO, "dumping all fragments:\n");
	for (f=qm->first_frag, i=0;((char*)f<(char*)qm->last_frag) && (i<10);
			f=FRAG_NEXT(f), i++){
		LOG(L_INFO, "    %3d. %c  address=%x  size=%d\n", i, 
				(f->u.reserved)?'a':'N',
				(char*)f+sizeof(struct fm_frag), f->size);
#ifdef DBG_F_MALLOC
		LOG(L_INFO, "            %s from %s: %s(%d)\n",
				(f->u.is_free)?"freed":"alloc'd", f->file, f->func, f->line);
#endif
	}
*/
	LOG(L_INFO, "dumping free list:\n");
	for(h=0,i=0;h<F_HASH_SIZE;h++){
		
		for (f=qm->free_hash[h],j=0; f; f=f->u.nxt_free, i++, j++)
			if (j) LOG(L_INFO, "hash= %3d. fragments no.: %5d\n", h, j);
		/*
		{
			LOG(L_INFO, "   %5d.[%3d:%3d] %c  address=%x  size=%d(%x)\n",
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
	LOG(L_INFO, "	Total: %6d fragments\n", i);
	LOG(L_INFO, "-----------------------------\n");
}




#endif
