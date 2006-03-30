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
 *  atomic operations and memory barriers (powerpc and powerpc64 versions)
 *  WARNING: atomic ops do not include memory barriers
 *  see atomic_ops.h for more details 
 *  WARNING: not tested on ppc64
 *
 *  Config defines:  - NOSMP
 *                   - __CPU_ppc64  (powerpc64 w/ 64 bits long and void*)
 *                   - __CPU_ppc    (powerpc or powerpc64 32bit mode)
 */
/* 
 * History:
 * --------
 *  2006-03-24  created by andrei
 */

#ifndef _atomic_ppc_h
#define _atomic_ppc_h

#define HAVE_ASM_INLINE_ATOMIC_OPS
#define HAVE_ASM_INLINE_MEMBAR

#ifdef __CPU_ppc64
#warning powerpc64 atomic code was not tested, please report problems to \
		serdev@iptel.org or andrei@iptel.org
#endif


#ifdef NOSMP
#define membar() asm volatile ("" : : : "memory") /* gcc do not cache barrier*/
#define membar_read()  membar()
#define membar_write() membar()
#else
#define membar() asm volatile ("sync \n\t" : : : "memory") 
/* lwsync orders LoadLoad, LoadStore and StoreStore */
#define membar_read() asm volatile ("lwsync \n\t" : : : "memory") 
/* on "normal" cached mem. eieio orders StoreStore */
#define membar_write() asm volatile ("eieio \n\t" : : : "memory") 
#endif /* NOSMP */


#define ATOMIC_ASM_OP0_int(op) \
	"1: lwarx  %0, 0, %2 \n\t" \
	"   " op " \n\t" \
	"   stwcx. %0, 0, %2 \n\t" \
	"   bne- 1b \n\t"

#define ATOMIC_ASM_OP3_int(op) \
	"1: lwarx  %0, 0, %2 \n\t" \
	"   " op " \n\t" \
	"   stwcx. %3, 0, %2 \n\t" \
	"   bne- 1b \n\t"

#ifdef __CPU_ppc64
#define ATOMIC_ASM_OP0_long(op) \
	"1: ldarx  %0, 0, %2 \n\t" \
	"   " op " \n\t" \
	"   stdcx. %0, 0, %2 \n\t" \
	"   bne- 1b \n\t"

#define ATOMIC_ASM_OP3_long(op) \
	"1: ldarx  %0, 0, %2 \n\t" \
	"   " op " \n\t" \
	"   stdcx. %3, 0, %2 \n\t" \
	"   bne- 1b \n\t"

#else /* __CPU_ppc */
#define ATOMIC_ASM_OP0_long ATOMIC_ASM_OP0_int
#define ATOMIC_ASM_OP3_long ATOMIC_ASM_OP3_int
#endif


#define ATOMIC_FUNC_DECL(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var) \
	{ \
		P_TYPE ret; \
		asm volatile( \
			ATOMIC_ASM_OP0_##P_TYPE(OP) \
			: "=&r"(ret), "=m"(*var) : "r"(var) : "cc" \
			); \
		return RET_EXPR; \
	}

/* same as above, but takes an extra param, v, which goes in %3 */
#define ATOMIC_FUNC_DECL1(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
															P_TYPE v) \
	{ \
		P_TYPE ret; \
		asm volatile( \
			ATOMIC_ASM_OP0_##P_TYPE(OP) \
			: "=&r"(ret), "=m"(*var) : "r"(var), "r"(v)  : "cc" \
			); \
		return RET_EXPR; \
	}

/* same as above, but uses ATOMIC_ASM_OP3, v in %3 and %3 not changed */
#define ATOMIC_FUNC_DECL3(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
															P_TYPE v) \
	{ \
		P_TYPE ret; \
		asm volatile( \
			ATOMIC_ASM_OP3_##P_TYPE(OP) \
			: "=&r"(ret), "=m"(*var) : "r"(var), "r"(v)  : "cc" \
			); \
		return RET_EXPR; \
	}



ATOMIC_FUNC_DECL(inc,      "addic  %0, %0,  1", int, void, /* no return */ )
ATOMIC_FUNC_DECL(dec,      "addic %0, %0,  -1", int, void, /* no return */ )
ATOMIC_FUNC_DECL1(and,     "and     %0, %0, %3", int, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,      "or     %0, %0, %3", int, void, /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "addic   %0, %0, 1", int, int, (ret==0) )
ATOMIC_FUNC_DECL(dec_and_test, "addic  %0, %0, -1", int, int, (ret==0) )
ATOMIC_FUNC_DECL3(get_and_set, /* no extra op needed */ , int, int,  ret)

ATOMIC_FUNC_DECL(inc,      "addic  %0, %0,  1", long, void, /* no return */ )
ATOMIC_FUNC_DECL(dec,      "addic %0, %0,  -1", long, void, /* no return */ )
ATOMIC_FUNC_DECL1(and,     "and     %0, %0, %3",long, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,      "or     %0, %0, %3", long, void, /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "addic   %0, %0, 1", long, long, (ret==0) )
ATOMIC_FUNC_DECL(dec_and_test, "addic  %0, %0, -1", long, long, (ret==0) )
ATOMIC_FUNC_DECL3(get_and_set, /* no extra op needed */ , long, long,  ret)


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
