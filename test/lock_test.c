/*
 *
 *  simple locking test program
 *  (no paralles stuff)
 * 
 *  Compile with: gcc -D__CPU_i386 -O3 on x86 machines and
 *                gcc -mips2 -O2 -D__CPU_mips2  on mips machines.
 *  -- andrei
 *
 *  
 */

#include <stdio.h>
#include "../fastlock.h"



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
