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
 *  atomic ops and memory barriers for arm (>= v3)
 *  see atomic_ops.h for more details 
 *
 * Config defines: - NOSMP
 *                 - __CPU_arm
 *                 - __CPU_arm6    - armv6 support (supports atomic ops
 *                                    via ldrex/strex)
 */ 
/* 
 * History:
 * --------
 *  2006-03-31  created by andrei
 */


#ifndef _atomic_arm_h
#define _atomic_arm_h



#warning "arm atomic operations support not tested"

#ifdef NOSMP
#define HAVE_ASM_INLINE_MEMBAR
#define membar() asm volatile ("" : : : "memory") /* gcc do not cache barrier*/
#define membar_read()  membar()
#define membar_write() membar()
#else /* SMP */
#warning SMP not supported for arm atomic ops, try compiling with -DNOSMP
/* fall back to default lock based barriers (don't define HAVE_ASM...) */
#endif /* NOSMP */


#ifdef __CPU_arm6


#define HAVE_ASM_INLINE_ATOMIC_OPS

/* hack to get some membars */
#ifndef NOSMP
#include "atomic_unknown.h"
#endif

/* main asm block 
 *  use %0 as input and write the output in %1*/
#define ATOMIC_ASM_OP(op) \
			"1:   ldrex %0, [%3] \n\t" \
			"     " op "\n\t" \
			"     strex %0, %1, [%3] \n\t" \
			"     cmp %0, #0 \n\t" \
			"     bne 1b \n\t"

/* same as above but writes %4 instead of %1, and %0 will contain 
 * the prev. val*/
#define ATOMIC_ASM_OP2(op) \
			"1:   ldrex %0, [%3] \n\t" \
			"     " op "\n\t" \
			"     strex %1, %4, [%3] \n\t" \
			"     cmp %1, #0 \n\t" \
			"     bne 1b \n\t"

/* no extra param, %0 contains *var, %1 should contain the result */
#define ATOMIC_FUNC_DECL(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var) \
	{ \
		P_TYPE ret, tmp; \
		asm volatile( \
			ATOMIC_ASM_OP(OP) \
			: "=&r"(tmp), "=&r"(ret), "=m"(*var) : "r"(var)  : "cc" \
			); \
		return RET_EXPR; \
	}

/* one extra param in %4 */
#define ATOMIC_FUNC_DECL1(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
														P_TYPE v) \
	{ \
		P_TYPE ret, tmp; \
		asm volatile( \
			ATOMIC_ASM_OP(OP) \
			: "=&r"(tmp), "=&r"(ret), "=m"(*var) : "r"(var), "r"(v) : "cc" \
			); \
		return RET_EXPR; \
	}


/* as above, but %4 should contain the result, and %0 is returned*/
#define ATOMIC_FUNC_DECL2(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
														P_TYPE v) \
	{ \
		P_TYPE ret, tmp; \
		asm volatile( \
			ATOMIC_ASM_OP2(OP) \
			: "=&r"(ret), "=&r"(tmp), "=m"(*var) : "r"(var), "r"(v) : "cc" \
			); \
		return RET_EXPR; \
	}



ATOMIC_FUNC_DECL(inc,      "add  %1, %0, #1", int, void, /* no return */ )
ATOMIC_FUNC_DECL(dec,      "sub  %1, %0, #1", int, void, /* no return */ )
ATOMIC_FUNC_DECL1(and,     "and  %1, %0, %4", int, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,      "orr  %1, %0, %4", int, void, /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "add  %1, %0, #1", int, int, ret )
ATOMIC_FUNC_DECL(dec_and_test, "sub  %1, %0, #1", int, int, ret )
ATOMIC_FUNC_DECL2(get_and_set, /* no extra op needed */ , int, int,  ret)

ATOMIC_FUNC_DECL(inc,      "add  %1, %0, #1", long, void, /* no return */ )
ATOMIC_FUNC_DECL(dec,      "sub  %1, %0, #1", long, void, /* no return */ )
ATOMIC_FUNC_DECL1(and,     "and  %1, %0, %4", long, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,      "orr  %1, %0, %4", long, void, /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "add  %1, %0, #1", long, long, ret )
ATOMIC_FUNC_DECL(dec_and_test, "sub  %1, %0, #1", long, long, ret )
ATOMIC_FUNC_DECL2(get_and_set, /* no extra op needed */ , long, long,  ret)

#define atomic_inc(var) atomic_inc_int(&(var)->val)
#define atomic_dec(var) atomic_dec_int(&(var)->val)
#define atomic_and(var, mask) atomic_and_int(&(var)->val, (mask))
#define atomic_or(var, mask)  atomic_or_int(&(var)->val, (mask))
#define atomic_dec_and_test(var) atomic_dec_and_test_int(&(var)->val)
#define atomic_inc_and_test(var) atomic_inc_and_test_int(&(var)->val)
#define atomic_get_and_set(var, i) atomic_get_and_set_int(&(var)->val, i)


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


#define mb_atomic_inc(var) mb_atomic_inc_int(&(var)->val)
#define mb_atomic_dec(var) mb_atomic_dec_int(&(var)->val)
#define mb_atomic_and(var, mask) mb_atomic_and_int(&(var)->val, (mask))
#define mb_atomic_or(var, mask)  mb_atomic_or_int(&(var)->val, (mask))
#define mb_atomic_dec_and_test(var) mb_atomic_dec_and_test_int(&(var)->val)
#define mb_atomic_inc_and_test(var) mb_atomic_inc_and_test_int(&(var)->val)
#define mb_atomic_get(var)	mb_atomic_get_int(&(var)->val)
#define mb_atomic_set(var, i)	mb_atomic_set_int(&(var)->val, i)
#define mb_atomic_get_and_set(var, i) mb_atomic_get_and_set_int(&(var)->val, i)


#else /* ! __CPU_arm6 => __CPU_arm */

/* no atomic ops for v <6 , only SWP supported
 * Atomic ops could be implemented if one bit is sacrificed and used like
 *  a spinlock, e.g:
 *          mov %r0, #0x1
 *       1: swp %r1, %r0, [&atomic_val]
 *          if (%r1 & 0x1) goto 1 # wait if first bit is 1 
 *          %r1>>=1  # restore the value (only 31 bits can be used )
 *          %r1=op (%r1, ...) 
 *          %r1<<=1   # shift back the value, such that the first bit is 0
 *          str %r1, [&atomic_val]  # write the value
 *
 * However only 31 bits could be used (=> atomic_*_int and atomic_*_long
 *  would still have to be lock based, since in these cases we guarantee all 
 *  the bits)  and I'm not sure there would be a significant performance
 *  benefit when compared with the fallback lock based version:
 *    lock(atomic_lock);
 *    atomic_val=op(*atomic_val, ...)
 *    unlock(atomic_lock);
 *
 *  -- andrei
 */

#endif /* __CPU_arm6 */


#endif
