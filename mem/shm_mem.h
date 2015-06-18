/*
 * shared mem stuff
 *
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

#ifndef shm_mem_h
#define shm_mem_h

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>

#ifndef SHM_MMAP

#include <sys/shm.h>

#endif

#include <sys/sem.h>
#include <string.h>
#include <errno.h>

/* fix DBG MALLOC stuff */

/* fix debug defines, DBG_F_MALLOC <=> DBG_QM_MALLOC */
#ifdef F_MALLOC
	#ifdef DBG_F_MALLOC
		#ifndef DBG_QM_MALLOC
			#define DBG_QM_MALLOC
		#endif
	#elif defined(DBG_QM_MALLOC)
		#define DBG_F_MALLOC
	#endif
#elif defined TLSF_MALLOC
	#ifdef DBG_TLSF_MALLOC
		#ifndef DBG_QM_MALLOC
			#define DBG_QM_MALLOC
		#endif
	#elif defined(DBG_QM_MALLOC)
		#define DBG_TLSF_MALLOC
	#endif
#endif



#include "../dprint.h"
#include "../lock_ops.h" /* we don't include locking.h on purpose */

#ifdef LL_MALLOC
#	include "ll_malloc.h"
#	define SHM_SAFE_MALLOC /* no need to lock */
	extern struct sfm_block* shm_block;
#ifdef __SUNPRO_C
#	define shm_malloc(...) sfm_malloc(shm_block, __VA_ARGS__)
#	define shm_free(...) sfm_free(shm_block, __VA_ARGS__)
#	define shm_realloc(...) sfm_malloc(shm_block, __VA_ARGS__)
	/* WARNING: test, especially if switched to real realloc */
#	define shm_resize(...)	sfm_realloc(shm_block, __VA_ARGS__)
#	define shm_info(...) sfm_info(shm_block, __VA_ARGS__)
#else /* __SUNPRO_C */
#	define shm_malloc(args...) sfm_malloc(shm_block, ## args)
#	define shm_free(args...) sfm_free(shm_block, ## args)
#	define shm_realloc(args...) sfm_malloc(shm_block, ## args)
	/* WARNING: test, especially if switched to real realloc */
#	define shm_resize(args...)	sfm_realloc(shm_block, ## args)
#	define shm_info(args...) sfm_info(shm_block, ## args)
#endif /* __SUNPRO_C */
#	define shm_malloc_unsafe  shm_malloc
#	define shm_free_unsafe shm_free
#	define shm_available	sfm_available(shm_block)
#	define shm_status() sfm_status(shm_block)
#	define shm_sums() do{}while(0)
#	define shm_malloc_init sfm_malloc_init
#	define shm_malloc_destroy(b) sfm_malloc_destroy(b)
#	define shm_malloc_on_fork()	sfm_pool_reset()
#elif SF_MALLOC
#	include "sf_malloc.h"
#	define SHM_SAFE_MALLOC /* no need to lock */
	extern struct sfm_block* shm_block;
#ifdef __SUNPRO_C
#	define shm_malloc(...) sfm_malloc(shm_block, __VA_ARGS__)
#	define shm_free(...) sfm_free(shm_block, __VA_ARGS__)
#	define shm_realloc(...) sfm_malloc(shm_block, __VA_ARGS__)
	/* WARNING: test, especially if switched to real realloc */
#	define shm_resize(...)	sfm_realloc(shm_block, __VA_ARGS__)
#	define shm_info(...) sfm_info(shm_block, __VA_ARGS__)
#else /* __SUNPRO_C */
#	define shm_malloc(args...) sfm_malloc(shm_block, ## args)
#	define shm_free(args...) sfm_free(shm_block, ## args)
#	define shm_realloc(args...) sfm_malloc(shm_block, ## args)
	/* WARNING: test, especially if switched to real realloc */
#	define shm_resize(args...)	sfm_realloc(shm_block, ## args)
#	define shm_info(args...) sfm_info(shm_block, ## args)
#endif /* __SUNPRO_C */
#	define shm_malloc_unsafe  shm_malloc
#	define shm_free_unsafe shm_free
#	define shm_available	sfm_available(shm_block)
#	define shm_status() sfm_status(shm_block)
#	define shm_sums() do{}while(0)
#	define shm_malloc_init sfm_malloc_init
#	define shm_malloc_destroy(b) sfm_malloc_destroy(b)
#	define shm_malloc_on_fork()	sfm_pool_reset()
#elif defined F_MALLOC
#	include "f_malloc.h"
	extern struct fm_block* shm_block;
#	define MY_MALLOC fm_malloc
#	define MY_FREE fm_free
#	define MY_REALLOC fm_realloc
#	define MY_STATUS fm_status
#	define MY_MEMINFO	fm_info
#	define MY_SUMS	fm_sums
#	define  shm_malloc_init fm_malloc_init
#	define shm_malloc_destroy(b) do{}while(0)
#	define shm_available() fm_available(shm_block)
#	define shm_malloc_on_fork() do{}while(0)
#elif defined DL_MALLOC
#	include "dl_malloc.h"
	extern mspace shm_block;
#	define MY_MALLOC mspace_malloc
#	define MY_FREE mspace_free
#	define MY_REALLOC mspace_realloc
#	define MY_STATUS(...) 0
#	define MY_SUMS do{}while(0)
#	define MY_MEMINFO	mspace_info
#	define  shm_malloc_init(buf, len, type) create_mspace_with_base(buf, len, 0)
#	define shm_malloc_destroy(b) do{}while(0)
#	define shm_malloc_on_fork() do{}while(0)
#elif defined TLSF_MALLOC
#	include "tlsf.h"
	extern pool_t shm_block;
#	define MY_MALLOC tlsf_malloc
#	define MY_FREE tlsf_free
#	define MY_REALLOC tlsf_realloc
#	define MY_STATUS tlsf_status
#	define MY_MEMINFO	tlsf_meminfo
#	define MY_SUMS tlsf_sums
#	define shm_malloc_init(mem, bytes, type) tlsf_create_with_pool((void*) mem, bytes)
#	define shm_malloc_destroy(b) do{}while(0)
#	define shm_available() tlsf_available(shm_block)
#	define shm_malloc_on_fork() do{}while(0)
#else
#	include "q_malloc.h"
	extern struct qm_block* shm_block;
#	define MY_MALLOC qm_malloc
#	define MY_FREE qm_free
#	define MY_REALLOC qm_realloc
#	define MY_STATUS qm_status
#	define MY_MEMINFO	qm_info
#	define MY_SUMS	qm_sums
#	define  shm_malloc_init qm_malloc_init
#	define shm_malloc_destroy(b) do{}while(0)
#	define shm_available() qm_available(shm_block)
#	define shm_malloc_on_fork() do{}while(0)
#endif

#ifndef SHM_SAFE_MALLOC
	extern gen_lock_t* mem_lock;
#endif


int shm_mem_init(int); /* calls shm_getmem & shm_mem_init_mallocs */
int shm_getmem(void);   /* allocates the memory (mmap or sysv shmap) */
int shm_mem_init_mallocs(void* mempool, unsigned long size); /* initialize
																the mallocs
																& the lock */
void shm_mem_destroy(void);



#ifdef SHM_SAFE_MALLOC
#define shm_lock() do{}while(0)
#define shm_unlock() do{}while(0)

#else /* ! SHM_SAFE_MALLOC */

#define shm_lock()    lock_get(mem_lock)
#define shm_unlock()  lock_release(mem_lock)

#ifdef DBG_QM_MALLOC

#include "src_loc.h"

#define shm_malloc_unsafe(_size ) \
	MY_MALLOC(shm_block, (_size), _SRC_LOC_, _SRC_FUNCTION_, _SRC_LINE_ )


inline static void* _shm_malloc(unsigned int size, 
	const char *file, const char *function, int line )
{
	void *p;
	
	shm_lock();
	p=MY_MALLOC(shm_block, size, file, function, line );
	shm_unlock();
	return p; 
}


inline static void* _shm_realloc(void *ptr, unsigned int size, 
		const char* file, const char* function, int line )
{
	void *p;
	shm_lock();
	p=MY_REALLOC(shm_block, ptr, size, file, function, line);
	shm_unlock();
	return p;
}

#define shm_malloc( _size ) _shm_malloc((_size), \
	_SRC_LOC_, _SRC_FUNCTION_, _SRC_LINE_ )

#define shm_realloc( _ptr, _size ) _shm_realloc( (_ptr), (_size), \
	_SRC_LOC_, _SRC_FUNCTION_, _SRC_LINE_ )



#define shm_free_unsafe( _p  ) \
	MY_FREE( shm_block, (_p), _SRC_LOC_, _SRC_FUNCTION_, _SRC_LINE_ )

#define shm_free(_p) \
do { \
		shm_lock(); \
		shm_free_unsafe( (_p)); \
		shm_unlock(); \
}while(0)



void* _shm_resize(void* ptr, unsigned int size, const char* f, const char* fn,
					int line);
#define shm_resize(_p, _s ) _shm_resize((_p), (_s), \
		_SRC_LOC_, _SRC_FUNCTION_, _SRC_LINE_ )
/*#define shm_resize(_p, _s ) shm_realloc( (_p), (_s))*/



#else /*DBQ_QM_MALLOC*/


#define shm_malloc_unsafe(_size) MY_MALLOC(shm_block, (_size))

inline static void* shm_malloc(unsigned int size)
{
	void *p;
	
	shm_lock();
	p=shm_malloc_unsafe(size);
	shm_unlock();
	 return p; 
}


inline static void* shm_realloc(void *ptr, unsigned int size)
{
	void *p;
	shm_lock();
	p=MY_REALLOC(shm_block, ptr, size);
	shm_unlock();
	return p;
}



#define shm_free_unsafe( _p ) MY_FREE(shm_block, (_p))

#define shm_free(_p) \
do { \
		shm_lock(); \
		shm_free_unsafe( _p ); \
		shm_unlock(); \
}while(0)



void* _shm_resize(void* ptr, unsigned int size);
#define shm_resize(_p, _s) _shm_resize( (_p), (_s))
/*#define shm_resize(_p, _s) shm_realloc( (_p), (_s))*/


#endif  /* DBG_QM_MALLOC */


#define shm_status() \
do { \
		shm_lock(); \
		MY_STATUS(shm_block); \
		shm_unlock(); \
}while(0)


#define shm_info(mi) \
do{\
	shm_lock(); \
	MY_MEMINFO(shm_block, mi); \
	shm_unlock(); \
}while(0)

#ifdef MY_SUMS
#define shm_sums() \
	do { \
		shm_lock(); \
		MY_SUMS(shm_block); \
		shm_unlock(); \
	}while(0)
	
#endif /* MY_SUMS */

#endif /* ! SHM_SAFE_MALLOC */

/* multi-process safe version of shm_available()
 */
unsigned long shm_available_safe();


#endif /* shm_mem_h */

#endif /* SHM_MEM */

