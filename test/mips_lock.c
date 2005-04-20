/*
 * $Id$
 * 
 *  simple locking test program
 *  (no paralles stuff)
 * 
 *  Compile with: gcc -D__CPU_i386 -O3 -o mips_lock on x86 machines and
 *                gcc -mips2 -O2 -D__CPU_mips  -o mips_lock on mips machines.
 *                gcc -O3 -D__CPU_alpha -o mips_lock on alphas
 *  -- andrei
 *
 *  
 */

#include <stdio.h>

typedef volatile int fl_lock_t;



int tsl(fl_lock_t* lock)
{
	long val;
	
#ifdef __CPU_alpha
	long tmp=0;
	/* lock low bit set to 1 when the lock is hold and to 0 otherwise */
	asm volatile(
		"1:  ldl %0, %1   \n\t"
		"    blbs %0, 2f  \n\t"  /* optimization if locked */
		"    ldl_l %0, %1 \n\t"
		"    blbs %0, 2f  \n\t" 
		"    lda %2, 1    \n\t"  /* or: or $31, 1, %2 ??? */
		"    stl_c %2, %1 \n\t"
		"    beq %2, 1b   \n\t"
		"    mb           \n\t"
		"2:               \n\t"
		:"=&r" (val), "=m"(*lock), "=r"(tmp)
		:"1"(*lock)  /* warning on gcc 3.4: replace it with m or remove
						it and use +m in the input line ? */
		: "memory"
	);
				
				
#elif defined __CPU_mips
	long tmp=0;
	
	asm volatile(
		".set noreorder\n\t"
		"1:  ll %1, %2   \n\t"
		"    li %0, 1 \n\t"
		"    sc %0, %2  \n\t"
		"    beqz %0, 1b \n\t"
		"    nop \n\t"
		".set reorder\n\t"
		: "=&r" (tmp), "=&r" (val), "=m" (*lock) 
		: "2" (*lock) 
		: "cc"
	);
#elif defined __CPU_i386
	val=1;
	asm volatile( 
		" xchg %b1, %0" : "=q" (val), "=m" (*lock) : "0" (val) 
	);
#else
#error "cpu type not defined, add -D__CPU_<type> when compiling"
#endif
	
	return val;
}



void release_lock(fl_lock_t* lock)
{
#ifdef __CPU_alpha
	asm volatile(
			"    mb          \n\t"
			"    stl $31, %0 \n\t"
			: "=m"(*lock)
			:
			: "memory"  /* because of the mb */
			);  
#elif defined __CPU_mips
	int tmp;
	tmp=0;
	asm volatile(
		".set noreorder \n\t"
		"    sync \n\t"
		"    sw $0, %0 \n\t"
		".set reorder \n\t"
		: /*no output*/  : "m" (*lock) : "memory"
	);
#elif defined __CPU_i386
	asm volatile(
		" movb $0, (%0)" : /*no output*/ : "r"(lock): "memory"
	); 
#else
#error "cpu type not defined, add -D__CPU_<type> when compiling"
#endif
}



int main(int argc, char** argv)
{
	fl_lock_t lock;
	int r;
	
	lock=0;
	printf("starting locking basic tests...\n");
	
	r=tsl(&lock);
	printf(" tsl should return 0                 ... %d\n", r);
	printf("     lock should be 1 now            ... %d\n", lock);
	r=tsl(&lock);
	printf(" tsl should return 1                 ... %d\n", r);
	printf("     lock should still be 1 now      ... %d\n", lock);
	release_lock(&lock);
	printf(" release_lock: lock should be 0 now  ... %d\n", lock);
	printf("trying tsl once more...\n");
	r=tsl(&lock);
	printf(" tsl should return 0                 ... %d\n", r);
	printf("     lock should be 1 now            ... %d\n", lock);
	printf("\ndone.\n");
	return 0;
}
