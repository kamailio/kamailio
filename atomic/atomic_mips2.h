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
 *  atomic operations and memory barriers (mips isa 2 and mips64 specific)
 *  WARNING: atomic ops do not include memory barriers
 *  see atomic_ops.h for more details 
 *  WARNING: not tested on mips64 (not even a compile test)
 *
 *  Config defines:  - NOSMP (in NOSMP mode it will also work on mips isa 1
 *                            cpus that support LL and SC, see MIPS_HAS_LLSC
 *                            in atomic_ops.h)
 *                   - __CPU_MIPS64 (mips64 arch., in 64 bit mode: long and
 *                                    void* are 64 bits)
 *                   - __CPU_MIPS2 or __CPU_MIPS && MIPS_HAS_LLSC && NOSMP
 *                                 (if __CPU_MIPS64 is not defined)
 */
/* 
 * History:
 * --------
 *  2006-03-08  created by andrei
 */


#ifndef _atomic_mips2_h
#define _atomic_mips2_h

#define HAVE_ASM_INLINE_ATOMIC_OPS
#define HAVE_ASM_INLINE_MEMBAR

#ifdef __CPU_mips64
#warning mips64 atomic code was not tested, please report problems to \
		serdev@iptel.org or andrei@iptel.org
#endif

#ifdef NOSMP
#define membar() asm volatile ("" : : : "memory") /* gcc do not cache barrier*/
#define membar_read()  membar()
#define membar_write() membar()
#else

#define membar() \
	asm volatile( \
			".set push \n\t" \
			".set noreorder \n\t" \
			".set mips2 \n\t" \
			"    sync\n\t" \
			".set pop \n\t" \
			: : : "memory" \
			) 

#define membar_read()  membar()
#define membar_write() membar()

#endif /* NOSMP */



/* main asm block */
#define ATOMIC_ASM_OP_int(op) \
			".set push \n\t" \
			".set noreorder \n\t" \
			".set mips2 \n\t" \
			"1:   ll %1, %0 \n\t" \
			"     " op "\n\t" \
			"     sc %2, %0 \n\t" \
			"     beqz %2, 1b \n\t" \
			"     nop \n\t" /* delay slot */ \
			".set pop \n\t" 

#ifdef __CPU_mips64
#define ATOMIC_ASM_OP_long(op) \
			".set push \n\t" \
			".set noreorder \n\t" \
			"1:   lld %1, %0 \n\t" \
			"     " op "\n\t" \
			"     scd %2, %0 \n\t" \
			"     beqz %2, 1b \n\t" \
			"     nop \n\t" /* delay slot */ \
			".set pop \n\t" 
#else /* ! __CPU_mips64 => __CPU_mips2 or __CPU_mips & MIPS_HAS_LLSC */
#define ATOMIC_ASM_OP_long(op) ATOMIC_ASM_OP_int(op)
#endif


#define ATOMIC_FUNC_DECL(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var) \
	{ \
		P_TYPE ret, tmp; \
		asm volatile( \
			ATOMIC_ASM_OP_##P_TYPE(OP) \
			: "=m"(*var), "=&r"(ret), "=&r"(tmp)  \
			: "m"(*var) \
			 \
			); \
		return RET_EXPR; \
	}


/* same as above, but with CT in %3 */
#define ATOMIC_FUNC_DECL_CT(NAME, OP, CT, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var) \
	{ \
		P_TYPE ret, tmp; \
		asm volatile( \
			ATOMIC_ASM_OP_##P_TYPE(OP) \
			: "=m"(*var), "=&r"(ret), "=&r"(tmp)  \
			: "r"((CT)), "m"(*var) \
			 \
			); \
		return RET_EXPR; \
	}


/* takes an extra param, i which goes in %3 */
#define ATOMIC_FUNC_DECL1(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
														P_TYPE i) \
	{ \
		P_TYPE ret, tmp; \
		asm volatile( \
			ATOMIC_ASM_OP_##P_TYPE(OP) \
			: "=m"(*var), "=&r"(ret), "=&r"(tmp)  \
			: "r"((i)), "m"(*var) \
			 \
			); \
		return RET_EXPR; \
	}


/* takes an extra param, like above, but i  goes in %2 */
#define ATOMIC_FUNC_DECL2(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
														P_TYPE i) \
	{ \
		P_TYPE ret; \
		asm volatile( \
			ATOMIC_ASM_OP_##P_TYPE(OP) \
			: "=m"(*var), "=&r"(ret), "+&r"(i)  \
			: "m"(*var) \
			 \
			); \
		return RET_EXPR; \
	}



ATOMIC_FUNC_DECL(inc,      "addiu %2, %1, 1", int, void, /* no return */ )
ATOMIC_FUNC_DECL_CT(dec,   "subu %2, %1, %3", 1,  int, void, /* no return */ )
ATOMIC_FUNC_DECL1(and, "and %2, %1, %3", int, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,  "or  %2, %1, %3", int, void,  /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "addiu %2, %1, 1", int, int, (ret+1)==0 )
ATOMIC_FUNC_DECL_CT(dec_and_test, "subu %2, %1, %3", 1, int, int, (ret-1)==0 )
ATOMIC_FUNC_DECL2(get_and_set, "" /* nothing needed */, int, int, ret )

#ifdef __CPU_mips64

ATOMIC_FUNC_DECL(inc,      "daddiu %2, %1, 1", long, void, /* no return */ )
ATOMIC_FUNC_DECL_CT(dec,   "dsubu %2, %1, %3", 1,  long, void, /* no return */ )
ATOMIC_FUNC_DECL1(and, "and %2, %1, %3", long, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,  "or  %2, %1, %3", long, void,  /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "daddiu %2, %1, 1", long, long, (ret+1)==0 )
ATOMIC_FUNC_DECL_CT(dec_and_test, "dsubu %2, %1, %3", 1,long, long, (ret-1)==0 )
ATOMIC_FUNC_DECL2(get_and_set, "" /* nothing needed */, long, long, ret )

#else /* ! __CPU_mips64 => __CPU_mips2 or __CPU_mips */

ATOMIC_FUNC_DECL(inc,      "addiu %2, %1, 1", long, void, /* no return */ )
ATOMIC_FUNC_DECL_CT(dec,   "subu %2, %1, %3", 1,  long, void, /* no return */ )
ATOMIC_FUNC_DECL1(and, "and %2, %1, %3", long, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,  "or  %2, %1, %3", long, void,  /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "addiu %2, %1, 1", long, long, (ret+1)==0 )
ATOMIC_FUNC_DECL_CT(dec_and_test, "subu %2, %1, %3", 1,long, long, (ret-1)==0 )
ATOMIC_FUNC_DECL2(get_and_set, "" /* nothing needed */, long, long, ret )

#endif /* __CPU_mips64 */

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
