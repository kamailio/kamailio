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
 * @brief Atomic operations and memory barriers (PowerPC and PowerPC64 versions)
 * 
 * Atomic operations and memory barriers (PowerPC and PowerPC64 versions)
 * \warning atomic ops do not include memory barriers see atomic_ops.h for
 * more details. 
 * \warning not tested on ppc64
 *
 * Config defines:
 * - NOSMP
 * - __CPU_ppc64  (powerpc64 w/ 64 bits long and void*)
 * - __CPU_ppc    (powerpc or powerpc64 32bit mode)
 * @ingroup atomic
 */

/* 
 * History:
 * --------
 *  2006-03-24  created by andrei
 *  2007-05-11  added atomic_add and atomic_cmpxchg (andrei)
 *  2007-05-18  reverted to addic instead of addi - sometimes gcc uses
 *               r0 as the second operand in addi and  addi rD,r0, val
 *               is a special case, equivalent with rD=0+val and not
 *               rD=r0+val (andrei)
 *  2007-05-29  added membar_depends(), membar_*_atomic_op and
 *                membar_*_atomic_setget (andrei)
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
#define membar_depends()  do {} while(0) /* really empty, not even a cc bar. */
/* lock barriers: empty, not needed for NOSMP; the lock/unlock should already
 * contain gcc barriers*/
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

#else
#define membar() asm volatile ("sync \n\t" : : : "memory") 
/* lwsync orders LoadLoad, LoadStore and StoreStore */
#define membar_read() asm volatile ("lwsync \n\t" : : : "memory") 
/* on "normal" cached mem. eieio orders StoreStore */
#define membar_write() asm volatile ("eieio \n\t" : : : "memory") 
#define membar_depends()  do {} while(0) /* really empty, not even a cc bar. */
#define membar_enter_lock() asm volatile("lwsync \n\t" : : : "memory")
/* for unlock lwsync will work too and is faster then sync
 *  [IBM Prgramming Environments Manual, D.4.2.2] */
#define membar_leave_lock() asm volatile("lwsync \n\t" : : : "memory")
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


#define ATOMIC_ASM_OP0_int(op) \
	"1: lwarx  %0, 0, %2 \n\t" \
	"   " op " \n\t" \
	"   stwcx. %0, 0, %2 \n\t" \
	"   bne- 1b \n\t"

#define ATOMIC_ASM_OP3_int(op) \
	"1: lwarx  %0, 0, %2 \n\t" \
	"   " op " \n\t" \
	"   stwcx. %3, 0, %2 \n\t" \
	"   bne- 1b \n\t" \
	"2: \n\t"

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
	"   bne- 1b \n\t" \
	"2: \n\t"

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

/* cmpxchg, %3=var, %0=*var, %4=old, %3=new  */
#define ATOMIC_CMPXCHG_DECL(NAME, P_TYPE) \
	inline static P_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
															P_TYPE old, \
															P_TYPE new_v) \
	{ \
		P_TYPE ret; \
		asm volatile( \
			ATOMIC_ASM_OP3_##P_TYPE("cmpw %0, %4 \n\t bne- 2f") \
			: "=&r"(ret), "=m"(*var) : "r"(var), "r"(new_v), "r"(old) \
				: "cc" \
			); \
		return ret; \
	}





ATOMIC_FUNC_DECL(inc,      "addic  %0, %0,  1", int, void, /* no return */ )
ATOMIC_FUNC_DECL(dec,      "addic %0, %0,  -1", int, void, /* no return */ )
ATOMIC_FUNC_DECL1(and,     "and     %0, %0, %3", int, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,      "or     %0, %0, %3", int, void, /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "addic   %0, %0, 1", int, int, (ret==0) )
ATOMIC_FUNC_DECL(dec_and_test, "addic  %0, %0, -1", int, int, (ret==0) )
ATOMIC_FUNC_DECL3(get_and_set, /* no extra op needed */ , int, int,  ret)
ATOMIC_CMPXCHG_DECL(cmpxchg, int)
ATOMIC_FUNC_DECL1(add, "add %0, %0, %3" , int, int,  ret)

ATOMIC_FUNC_DECL(inc,      "addic  %0, %0,  1", long, void, /* no return */ )
ATOMIC_FUNC_DECL(dec,      "addic %0, %0,  -1", long, void, /* no return */ )
ATOMIC_FUNC_DECL1(and,     "and     %0, %0, %3",long, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,      "or     %0, %0, %3", long, void, /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "addic   %0, %0, 1", long, long, (ret==0) )
ATOMIC_FUNC_DECL(dec_and_test, "addic  %0, %0, -1", long, long, (ret==0) )
ATOMIC_FUNC_DECL3(get_and_set, /* no extra op needed */ , long, long,  ret)
ATOMIC_CMPXCHG_DECL(cmpxchg, long)
ATOMIC_FUNC_DECL1(add, "add %0, %0, %3" , long, long,  ret)


#define atomic_inc(var) atomic_inc_int(&(var)->val)
#define atomic_dec(var) atomic_dec_int(&(var)->val)
#define atomic_and(var, mask) atomic_and_int(&(var)->val, (mask))
#define atomic_or(var, mask)  atomic_or_int(&(var)->val, (mask))
#define atomic_dec_and_test(var) atomic_dec_and_test_int(&(var)->val)
#define atomic_inc_and_test(var) atomic_inc_and_test_int(&(var)->val)
#define atomic_get_and_set(var, i) atomic_get_and_set_int(&(var)->val, i)
#define atomic_cmpxchg(var, o, n) atomic_cmpxchg_int(&(var)->val, (o), (n))
#define atomic_add(var, i) atomic_add_int(&(var)->val, (i))


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
#define mb_atomic_cmpxchg(v, o, n)	atomic_cmpxchg_int(&(v)->val, o, n)
#define mb_atomic_add(v, a)	atomic_add_int(&(v)->val, a)


#endif
