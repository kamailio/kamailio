/* $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * History: 
 * merged from Andrei's qmalloc and many fragments from Regents 
 * University of California NetBSD malloc used; see
 * http://www.ajk.tele.fi/libc/stdlib/malloc.c.html#malloc for more
 * details including redistribution policy; this policy asks for
 * displaying the copyright:
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 *
 * About:
 * aggressive, wasteful and very quick malloc library built for
 * servers that continuously alocate and release chunks of only
 * few sizes:
 * - free lists are organized by size (which eliminates long list traversal 
 *   thru short free chunks if a long one is asked)
 * - quite a few sizes are supported --> this results in more waste
 *    (unused place in a chunk) however memory can be well reused after
 *    freeing
 * - the last bucket holds unlikely, huge, variable length chunks;
 *   they are maintained as stack growing from the opposite direction
 *   of our heap; coalesing is enabled; stack-like first-fit used to
 *   find free fragments
 *
 * TODO: possibly, some delayed coalescation wouldn't hurt; also, further
 * optimization will certainly speed-up the entire process a lot; using
 * knowledge of application as well as trying to make pipeline happy
 * (from which I am sadly far away now)
 * provides optimization space; trying to use other memory allocators
 * to compare would be a great thing to do too;
 *
 * also, comparing to other memory allocaters (like Horde) would not
 * be a bad idea: those folks have been doing that for ages and specifically
 * Horde has been heavily optimized for multi-processor machines
 *
 * References:
 *   - list of malloc implementations: http://www.cs.colorado.edu/~zorn/Malloc.html
 *   - a white-paper: http://g.oswego.edu/dl/html/malloc.html
 *   - Paul R. Wilson, Mark S. Johnstone, Michael Neely, and David Boles: 
       ``Dynamic Storage Allocation: A Survey and Critical Review'' in International 
       Workshop on Memory Management, September 1995, 
       ftp://ftp.cs.utexas.edu/pub/garbage/allocsrv.ps
 *   - ptmalloc: http://www.malloc.de/en/
 *   - GNU C-lib malloc: http://www.gnu.org/manual/glibc-2.0.6/html_chapter/libc_3.html
 *   - delorie malocs: http://www.delorie.com/djgpp/malloc/
 *
 */


#ifdef VQ_MALLOC

#include <stdlib.h>

#include "../config.h"
#include "../globals.h"
#include "vq_malloc.h"
#include "../dprint.h"
#include "../globals.h"

#define BIG_BUCKET(_qm) ((_qm)->max_small_bucket+1)
#define IS_BIGBUCKET(_qm, _bucket) ((_bucket)==BIG_BUCKET(_qm)) 

#ifdef DBG_QM_MALLOC
#define ASSERT(a)	\
	my_assert(a, __LINE__, __FILE__, __FUNCTION__ )
#else
#define ASSERT(a)
#endif

#ifdef DBG_QM_MALLOC
#	define MORE_CORE(_q,_b,_s) (more_core( (_q), (_b), (_s), file, func, line ))
#else
#	define MORE_CORE(_q,_b,_s) (more_core( (_q), (_b), (_s) ))
#endif



/* dimensioning buckets: define the step function constants for size2bucket */
int s2b_step[] = {8, 16, 32, 64, 128, 256, 512, 1024, 1536, 2048, 2560, MAX_FIXED_BLOCK, EO_STEP };

void my_assert( int assertation, int line, char *file, char *function )
{
	if (assertation) return;

	LOG(L_CRIT,"CRIT: assertation failed in %s (%s:%d)\n",
		function, file, line);
	abort();
}

#ifdef DBG_QM_MALLOC
void vqm_debug_frag(struct vqm_block* qm, struct vqm_frag* f)
{


	if (f->check!=ST_CHECK_PATTERN){
		LOG(L_CRIT, "BUG: vqm_*: fragm. %p beginning overwritten(%x)!\n",
				f, f->check);
		vqm_status(qm);
		abort();
	};
	if (memcmp(f->end_check, END_CHECK_PATTERN, END_CHECK_PATTERN_LEN)!=0) {
		LOG(L_CRIT, "BUG: vqm_*: fragm. %p end overwritten(%.*s)!\n",
				f, END_CHECK_PATTERN_LEN, f->end_check );
		vqm_status(qm);
		abort();
	}
}
#endif


/* takes  demanded size without overhead as input, returns bucket number
   and changed the demanded size to size really used including all
   possible overhead
 */
unsigned char size2bucket( struct vqm_block* qm, int *size  )
{
	unsigned char b;	
	unsigned int real_size;
	unsigned int exceeds;

	real_size = *size+ VQM_OVERHEAD;

#ifdef DBG_QM_MALLOC
	real_size+=END_CHECK_PATTERN_LEN;
#endif
	real_size+=((exceeds = (real_size % 8 )) ? 8 - exceeds : 0);
	ASSERT( !(real_size%8) );
	/* "small" chunk sizes <=1k translated using a table */
	if ( real_size < MAX_FIXED_BLOCK ) {
		b = qm->s2b[ real_size ];
		*size = qm->s2s[ real_size ];
	/* there might be various allocations slightly 1>1k, I still
	   don't want to be too agressive and increase useless 
	   allocations in small steps
    	*/	
	} else {
		b = BIG_BUCKET(qm); 
		*size = MAX_FIXED_BLOCK + 
			(real_size-MAX_FIXED_BLOCK+BLOCK_STEP) 
			/ BLOCK_STEP * BLOCK_STEP;
	}
	/*size must be a multiple of 8*/
	ASSERT( !(*size%8) );
	return b;
}


/* init malloc and return a qm_block */
struct vqm_block* vqm_malloc_init(char* address, unsigned int size)
{
	char* start;
	struct vqm_block* qm;
	unsigned int init_overhead;
	unsigned char b;	/* bucket iterator */
	unsigned int s;		/* size iterator */
	char *end;
	
	/* make address and size multiple of 8*/
	start=(char*)( ((unsigned int)address%8)?((unsigned int)address+8)/8*8:
			(unsigned int)address);
	if (size<start-address) return 0;
	size-=(start-address);
	if (size <8) return 0;
	size=(size%8)?(size-8)/8*8:size;

	init_overhead=sizeof(struct vqm_block);
	if (size < init_overhead)
	{
		/* not enough mem to create our control structures !!!*/
		return 0;
	}
	end = start + size;
	qm=(struct vqm_block*)start;
	memset(qm, 0, sizeof(struct vqm_block));
	size-=init_overhead;

	/* definition of the size2bucket table */
	for (s=0, b=0; s<MAX_FIXED_BLOCK ; s++) {
		while (s>s2b_step[b]) b++;
		if (b>MAX_BUCKET) {
			LOG(L_CRIT, "CRIT: vqm_malloc_init: attempt to install too many buckets,"
				"s2b_step > MAX_BUCKET\n");
			return 0;
		}
		qm->s2b[s] = b;
		qm->s2s[s] = s2b_step[b];
	}
	qm->max_small_bucket = b;

	/* from where we will draw memory */
	qm->core = (char *) ( start + sizeof( struct vqm_block ) );
	qm->free_core = size;
	/* remember for bound checking */
	qm->init_core = qm->core;
	qm->core_end = end;
	/* allocate big chunks from end */
	qm->big_chunks = end;
	
	return qm;
}



struct vqm_frag *more_core(	struct vqm_block* qm, 
				unsigned char bucket, unsigned int size
#ifdef DBG_QM_MALLOC
				, char *file, char *func, unsigned int line
#endif
			 ) 
{
	struct vqm_frag *new_chunk;
	struct vqm_frag_end *end;

	if (qm->free_core<size) return 0;

	/* update core */
	if (IS_BIGBUCKET(qm, bucket)) {
		qm->big_chunks-=size;
		new_chunk = (struct vqm_frag *) qm->big_chunks ;	
	} else {
		new_chunk = (struct vqm_frag *) qm->core;	
		qm->core+=size;
	}
	qm->free_core-=size;

	/* initialize the new fragment */
	new_chunk->u.inuse.bucket = bucket;
	new_chunk->size = size;

	end=FRAG_END( new_chunk );
	end->size=size;

	return new_chunk;
}

static inline void vqm_detach_free( struct vqm_block* qm, struct vqm_frag* frag)
{

	struct vqm_frag *prev, *next;

	prev=FRAG_END(frag)->prv_free; 
	next=frag->u.nxt_free;

	if (prev) prev->u.nxt_free=next; 
	else qm->next_free[BIG_BUCKET(qm)]=next;

	if (next) FRAG_END(next)->prv_free=prev; 
	 
}


#ifdef DBG_QM_MALLOC
void* vqm_malloc(struct vqm_block* qm, unsigned int size, 
	char* file, char* func, unsigned int line)
#else
void* vqm_malloc(struct vqm_block* qm, unsigned int size)
#endif
{
	struct vqm_frag *new_chunk, *f;
	unsigned char bucket;
	
#ifdef DBG_QM_MALLOC
	unsigned int demanded_size;
	DBG("vqm_malloc(%p, %d) called from %s: %s(%d)\n", qm, size, file,
	 func, line);
	demanded_size = size;
#endif
	new_chunk=0;
    	/* what's the bucket? what's the total size incl. overhead? */
	bucket = size2bucket( qm, &size );

	if (IS_BIGBUCKET(qm, bucket)) {	/* the kilo-bucket uses first-fit */
#ifdef DBG_QM_MALLOC
		DBG("vqm_malloc: processing a big fragment\n");
#endif
		for (f=qm->next_free[bucket] ; f; f=f->u.nxt_free ) 
			if (f->size>=size) { /* first-fit */
				new_chunk=f;
				VQM_DEBUG_FRAG(qm, f);
				vqm_detach_free(qm,f);
				break;
			}
	} else if (  (new_chunk=qm->next_free[ bucket ]) ) { /*fixed size bucket*/
			VQM_DEBUG_FRAG(qm, new_chunk);
			/*detach it from the head of bucket's free list*/
			qm->next_free[ bucket ] = new_chunk->u.nxt_free;
	}

	if (!new_chunk) { /* no chunk can be reused; slice one from the core */
		new_chunk=MORE_CORE( qm, bucket, size );
		if (!new_chunk) {
#ifdef DBG_QM_MALLOC
			LOG(L_DBG, "vqm_malloc(%p, %d) called from %s: %s(%d)\n", 
				qm, size, file, func, line);
#else
			LOG(L_DBG, "vqm_malloc(%p, %d) called from %s: %s(%d)\n", 
				qm, size);
#endif
			return 0;
		}
	}
	new_chunk->u.inuse.magic = FR_USED;
	new_chunk->u.inuse.bucket=bucket;
#ifdef DBG_QM_MALLOC
	new_chunk->file=file;
	new_chunk->func=func;
	new_chunk->line=line;
	new_chunk->demanded_size=demanded_size;
	qm->usage[ bucket ]++;
	DBG("vqm_malloc( %p, %d ) returns address %p in bucket %d, real-size %d \n",
		qm, demanded_size, (char*)new_chunk+sizeof(struct vqm_frag), 
		bucket, size );

	new_chunk->end_check=(char*)new_chunk+sizeof(struct vqm_frag)+demanded_size;
	memcpy(  new_chunk->end_check, END_CHECK_PATTERN, END_CHECK_PATTERN_LEN );
	new_chunk->check=ST_CHECK_PATTERN;
#endif
	return (char*)new_chunk+sizeof(struct vqm_frag);
}

#ifdef DBG_QM_MALLOC
void vqm_free(struct vqm_block* qm, void* p, char* file, char* func, 
				unsigned int line)
#else
void vqm_free(struct vqm_block* qm, void* p)
#endif
{
	struct vqm_frag *f, *next, *prev, *first_big;
	unsigned char b;

#ifdef DBG_QM_MALLOC
	DBG("vqm_free(%p, %p), called from %s: %s(%d)\n", 
		qm, p, file, func, line);
	if (p>(void *)qm->core_end || p<(void*)qm->init_core){
		LOG(L_CRIT, "BUG: vqm_free: bad pointer %p (out of memory block!) - "
				"aborting\n", p);
		abort();
	}
#endif
	if (p==0) {
		DBG("WARNING:vqm_free: free(0) called\n");
		return;
	}
	f=(struct  vqm_frag*) ((char*)p-sizeof(struct vqm_frag));
	b=f->u.inuse.bucket;
#ifdef DBG_QM_MALLOC
	VQM_DEBUG_FRAG(qm, f);
	if ( ! FRAG_ISUSED(f) ) {
		LOG(L_CRIT, "BUG: vqm_free: freeing already freed pointer,"
				" first freed: %s: %s(%d) - aborting\n",
				f->file, f->func, f->line);
		abort();
	}
	if ( b>MAX_BUCKET ) {
		LOG(L_CRIT, "BUG: vqm_free: fragment with too high bucket nr: "
				"%d, allocated: %s: %s(%d) - aborting\n",
				b, f->file, f->func, f->line); 
		abort();
	}
	DBG("vqm_free: freeing %d bucket block alloc'ed from %s: %s(%d)\n", 
		f->u.inuse.bucket, f->file, f->func, f->line);
	f->file=file; f->func=func; f->line=line;
	qm->usage[ f->u.inuse.bucket ]--;
#endif
	if (IS_BIGBUCKET(qm,b)) {
		next=FRAG_NEXT(f);
		if  ((char *)next +sizeof( struct vqm_frag) < qm->core_end) {
			VQM_DEBUG_FRAG(qm, next);
			if (! FRAG_ISUSED(next)) { /* coalescate with next fragment */
				DBG("vqm_free: coalescated with next\n");
				vqm_detach_free(qm, next);
				f->size+=next->size;
				FRAG_END(f)->size=f->size;
			}
		}
		first_big = qm->next_free[b];
		if (first_big &&  f>first_big) {
			prev=FRAG_PREV(f);
			VQM_DEBUG_FRAG(qm, prev);
			if (!FRAG_ISUSED(prev)) { /* coalescate with prev fragment */
				DBG("vqm_free: coalescated with prev\n");
				vqm_detach_free(qm, prev );
				prev->size+=f->size;
				f=prev;
				FRAG_END(f)->size=f->size;
			}
		}
		if ((char *)f==qm->big_chunks) { /* release unused core */
			DBG("vqm_free: big chunk released\n");
			qm->free_core+=f->size;
			qm->big_chunks+=f->size;
			return;
		}		
		first_big = qm->next_free[b];
		/* fix reverse link (used only for BIG_BUCKET */
		if (first_big) FRAG_END(first_big)->prv_free=f;
		FRAG_END(f)->prv_free=0;
	} else first_big = qm->next_free[b];
	f->u.nxt_free = first_big; /* also clobbers magic */
	qm->next_free[b] = f;
}

void dump_frag( struct vqm_frag* f, int i )
{
	LOG(memlog, "    %3d. address=%p  real size=%d bucket=%d\n", i, 
		(char*)f+sizeof(struct vqm_frag), f->size, f->u.inuse.bucket);
#ifdef DBG_QM_MALLOC
	LOG(memlog, "            demanded size=%d\n", f->demanded_size );
	LOG(memlog, "            alloc'd from %s: %s(%d)\n",
		f->file, f->func, f->line);
	LOG(memlog, "        start check=%x, end check= %.*s\n",
			f->check, END_CHECK_PATTERN_LEN, f->end_check );
#endif
}

void vqm_status(struct vqm_block* qm)
{
	struct vqm_frag* f;
	unsigned int i,on_list;

	LOG(memlog, "vqm_status (%p):\n", qm);
	if (!qm) return;
	LOG(memlog, " heap size= %d, available: %d\n", 
		qm->core_end-qm->init_core, qm->free_core );
	
	LOG(memlog, "dumping unfreed fragments:\n");
	for (f=(struct vqm_frag*)qm->init_core, i=0;(char*)f<(char*)qm->core;
		f=FRAG_NEXT(f) ,i++) if ( FRAG_ISUSED(f) ) dump_frag(f, i);

	LOG(memlog, "dumping unfreed big fragments:\n");
    for (f=(struct vqm_frag*)qm->big_chunks,i=0;(char*)f<(char*)qm->core_end;
		f=FRAG_NEXT(f) ,i++) if ( FRAG_ISUSED(f) ) dump_frag( f, i );

#ifdef DBG_QM_MALLOC
	DBG("dumping bucket statistics:\n");
	for (i=0; i<=BIG_BUCKET(qm); i++) {
		for(on_list=0, f=qm->next_free[i]; f; f=f->u.nxt_free ) on_list++;
		LOG(L_DBG, "    %3d. bucket: in use: %ld, on free list: %d\n", 
			i, qm->usage[i], on_list );
	}
#endif
	LOG(memlog, "-----------------------------\n");
}



#endif
