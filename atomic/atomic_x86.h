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
 * @brief Atomic operations and memory barriers (x86 and x86_64/amd64 specific)
 * 
 * Atomic operations and memory barriers (x86 and x86_64/amd64 specific)
 * \warning atomic ops do not include memory barriers, see atomic_ops.h for more
 * details.
 *
 * Config defines:
 * - NOSMP
 * - X86_OOSTORE (out of order store, defined by default)
 * - X86_64_OOSTORE, like X86_OOSTORE, but for x86_64 CPUs, default off
 * - __CPU_x86_64 (64 bit mode, long and void* is 64 bit and the CPU has all
 *   of the mfence, lfence and sfence instructions)
 * - __CPU_i386  (486+, 32 bit)
 * @ingroup atomic
 */
/* 
 * History:
 * --------
 *  2006-03-08  created by andrei
 *  2007-05-07  added cmpxchg (andrei)
 *  2007-05-08  added atomic_add (andrei)
 *  2007-05-29  added membar_depends(), membar_*_atomic_op and
 *                membar_*_atomic_setget (andrei)
 */

#ifndef _atomic_x86_h
#define _atomic_x86_h

#define HAVE_ASM_INLINE_ATOMIC_OPS
#define HAVE_ASM_INLINE_MEMBAR

#ifdef NOSMP
#define __LOCK_PREF 
#else
#define __LOCK_PREF "lock ;"
#endif


/* memory barriers */

#ifdef NOSMP

#define membar()	asm volatile ("" : : : "memory")
#define membar_read()	membar()
#define membar_write()	membar()
#define membar_depends()  do {} while(0) /* really empty, not even a cc bar. */
/* lock barrriers: empty, not needed for NOSMP; the lock/unlock should already
 * contain gcc barriers*/
#define membar_enter_lock() do {} while(0)
#define membar_leave_lock() do {} while(0)
/* membars after or before atomic_ops or atomic_setget -> use these or
 *  mb_<atomic_op_name>() if you need a memory barrier in one of these
 *  situations (on some archs where the atomic operations imply memory
 *   barriers is better to use atomic_op_x(); membar_atomic_op() then
 *    atomic_op_x(); membar()) */
#define membar_atomic_op()				do {} while(0)
#define membar_atomic_setget()			membar()
#define membar_write_atomic_op()		do {} while(0)
#define membar_write_atomic_setget()	membar_write()
#define membar_read_atomic_op()			do {} while(0)
#define membar_read_atomic_setget()		membar_read()

#else

/* although most x86 do stores in order, we're playing it safe and use
 *  oostore ready write barriers */
#define X86_OOSTORE 

#ifdef __CPU_x86_64
/*
#define membar() \
	asm volatile( \
					" lock; addq $0, 0(%%rsp) \n\t " \
					: : : "memory" \
				) 
*/
#define membar() 		asm volatile( " mfence \n\t " : : : "memory" )
#define membar_read()	asm volatile( " lfence \n\t " : : : "memory" )
#ifdef X86_64_OOSTORE
#define membar_write()	asm volatile( " sfence \n\t " : : : "memory" )
#else
#define membar_write()	asm volatile ("" : : : "memory") /* gcc don't cache*/
#endif /* X86_OOSTORE */

#else /* ! __CPU_x86_64  => __CPU_i386*/
/* membar: lfence, mfence, sfence available only on newer cpus, so for now
 * stick to lock addl */
#define membar() asm volatile(" lock; addl $0, 0(%%esp) \n\t " : : : "memory" )
#define membar_read()	membar()
#ifdef X86_OOSTORE
/* out of order store version */
#define membar_write()	membar()
#else
/* no oostore, most x86 cpus => do nothing, just a gcc do_not_cache barrier*/
#define membar_write()	asm volatile ("" : : : "memory")
#endif /* X86_OOSTORE */

#endif /* __CPU_x86_64 */

#define membar_depends()  do {} while(0) /* really empty, not even a cc bar. */
/* lock barrriers: empty, not needed on x86 or x86_64 (atomic ops already
 *  force the barriers if needed); the lock/unlock should already contain the 
 *  gcc do_not_cache barriers*/
#define membar_enter_lock() do {} while(0)
#define membar_leave_lock() do {} while(0)
/* membars after or before atomic_ops or atomic_setget -> use these or
 *  mb_<atomic_op_name>() if you need a memory barrier in one of these
 *  situations (on some archs where the atomic operations imply memory
 *   barriers is better to use atomic_op_x(); membar_atomic_op() then
 *    atomic_op_x(); membar()) */
#define membar_atomic_op()				do {} while(0)
#define membar_atomic_setget()			membar()
#define membar_write_atomic_op()		do {} while(0)
#define membar_write_atomic_setget()	membar_write()
#define membar_read_atomic_op()			do {} while(0)
#define membar_read_atomic_setget()		membar_read()


#endif /* NOSMP */

/* 1 param atomic f */
#define ATOMIC_FUNC_DECL1(NAME, OP, P_TYPE) \
	inline static void atomic_##NAME##_##P_TYPE (volatile P_TYPE* var) \
	{ \
		asm volatile( \
				__LOCK_PREF " " OP " \n\t" \
				: "=m"(*var) : "m"(*var) : "cc", "memory" \
				); \
	}

/* 2 params atomic f */
#define ATOMIC_FUNC_DECL2(NAME, OP, P_TYPE) \
	inline static void atomic_##NAME##_##P_TYPE (volatile P_TYPE* var, \
			                                    P_TYPE v) \
	{ \
		asm volatile( \
				__LOCK_PREF " " OP " \n\t" \
				: "=m"(*var) : "ri" (v), "m"(*var) : "cc", "memory" \
				); \
	}

#if defined __GNUC__ &&  __GNUC__ < 3 && __GNUC_MINOR__ < 9
/* gcc version < 2.9 */
#define ATOMIC_FUNC_XCHG(NAME, OP, TYPE) \
	inline static TYPE atomic_##NAME##_##TYPE(volatile TYPE* var, TYPE v) \
{ \
	asm volatile( \
			OP " \n\t" \
			: "=q"(v), "=m"(*var) :"m"(*var), "0"(v) : "memory" \
			); \
	return v; \
}
#else
#define ATOMIC_FUNC_XCHG(NAME, OP, TYPE) \
	inline static TYPE atomic_##NAME##_##TYPE(volatile TYPE* var, TYPE v) \
{ \
	asm volatile( \
			OP " \n\t" \
			: "+q"(v), "=m"(*var) : "m"(*var) : "memory" \
			); \
	return v; \
}
#endif /* gcc & gcc version < 2.9 */

/* returns a value, 1 param */
#define ATOMIC_FUNC_TEST(NAME, OP, P_TYPE, RET_TYPE) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE(volatile P_TYPE* var) \
	{ \
		char ret; \
		asm volatile( \
				__LOCK_PREF " " OP "\n\t" \
				"setz %1 \n\t" \
				: "=m" (*var), "=qm"(ret) : "m"(*var) : "cc", "memory" \
				); \
		return ret; \
	}

/* returns a value, 3 params (var, old, new)
 * The returned value is the value before the xchg:
 *  if ret!=old => cmpxchg failed and ret is var's value
 *  else  => success and new_v is var's new value */
#define ATOMIC_FUNC_CMPXCHG(NAME, OP, P_TYPE, RET_TYPE) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE(volatile P_TYPE* var, \
													P_TYPE old, P_TYPE new_v)\
	{ \
		P_TYPE ret; \
		asm volatile( \
				__LOCK_PREF " " OP "\n\t" \
				: "=a"(ret), "=m" (*var) :\
					"r"(new_v), "m"(*var), "0"(old):\
					"cc", "memory" \
				); \
		return ret; \
	}

/* similar w/ XCHG but with LOCK prefix, relaxed constraints & diff. return */
#define ATOMIC_FUNC_XADD(NAME, OP, TYPE) \
	inline static TYPE atomic_##NAME##_##TYPE(volatile TYPE* var, TYPE v) \
{ \
	TYPE ret; \
	asm volatile( \
			__LOCK_PREF " " OP " \n\t" \
			: "=r"(ret), "=m"(*var) :"m"(*var), "0"(v) : "cc", "memory" \
			); \
	return ret+v; \
}

ATOMIC_FUNC_DECL1(inc, "incl %0", int)
ATOMIC_FUNC_DECL1(dec, "decl %0", int)
ATOMIC_FUNC_DECL2(and, "andl %1, %0", int)
ATOMIC_FUNC_DECL2(or,  "orl %1, %0", int)
ATOMIC_FUNC_TEST(inc_and_test, "incl %0", int, int)
ATOMIC_FUNC_TEST(dec_and_test, "decl %0", int, int)
ATOMIC_FUNC_XCHG(get_and_set,  "xchgl %1, %0", int)
ATOMIC_FUNC_CMPXCHG(cmpxchg, "cmpxchgl %2, %1", int , int)
ATOMIC_FUNC_XADD(add, "xaddl %0, %1", int) 
#ifdef __CPU_x86_64
ATOMIC_FUNC_DECL1(inc, "incq %0", long)
ATOMIC_FUNC_DECL1(dec, "decq %0", long)
ATOMIC_FUNC_DECL2(and, "andq %1, %0", long)
ATOMIC_FUNC_DECL2(or,  "orq %1, %0", long)
ATOMIC_FUNC_TEST(inc_and_test, "incq %0", long, int)
ATOMIC_FUNC_TEST(dec_and_test, "decq %0", long, int)
ATOMIC_FUNC_XCHG(get_and_set,  "xchgq %1, %0", long)
ATOMIC_FUNC_CMPXCHG(cmpxchg, "cmpxchgq %2, %1", long , long)
ATOMIC_FUNC_XADD(add, "xaddq %0, %1",long) 
#else
ATOMIC_FUNC_DECL1(inc, "incl %0", long)
ATOMIC_FUNC_DECL1(dec, "decl %0", long)
ATOMIC_FUNC_DECL2(and, "andl %1, %0", long)
ATOMIC_FUNC_DECL2(or,  "orl %1, %0", long)
ATOMIC_FUNC_TEST(inc_and_test, "incl %0", long, int)
ATOMIC_FUNC_TEST(dec_and_test, "decl %0", long, int)
ATOMIC_FUNC_XCHG(get_and_set,  "xchgl %1, %0", long)
ATOMIC_FUNC_CMPXCHG(cmpxchg, "cmpxchgl %2, %1", long , long)
ATOMIC_FUNC_XADD(add, "xaddl %0, %1",long) 
#endif

#define atomic_inc(var) atomic_inc_int(&(var)->val)
#define atomic_dec(var) atomic_dec_int(&(var)->val)
#define atomic_and(var, mask) atomic_and_int(&(var)->val, (mask))
#define atomic_or(var, mask)  atomic_or_int(&(var)->val, (mask))
#define atomic_dec_and_test(var) atomic_dec_and_test_int(&(var)->val)
#define atomic_inc_and_test(var) atomic_inc_and_test_int(&(var)->val)
#define atomic_get_and_set(var, i) atomic_get_and_set_int(&(var)->val, i)
#define atomic_cmpxchg(var, old, newv) \
		atomic_cmpxchg_int(&(var)->val, old, newv)
#define atomic_add(var, v) atomic_add_int(&(var)->val, v)


#ifdef NOSMP

#define mb_atomic_set_int(v, i) \
	do{ \
		membar(); atomic_set_int(v, i); \
	}while(0)

#define mb_atomic_set_long(v, i) \
	do{ \
		membar(); atomic_set_long(v, i); \
	}while(0)



inline static int mb_atomic_get_int(volatile int* v)
{
	membar(); return atomic_get_int(v);
}

inline static long mb_atomic_get_long(volatile long* v)
{
	membar(); return atomic_get_long(v);
}


#else /* NOSMP */


inline static void mb_atomic_set_int(volatile int* v, int i)
{
	asm volatile(
			"xchgl %1, %0 \n\t"
#if defined __GNUC__ &&  __GNUC__ < 3 && __GNUC_MINOR__ < 9
			: "=q"(i), "=m"(*v) : "m"(*v), "0"(i) : "memory" 
#else
			: "+q"(i), "=m"(*v) : "m"(*v) : "memory" 
#endif
			);
}


inline static void mb_atomic_set_long(volatile long* v, long l)
{
	asm volatile(
#ifdef __CPU_x86_64
			"xchgq %1, %0 \n\t"
#else
			"xchgl %1, %0 \n\t"
#endif
#if defined __GNUC__ &&  __GNUC__ < 3 && __GNUC_MINOR__ < 9
			: "=q"(l), "=m"(*v) : "m"(*v), "0"(l) : "memory" 
#else
			: "+q"(l), "=m"(*v) : "m"(*v) : "memory" 
#endif
			);
}


inline static int mb_atomic_get_int(volatile int* var)
{
	int ret;
	
	asm volatile(
			__LOCK_PREF " cmpxchgl %0, %1 \n\t"
			: "=a"(ret)  : "m"(*var) : "cc", "memory"
			);
	return ret;
}

inline static long mb_atomic_get_long(volatile long* var)
{
	long ret;
	
	asm volatile(
#ifdef __CPU_x86_64
			__LOCK_PREF " cmpxchgq %0, %1 \n\t"
#else
			__LOCK_PREF " cmpxchgl %0, %1 \n\t"
#endif
			: "=a"(ret)  : "m"(*var) : "cc", "memory"
			);
	return ret;
}

#endif /* NOSMP */


/* on x86 atomic intructions act also as barriers */
#define mb_atomic_inc_int(v)	atomic_inc_int(v)
#define mb_atomic_dec_int(v)	atomic_dec_int(v)
#define mb_atomic_or_int(v, m)	atomic_or_int(v, m)
#define mb_atomic_and_int(v, m)	atomic_and_int(v, m)
#define mb_atomic_inc_and_test_int(v)	atomic_inc_and_test_int(v)
#define mb_atomic_dec_and_test_int(v)	atomic_dec_and_test_int(v)
#define mb_atomic_get_and_set_int(v, i)	atomic_get_and_set_int(v, i)
#define mb_atomic_cmpxchg_int(v, o, n)	atomic_cmpxchg_int(v, o, n)
#define mb_atomic_add_int(v, a)	atomic_add_int(v, a)

#define mb_atomic_inc_long(v)	atomic_inc_long(v)
#define mb_atomic_dec_long(v)	atomic_dec_long(v)
#define mb_atomic_or_long(v, m)	atomic_or_long(v, m)
#define mb_atomic_and_long(v, m)	atomic_and_long(v, m)
#define mb_atomic_inc_and_test_long(v)	atomic_inc_and_test_long(v)
#define mb_atomic_dec_and_test_long(v)	atomic_dec_and_test_long(v)
#define mb_atomic_get_and_set_long(v, i)	atomic_get_and_set_long(v, i)
#define mb_atomic_cmpxchg_long(v, o, n)	atomic_cmpxchg_long(v, o, n)
#define mb_atomic_add_long(v, a)	atomic_add_long(v, a)

#define mb_atomic_inc(v)	atomic_inc(v)
#define mb_atomic_dec(v)	atomic_dec(v)
#define mb_atomic_or(v, m)	atomic_or(v, m)
#define mb_atomic_and(v, m)	atomic_and(v, m)
#define mb_atomic_inc_and_test(v)	atomic_inc_and_test(v)
#define mb_atomic_dec_and_test(v)	atomic_dec_and_test(v)
#define mb_atomic_get(v)	mb_atomic_get_int( &(v)->val)
#define mb_atomic_set(v, i)	mb_atomic_set_int(&(v)->val, i)
#define mb_atomic_get_and_set(v, i)	atomic_get_and_set_int(&(v)->val, i)
#define mb_atomic_cmpxchg(v, o, n)	atomic_cmpxchg_int(&(v)->val, o, n)
#define mb_atomic_add(v, a)	atomic_add_int(&(v)->val, a)


#endif
