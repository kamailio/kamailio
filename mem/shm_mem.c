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
 */

/**
 * \file
 * \brief  Shared memory functions
 * \ingroup mem
 */


#ifdef SHM_MEM

#include <stdlib.h>

#include "shm_mem.h"
#include "../config.h"
#include "../globals.h"
#include "memdbg.h"

#ifdef  SHM_MMAP

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h> /*open*/
#include <sys/stat.h>
#include <fcntl.h>

#endif

#define _ROUND2TYPE(s, type) \
	(((s)+(sizeof(type)-1))&(~(sizeof(type)-1)))
#define _ROUND_LONG(s) _ROUND2TYPE(s, long)


#ifndef SHM_MMAP
static int shm_shmid=-1; /*shared memory id*/
#endif

#ifndef SHM_SAFE_MALLOC
gen_lock_t* mem_lock=0;
#endif

static void* shm_mempool=(void*)-1;
#ifdef LL_MALLOC
	struct sfm_block* shm_block;
#elif SF_MALLOC
	struct sfm_block* shm_block;
#elif F_MALLOC
	struct fm_block* shm_block;
#elif DL_MALLOC
	mspace shm_block;
#elif TLSF_MALLOC
	tlsf_t shm_block;
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
   (It is beneficial to do so because vq_malloc is pretty stateful
    and if we ask for a new buffer size, we can still make it happy
    with current buffer); if so, we return current buffer again;
    otherwise, we free it, allocate a new one and return it; no
    guarantee for buffer content; if allocation fails, we return
    NULL
*/

#ifdef DBG_QM_MALLOC
void* _shm_resize( void* p, unsigned int s, const char* file, const char* func,
							int line)
#else
void* _shm_resize( void* p , unsigned int s)
#endif
{
	if (p==0) {
		DBG("WARNING:vqm_resize: resize(0) called\n");
		return shm_malloc( s );
	}
	return sh_realloc( p, s ); 
}





int shm_getmem(void)
{

#ifdef SHM_MMAP
#ifndef USE_ANON_MMAP
	int fd;
#endif
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
#ifdef USE_ANON_MMAP
	shm_mempool=mmap(0, shm_mem_size, PROT_READ|PROT_WRITE,
					 MAP_ANON|MAP_SHARED, -1 ,0);
#else
	fd=open("/dev/zero", O_RDWR);
	if (fd==-1){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not open /dev/zero: %s\n",
				strerror(errno));
		return -1;
	}
	shm_mempool=mmap(0, shm_mem_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd ,0);
	/* close /dev/zero */
	close(fd);
#endif /* USE_ANON_MMAP */
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



int shm_mem_init_mallocs(void* mempool, unsigned long pool_size)
{
	/* init it for malloc*/
	shm_block=shm_malloc_init(mempool, pool_size, MEM_TYPE_SHM);
	if (shm_block==0){
		LOG(L_CRIT, "ERROR: shm_mem_init: could not initialize shared"
				" malloc\n");
		shm_mem_destroy();
		return -1;
	}
#ifndef SHM_SAFE_MALLOC
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
#endif  /*SHM SAFE_MALLOC */
	
	DBG("shm_mem_init: success\n");
	
	return 0;
}


int shm_mem_init(int force_alloc)
{
	int ret;
	long sz;
	long* p;
	long* end;
	
	ret=shm_getmem();
	if (ret<0) return ret;
	if (force_alloc){
		sz=sysconf(_SC_PAGESIZE);
		DBG("shm_mem_init: %ld bytes/page\n", sz);
		if ((sz<sizeof(*p)) || (_ROUND_LONG(sz)!=sz)){
			LOG(L_WARN, "shm_mem_init: invalid page size %ld, using 4096\n",
					sz);
			sz=4096; /* invalid page size, use 4096 */
		}
		end=shm_mempool+shm_mem_size-sizeof(*p);
		/* touch one word in every page */
		for(p=(long*)_ROUND_LONG((long)shm_mempool); p<=end;
										p=(long*)((char*)p+sz))
			*p=0; 
	}
	return shm_mem_init_mallocs(shm_mempool, shm_mem_size);
}


void shm_mem_destroy(void)
{
#ifndef SHM_MMAP
	struct shmid_ds shm_info;
#endif
	
	DBG("shm_mem_destroy\n");
#ifndef SHM_SAFE_MALLOC
	if (mem_lock){
		DBG("destroying the shared memory lock\n");
		lock_destroy(mem_lock); /* we don't need to dealloc it*/
	}
#endif  /*SHM SAFE_MALLOC */
	if (shm_block){
		shm_malloc_destroy(shm_block);
		shm_block=0;
	}
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
}

unsigned long shm_available_safe()
{
	unsigned long ret;
	shm_lock();
	ret = shm_available();
	shm_unlock();
	return ret;
}

#endif
