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
 *  atomic operations and memory barriers (sparc64 version, 32 and 64 bit modes)
 *  WARNING: atomic ops do not include memory barriers
 *  see atomic_ops.h for more details 
 *
 *  Config defs: - SPARC64_MODE (if defined long is assumed to be 64 bits
 *                               else long & void* are assumed to be 32 for
 *                               sparc32plus code)
 *               - NOSMP
 */
/* 
 * History:
 * --------
 *  2006-03-28  created by andrei
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
#else /* SMP */
#define membar() \
	asm volatile ( \
			"membar #LoadLoad | #LoadStore | #StoreStore | #StoreLoad \n\t" \
			: : : "memory")

#define membar_read() asm volatile ("membar #LoadLoad \n\t" : : : "memory")
#define membar_write() asm volatile ("membar #StoreStore \n\t" : : : "memory")
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




ATOMIC_FUNC_DECL(inc,      "add  %0,  1, %1", int, void, /* no return */ )
ATOMIC_FUNC_DECL(dec,      "sub  %0,  1, %1", int, void, /* no return */ )
ATOMIC_FUNC_DECL1(and,     "and  %0, %4, %1", int, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,      "or   %0, %4, %1", int, void, /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "add   %0, 1, %1", int, int, ((ret+1)==0) )
ATOMIC_FUNC_DECL(dec_and_test, "sub   %0, 1, %1", int, int, ((ret-1)==0) )
ATOMIC_FUNC_DECL1(get_and_set, "mov %4, %1" , int, int,  ret)


ATOMIC_FUNC_DECL(inc,      "add  %0,  1, %1", long, void, /* no return */ )
ATOMIC_FUNC_DECL(dec,      "sub  %0,  1, %1", long, void, /* no return */ )
ATOMIC_FUNC_DECL1(and,     "and  %0, %4, %1", long, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,      "or   %0, %4, %1", long, void, /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "add   %0, 1, %1", long, long, ((ret+1)==0) )
ATOMIC_FUNC_DECL(dec_and_test, "sub   %0, 1, %1", long, long, ((ret-1)==0) )
ATOMIC_FUNC_DECL1(get_and_set, "mov %4, %1" , long, long,  ret)


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

#endif
