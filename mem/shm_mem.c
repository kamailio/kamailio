/* $Id$
 *
 * Shared memory functions
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
 */
/*
 * History:
 * --------
 *  2003-03-12  splited shm_mem_init in shm_getmem & shm_mem_init_mallocs
 *               (andrei)
 */


#ifdef SHM_MEM

#include <stdlib.h>

#include "shm_mem.h"
#include "../config.h"
#include "../globals.h"

#ifdef  SHM_MMAP

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h> /*open*/
#include <sys/stat.h>
#include <fcntl.h>

#endif



#ifndef SHM_MMAP
static int shm_shmid=-1; /*shared memory id*/
#endif

gen_lock_t* mem_lock=0;

static void* shm_mempool=(void*)-1;
#ifdef VQ_MALLOC
	struct vqm_block* shm_block;
#elif F_MALLOC
	struct fm_block* shm_block;
#else
	struct qm_block* shm_block;
#endif


inline static void* sh_realloc(void* p, unsigned int size)
{
	void *r;
	shm_lock(); 
	shm_free_unsafe(p);
	r=shm_malloc_unsafe(size);
	shm_unlock();
	return r;
}

/* look at a buffer if there is perhaps enough space for the new size
   (It is benefitial to do so because vq_malloc is pretty stateful
    and if we ask for a new buffer size, we can still make it happy
    with current buffer); if so, we return current buffer again;
    otherwise, we free it, allocate a new one and return it; no
    guarantee for buffer content; if allocation fails, we return
    NULL
*/

#ifdef DBG_QM_MALLOC
void* _shm_resize( void* p, unsigned int s, char* file, char* func, int line)
#else
void* _shm_resize( void* p , unsigned int s)
#endif
{
#ifdef VQ_MALLOC
	struct vqm_frag *f;
#endif
	if (p==0) {
		DBG("WARNING:vqm_resize: resize(0) called\n");
		return shm_malloc( s );
	}
#	ifdef DBG_QM_MALLOC
#	ifdef VQ_MALLOC
	f=(struct  vqm_frag*) ((char*)p-sizeof(struct vqm_frag));
	DBG("_shm_resize(%p, %d), called from %s: %s(%d)\n",  
		p, s, file, func, line);
	VQM_DEBUG_FRAG(shm_block, f);
	if (p>(void *)shm_block->core_end || p<(void*)shm_block->init_core){
		LOG(L_CRIT, "BUG: vqm_free: bad pointer %p (out of memory block!) - "
				"aborting\n", p);
		abort();
	}
#endif
#	endif
	return sh_realloc( p, s ); 
}





int shm_getmem()
{

#ifdef SHM_MMAP
	int fd;
#else
	struct shmid_ds shm_info;
#endif

#ifdef SHM_MMAP
	if (shm_mempool && (shm_mempool!=(void*)-1)){
#else
	if ((shm_shmid!=-1)||(shm_mempool!=(void*)-1)){
#endif
		LOG(L_CRIT, "BUG: shm_mem_init: shm already initialized\n");
		return -1;
	}
	
#ifdef SHM_MMAP
	fd=open("/dev/zero", O_RDWR);
	if (fd==-1){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not open /dev/zero: %s\n",
				strerror(errno));
		return -1;
	}
	shm_mempool=mmap(0, shm_mem_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd ,0);
	/* close /dev/zero */
	close(fd);
#else
	
	shm_shmid=shmget(IPC_PRIVATE, /* SHM_MEM_SIZE */ shm_mem_size , 0700);
	if (shm_shmid==-1){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not allocate shared memory"
				" segment: %s\n", strerror(errno));
		return -1;
	}
	shm_mempool=shmat(shm_shmid, 0, 0);
#endif
	if (shm_mempool==(void*)-1){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not attach shared memory"
				" segment: %s\n", strerror(errno));
		/* destroy segment*/
		shm_mem_destroy();
		return -1;
	}
	return 0;
}



int shm_mem_init_mallocs(void* mempool, int pool_size)
{
	/* init it for malloc*/
	shm_block=shm_malloc_init(mempool, pool_size);
	if (shm_block==0){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not initialize shared"
				" malloc\n");
		shm_mem_destroy();
		return -1;
	}
	mem_lock=shm_malloc_unsafe(sizeof(gen_lock_t)); /* skip lock_alloc, 
													   race cond*/
	if (mem_lock==0){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not allocate lock\n");
		shm_mem_destroy();
		return -1;
	}
	if (lock_init(mem_lock)==0){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not initialize lock\n");
		shm_mem_destroy();
		return -1;
	}
	
	DBG("shm_mem_init: success\n");
	
	return 0;
}


int shm_mem_init()
{
	int ret;
	
	ret=shm_getmem();
	if (ret<0) return ret;
	return shm_mem_init_mallocs(shm_mempool, shm_mem_size);
}


void shm_mem_destroy()
{
#ifndef SHM_MMAP
	struct shmid_ds shm_info;
#endif
	
	DBG("shm_mem_destroy\n");
	if (shm_mempool && (shm_mempool!=(void*)-1)) {
#ifdef SHM_MMAP
		munmap(shm_mempool, /* SHM_MEM_SIZE */ shm_mem_size );
#else
		shmdt(shm_mempool);
#endif
		shm_mempool=(void*)-1;
	}
#ifndef SHM_MMAP
	if (shm_shmid!=-1) {
		shmctl(shm_shmid, IPC_RMID, &shm_info);
		shm_shmid=-1;
	}
#endif
	if (mem_lock){
		DBG("destroying the shared memory lock\n");
		lock_destroy(mem_lock); /* we don't need to dealloc it*/
	}
}


#endif
