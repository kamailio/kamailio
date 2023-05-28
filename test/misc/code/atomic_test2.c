/*
 *
 *  simple atomic ops testing program
 *  (no parallel stuff, just see if the opcodes are "legal")
 *
 *  Defines: TYPE - not defined => use atomic_t and the corresponding
 *                  atomic functions
 *                - long => use volatile long* and  the atomic_*_long functions
 *                - int  => use volatile int* and the atomic_*_int functions
 *           MEMBAR - if defined use mb_atomic_* instead of atomic_*
 *           NOSMP - use non smp versions
 *           NOASM - don't use asm inline version
 *           __CPU_xxx - use __CPU_xxx code
 *           SPARC64_MODE - compile for a sparc 64 in 64 bit mode (gcc -m64
 *                          must be used on solaris in this case)
 *  Example:  
 *    gcc -Wall -O3 -D__CPU_i386 -DNOSMP -DMEMBAR -DTYPE=long atomic_test2.c
 * 
 *  Compile with: gcc -Wall -O3 -D__CPU_i386  ... on x86 machines
 *                gcc -Wall -O3 -D__CPU_x86_64 ... on amd64 machines
 *                gcc -mips2 -Wall -O2 -D__CPU_mips2  ... on mips machines
 *                gcc -m64 -Wall -O2 -D__CPU_mips64 ... on mips64 machines
 *                gcc -O3 -Wall -D__CPU_ppc ... on powerpc machines
 *                gcc -m64 -O3 -Wall -D__CPU_ppc64 ... on powerpc machines
 *                gcc -m64 -O3 -Wall -D__CPU_sparc64 -DSPARC64_MODE ... on 
 *                                                   ultrasparc machines
 *                gcc -mcpu=v9 -O3 -Wall -D__CPU_sparc64  ... for 32 bit code 
 *                                                   (sparc32plus) on 
 *                                                   ultrasparc machines
 *                gcc -O3 -Wall -D__CPU_sparc ... on sparc v8 machines
 *  -- andrei
 *
 *  
 */

#include <stdio.h>

#ifndef NOASM
#define CC_GCC_LIKE_ASM
#endif

#include "../atomic_ops.h"

#if defined ATOMIC_OPS_USE_LOCK  || defined MEMBAR_USES_LOCK || \
	defined ATOMIC_OPS_USE_LOCK_SET
/* hack to make lock work */
#include "../lock_ops.h"
#endif

#ifdef MEMBAR_USES_LOCK
gen_lock_t* __membar_lock=0; /* init in atomic_ops.c */
gen_lock_t dummy_membar_lock;
#endif

#ifdef ATOMIC_OPS_USE_LOCK_SET
gen_lock_set_t* _atomic_lock_set=0;
gen_lock_set_t dummy_atomic_lock_set;
gen_lock_t locks_array[_ATOMIC_LS_SIZE];
#elif defined ATOMIC_OPS_USE_LOCK
gen_lock_t* _atomic_lock=0;
gen_lock_t dummy_atomic_lock;
#endif /* ATOMIC_OPS_USE_LOCK / _SET */




#if defined MB || defined MEMBAR
#undef MB
#define MB mb_
#define MEMBAR_STR "membar "
#else
#define MB  /* empty */
#define MEMBAR_STR ""
#endif

#ifndef TYPE
#define SUF
#define ATOMIC_TYPE atomic_t
#define VALUE_TYPE volatile int
#define get_val(v)	(v->val)
#else
#define _SUF(T) _##T
#define _SUF1(T) _SUF(T)
#define SUF _SUF1(TYPE)
#define ATOMIC_TYPE volatile TYPE
#define VALUE_TYPE ATOMIC_TYPE
#define get_val(v)	(*v)
#endif


#define _STR(S) #S
#define STR(S) _STR(S)

static char* flags=
#ifdef NOASM
	"no_inline_asm "
#endif
#ifdef NOSMP
	"nosmp "
#else
	"smp "
#endif
	MEMBAR_STR
#ifndef HAVE_ASM_INLINE_MEMBAR
	"no_asm_membar(slow) "
#endif
#ifndef HAVE_ASM_INLINE_ATOMIC_OPS
	"no_asm_atomic_ops"
#ifdef ATOMIC_OPS_USE_LOCK_SET
	":use_lock_set"
#elif defined ATOMIC_OPS_USE_LOCK
	":use_lock"
#endif
	" "
#endif
#ifdef TYPE
	STR(TYPE) " "
#else
	"atomic_t "
#endif
;



/* macros for atomic_* functions */

#define _AT_DECL(OP, P, S) \
	P##atomic_##OP##S


/* to make sure all the macro passed as params are expanded,
 *  go through a 2 level deep macro decl. */
#define _AT_DECL1(OP, P, S) _AT_DECL(OP, P, S)
#define AT_DECL(OP) _AT_DECL1(OP, MB, SUF)


#define at_set	AT_DECL(set)
#define at_get	AT_DECL(get)

#define at_inc	AT_DECL(inc)
#define at_dec	AT_DECL(dec)
#define at_inc_and_test	AT_DECL(inc_and_test)
#define at_dec_and_test	AT_DECL(dec_and_test)
#define at_and	AT_DECL(and)
#define at_or	AT_DECL(or)
#define at_get_and_set	AT_DECL(get_and_set)
#define at_cmpxchg	AT_DECL(cmpxchg)
#define at_add	AT_DECL(add)


#define CHECK_ERR(txt, x, y) \
	if (x!=y) { \
		fprintf(stderr, "ERROR: line %d: %s failed: expected 0x%02x but got "\
						"0x%02x.\n", \
						__LINE__, #txt, (unsigned) x, (unsigned) y);\
		goto error; \
	}

#define VERIFY(ops, y) \
	ops ; \
	CHECK_ERR( ops, y, get_val(v))


int main(int argc, char** argv)
{
	ATOMIC_TYPE var;
	VALUE_TYPE r;
	
	ATOMIC_TYPE* v;
	
	v=&var;
	
	
#ifdef MEMBAR_USES_LOCK
	__membar_lock=&dummy_membar_lock;
	if (lock_init(__membar_lock)==0){
		fprintf(stderr, "ERROR: failed to initialize membar_lock\n");
		__membar_lock=0;
		goto error;
	}
	_membar_lock; /* start with the lock "taken" so that we can safely use
					 unlock/lock sequences on it later */
#endif
#ifdef ATOMIC_OPS_USE_LOCK_SET
	/* init the lock (emulate atomic_ops.c) */
	dummy_atomic_lock_set.locks=&locks_array[0];
	_atomic_lock_set=&dummy_atomic_lock_set;
	if (lock_set_init(_atomic_lock_set)==0){
		fprintf(stderr, "ERROR: failed to initialize atomic_lock\n");
		_atomic_lock_set=0;
		goto error;
	}
#elif defined ATOMIC_OPS_USE_LOCK
	/* init the lock (emulate atomic_ops.c) */
	_atomic_lock=&dummy_atomic_lock;
	if (lock_init(_atomic_lock)==0){
		fprintf(stderr, "ERROR: failed to initialize atomic_lock\n");
		_atomic_lock=0;
		goto error;
	}
#endif
	
	printf("%s\n", flags);
	
	printf("starting memory barrier opcode tests...\n");
	membar();
	printf(" membar() .............................. ok\n");
	membar_write();
	printf(" membar_write() ........................ ok\n");
	membar_read();
	printf(" membar_read() ......................... ok\n");
	membar_depends();
	printf(" membar_depends() ...................... ok\n");
	membar_enter_lock();
	printf(" membar_enter_lock() ................... ok\n");
	membar_leave_lock();
	printf(" membar_leave_lock() ................... ok\n");
	membar_atomic_op();
	printf(" membar_atomic_op() .................... ok\n");
	membar_atomic_setget();
	printf(" membar_atomic_setget() ................ ok\n");
	membar_read_atomic_op();
	printf(" membar_read_atomic_op() ............... ok\n");
	membar_read_atomic_setget();
	printf(" membar_read_atomic_setget() ........... ok\n");
	membar_write_atomic_op();
	printf(" membar_write_atomic_op() .............. ok\n");
	membar_write_atomic_setget();
	printf(" membar_write_atomic_setget() .......... ok\n");
	
	printf("\nstarting atomic ops basic tests...\n");
	
	VERIFY(at_set(v, 1), 1);
	printf(" atomic_set, v should be 1 ............. %2d\n", (int)at_get(v));
	VERIFY(at_inc(v), 2);
	printf(" atomic_inc, v should be 2 ............. %2d\n", (int)at_get(v));
	VERIFY(r=at_inc_and_test(v), 3);
	printf(" atomic_inc_and_test, v should be  3 ... %2d\n", (int)at_get(v));
	printf("                      r should be  0 ... %2d\n", (int)r);
	
	VERIFY(at_dec(v), 2);
	printf(" atomic_dec, v should be 2 ............. %2d\n", (int)at_get(v));
	VERIFY(r=at_dec_and_test(v), 1);
	printf(" atomic_dec_and_test, v should be  1 ... %2d\n", (int)at_get(v));
	printf("                      r should be  0 ... %2d\n", (int)r);
	VERIFY(r=at_dec_and_test(v), 0);
	printf(" atomic_dec_and_test, v should be  0 ... %2d\n", (int)at_get(v));
	printf("                      r should be  1 ... %2d\n", (int)r);
	VERIFY(r=at_dec_and_test(v), -1);
	printf(" atomic_dec_and_test, v should be -1 ... %2d\n", (int)at_get(v));
	printf("                      r should be  0 ... %2d\n", (int)r);
	
	VERIFY(at_and(v, 2), 2);
	printf(" atomic_and, v should be 2 ............. %2d\n", (int)at_get(v));
	
	VERIFY(at_or(v, 5), 7);
	printf(" atomic_or,  v should be 7 ............. %2d\n", (int)at_get(v));
	VERIFY(r=at_get_and_set(v, 0), 0);
	printf(" atomic_get_and_set, v should be 0 ..... %2d\n", (int)at_get(v));
	VERIFY(r=at_cmpxchg(v, 0, 7), 7);
	CHECK_ERR(cmpxchg, r, 0);
	printf(" atomic_cmpxchg, v should be 7 ......... %2d\n", (int)at_get(v));
	printf("                 r should be 0 ......... %2d\n", (int)r);
	VERIFY(r=at_cmpxchg(v, 2, 3), 7);
	CHECK_ERR(cmpxchg, r, 7);
	printf(" atomic_cmpxchg (fail), v should be 7 .. %2d\n", (int)at_get(v));
	printf("                        r should be 7 .. %2d\n", (int)r);
	VERIFY(r=at_add(v, 2), 9);
	CHECK_ERR(atomic_add, r, 9);
	printf(" atomic_add, v should be 9 ............. %2d\n", (int)at_get(v));
	printf("             r should be 9 ............. %2d\n", (int)r);
	VERIFY(r=at_add(v, -10), -1);
	CHECK_ERR(atomic_add, r, -1);
	printf(" atomic_add, v should be -1 ............ %2d\n", (int)at_get(v));
	printf("             r should be -1 ............ %2d\n", (int)r);

	
	printf("\ndone.\n");
#ifdef MEMBAR_USES_LOCK
	lock_destroy(__membar_lock);
#endif
#ifdef ATOMIC_OPS_USE_LOCK_SET
	lock_set_destroy(_atomic_lock_set);
#elif defined ATOMIC_OPS_USE_LOCK
	lock_destroy(_atomic_lock);
#endif
	return 0;
error:
#ifdef MEMBAR_USES_LOCK
	if (__membar_lock)
		lock_destroy(__membar_lock);
#endif
#ifdef ATOMIC_OPS_USE_LOCK_SET
	if (_atomic_lock_set)
		lock_set_destroy(_atomic_lock_set);
#elif defined ATOMIC_OPS_USE_LOCK
	if (_atomic_lock)
		lock_destroy(_atomic_lock);
#endif
	return -1;
}
