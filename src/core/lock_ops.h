/*
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

/*!
* \file
* \brief Kamailio core :: Kamailio locking library
* \ingroup core
* \author andrei
* Module: \ref core
*
 *   WARNING: do not include this file directly, use instead locking.h
 *   (unless you don't need to alloc/dealloc locks)
 *
 *
Implements:

	simple locks:
	-------------
	gen_lock_t* lock_init(gen_lock_t* lock); - inits the lock
	void    lock_destroy(gen_lock_t* lock);  - removes the lock (e.g sysv rmid)
	void    lock_get(gen_lock_t* lock);      - lock (mutex down)
	void    lock_release(gen_lock_t* lock);  - unlock (mutex up)
	int     lock_try(gen_lock_t* lock);      - tries to get the lock, returns
	                                            0 on success and !=0 on failure
	
	lock sets: 
	----------
	gen_lock_set_t* lock_set_init(gen_lock_set_t* set);  - inits the lock set
	void lock_set_destroy(gen_lock_set_t* s);        - removes the lock set
	void lock_set_get(gen_lock_set_t* s, int i);     - locks sem i from the set
	void lock_set_release(gen_lock_set_t* s, int i)  - unlocks sem i from the
	                                                   set
	int  lock_set_try(gen_lock_set_t* s, int i);    - tries to lock the sem i,
	                                                  returns 0 on success and
	                                                  !=0 on failure
	
	defines:
	--------
	GEN_LOCK_T_PREFERRED - defined if using  arrays of gen_lock_t is as good as
	                      using a lock set (gen_lock_set_t). 
						  In general is better to have the locks "close" or 
						  inside the protected data structure rather then 
						  having a separate array or lock set. However in some
						  case (e.g. SYSV_LOCKS) is better to use lock sets,
						  either due to lock number limitations, excesive 
						  performance or memory overhead. In this cases
						  GEN_LOCK_T_PREFERRED will not be defined.
	GEN_LOCK_T_UNLIMITED - defined if there is no system imposed limit on
	                       the number of locks (other then the memory).
	GEN_LOCK_SET_T_UNLIMITED
	                      - like above but for the size of a lock set.

WARNING: - lock_set_init may fail for large number of sems (e.g. sysv). 
         - signals are not treated! (some locks are "awakened" by the signals)
*/

#ifndef _lock_ops_h
#define _lock_ops_h

#ifdef USE_FUTEX
#include "futexlock.h"
/* if no native atomic ops support => USE_FUTEX will be undefined */
#endif


#ifdef USE_FUTEX

typedef futex_lock_t gen_lock_t;

#define lock_destroy(lock) /* do nothing */
#define lock_init(lock) futex_init(lock)
#define lock_try(lock)  futex_try(lock)
#define lock_get(lock)  futex_get(lock)
#define lock_release(lock) futex_release(lock)


#elif defined FAST_LOCK
#include "fastlock.h"

typedef fl_lock_t gen_lock_t;


#define lock_destroy(lock) /* do nothing */ 

inline static gen_lock_t* lock_init(gen_lock_t* lock)
{
	init_lock(*lock);
	return lock;
}

#define lock_try(lock) try_lock(lock)
#define lock_get(lock) get_lock(lock)
#define lock_release(lock) release_lock(lock)


#elif defined USE_PTHREAD_MUTEX
#include <pthread.h>

typedef pthread_mutex_t gen_lock_t;

#define lock_destroy(lock) /* do nothing */ 

inline static gen_lock_t* lock_init(gen_lock_t* lock)
{
	if (pthread_mutex_init(lock, 0)==0) return lock;
	else return 0;
}

#define lock_try(lock) pthread_mutex_trylock(lock)
#define lock_get(lock) pthread_mutex_lock(lock)
#define lock_release(lock) pthread_mutex_unlock(lock)



#elif defined USE_POSIX_SEM
#include <semaphore.h>

typedef sem_t gen_lock_t;

#define lock_destroy(lock) /* do nothing */ 

inline static gen_lock_t* lock_init(gen_lock_t* lock)
{
	if (sem_init(lock, 1, 1)<0) return 0;
	return lock;
}

#define lock_try(lock) sem_trywait(lock)
#define lock_get(lock) sem_wait(lock)
#define lock_release(lock) sem_post(lock)


#elif defined USE_SYSV_SEM
#include <sys/ipc.h>
#include <sys/sem.h>

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "dprint.h"
#include "globals.h" /* uid */

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




inline static gen_lock_t* lock_init(gen_lock_t* lock)
{
	union semun su;
	int euid;
	
	euid=geteuid();
	if (uid && uid!=euid)
		seteuid(uid); /* set euid to the cfg. requested one */
	*lock=semget(IPC_PRIVATE, 1, 0700);
	if (uid && uid!=euid)
		seteuid(euid); /* restore it */
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


/* returns 0 if it got the lock, -1 otherwise */
inline static int lock_try(gen_lock_t* lock)
{
	struct sembuf sop;

	sop.sem_num=0;
	sop.sem_op=-1; /* down */
	sop.sem_flg=IPC_NOWAIT; 
tryagain:
	if (semop(*lock, &sop, 1)==-1){
		if (errno==EAGAIN){
			return -1;
		}else if (errno==EINTR){
			DBG("lock_get: signal received while waiting for on a mutex\n");
			goto tryagain;
		}else{
			LM_CRIT("sysv: %s (%d)\n", strerror(errno), errno);
			return -1;
		}
	}
	return 0;
}

inline static void lock_get(gen_lock_t* lock)
{
	struct sembuf sop;

	sop.sem_num=0;
	sop.sem_op=-1; /* down */
	sop.sem_flg=0; 
tryagain:
	if (semop(*lock, &sop, 1)==-1){
		if (errno==EINTR){
			DBG("lock_get: signal received while waiting for on a mutex\n");
			goto tryagain;
		}else{
			LM_CRIT("sysv: %s (%d)\n", strerror(errno), errno);
		}
	}
}

inline static void lock_release(gen_lock_t* lock)
{
	struct sembuf sop;
	
	sop.sem_num=0;
	sop.sem_op=1; /* up */
	sop.sem_flg=0; 
tryagain:
	if (semop(*lock, &sop, 1)==-1){
		if (errno==EINTR){
			/* very improbable*/
			DBG("lock_release: signal received while releasing a mutex\n");
			goto tryagain;
		}else{
			LM_CRIT("sysv: %s (%d)\n", strerror(errno), errno);
		}
	}
}


#else
#error "no locking method selected"
#endif


/* lock sets */

#if defined(FAST_LOCK) || defined(USE_PTHREAD_MUTEX) || \
	defined(USE_POSIX_SEM) || defined(USE_FUTEX)
#define GEN_LOCK_T_PREFERRED
#define GEN_LOCK_T_PREFERED  /* backwards compat. */
#define GEN_LOCK_T_UNLIMITED
#define GEN_LOCK_SET_T_UNLIMITED

struct gen_lock_set_t_ {
	long size;
	gen_lock_t* locks;
}; /* must be  aligned (32 bits or 64 depending on the arch)*/
typedef struct gen_lock_set_t_ gen_lock_set_t;


#define lock_set_destroy(lock_set) /* do nothing */

inline static gen_lock_set_t* lock_set_init(gen_lock_set_t* s)
{
	int r;
	for (r=0; r<s->size; r++) if (lock_init(&s->locks[r])==0) return 0;
	return s;
}

/* WARNING: no boundary checks!*/
#define lock_set_try(set, i) lock_try(&set->locks[i])
#define lock_set_get(set, i) lock_get(&set->locks[i])
#define lock_set_release(set, i) lock_release(&set->locks[i])

#elif defined(USE_SYSV_SEM)
#undef GEN_LOCK_T_PREFERRED
#undef GEN_LOCK_T_PREFERED  /* backwards compat. */
#undef GEN_LOCK_T_UNLIMITED
#undef GEN_LOCK_SET_T_UNLIMITED
#define GEN_LOCK_T_LIMITED
#define GEN_LOCK_SET_T_LIMITED

struct gen_lock_set_t_ {
	int size;
	int semid;
};


typedef struct gen_lock_set_t_ gen_lock_set_t;
inline static gen_lock_set_t* lock_set_init(gen_lock_set_t* s)
{
	union semun su;
	int r;
	int euid;

	euid=geteuid();
	if (uid && uid!=euid)
		seteuid(uid); /* set euid to the cfg. requested one */
	s->semid=semget(IPC_PRIVATE, s->size, 0700);
	if (uid && uid!=euid)
		seteuid(euid); /* restore euid */
	if (s->semid==-1){
		LM_CRIT("(SYSV): semget (..., %d, 0700) failed: %s\n",
				s->size, strerror(errno));
		return 0;
	}
	su.val=1;
	for (r=0; r<s->size; r++){
		if (semctl(s->semid, r, SETVAL, su)==-1){
			LM_CRIT("(SYSV): semctl failed on sem %d: %s\n", r, strerror(errno));
			semctl(s->semid, 0, IPC_RMID, (union semun)(int)0);
			return 0;
		}
	}
	return s;
}

inline static void lock_set_destroy(gen_lock_set_t* s)
{
	semctl(s->semid, 0, IPC_RMID, (union semun)(int)0);
}


/* returns 0 if it "gets" the lock, -1 otherwise */
inline static int lock_set_try(gen_lock_set_t* s, int n)
{
	struct sembuf sop;
	
	sop.sem_num=n;
	sop.sem_op=-1; /* down */
	sop.sem_flg=IPC_NOWAIT; 
tryagain:
	if (semop(s->semid, &sop, 1)==-1){
		if (errno==EAGAIN){
			return -1;
		}else if (errno==EINTR){
			DBG("lock_get: signal received while waiting for on a mutex\n");
			goto tryagain;
		}else{
			LM_CRIT("sysv: %s (%d)\n", strerror(errno), errno);
			return -1;
		}
	}
	return 0;
}


inline static void lock_set_get(gen_lock_set_t* s, int n)
{
	struct sembuf sop;
	sop.sem_num=n;
	sop.sem_op=-1; /* down */
	sop.sem_flg=0;
tryagain:
	if (semop(s->semid, &sop, 1)==-1){
		if (errno==EINTR){
			DBG("lock_set_get: signal received while waiting on a mutex\n");
			goto tryagain;
		}else{
			LM_CRIT("sysv: %s (%d)\n", strerror(errno), errno);
		}
	}
}

inline static void lock_set_release(gen_lock_set_t* s, int n)
{
	struct sembuf sop;
	sop.sem_num=n;
	sop.sem_op=1; /* up */
	sop.sem_flg=0;
tryagain:
	if (semop(s->semid, &sop, 1)==-1){
		if (errno==EINTR){
			/* very improbable */
			DBG("lock_set_release: signal received while releasing mutex\n");
			goto tryagain;
		}else{
			LM_CRIT("sysv: %s (%d)\n", strerror(errno), errno);
		}
	}
}
#else 
#error "no lock set method selected"
#endif


#endif
