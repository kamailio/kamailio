/*
 *
 *  simple atomic ops testing program
 *  (no paralles stuff)
 * 
 *  Compile with: gcc -D__CPU_i386 -O3 on x86 machines and
 *                gcc -mips2 -O2 -D__CPU_mips2  on mips machines.
 *  -- andrei
 *
 *  
 */

#include <stdio.h>
#include "../atomic_ops.h"



int main(int argc, char** argv)
{
	int r;
	atomic_t v;
#ifdef NOSMP
	printf("no-smp mode\n");
#else
	printf("smp mode\n");
#endif
	
	printf("\nstarting memory barrier opcode tests...\n");
	membar();
	printf(" membar() .............................. ok\n");
	membar_write();
	printf(" membar_write() ........................ ok\n");
	membar_read();
	printf(" membar_read() ......................... ok\n");
	
	printf("\nstarting atomic ops basic tests...\n");
	
	atomic_set(&v, 1);
	printf(" atomic_set, v should be 1 ............. %2ld\n", atomic_get(&v));
	atomic_inc(&v);
	printf(" atomic_inc, v should be 2 ............. %2ld\n", atomic_get(&v));
	r=atomic_inc_and_test(&v);
	printf(" atomic_inc_and_test, v should be  3 ... %2ld\n", atomic_get(&v));
	printf("                      r should be  0 ... %2d\n", r);
	
	atomic_dec(&v);
	printf(" atomic_dec, v should be 2 ............. %2ld\n", atomic_get(&v));
	r=atomic_dec_and_test(&v);
	printf(" atomic_dec_and_test, v should be  1 ... %2ld\n", atomic_get(&v));
	printf("                      r should be  0 ... %2d\n", r);
	r=atomic_dec_and_test(&v);
	printf(" atomic_dec_and_test, v should be  0 ... %2ld\n", atomic_get(&v));
	printf("                      r should be  1 ... %2d\n", r);
	r=atomic_dec_and_test(&v);
	printf(" atomic_dec_and_test, v should be -1 ... %2ld\n", atomic_get(&v));
	printf("                      r should be  0 ... %2d\n", r);
	
	atomic_and(&v, 2);
	printf(" atomic_and, v should be 2 ............. %2ld\n", atomic_get(&v));
	
	atomic_or(&v, 5);
	printf(" atomic_or,  v should be 7 ............. %2ld\n", atomic_get(&v));
	
	printf("\ndone.\n");
	return 0;
}
