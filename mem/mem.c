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
 *
 *
 */

/**
 * \file
 * \brief Main definitions for memory manager
 * 
 * Main definitions for memory manager, like malloc, free and realloc
 * \ingroup mem
 */


#include <stdio.h>
#include <stdlib.h>
#include "../config.h"
#include "../dprint.h"
#include "../globals.h"
#include "mem.h"

#ifdef PKG_MALLOC
#include "q_malloc.h"
#endif

#ifdef SHM_MEM
#include "shm_mem.h"
#endif

#ifdef PKG_MALLOC
	#ifndef DL_MALLOC
	char* mem_pool = 0;
	#endif

	#ifdef F_MALLOC
		struct fm_block* mem_block = 0;
	#elif defined DL_MALLOC
		/* don't need this */
	#elif defined TLSF_MALLOC
		tlsf_t mem_block = 0;
	#else
		struct qm_block* mem_block = 0;
	#endif
#endif


/**
 * \brief Initialize private memory pool
 * \return 0 if the memory allocation was successful, -1 otherwise
 */
int init_pkg_mallocs(void)
{
#ifdef PKG_MALLOC
	/*init mem*/
	#ifndef DL_MALLOC
		if (pkg_mem_size == 0)
			pkg_mem_size = PKG_MEM_POOL_SIZE;
		mem_pool = malloc(pkg_mem_size);
	#endif
	#ifdef F_MALLOC
		if (mem_pool)
			mem_block=fm_malloc_init(mem_pool, pkg_mem_size, MEM_TYPE_PKG);
	#elif DL_MALLOC
		/* don't need this */
	#elif TLSF_MALLOC
		mem_block = tlsf_create_with_pool(mem_pool, pkg_mem_size);
	#else
		if (mem_pool)
			mem_block=qm_malloc_init(mem_pool, pkg_mem_size, MEM_TYPE_PKG);
	#endif
	#ifndef DL_MALLOC
	if (mem_block==0){
		LOG(L_CRIT, "could not initialize memory pool\n");
		fprintf(stderr, "Too much pkg memory demanded: %ld bytes\n",
						pkg_mem_size);
		return -1;
	}
	#endif
#endif
	return 0;
}



/**
 * \brief Destroy private memory pool
 */
void destroy_pkg_mallocs(void)
{
#ifdef PKG_MALLOC
	#ifndef DL_MALLOC
		if (mem_pool) {
			free(mem_pool);
			mem_pool = 0;
		}
	#endif
#endif /* PKG_MALLOC */
}


/**
 * \brief Initialize shared memory pool
 * \param force_alloc Force allocation of memory, e.g. initialize complete block with zero
 * \return 0 if the memory allocation was successful, -1 otherwise
 */
int init_shm_mallocs(int force_alloc)
{
#ifdef SHM_MEM
	if (shm_mem_init(force_alloc)<0) {
		LOG(L_CRIT, "could not initialize shared memory pool, exiting...\n");
		 fprintf(stderr, "Too much shared memory demanded: %ld\n",
			shm_mem_size );
		return -1;
	}
#endif
	return 0;
}
