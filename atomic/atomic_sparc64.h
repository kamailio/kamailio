/* 
 * Copyright (C) 2006 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief Atomic operations and memory barriers (SPARC64 version, 32 and 64 bit modes)
 * 
 * Atomic operations and memory barriers (SPARC64 version, 32 and 64 bit modes)
 * \warning atomic ops do not include memory barriers see atomic_ops.h for
 * more details.
 *
 * Config defines:
 * - SPARC64_MODE (if defined long is assumed to be 64 bits else long & void*
 *   are assumed to be 32 for SPARC32plus code)
 * - NOSMP
 * @ingroup atomic
 */

/* 
 * History:
 * --------
 *  2006-03-28  created by andrei
 *  2007-05-08 added atomic_add and atomic_cmpxchg (andrei)
 *  2007-05-29  added membar_depends(), membar_*_atomic_op and
 *                membar_*_atomic_setget (andrei)
 */


#ifndef _atomic_sparc64_h
#define _atomic_sparc64_h

#define HAVE_ASM_INLINE_ATOMIC_OPS
#define HAVE_ASM_INLINE_MEMBAR



/* try to guess if in SPARC64_MODE */
#if ! defined SPARC64_MODE && \
	(defined __LP64__ || defined _LP64 || defined __arch64__)
#define SPARC64_MODE
#endif


#ifdef NOSMP
#define membar() asm volatile ("" : : : "memory") /* gcc do not cache barrier*/
#define membar_read()  membar()
#define membar_write() membar()
#define membar_depends()  do {} while(0) /* really empty, not even a cc bar. */
/*  memory barriers for lock & unlock where lock & unlock are inline asm
 *  functions that use atomic ops (and both of them use at least a store to
 *  the lock). membar_enter_lock() is at most a StoreStore|StoreLoad barrier
 *   and membar_leave_lock() is at most a LoadStore|StoreStore barries
 *  (if the atomic ops on the specific arhitecture imply these barriers
 *   => these macros will be empty)
 *   Warning: these barriers don't force LoadLoad ordering between code
 *    before the lock/membar_enter_lock() and code 
 *    after membar_leave_lock()/unlock()
 *
 *  Usage: lock(); membar_enter_lock(); .... ; membar_leave_lock(); unlock()
 */
#define membar_enter_lock() do {} while(0)
#define membar_leave_lock() do {} while(0)
/* membars after or before atomic_ops or atomic_setget -> use these or
 *  mb_<atomic_op_name>() if you need a memory barrier in one of these
 *  situations (on some archs where the atomic operations imply memory
 *   barriers is better to use atomic_op_x(); membar_atomic_op() then
 *    atomic_op_x(); membar()) */
#define membar_atomic_op()				membar()
#define membar_atomic_setget()			membar()
#define membar_write_atomic_op()		membar_write()
#define membar_write_atomic_setget()	membar_write()
#define membar_read_atomic_op()			membar_read()
#define membar_read_atomic_setget()		membar_read()
#else /* SMP */
#define membar() \
	asm volatile ( \
			"membar #LoadLoad | #LoadStore | #StoreStore | #StoreLoad \n\t" \
			: : : "memory")

#define membar_read() asm volatile ("membar #LoadLoad \n\t" : : : "memory")
#define membar_write() asm volatile ("membar #StoreStore \n\t" : : : "memory")
#define membar_depends()  do {} while(0) /* really empty, not even a cc bar. */
#define membar_enter_lock() \
	asm volatile ("membar #StoreStore | #StoreLoad \n\t" : : : "memory");
#define membar_leave_lock() \
	asm volatile ("membar #LoadStore | #StoreStore \n\t" : : : "memory");
/* membars after or before atomic_ops or atomic_setget -> use these or
 *  mb_<atomic_op_name>() if you need a memory barrier in one of these
 *  situations (on some archs where the atomic operations imply memory
 *   barriers is better to use atomic_op_x(); membar_atomic_op() then
 *    atomic_op_x(); membar()) */
#define membar_atomic_op()				membar()
#define membar_atomic_setget()			membar()
#define membar_write_atomic_op()		membar_write()
#define membar_write_atomic_setget()	membar_write()
#define membar_read_atomic_op()			membar_read()
#define membar_read_atomic_setget()		membar_read()
#endif /* NOSMP */



/* 32 bit version, op should store the result in %1, and use %0 as input,
 *  both %0 and %1 are modified */
#define ATOMIC_ASM_OP_int(op)\
	"   ldsw [%3], %0 \n\t"  /* signed or lduw? */ \
	"1: " op " \n\t" \
	"   cas  [%3], %0, %1 \n\t" \
	"   cmp %0, %1 \n\t" \
	"   bne,a,pn  %%icc, 1b \n\t"  /* predict not taken, annul */ \
	"   mov %1, %0\n\t"  /* delay slot */

#ifdef SPARC64_MODE
/* 64 bit version, same as above */
#define ATOMIC_ASM_OP_long(op)\
	"   ldx [%3], %0 \n\t" \
	"1: " op " \n\t" \
	"   casx  [%3], %0, %1 \n\t" \
	"   cmp %0, %1 \n\t" \
	"   bne,a,pn  %%xcc, 1b \n\t"  /* predict not taken, annul */ \
	"   mov %1, %0\n\t"  /* delay slot */
	
#else /* no SPARC64_MODE => 32bit mode on a sparc64*/
#define ATOMIC_ASM_OP_long(op) ATOMIC_ASM_OP_int(op)
#endif

#define ATOMIC_FUNC_DECL(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var) \
	{ \
		P_TYPE ret, tmp; \
		asm volatile( \
			ATOMIC_ASM_OP_##P_TYPE(OP) \
			: "=&r"(ret), "=&r"(tmp), "=m"(*var) : "r"(var) : "cc" \
			); \
		return RET_EXPR; \
	}


/* same as above, but takes an extra param, v, which goes in %4 */
#define ATOMIC_FUNC_DECL1(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
															P_TYPE v) \
	{ \
		P_TYPE ret, tmp; \
		asm volatile( \
			ATOMIC_ASM_OP_##P_TYPE(OP) \
			: "=&r"(ret), "=&r"(tmp), "=m"(*var) : "r"(var), "r"(v) : "cc" \
			); \
		return RET_EXPR; \
	}

/* same as above, but uses a short 1 op sequence 
 * %2 (or %1) is var, %0 is  v and return (ret)*/
#define ATOMIC_FUNC_DECL1_RAW(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
															P_TYPE v) \
	{ \
		P_TYPE ret; \
		asm volatile( \
			OP "\n\t" \
			: "=&r"(ret), "=m"(*var) : "r"(var), "0"(v) : "cc" \
			); \
		return RET_EXPR; \
	}

/* same as above, but takes two extra params, v, which goes in %4
 * and uses a short 1 op sequence:
 * %2 (or %1) is var, %3 is v1 and %0 is v2 & result (ret) */
#define ATOMIC_FUNC_DECL2_CAS(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
													P_TYPE v1, P_TYPE v2) \
	{ \
		P_TYPE ret; \
		asm volatile( \
			OP "\n\t" \
			: "=&r"(ret), "=m"(*var) : "r"(var), "r"(v1), "0"(v2) : "cc" \
			); \
		return RET_EXPR; \
	}



ATOMIC_FUNC_DECL(inc,      "add  %0,  1, %1", int, void, /* no return */ )
ATOMIC_FUNC_DECL(dec,      "sub  %0,  1, %1", int, void, /* no return */ )
ATOMIC_FUNC_DECL1(and,     "and  %0, %4, %1", int, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,      "or   %0, %4, %1", int, void, /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "add   %0, 1, %1", int, int, ((ret+1)==0) )
ATOMIC_FUNC_DECL(dec_and_test, "sub   %0, 1, %1", int, int, ((ret-1)==0) )
/* deprecated but probably better then CAS for futexes */
ATOMIC_FUNC_DECL1_RAW(get_and_set, "swap [%2], %0", int, int, ret)
/*ATOMIC_FUNC_DECL1(get_and_set, "mov %4, %1" , int, int,  ret)*/
ATOMIC_FUNC_DECL1(add,     "add  %0, %4, %1", int, int,  ret+v)
ATOMIC_FUNC_DECL2_CAS(cmpxchg, "cas  [%2], %3, %0", int, int,  ret)


ATOMIC_FUNC_DECL(inc,      "add  %0,  1, %1", long, void, /* no return */ )
ATOMIC_FUNC_DECL(dec,      "sub  %0,  1, %1", long, void, /* no return */ )
ATOMIC_FUNC_DECL1(and,     "and  %0, %4, %1", long, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,      "or   %0, %4, %1", long, void, /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "add   %0, 1, %1", long, long, ((ret+1)==0) )
ATOMIC_FUNC_DECL(dec_and_test, "sub   %0, 1, %1", long, long, ((ret-1)==0) )
ATOMIC_FUNC_DECL1(get_and_set, "mov %4, %1" , long, long,  ret)
ATOMIC_FUNC_DECL1(add,     "add  %0, %4, %1", long, long,  ret+v)
#ifdef SPARC64_MODE
ATOMIC_FUNC_DECL2_CAS(cmpxchg, "casx  [%2], %3, %0", long, long,  ret)
#else
ATOMIC_FUNC_DECL2_CAS(cmpxchg, "cas   [%2], %3, %0", long, long,  ret)
#endif


#define atomic_inc(var) atomic_inc_int(&(var)->val)
#define atomic_dec(var) atomic_dec_int(&(var)->val)
#define atomic_and(var, mask) atomic_and_int(&(var)->val, (mask))
#define atomic_or(var, mask)  atomic_or_int(&(var)->val, (mask))
#define atomic_dec_and_test(var) atomic_dec_and_test_int(&(var)->val)
#define atomic_inc_and_test(var) atomic_inc_and_test_int(&(var)->val)
#define atomic_get_and_set(var, i) atomic_get_and_set_int(&(var)->val, i)
#define atomic_add(var, i) atomic_add_int(&(var)->val, i)
#define atomic_cmpxchg(var, old, new_v) \
	atomic_cmpxchg_int(&(var)->val, old, new_v)



/* with integrated membar */

#define mb_atomic_set_int(v, i) \
	do{ \
		membar(); \
		atomic_set_int(v, i); \
	}while(0)



inline static int mb_atomic_get_int(volatile int* v)
{
	membar();
	return atomic_get_int(v);
}


#define mb_atomic_inc_int(v) \
	do{ \
		membar(); \
		atomic_inc_int(v); \
	}while(0)

#define mb_atomic_dec_int(v) \
	do{ \
		membar(); \
		atomic_dec_int(v); \
	}while(0)

#define mb_atomic_or_int(v, m) \
	do{ \
		membar(); \
		atomic_or_int(v, m); \
	}while(0)

#define mb_atomic_and_int(v, m) \
	do{ \
		membar(); \
		atomic_and_int(v, m); \
	}while(0)

inline static int mb_atomic_inc_and_test_int(volatile int* v)
{
	membar();
	return atomic_inc_and_test_int(v);
}

inline static int mb_atomic_dec_and_test_int(volatile int* v)
{
	membar();
	return atomic_dec_and_test_int(v);
}


inline static int mb_atomic_get_and_set_int(volatile int* v, int i)
{
	membar();
	return atomic_get_and_set_int(v, i);
}

inline static int mb_atomic_cmpxchg_int(volatile int* v, int o, int n)
{
	membar();
	return atomic_cmpxchg_int(v, o, n);
}

inline static int mb_atomic_add_int(volatile int* v, int i)
{
	membar();
	return atomic_add_int(v, i);
}



#define mb_atomic_set_long(v, i) \
	do{ \
		membar(); \
		atomic_set_long(v, i); \
	}while(0)



inline static long mb_atomic_get_long(volatile long* v)
{
	membar();
	return atomic_get_long(v);
}


#define mb_atomic_inc_long(v) \
	do{ \
		membar(); \
		atomic_inc_long(v); \
	}while(0)


#define mb_atomic_dec_long(v) \
	do{ \
		membar(); \
		atomic_dec_long(v); \
	}while(0)

#define mb_atomic_or_long(v, m) \
	do{ \
		membar(); \
		atomic_or_long(v, m); \
	}while(0)

#define mb_atomic_and_long(v, m) \
	do{ \
		membar(); \
		atomic_and_long(v, m); \
	}while(0)

inline static long mb_atomic_inc_and_test_long(volatile long* v)
{
	membar();
	return atomic_inc_and_test_long(v);
}

inline static long mb_atomic_dec_and_test_long(volatile long* v)
{
	membar();
	return atomic_dec_and_test_long(v);
}


inline static long mb_atomic_get_and_set_long(volatile long* v, long l)
{
	membar();
	return atomic_get_and_set_long(v, l);
}

inline static long mb_atomic_cmpxchg_long(volatile long* v, long o, long n)
{
	membar();
	return atomic_cmpxchg_long(v, o, n);
}

inline static long mb_atomic_add_long(volatile long* v, long i)
{
	membar();
	return atomic_add_long(v, i);
}


#define mb_atomic_inc(var) mb_atomic_inc_int(&(var)->val)
#define mb_atomic_dec(var) mb_atomic_dec_int(&(var)->val)
#define mb_atomic_and(var, mask) mb_atomic_and_int(&(var)->val, (mask))
#define mb_atomic_or(var, mask)  mb_atomic_or_int(&(var)->val, (mask))
#define mb_atomic_dec_and_test(var) mb_atomic_dec_and_test_int(&(var)->val)
#define mb_atomic_inc_and_test(var) mb_atomic_inc_and_test_int(&(var)->val)
#define mb_atomic_get(var)	mb_atomic_get_int(&(var)->val)
#define mb_atomic_set(var, i)	mb_atomic_set_int(&(var)->val, i)
#define mb_atomic_get_and_set(var, i) mb_atomic_get_and_set_int(&(var)->val, i)
#define mb_atomic_cmpxchg(var, o, n) mb_atomic_cmpxchg_int(&(var)->val, o, n)
#define mb_atomic_add(var, i) mb_atomic_add_int(&(var)->val, i)


#endif
