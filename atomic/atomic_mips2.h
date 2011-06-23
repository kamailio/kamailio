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
 * @brief Atomic operations and memory barriers (MIPS isa 2 and MIPS64 specific)
 * 
 * Atomic operations and memory barriers (MIPS isa 2 and MIPS64 specific)
 * \warning atomic ops do not include memory barriers, see atomic_ops.h for
 * more details.
 * \warning not tested on MIPS64 (not even a compile test)
 *
 * Config defines:
 * - NOSMP (in NOSMP mode it will also work on mips isa 1 CPUs that support
 *   LL and SC, see MIPS_HAS_LLSC in atomic_ops.h)
 * - __CPU_MIPS64 (mips64 arch., in 64 bit mode: long and void* are 64 bits)
 * - __CPU_MIPS2 or __CPU_MIPS && MIPS_HAS_LLSC && NOSMP (if __CPU_MIPS64 is not defined)
 * @ingroup atomic
 */

/* 
 * History:
 * --------
 *  2006-03-08  created by andrei
 *  2007-05-10  added atomic_add & atomic_cmpxchg (andrei)
 *  2007-05-29  added membar_depends(), membar_*_atomic_op and
 *                membar_*_atomic_setget (andrei)
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
#define membar_depends()  do {} while(0) /* really empty, not even a cc bar. */
#define membar_enter_lock() membar()
#define membar_leave_lock() membar()
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


/* %0=var, %1=*var, %2=new, %3=old :
 * ret=*var; if *var==old  then *var=new; return ret
 * => if succesfull (changed var to new)  ret==old */
#define ATOMIC_CMPXCHG_DECL(NAME, P_TYPE) \
	inline static P_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
														P_TYPE old, \
														P_TYPE new_v) \
	{ \
		asm volatile( \
			ATOMIC_ASM_OP_##P_TYPE("bne %1, %3, 2f \n\t nop") \
			"2:    \n\t" \
			: "=m"(*var), "=&r"(old), "=r"(new_v)  \
			: "r"(old), "m"(*var), "2"(new_v) \
			 \
			); \
		return old; \
	}

ATOMIC_FUNC_DECL(inc,      "addiu %2, %1, 1", int, void, /* no return */ )
ATOMIC_FUNC_DECL_CT(dec,   "subu %2, %1, %3", 1,  int, void, /* no return */ )
ATOMIC_FUNC_DECL1(and, "and %2, %1, %3", int, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,  "or  %2, %1, %3", int, void,  /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "addiu %2, %1, 1", int, int, (ret+1)==0 )
ATOMIC_FUNC_DECL_CT(dec_and_test, "subu %2, %1, %3", 1, int, int, (ret-1)==0 )
ATOMIC_FUNC_DECL2(get_and_set, "" /* nothing needed */, int, int, ret )
ATOMIC_CMPXCHG_DECL(cmpxchg, int)
ATOMIC_FUNC_DECL1(add, "addu %2, %1, %3 \n\t move %1, %2", int, int, ret )

#ifdef __CPU_mips64

ATOMIC_FUNC_DECL(inc,      "daddiu %2, %1, 1", long, void, /* no return */ )
ATOMIC_FUNC_DECL_CT(dec,   "dsubu %2, %1, %3", 1,  long, void, /* no return */ )
ATOMIC_FUNC_DECL1(and, "and %2, %1, %3", long, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,  "or  %2, %1, %3", long, void,  /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "daddiu %2, %1, 1", long, long, (ret+1)==0 )
ATOMIC_FUNC_DECL_CT(dec_and_test, "dsubu %2, %1, %3", 1,long, long, (ret-1)==0 )
ATOMIC_FUNC_DECL2(get_and_set, "" /* nothing needed */, long, long, ret )
ATOMIC_CMPXCHG_DECL(cmpxchg, long)
ATOMIC_FUNC_DECL1(add, "daddu %2, %1, %3 \n\t move %1, %2", long, long, ret )

#else /* ! __CPU_mips64 => __CPU_mips2 or __CPU_mips */

ATOMIC_FUNC_DECL(inc,      "addiu %2, %1, 1", long, void, /* no return */ )
ATOMIC_FUNC_DECL_CT(dec,   "subu %2, %1, %3", 1,  long, void, /* no return */ )
ATOMIC_FUNC_DECL1(and, "and %2, %1, %3", long, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,  "or  %2, %1, %3", long, void,  /* no return */ )
ATOMIC_FUNC_DECL(inc_and_test, "addiu %2, %1, 1", long, long, (ret+1)==0 )
ATOMIC_FUNC_DECL_CT(dec_and_test, "subu %2, %1, %3", 1,long, long, (ret-1)==0 )
ATOMIC_FUNC_DECL2(get_and_set, "" /* nothing needed */, long, long, ret )
ATOMIC_CMPXCHG_DECL(cmpxchg, long)
ATOMIC_FUNC_DECL1(add, "addu %2, %1, %3 \n\t move %1, %2", long, long, ret )

#endif /* __CPU_mips64 */

#define atomic_inc(var) atomic_inc_int(&(var)->val)
#define atomic_dec(var) atomic_dec_int(&(var)->val)
#define atomic_and(var, mask) atomic_and_int(&(var)->val, (mask))
#define atomic_or(var, mask)  atomic_or_int(&(var)->val, (mask))
#define atomic_dec_and_test(var) atomic_dec_and_test_int(&(var)->val)
#define atomic_inc_and_test(var) atomic_inc_and_test_int(&(var)->val)
#define atomic_get_and_set(var, i) atomic_get_and_set_int(&(var)->val, i)
#define atomic_add(var, i) atomic_add_int(&(var)->val, i)
#define atomic_cmpxchg(var, old, new_v)  \
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
