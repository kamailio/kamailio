/*
 * fast architecture specific locking
 *
 * $Id$
 *
 * 
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *
 *History:
 *--------
 *  2002-02-05  created by andrei
 *  2003-01-16  added PPC locking code contributed by Dinos Dorkofikis
 *               <kdor@intranet.gr>
 *  2004-09-12  added MIPS locking for ISA>=2 (>r3000)  (andrei)
 *
 */


#ifndef fastlock_h
#define fastlock_h

#ifdef HAVE_SCHED_YIELD
#include <sched.h>
#else
#include <unistd.h>
	/* fake sched_yield */
	#define sched_yield()	sleep(0)
#endif


typedef  volatile int fl_lock_t;



#define init_lock( l ) (l)=0



/*test and set lock, ret 1 if lock held by someone else, 0 otherwise*/
inline static int tsl(fl_lock_t* lock)
{
	int val;

#ifdef __CPU_i386

#ifdef NOSMP
	val=0;
	asm volatile(
		" btsl $0, %1 \n\t"
		" adcl $0, %0 \n\t"
		: "=q" (val), "=m" (*lock) : "0"(val) : "memory", "cc" /* "cc" */
	);
#else
	val=1;
	asm volatile( 
		" xchg %b1, %0" : "=q" (val), "=m" (*lock) : "0" (val) : "memory"
	);
#endif /*NOSMP*/
#elif defined __CPU_sparc64
	asm volatile(
			"ldstub [%1], %0 \n\t"
#ifndef NOSMP
			"membar #StoreStore | #StoreLoad \n\t"
#endif
			: "=r"(val) : "r"(lock):"memory"
	);
	
#elif defined __CPU_arm
	asm volatile(
			"# here \n\t"
			"swpb %0, %1, [%2] \n\t"
			: "=r" (val)
			: "r"(1), "r" (lock) : "memory"
	);
	
#elif defined __CPU_ppc
	asm volatile(
			"1: lwarx  %0, 0, %2\n\t"
			"   cmpwi  %0, 0\n\t"
			"   bne    0f\n\t"
			"   stwcx. %1, 0, %2\n\t"
			"   bne-   1b\n\t"
			"0:\n\t"
			: "=r" (val)
			: "r"(1), "b" (lock) :
			"memory", "cc"
        );
#elif defined __CPU_mips2
	long tmp;
	tmp=1; /* just to kill a gcc 2.95 warning */
	
	asm volatile(
		".set noreorder\n\t"
		"1:  ll %1, %2   \n\t"
		"    li %0, 1 \n\t"
		"    sc %0, %2  \n\t"
		"    beqz %0, 1b \n\t"
		"    nop \n\t"
		".set reorder\n\t"
		: "=&r" (tmp), "=&r" (val), "=m" (*lock) 
		: "0" (tmp), "2" (*lock) 
		: "cc"
	);
#else
#error "unknown architecture"
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
#ifdef __CPU_i386
	char val;
	val=0;
	asm volatile(
		" movb $0, (%0)" : /*no output*/ : "r"(lock): "memory"
		/*" xchg %b0, %1" : "=q" (val), "=m" (*lock) : "0" (val) : "memory"*/
	); 
#elif defined __CPU_sparc64
	asm volatile(
#ifndef NOSMP
			"membar #LoadStore | #StoreStore \n\t" /*is this really needed?*/
#endif
			"stb %%g0, [%0] \n\t"
			: /*no output*/
			: "r" (lock)
			: "memory"
	);
#elif defined __CPU_arm
	asm volatile(
		" str %0, [%1] \n\r" 
		: /*no outputs*/ 
		: "r"(0), "r"(lock)
		: "memory"
	);
#elif defined __CPU_ppc
	asm volatile(
			"sync\n\t"
			"stw %0, 0(%1)\n\t"
			: /* no output */
			: "r"(0), "b" (lock)
			: "memory"
        );
	*lock = 0;
#elif defined __CPU_mips2
		asm volatile(
		".set noreorder \n\t"
		"    sync \n\t"
		"    sw $0, %0 \n\t"
		".set reorder \n\t"
		: /*no output*/  : "m" (*lock) : "memory"
	);
#else
#error "unknown architecture"
#endif
}



#endif
