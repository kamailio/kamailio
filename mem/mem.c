/*
 * $Id *
 */

#include <stdio.h>
#include "../config.h"
#include "../dprint.h"
#include "../globals.h"
#include "mem.h"

#ifdef PKG_MALLOC
	#ifdef VQ_MALLOC
		#include "vq_malloc.h"
	#else
		#include "q_malloc.h"
	#endif
#endif

#ifdef SHM_MEM
#include "shm_mem.h"
#endif

#ifdef PKG_MALLOC
	char mem_pool[PKG_MEM_POOL_SIZE];
	#ifdef VQ_MALLOC
		struct vqm_block* mem_block;
	#elif defined F_MALLOC
		struct fm_block* mem_block;
	#else
		struct qm_block* mem_block;
	#endif
#endif

int init_mallocs()
{
#ifdef PKG_MALLOC
	/*init mem*/
	#ifdef VQ_MALLOC
		mem_block=vqm_malloc_init(mem_pool, PKG_MEM_POOL_SIZE);
	#elif F_MALLOC
		mem_block=fm_malloc_init(mem_pool, PKG_MEM_POOL_SIZE);
	#else
		mem_block=qm_malloc_init(mem_pool, PKG_MEM_POOL_SIZE);
	#endif
	if (mem_block==0){
		LOG(L_CRIT, "could not initialize memory pool\n");
		fprintf(stderr, "Too much pkg memory demanded: %d\n",
			PKG_MEM_POOL_SIZE );
		return -1;
	}
#endif

#ifdef SHM_MEM
	if (shm_mem_init()<0) {
		LOG(L_CRIT, "could not initialize shared memory pool, exiting...\n");
		 fprintf(stderr, "Too much shared memory demanded: %d\n",
			shm_mem_size );
		return -1;
	}
#endif
	return 0;

}


