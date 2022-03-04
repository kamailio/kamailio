/*
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
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

#include <stdlib.h>

#include "../config.h"
#include "../globals.h"
#include "memdbg.h"
#include "shm.h"

#ifdef  SHM_MMAP

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h> /*open*/
#include <sys/stat.h>
#include <fcntl.h>

#endif

#include "memcore.h"

#define SHM_CORE_POOLS_SIZE	4

#define _ROUND2TYPE(s, type) \
	(((s)+(sizeof(type)-1))&(~(sizeof(type)-1)))
#define _ROUND_LONG(s) _ROUND2TYPE(s, long)


void shm_core_destroy(void);

#ifndef SHM_MMAP
static int _shm_core_shmid[SHM_CORE_POOLS_SIZE] = { -1 }; /*shared memory id*/
#endif

static void* _shm_core_pools_mem[SHM_CORE_POOLS_SIZE] = { (void*)-1 };
static int   _shm_core_pools_num = 1;

sr_shm_api_t _shm_root = {0};

/**
 *
 */
int shm_core_pools_init(void)
{
	int i;
	int pinit;

#ifdef SHM_MMAP
#ifndef USE_ANON_MMAP
	int fd;
#endif
#else
	struct shmid_ds shm_info;
#endif

	pinit = 0;
	for(i = 0; i < _shm_core_pools_num; i++) {
#ifdef SHM_MMAP
		if(_shm_core_pools_mem[i] != (void *)-1) {
#else
		if((_shm_core_shmid[i] != -1)||(_shm_core_pools_mem[i] != (void *)-1)) {
#endif
			LM_DBG("shm pool[%d] already initialized\n", i);
			pinit++;
		}
	}

	if(pinit!=0) {
		if(pinit==_shm_core_pools_num) {
			LM_DBG("all shm pools initialized\n");
			return 0;
		} else {
			LM_CRIT("partial initialization of shm pools (%d / %d)\n",
					pinit, _shm_core_pools_num);
			return -1;
		}
	} else {
		LM_DBG("preparing to initialize shm core pools\n");
	}

	for(i = 0; i < _shm_core_pools_num; i++) {
#ifdef SHM_MMAP
#ifdef USE_ANON_MMAP
		_shm_core_pools_mem[i] = mmap(0, shm_mem_size, PROT_READ | PROT_WRITE,
				MAP_ANON | MAP_SHARED, -1, 0);
#else
		fd = open("/dev/zero", O_RDWR);
		if(fd == -1) {
			LOG(L_CRIT, "could not open /dev/zero [%d]: %s\n",
					i, strerror(errno));
			return -1;
		}
		_shm_core_pools_mem[i] = mmap(
				0, shm_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		/* close /dev/zero */
		close(fd);
#endif /* USE_ANON_MMAP */
#else

		_shm_core_shmid[i] = shmget(IPC_PRIVATE, shm_mem_size, 0700);
		if(_shm_core_shmid[i] == -1) {
			LOG(L_CRIT, "could not allocate shared memory segment[%d]: %s\n",
					i, strerror(errno));
			return -1;
		}
		_shm_core_pools_mem[i] = shmat(_shm_core_shmid[i], 0, 0);
#endif
		if(_shm_core_pools_mem[i] == (void *)-1) {
			LOG(L_CRIT, "could not attach shared memory segment[%d]: %s\n",
					i, strerror(errno));
			/* destroy segment*/
			shm_core_destroy();
			return -1;
		}
	}
	return 0;
}

/**
 *
 */
void* shm_core_get_pool(void)
{
	int ret;
	long sz;
	long* p;
	long* end;
	int i;

	ret=shm_core_pools_init();
	if (ret<0)
		return NULL;

	for(i = 0; i < _shm_core_pools_num; i++) {
		if(shm_force_alloc) {
			sz = sysconf(_SC_PAGESIZE);
			DBG("%ld bytes/page\n", sz);
			if((sz < sizeof(*p)) || (_ROUND_LONG(sz) != sz)) {
				LOG(L_WARN, "invalid page size %ld, using 4096\n", sz);
				sz = 4096; /* invalid page size, use 4096 */
			}
			end = _shm_core_pools_mem[i] + shm_mem_size - sizeof(*p);
			/* touch one word in every page */
			for(p = (long *)_ROUND_LONG((long)_shm_core_pools_mem[i]); p <= end;
					p = (long *)((char *)p + sz))
				*p = 0;
		}
	}
	return _shm_core_pools_mem[0];
}

/**
 *
 */
int shm_address_in(void *p)
{
	int i;

	for(i = 0; i < _shm_core_pools_num; i++) {
		if(_shm_core_pools_mem[i] == (void *)-1) {
			continue;
		}
		if(((char*)p >= (char*)_shm_core_pools_mem[i])
				&& ((char*)p < ((char*)_shm_core_pools_mem[i]) + shm_mem_size)) {
			/* address in shm zone */
			return 1;
		}
	}

	/* address not in shm zone */
	return 0;
}

/**
 *
 */
void shm_core_destroy(void)
{
	int i;

#ifndef SHM_MMAP
	struct shmid_ds shm_info;
#endif

	for(i = 0; i < _shm_core_pools_num; i++) {
		if(_shm_core_pools_mem[i] != (void *)-1) {
#ifdef SHM_MMAP
			munmap(_shm_core_pools_mem[i], /* SHM_MEM_SIZE */ shm_mem_size);
#else
			shmdt(_shm_core_pools_mem[i]);
#endif
			_shm_core_pools_mem[i] = (void *)-1;
		}
#ifndef SHM_MMAP
		if(_shm_core_shmid[i] != -1) {
			shmctl(_shm_core_shmid[i], IPC_RMID, &shm_info);
			_shm_core_shmid[i] = -1;
		}
#endif
	}
}

/**
 *
 */
int shm_init_api(sr_shm_api_t *ap)
{
	memset(&_shm_root, 0, sizeof(sr_shm_api_t));
	_shm_root.mname          = ap->mname;
	_shm_root.mem_pool       = ap->mem_pool;
	_shm_root.mem_block      = ap->mem_block;
	_shm_root.xmalloc        = ap->xmalloc;
	_shm_root.xmallocxz      = ap->xmallocxz;
	_shm_root.xmalloc_unsafe = ap->xmalloc_unsafe;
	_shm_root.xfree          = ap->xfree;
	_shm_root.xfree_unsafe   = ap->xfree_unsafe;
	_shm_root.xrealloc       = ap->xrealloc;
	_shm_root.xreallocxf     = ap->xreallocxf;
	_shm_root.xresize        = ap->xresize;
	_shm_root.xstatus        = ap->xstatus;
	_shm_root.xinfo          = ap->xinfo;
	_shm_root.xreport        = ap->xreport;
	_shm_root.xavailable     = ap->xavailable;
	_shm_root.xsums          = ap->xsums;
	_shm_root.xdestroy       = ap->xdestroy;
	_shm_root.xmodstats      = ap->xmodstats;
	_shm_root.xfmodstats     = ap->xfmodstats;
	_shm_root.xglock         = ap->xglock;
	_shm_root.xgunlock       = ap->xgunlock;
	return 0;

}

/**
 *
 */
int shm_init_manager(char *name)
{
	if(strcmp(name, "fm")==0
			|| strcmp(name, "f_malloc")==0
			|| strcmp(name, "fmalloc")==0) {
		/*fast malloc*/
		return fm_malloc_init_shm_manager();
	} else if(strcmp(name, "qm")==0
			|| strcmp(name, "q_malloc")==0
			|| strcmp(name, "qmalloc")==0) {
		/*quick malloc*/
		return qm_malloc_init_shm_manager();
	} else if(strcmp(name, "tlsf")==0
			|| strcmp(name, "tlsf_malloc")==0) {
		/*tlsf malloc*/
		return tlsf_malloc_init_shm_manager();
	} else if(strcmp(name, "sm")==0) {
		/*system malloc*/
	} else {
		/*custom malloc - module*/
	}
	return -1;
}

/**
 *
 */
void shm_destroy_manager(void)
{
	if(_shm_root.xdestroy) {
		LM_DBG("destroying memory manager: %s\n",
				(_shm_root.mname)?_shm_root.mname:"unknown");
		_shm_root.xdestroy();
	}
	shm_core_destroy();
}

/**
 *
 */
void shm_print_manager(void)
{
	LM_DBG("shm - using memory manager: %s\n",
			(_shm_root.mname)?_shm_root.mname:"unknown");
}
