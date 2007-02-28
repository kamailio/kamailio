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
 *  atomic operations and memory barriers (alpha specific)
 *  WARNING: atomic ops do not include memory barriers
 *  see atomic_ops.h for more details 
 *
 *  Config defines:  - NOSMP 
 *                   - __CPU_alpha
 */
/* 
 * History:
 * --------
 *  2006-03-31  created by andrei
 */


#ifndef _atomic_alpha_h
#define _atomic_alpha_h

#define HAVE_ASM_INLINE_ATOMIC_OPS
#define HAVE_ASM_INLINE_MEMBAR

#warning alpha atomic code was not tested, please report problems to \
		serdev@iptel.org or andrei@iptel.org

#ifdef NOSMP
#define membar() asm volatile ("" : : : "memory") /* gcc do not cache barrier*/
#define membar_read()  membar()
#define membar_write() membar()
#else

#define membar()		asm volatile ("    mb \n\t" : : : "memory" ) 
#define membar_read()	membar()
#define membar_write()	asm volatile ("    wmb \n\t" : : : "memory" )

#endif /* NOSMP */



/* main asm block 
 * if store failes, jump _forward_ (optimization, because back jumps are
 *  always predicted to happen on alpha )*/
#define ATOMIC_ASM_OP00_int(op) \
			"1:   ldl_l %0, %2 \n\t" \
			"     " op "\n\t" \
			"     stl_c %0, %2 \n\t" \
			"     beq %0, 2f \n\t" \
			".subsection 2 \n\t" \
			"2:   br 1b \n\t" \
			".previous \n\t"

/* as above, but output in %1 instead of %0 (%0 is not clobbered) */
#define ATOMIC_ASM_OP01_int(op) \
			"1:   ldl_l %0, %3 \n\t" \
			"     " op "\n\t" \
			"     stl_c %1, %3 \n\t" \
			"     beq %1, 2f \n\t" \
			".subsection 2 \n\t" \
			"2:   br 1b \n\t" \
			".previous \n\t"

#define ATOMIC_ASM_OP00_long(op) \
			"1:   ldq_l %0, %2 \n\t" \
			"     " op "\n\t" \
			"     stq_c %0, %2 \n\t" \
			"     beq %0, 2f \n\t" \
			".subsection 2 \n\t" \
			"2:   br 1b \n\t" \
			".previous \n\t"

/* as above, but output in %1 instead of %0 (%0 is not clobbered) */
#define ATOMIC_ASM_OP01_long(op) \
			"1:   ldq_l %0, %3 \n\t" \
			"     " op "\n\t" \
			"     stq_c %1, %3 \n\t" \
			"     beq %1, 2f \n\t" \
			".subsection 2 \n\t" \
			"2:   br 1b \n\t" \
			".previous \n\t"



/* input in %0, output in %0 */
#define ATOMIC_FUNC_DECL0_0(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var) \
	{ \
		P_TYPE ret; \
		asm volatile( \
			ATOMIC_ASM_OP00_##P_TYPE(OP) : "=&r"(ret), "=m"(*var) : "m"(*var) \
			); \
		return RET_EXPR; \
	}


#if defined __GNUC__ &&  __GNUC__ < 3 && __GNUC_MINOR__ < 9
#define ATOMIC_FUNC_DECL01_1(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
														P_TYPE v ) \
	{ \
		P_TYPE ret; \
		asm volatile( \
			ATOMIC_ASM_OP01_##P_TYPE(OP) \
			: "=&r"(ret), "=r"(v), "=m"(*var), "0"(v)  : "m"(*var) \
			); \
		return RET_EXPR; \
	}
#else
/* input in %0, and %1 (param), output in %1,  %0 goes in ret */
#define ATOMIC_FUNC_DECL01_1(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
														P_TYPE v ) \
	{ \
		P_TYPE ret; \
		asm volatile( \
			ATOMIC_ASM_OP01_##P_TYPE(OP) \
			: "=&r"(ret), "+r"(v), "=m"(*var)  : "m"(*var) \
			); \
		return RET_EXPR; \
	}
#endif /* gcc && gcc version < 2.9 */

/* input in %0, output in %1, %0 goes in ret */
#define ATOMIC_FUNC_DECL0_1(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var) \
	{ \
		P_TYPE ret, tmp; \
		asm volatile( \
			ATOMIC_ASM_OP01_##P_TYPE(OP) \
			: "=&r"(ret), "=&r"(tmp), "=m"(*var)  : "m"(*var) \
			); \
		return RET_EXPR; \
	}


/* input in %0 and %3 (param), output in %0 */
#define ATOMIC_FUNC_DECL03_0(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
														P_TYPE v) \
	{ \
		P_TYPE ret; \
		asm volatile( \
			ATOMIC_ASM_OP00_##P_TYPE(OP) \
			: "=&r"(ret), "=m"(*var)  : "m"(*var), "r"(v) \
			); \
		return RET_EXPR; \
	}


ATOMIC_FUNC_DECL0_0(inc, "addl %0, 1, %0", int, void, /* no return */ )
ATOMIC_FUNC_DECL0_0(dec, "subl %0, 1, %0", int, void, /* no return */ )
ATOMIC_FUNC_DECL03_0(and, "and %0, %3, %0", int, void, /* no return */ )
ATOMIC_FUNC_DECL03_0(or,  "bis %0, %3, %0", int, void, /* no return */ )
ATOMIC_FUNC_DECL0_1(inc_and_test, "addl %0, 1, %1", int, int, (ret+1)==0 )
ATOMIC_FUNC_DECL0_1(dec_and_test, "subl %0, 1, %1", int, int, (ret-1)==0 )
ATOMIC_FUNC_DECL01_1(get_and_set, "" /* nothing needed */, int, int, ret )

ATOMIC_FUNC_DECL0_0(inc, "addq %0, 1, %0", long, void, /* no return */ )
ATOMIC_FUNC_DECL0_0(dec, "subq %0, 1, %0", long, void, /* no return */ )
ATOMIC_FUNC_DECL03_0(and, "and %0, %3, %0", long, void, /* no return */ )
ATOMIC_FUNC_DECL03_0(or,  "bis %0, %3, %0", long, void, /* no return */ )
ATOMIC_FUNC_DECL0_1(inc_and_test, "addq %0, 1, %1", long, long, (ret+1)==0 )
ATOMIC_FUNC_DECL0_1(dec_and_test, "subq %0, 1, %1", long, long, (ret-1)==0 )
ATOMIC_FUNC_DECL01_1(get_and_set, "" /* nothing needed */, long, long, ret )


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
