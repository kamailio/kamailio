#ifndef _PIKE_LOCK_H
#define _PIKE_LOCK_Hi

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#ifdef  FAST_LOCK
#include "../../fastlock.h"
#endif



#ifdef FAST_LOCK
#define ser_lock_t fl_lock_t
#else
typedef struct {
	int semaphore_set;
	int semaphore_index;
} ser_lock_t;
#endif



#ifndef FAST_LOCK
int change_semaphore( ser_lock_t* s  , int val );
#endif

ser_lock_t* create_semaphores(int nr);
void destroy_semaphores(ser_lock_t *sem_set);



/* lock semaphore s */
static inline int lock( ser_lock_t* s )
{
#ifdef FAST_LOCK
	get_lock(s);
	return 0;
#else
	return change_semaphore( s, -1 );
#endif
}



static inline int unlock( ser_lock_t* s )
{
#ifdef FAST_LOCK
	release_lock(s);
	return 0;
#else
	return change_semaphore( s, +1 );
#endif
}



#endif
