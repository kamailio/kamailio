/* $Id$
 *
 */

#if !defined(VQ_MALLOC_H) && defined(VQ_MALLOC)
#define VQ_MALLOC_H

#include "../config.h"


/* indicates this fragment is not in use (must not be offset of valid
   aligned fragment beginning
*/
#define	FR_USED		0xef

/*useful macros*/
#define FRAG_END(f)  \
	((struct vqm_frag_end*)((char*)(f)-sizeof(struct vqm_frag_end)+ \
	(f)->size))

#define FRAG_NEXT(f) \
	((struct vqm_frag*)((char*)(f)+(f)->size))

#define PREV_FRAG_END(f) \
	((struct vqm_frag_end*)((char*)(f)-sizeof(struct vqm_frag_end)))

#define FRAG_PREV(f) \
	( (struct vqm_frag*) ( (char*)(f) - PREV_FRAG_END(f)->size ))

#define FRAG_ISUSED(f) \
	((f)->u.inuse.magic==FR_USED)

/* just a bumper for the step function */
#define EO_STEP                         -1


#ifdef DBG_QM_MALLOC
#define ST_CHECK_PATTERN   	0xf0f0f0f0
#define END_CHECK_PATTERN  	"sExP"
#define END_CHECK_PATTERN_LEN 	4

#define VQM_DEBUG_FRAG(qm, f) vqm_debug_frag( (qm), (f))
#else
#define VQM_DEBUG_FRAG(qm, f)
#endif


struct vqm_frag {
	/* XXX */
	/* total chunk size including all overhead/bellowfoot/roundings/etc */
	/* useless as otherwise size implied by bucket (if I really want to save 
       bytes, I'll remove it  from here */
	unsigned int size;
	union{
		/* pointer to next chunk in a bucket if free */
		struct vqm_frag* nxt_free; 
		struct {   /* or bucket number if busy */
			unsigned char magic;
			unsigned char bucket;
        } inuse;
	} u;
#ifdef DBG_QM_MALLOC
	/* source code info */
	char* file;
	char* func;
	unsigned int line;
	/* your safety is important to us! safety signatures */
	unsigned int check;
	char *end_check;
	/* the size user was originally asking for */
	unsigned int demanded_size;
#endif
};

struct vqm_frag_end{
	/* total chunk size including all overhead/bellowfoot/roundings/etc */
	unsigned int size; 
	/* XXX */
	/* used only for variable-size chunks; might have different
           data structures for variable/fixed length chunks */
	struct vqm_frag* prv_free;
};


struct vqm_block{
	/* size to bucket table */
	unsigned char s2b[ MAX_FIXED_BLOCK ];
	/* size to rounded size */
	unsigned short s2s[ MAX_FIXED_BLOCK ];
	unsigned char max_small_bucket;

	/* core gained on init ... */
	char *core, *init_core, *core_end;
	/* ... and its available net amount; note that there's lot of
           free memory in buckets too -- this just tells about memory
	   which has not been assigned to chunks  */
	unsigned int free_core;
	/* we allocate huge chunks from the end on; this is the
	   pointer to big chunks
    */
	char *big_chunks;

	struct vqm_frag* next_free[ MAX_BUCKET +1];
#ifdef DBG_QM_MALLOC
	unsigned long usage[ MAX_BUCKET +1];
#endif
};



struct vqm_block* vqm_malloc_init(char* address, unsigned int size);

#ifdef DBG_QM_MALLOC
void* vqm_malloc(struct vqm_block*, unsigned int size, char* file, char* func, 
					unsigned int line);
#else
void* vqm_malloc(struct vqm_block*, unsigned int size);
#endif

#ifdef DBG_QM_MALLOC
void  vqm_free(struct vqm_block*, void* p, char* file, char* func, 
				unsigned int line);
#else
void  vqm_free(struct vqm_block*, void* p);
#endif

void  vqm_status(struct vqm_block*);


#endif
