/*
 *
 *  simple atomic ops testing program
 *  (no parallel stuff, just see if the opcodes are "legal")
 * 
 *  Compile with: gcc -Wall -O3 -D__CPU_i386  on x86 machines
 *                gcc -Wall -O3 -D__CPU_x86_64 on amd64 machines
 *                gcc -mips2 -Wall -O2 -D__CPU_mips2  on mips machines
 *                gcc -m64 -Wall -O2 -D__CPU_mips64 on mips64 machines
 *                gcc -O3 -Wall -D__CPU_ppc on powerpc machines
 *                gcc -m64 -O3 -Wall -D__CPU_ppc64 on powerpc machines
 *                gcc -m64 -O3 -Wall -D__CPU_sparc64 -DSPARC64_MODE on 
 *                                                   ultrasparc machines
 *  -- andrei
 *
 *  
 */

#include <stdio.h>

#define CC_GCC_LIKE_ASM

#include "../atomic_ops.h"

#ifdef ATOMIC_OPS_USE_LOCK 
/* hack to make lock work */
#include "../lock_ops.h"

gen_lock_t* _atomic_lock;

gen_lock_t dummy_lock;

#endif

int main(int argc, char** argv)
{
	int r;
	atomic_t v;
#ifdef ATOMIC_OPS_USE_LOCK
	/* init the lock (emulate atomic_ops.c) */
	_atomic_lock=&dummy_lock;
	if (lock_init(_atomic_lock)==0){
		fprintf(stderr, "ERROR: failed to initialize the lock\n");
		goto error;
	}
#endif
	
#ifdef NOSMP
	printf("no-smp mode\n");
#else
	printf("smp mode\n");
#endif
	
	printf("starting memory barrier opcode tests...\n");
	membar();
	printf(" membar() .............................. ok\n");
	membar_write();
	printf(" membar_write() ........................ ok\n");
	membar_read();
	printf(" membar_read() ......................... ok\n");
	
	printf("\nstarting atomic ops basic tests...\n");
	
	mb_atomic_set(&v, 1);
	printf(" atomic_set, v should be 1 ............. %2d\n", mb_atomic_get(&v));
	mb_atomic_inc(&v);
	printf(" atomic_inc, v should be 2 ............. %2d\n", mb_atomic_get(&v));
	r=mb_atomic_inc_and_test(&v);
	printf(" atomic_inc_and_test, v should be  3 ... %2d\n", mb_atomic_get(&v));
	printf("                      r should be  0 ... %2d\n", r);
	
	mb_atomic_dec(&v);
	printf(" atomic_dec, v should be 2 ............. %2d\n", mb_atomic_get(&v));
	r=mb_atomic_dec_and_test(&v);
	printf(" atomic_dec_and_test, v should be  1 ... %2d\n", mb_atomic_get(&v));
	printf("                      r should be  0 ... %2d\n", r);
	r=mb_atomic_dec_and_test(&v);
	printf(" atomic_dec_and_test, v should be  0 ... %2d\n", mb_atomic_get(&v));
	printf("                      r should be  1 ... %2d\n", r);
	r=mb_atomic_dec_and_test(&v);
	printf(" atomic_dec_and_test, v should be -1 ... %2d\n", mb_atomic_get(&v));
	printf("                      r should be  0 ... %2d\n", r);
	
	mb_atomic_and(&v, 2);
	printf(" atomic_and, v should be 2 ............. %2d\n", mb_atomic_get(&v));
	
	mb_atomic_or(&v, 5);
	r=mb_atomic_get_and_set(&v, 0);
	printf(" atomic_or,  v should be 7 ............. %2d\n", r);
	printf(" atomic_get_and_set, v should be 0 ..... %2d\n", mb_atomic_get(&v));

	
	printf("\ndone.\n");
#ifdef ATOMIC_OPS_USE_LOCK
	lock_destroy(_atomic_lock);
#endif
	return 0;
#ifdef ATOMIC_OPS_USE_LOCK
error:
	return -1;
#endif
}
