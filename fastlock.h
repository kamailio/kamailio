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




typedef  volatile int fl_lock_t;



#define init_lock( l ) (l)=0



/*test and set lock, ret 1 if lock held by someone else, 0 otherwise*/
inline static int tsl(fl_lock_t* lock)
{
	volatile int val;

#ifdef __i386

#ifdef NOSMP
	val=0;
	asm volatile(
		"  btsl $0, %1 \n\t"
		" adcl $0, %0 \n\t"
		: "=q" (val), "=m" (*lock) : "0"(val) : "memory"
	);
#else
	val=1;
	asm volatile( 
		" xchg %b0, %1" : "=q" (val), "=m" (*lock) : "0" (val) : "memory"
	);
#endif /*NOSMP*/
#elif defined __sparc
	asm volatile(
			"ldstub [%1], %0 \n\t"
#ifndef NOSMP
			"membar #StoreStore | #StoreLoad \n\t"
#endif
			: "=r"(val) : "r"(lock):"memory"
	);
#else
#error "unknown arhitecture"
#endif
	return val;
}



inline static void get_lock(fl_lock_t* lock)
{
#ifdef ADAPTIVE_WAIT
	int i=ADAPTIVE_WAIT_LOOPS;
#endif
	
	while(tsl(lock)){
#ifdef BUSY_WAIT
#elif defined ADAPTIVE_WAIT
		if (i>0) i--;
		else sched_yield();
#else
		sched_yield();
#endif
	}
}



inline static void release_lock(fl_lock_t* lock)
{
#ifdef __i386
	char val;
	val=0;
	asm volatile(
		" movb $0, (%0)" : /*no output*/ : "r"(lock): "memory"
		/*" xchg %b0, %1" : "=q" (val), "=m" (*lock) : "0" (val) : "memory"*/
	); 
#elif defined __sparc
	asm volatile(
#ifndef NOSMP
			"membar #LoadStore | #StoreStore \n\t" /*is this really needed?*/
#endif
			"stb %%g0, [%0] \n\t"
			: /*no output*/
			: "r" (lock)
			: "memory"
	);
#else
#error "unknown arhitecture"
#endif
}



#endif
