/*
 * fast arhitecture specific locking
 *
 * $Id$
 *
 * 
 */



#ifndef fastlock_h
#define fastlock_h


#include <sched.h>


#ifdef __i386


typedef  volatile int lock_t;



#define init_lock( l ) (l)=0



/*test and set lock, ret 1 if lock held by someone else, 0 otherwise*/
inline static int tsl(lock_t* lock)
{
	volatile char val;
	
	val=1;
	asm volatile( 
		" xchg %b0, %1" : "=q" (val), "=m" (*lock) : "0" (val) : "memory"
	);
	return val;
}



inline static void get_lock(lock_t* lock)
{
	
	while(tsl(lock)){
		sched_yield();
	}
}



inline static void release_lock(lock_t* lock)
{
	char val;

	val=0;
	asm volatile(
		" xchg %b0, %1" : "=q" (val), "=m" (*lock) : "0" (val) : "memory"
	);
}

#endif


#endif
