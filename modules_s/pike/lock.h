#ifndef _PIKE_LOCK_H
#define _PIKE_LOCK_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#ifdef  FAST_LOCK
#include "../../fastlock.h"
#endif



#ifdef FAST_LOCK
#define pike_lock fl_lock_t
#else
typedef typedef struct {
	int semaphore_set;
	int semaphore_index;
} pike_lock;
#endif



#ifndef FAST_LOCK
int change_semaphore( pike_lock *s  , int val );
#endif

pike_lock* create_semaphores(int nr);
void destroy_semaphores(pike_lock *sem_set);



/* lock semaphore s */
static inline int lock( pike_lock *s )
{
#ifdef FAST_LOCK
	get_lock(s);
	return 0;
#else
	return change_semaphore( s, -1 );
#endif
}



static inline int unlock( pike_lock *s )
{
#ifdef FAST_LOCK
	release_lock(s);
	return 0;
#else
	return change_semaphore( s, +1 );
#endif
}



#endif
