/* $Id$*
 *
 * shared mem stuff
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
#elif defined F_MALLOC
#	include "f_malloc.h"
	extern struct fm_block* shm_block;
#	define MY_MALLOC fm_malloc
#	define MY_FREE fm_free
#	define MY_STATUS fm_status
#else
#	include "q_malloc.h"
	extern struct qm_block* shm_block;
#	define MY_MALLOC qm_malloc
#	define MY_FREE qm_free
#	define MY_STATUS qm_status
#endif

#ifdef FAST_LOCK
#include "../fastlock.h"
	
	extern fl_lock_t* mem_lock;
#else
extern  int shm_semid;
#endif


int shm_mem_init();
void shm_mem_destroy();


#ifdef FAST_LOCK

#define shm_lock()    get_lock(mem_lock)
#define shm_unlock()  release_lock(mem_lock)

#else
/* inline functions (do not move them to *.c, they won't be inlined anymore) */
static inline void shm_lock()
{

	struct sembuf sop;
	int ret;
	
	sop.sem_num=0;
	sop.sem_op=-1; /*down*/
	sop.sem_flg=0 /*SEM_UNDO*/;
again:
	ret=semop(shm_semid, &sop, 1);

	switch(ret){
		case 0: /*ok*/
			break;
		case EINTR: /*interrupted by signal, try again*/
			DBG("sh_lock: interrupted by signal, trying again...\n");
			goto again;
		default:
			LOG(L_ERR, "ERROR: sh_lock: error waiting on semaphore: %s\n",
					strerror(errno));
	}
}



static inline void shm_unlock()
{
	struct sembuf sop;
	int ret;
	
	sop.sem_num=0;
	sop.sem_op=1; /*up*/
	sop.sem_flg=0 /*SEM_UNDO*/;
again:
	ret=semop(shm_semid, &sop, 1);
	/*should ret immediately*/
	switch(ret){
		case 0: /*ok*/
			break;
		case EINTR: /*interrupted by signal, try again*/
			DBG("sh_lock: interrupted by signal, trying again...\n");
			goto again;
		default:
			LOG(L_ERR, "ERROR: sh_lock: error waiting on semaphore: %s\n",
					strerror(errno));
	}
}

/* ret -1 on erro*/
#endif



#ifdef DBG_QM_MALLOC

#define shm_malloc_unsafe(_size ) \
	MY_MALLOC(shm_block, (_size), __FILE__, __FUNCTION__, __LINE__ )


inline static void* _shm_malloc(unsigned int size, 
	char *file, char *function, int line )
{
	void *p;
	
	shm_lock();\
	p=MY_MALLOC(shm_block, size, file, function, line );
	shm_unlock();
	return p; 
}

#define shm_malloc( _size ) _shm_malloc((_size), \
	__FILE__, __FUNCTION__, __LINE__ )



#define shm_free_unsafe( _p  ) \
	MY_FREE( shm_block, (_p), __FILE__, __FUNCTION__, __LINE__ )

#define shm_free(_p) \
do { \
		shm_lock(); \
		shm_free_unsafe( (_p)); \
		shm_unlock(); \
}while(0)

void* _shm_resize( void*, unsigned int, char*, char*, unsigned int);
#define shm_resize(_p, _s ) \
	_shm_resize( (_p), (_s),   __FILE__, __FUNCTION__, __LINE__)



#else


#define shm_malloc_unsafe(_size) MY_MALLOC(shm_block, (_size))

inline static void* shm_malloc(unsigned int size)
{
	void *p;
	
	shm_lock();
	p=shm_malloc_unsafe(size);
	shm_unlock();
	 return p; 
}


void* _shm_resize( void*, unsigned int);

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
		/*shm_lock();*/ \
		MY_STATUS(shm_block); \
		/*shm_unlock();*/ \
}while(0)




#endif

#endif

