/* 
 * $Id$
 *
 */

#ifndef _SMART_LOCK_H_
#define _SMART_LOCK_H_

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "../../dprint.h"
#ifdef  FAST_LOCK
#include "../../fastlock.h"
#endif



#ifdef FAST_LOCK
#define smart_lock fl_lock_t
#else
typedef struct _smart_lock{
	int semaphore_set;
	int semaphore_index;
} smart_lock;
#endif



#ifndef FAST_LOCK
int change_semaphore( smart_lock *s  , int val );
#endif

smart_lock* create_semaphores(int nr);
void destroy_semaphores(smart_lock *sem_set);



/* lock semaphore s */
static inline int s_lock( smart_lock *s )
{
#ifdef FAST_LOCK
	get_lock(s);
	return 0;
#else
	return change_semaphore( s, -1 );
#endif
}


/* ulock semaphore */
static inline int s_unlock( smart_lock *s )
{
#ifdef FAST_LOCK
	release_lock(s);
	return 0;
#else
	return change_semaphore( s, +1 );
#endif
}

/* lock semaphore s */
static inline int s_lock_at(smart_lock *s, int i)
{
	DBG("JABBER: s_lock_at: <%d>\n", i);
#ifdef FAST_LOCK
	get_lock(&s[i]);
	return 0;
#else
	return change_semaphore( &s[i], -1 );
#endif
}


/* ulock semaphore */
static inline int s_unlock_at(smart_lock *s, int i)
{
	DBG("JABBER: s_unlock_at: <%d>\n", i);
#ifdef FAST_LOCK
	release_lock(&s[i]);
	return 0;
#else
	return change_semaphore( &s[i], +1 );
#endif
}

#endif
