/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *
 *
 * History:
 * --------
 *  2003-04-08  init_mallocs split into init_{pkg,shm}_malloc (andrei)
 * 
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


int init_pkg_mallocs()
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
	return 0;
}



int init_shm_mallocs()
{
#ifdef SHM_MEM
	if (shm_mem_init()<0) {
		LOG(L_CRIT, "could not initialize shared memory pool, exiting...\n");
		 fprintf(stderr, "Too much shared memory demanded: %ld\n",
			shm_mem_size );
		return -1;
	}
#endif
	return 0;
}


