/* $Id$
 *
 * simple & fast malloc library
 */

#ifndef q_malloc_h
#define q_malloc_h


struct qm_frag{
	unsigned int size;
	union{
		struct qm_frag* nxt_free;
		int is_free;
	}u;
};

struct qm_frag_end{
	unsigned int size;
	struct qm_frag* prev_free;
};


struct qm_block{
	unsigned int init_size;
	unsigned int size; /* total size */
	unsigned int used; /* alloc'ed size*/
	unsigned int real_used; /* used+malloc overhead*/
	
	struct qm_frag* first_frag;
	struct qm_frag_end* last_frag_end;
	
	struct qm_frag free_lst;
	struct qm_frag_end free_lst_end;
};



struct qm_block* qm_malloc_init(char* address, unsigned int size);
void* qm_malloc(struct qm_block*, unsigned int size);
void  qm_free(struct qm_block*, void* p);
void  qm_status(struct qm_block*);


#endif
