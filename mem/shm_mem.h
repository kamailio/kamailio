/* $Id$*
 *
 * shared mem stuff
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



#include "../dprint.h"

#ifdef VQ_MALLOC
#	include "vq_malloc.h"
	extern struct vqm_block* shm_block;
#	define MY_MALLOC vqm_malloc
#	define MY_FREE vqm_free
#	define MY_STATUS vqm_status
#else
#	include "q_malloc.h"
	extern struct qm_block* shm_block;
#	define MY_MALLOC qm_malloc
#	define MY_FREE qm_free
#	define MY_STATUS qm_status
#endif
extern int shm_semid;

int shm_mem_init();
void shm_mem_destroy();
inline void shm_lock();
inline void shm_unlock();




#ifdef DBG_QM_MALLOC

#define shm_malloc_unsafe(_size ) \
	MY_MALLOC(shm_block, (_size), __FILE__, __FUNCTION__, __LINE__ )
#define shm_malloc(_size) \
({\
	void *p;\
	\
	shm_lock();\
	p=shm_malloc_unsafe( (_size) );\
	shm_unlock();\
	p; \
})

#define shm_free_unsafe( _p  ) \
	MY_FREE( shm_block, (_p), __FILE__, __FUNCTION__, __LINE__ )
#define shm_free(_p) \
do { \
		shm_lock(); \
		shm_free_unsafe( (_p)); \
		shm_unlock(); \
}while(0)

#define shm_resize(_p, _s ) \
	_shm_resize( (_p), (_s),   __FILE__, __FUNCTION__, __LINE__)

#else

#define shm_malloc_unsafe(_size) MY_MALLOC(shm_block, (_size))
#define shm_malloc(size) \
({\
	void *p;\
	\
		shm_lock();\
		p=shm_malloc_unsafe(size); \
		shm_unlock();\
	 p; \
})


#define shm_free_unsafe( _p ) MY_FREE(shm_block, (_p))
#define shm_free(_p) \
do { \
		shm_lock(); \
		shm_free_unsafe( _p ); \
		shm_unlock(); \
}while(0)

#define shm_resize(_p, _s) _shm_resize( (_p), (_s))

#endif


#define shm_status() \
do { \
		shm_lock(); \
		MY_STATUS(shm_block); \
		shm_unlock(); \
}while(0)




#endif

#endif

