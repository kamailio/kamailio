/* 
 * $Id$
 * 
 * Copyright (C) 2006 iptelorg GmbH
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
 *  atomic operations and memory barriers (x86 and x86_64/amd64 specific)
 *  WARNING: atomic ops do not include memory barriers
 *  see atomic_ops.h for more details 
 *
 *  Config defines:   - NOSMP
 *                    - X86_OOSTORE (out of order store, defined by default)
 *                    - X86_64_OOSTORE, like X86_OOSTORE, but for x86_64 cpus,
 *                      default off
 *                    - __CPU_x86_64 (64 bit mode, long and void* is 64 bit and
 *                                    the cpu has all of the mfence, lfence
 *                                    and sfence instructions)
 *                    - __CPU_i386  (486+, 32 bit)
 */
/* 
 * History:
 * --------
 *  2006-03-08  created by andrei
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

ATOMIC_FUNC_DECL1(inc, "incl %0", int)
ATOMIC_FUNC_DECL1(dec, "decl %0", int)
ATOMIC_FUNC_DECL2(and, "andl %1, %0", int)
ATOMIC_FUNC_DECL2(or,  "orl %1, %0", int)
ATOMIC_FUNC_TEST(inc_and_test, "incl %0", int, int)
ATOMIC_FUNC_TEST(dec_and_test, "decl %0", int, int)
ATOMIC_FUNC_XCHG(get_and_set,  "xchgl %1, %0", int)
#ifdef __CPU_x86_64
ATOMIC_FUNC_DECL1(inc, "incq %0", long)
ATOMIC_FUNC_DECL1(dec, "decq %0", long)
ATOMIC_FUNC_DECL2(and, "andq %1, %0", long)
ATOMIC_FUNC_DECL2(or,  "orq %1, %0", long)
ATOMIC_FUNC_TEST(inc_and_test, "incq %0", long, int)
ATOMIC_FUNC_TEST(dec_and_test, "decq %0", long, int)
ATOMIC_FUNC_XCHG(get_and_set,  "xchgq %1, %0", long)
#else
ATOMIC_FUNC_DECL1(inc, "incl %0", long)
ATOMIC_FUNC_DECL1(dec, "decl %0", long)
ATOMIC_FUNC_DECL2(and, "andl %1, %0", long)
ATOMIC_FUNC_DECL2(or,  "orl %1, %0", long)
ATOMIC_FUNC_TEST(inc_and_test, "incl %0", long, int)
ATOMIC_FUNC_TEST(dec_and_test, "decl %0", long, int)
ATOMIC_FUNC_XCHG(get_and_set,  "xchgl %1, %0", long)
#endif

#define atomic_inc(var) atomic_inc_int(&(var)->val)
#define atomic_dec(var) atomic_dec_int(&(var)->val)
#define atomic_and(var, mask) atomic_and_int(&(var)->val, (mask))
#define atomic_or(var, mask)  atomic_or_int(&(var)->val, (mask))
#define atomic_dec_and_test(var) atomic_dec_and_test_int(&(var)->val)
#define atomic_inc_and_test(var) atomic_inc_and_test_int(&(var)->val)
#define atomic_get_and_set(var, i) atomic_get_and_set_int(&(var)->val, i)


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

#define mb_atomic_inc_long(v)	atomic_inc_long(v)
#define mb_atomic_dec_long(v)	atomic_dec_long(v)
#define mb_atomic_or_long(v, m)	atomic_or_long(v, m)
#define mb_atomic_and_long(v, m)	atomic_and_long(v, m)
#define mb_atomic_inc_and_test_long(v)	atomic_inc_and_test_long(v)
#define mb_atomic_dec_and_test_long(v)	atomic_dec_and_test_long(v)
#define mb_atomic_get_and_set_long(v, i)	atomic_get_and_set_long(v, i)

#define mb_atomic_inc(v)	atomic_inc(v)
#define mb_atomic_dec(v)	atomic_dec(v)
#define mb_atomic_or(v, m)	atomic_or(v, m)
#define mb_atomic_and(v, m)	atomic_and(v, m)
#define mb_atomic_inc_and_test(v)	atomic_inc_and_test(v)
#define mb_atomic_dec_and_test(v)	atomic_dec_and_test(v)
#define mb_atomic_get(v)	mb_atomic_get_int( &(v)->val)
#define mb_atomic_set(v, i)	mb_atomic_set_int(&(v)->val, i)
#define mb_atomic_get_and_set(v, i)	atomic_get_and_set_int(&(v)->val, i)


#endif
