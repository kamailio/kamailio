/* $Id$ */
/*
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
 *   ser locking library
 *
 *  2002-12-16  created by andrei
 *  2003-02-20  s/gen_lock_t/gen_lock_t/ to avoid a type conflict 
 *               on solaris  (andrei)
 *
Implements:

	gen_lock_t* lock_alloc();               - allocates a lock in shared mem.
	gen_lock_t* lock_init(gen_lock_t* lock);    - inits the lock
	void    lock_destroy(gen_lock_t* lock); - removes the lock (e.g sysv rmid)
	void    lock_dealloc(gen_lock_t* lock); - deallocates the lock's shared m.
	void    lock_get(gen_lock_t* lock);     - lock (mutex down)
	void    lock_release(gen_lock_t* lock); - unlock (mutex up)
*/

#ifndef _locking_h
#define _locking_h


#ifdef FAST_LOCK
#include "fastlock.h"

typedef fl_lock_t gen_lock_t;

#define lock_alloc() shm_malloc(sizeof(gen_lock_t))
#define lock_destroy(lock) /* do nothing */ 
#define lock_dealloc(lock) shm_free(lock)

inline static gen_lock_t* lock_init(gen_lock_t* lock)
{
	init_lock(*lock);
	return lock;
}

#define lock_get(lock) get_lock(lock)
#define lock_release(lock) release_lock(lock)



#elif defined USE_PTHREAD_MUTEX
#include <pthread.h>

typedef pthread_mutex_t gen_lock_t;

#define lock_alloc() shm_malloc(sizeof(gen_lock_t))
#define lock_destroy(lock) /* do nothing */ 
#define lock_dealloc(lock) shm_free(lock)

inline static gen_lock_t* lock_init(gen_lock_t* lock)
{
	if (pthread_mutex_init(lock, 0)==0) return lock;
	else return 0;
}

#define lock_get(lock) pthread_mutex_lock(lock)
#define lock_release(lock) pthread_mutex_unlock(lock)



#elif defined USE_POSIX_SEM
#include <semaphore.h>

typedef sem_t gen_lock_t;

#define lock_alloc() shm_malloc(sizeof(gen_lock_t))
#define lock_destroy(lock) /* do nothing */ 
#define lock_dealloc(lock) shm_free(lock)

inline static gen_lock_t* lock_init(gen_lock_t* lock)
{
	if (sem_init(lock, 1, 1)<0) return 0;
	return lock;
}

#define lock_get(lock) sem_wait(lock)
#define lock_release(lock) sem_post(lock)


#elif defined USE_SYSV_SEM
#include <sys/ipc.h>
#include <sys/sem.h>

#if ((defined(HAVE_UNION_SEMUN) || defined(__GNU_LIBRARY__) )&& !defined(_SEM_SEMUN_UNDEFINED)) 
	
	/* union semun is defined by including sem.h */
#else
	/* according to X/OPEN we have to define it ourselves */
	union semun {
		int val;                      /* value for SETVAL */
		struct semid_ds *buf;         /* buffer for IPC_STAT, IPC_SET */
		unsigned short int *array;    /* array for GETALL, SETALL */
		struct seminfo *__buf;        /* buffer for IPC_INFO */
	};
#endif

typedef int gen_lock_t;

#define lock_alloc() shm_malloc(sizeof(gen_lock_t))
#define lock_dealloc(lock) shm_free(lock)


inline static gen_lock_t* lock_init(gen_lock_t* lock)
{
	union semun su;
	
	*lock=semget(IPC_PRIVATE, 1, 0700);
	if (*lock==-1) return 0;
	su.val=1;
	if (semctl(*lock, 0, SETVAL, su)==-1){
		/* init error*/
		return 0;
	}
	return lock;
}

inline static void lock_destroy(gen_lock_t* lock)
{
	semctl(*lock, 0, IPC_RMID, (union semun)(int)0);
}

#define lock_dealloc(lock) shm_free(lock)

inline static void lock_get(gen_lock_t* lock)
{
	struct sembuf sop;

	sop.sem_num=0;
	sop.sem_op=-1; /* down */
	sop.sem_flg=0; /*SEM_UNDO*/
	semop(*lock, &sop, 1);
}

inline static void lock_release(gen_lock_t* lock)
{
	struct sembuf sop;
	
	sop.sem_num=0;
	sop.sem_op=1; /* up */
	sop.sem_flg=0; /* SEM_UNDO*/
	semop(*lock, &sop, 1);
}

#else
#error "no locking method selected"
#endif


/*shm_{malloc, free}*/
#include "mem/mem.h"
#ifdef SHM_MEM
#include "mem/shm_mem.h"
#else
#error "locking requires shared memroy support"
#endif

#endif
