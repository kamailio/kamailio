/* $Id$
 *
 * simple & fast malloc library
 */

#if !defined(q_malloc_h) && !defined(VQ_MALLOC) && !defined(F_MALLOC)
#define q_malloc_h



/* defs*/

#define ROUNDTO		16 /* size we round to, must be = 2^n  and also
						 sizeof(qm_frag)+sizeof(qm_frag_end)
						 must be mutliple of ROUNDTO!
					   */
#define MIN_FRAG_SIZE	ROUNDTO



#define QM_MALLOC_OPTIMIZE_FACTOR 10 /*used below */
#define QM_MALLOC_OPTIMIZE  (1<<QM_MALLOC_OPTIMIZE_FACTOR)
								/* size to optimize for,
									(most allocs < this size),
									must be 2^k */

#define QM_HASH_SIZE (QM_MALLOC_OPTIMIZE/ROUNDTO + \
		(32-QM_MALLOC_OPTIMIZE_FACTOR)+1)

/* hash structure:
 * 0 .... QM_MALLOC_OPTIMIE/ROUNDTO  - small buckets, size increases with
 *                            ROUNDTO from bucket to bucket
 * +1 .... end -  size = 2^k, big buckets */

struct qm_frag{
	unsigned int size;
	union{
		struct qm_frag* nxt_free;
		int is_free;
	}u;
#ifdef DBG_QM_MALLOC
	char* file;
	char* func;
	unsigned int line;
	unsigned int check;
#endif
};

struct qm_frag_end{
#ifdef DBG_QM_MALLOC
	unsigned int check1;
	unsigned int check2;
	unsigned int reserved1;
	unsigned int reserved2;
#endif
	unsigned int size;
	struct qm_frag* prev_free;
};



struct qm_frag_full{
	struct qm_frag head;
	struct qm_frag_end tail;
};


struct qm_block{
	unsigned int size; /* total size */
	unsigned int used; /* alloc'ed size*/
	unsigned int real_used; /* used+malloc overhead*/
	unsigned int max_real_used;
	
	struct qm_frag* first_frag;
	struct qm_frag_end* last_frag_end;
	
	struct qm_frag_full free_hash[QM_HASH_SIZE];
	/*struct qm_frag_end free_lst_end;*/
};



struct qm_block* qm_malloc_init(char* address, unsigned int size);

#ifdef DBG_QM_MALLOC
void* qm_malloc(struct qm_block*, unsigned int size, char* file, char* func, 
					unsigned int line);
#else
void* qm_malloc(struct qm_block*, unsigned int size);
#endif

#ifdef DBG_QM_MALLOC
void  qm_free(struct qm_block*, void* p, char* file, char* func, 
				unsigned int line);
#else
void  qm_free(struct qm_block*, void* p);
#endif

void  qm_status(struct qm_block*);


#endif
