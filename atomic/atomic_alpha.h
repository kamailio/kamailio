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
 * @brief Atomic operations and memory barriers (alpha specific)
 *
 * Atomic operations and memory barriers (alpha specific)
 * \warning atomic ops do not include memory barriers, see atomic_ops.h
 * for more details.
 * 
 * Config defines:
 * - NOSMP 
 * - __CPU_alpha
 * @ingroup atomic
 */
/* 
 * History:
 * --------
 *  2006-03-31  created by andrei
 *  2007-05-10  added atomic_add & atomic_cmpxchg (andrei)
 *  2007-05-29  added membar_depends(), membar_*_atomic_op and
 *                membar_*_atomic_setget (andrei)
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

#define membar()		asm volatile ("    mb \n\t" : : : "memory" ) 
#define membar_read()	membar()
#define membar_write()	asm volatile ("    wmb \n\t" : : : "memory" )
#define membar_depends()	asm volatile ("mb \n\t" : : : "memory" )
#define membar_enter_lock() asm volatile("mb \n\t" : : : "memory")
#define membar_leave_lock() asm volatile("mb \n\t" : : : "memory")

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
			: "=&r"(ret), "=r"(v), "=m"(*var)  : "m"(*var), "1"(v) \
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

/* input in %0 and %3 (param), output in %0 */
/* cmpxchg var in %1, old in %0, new_v in %
 * makes the xchg if old==*var
 * returns initial *var (so if ret==old => new_v was written into var)*/
#define ATOMIC_CMPXCHG_DECL(NAME,  P_TYPE) \
	inline static P_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
														P_TYPE old, \
														P_TYPE new_v) \
	{ \
		P_TYPE ret; \
		P_TYPE tmp; \
		asm volatile( \
			ATOMIC_ASM_OP01_##P_TYPE("subq  %0, %5, %2 \n\t bne %2, 3f")\
			"3:    \n\t" \
			: "=&r"(ret), "=&r"(new_v), "=r"(tmp), "=m"(*var)  :\
				"m"(*var), "r"(old), "1"(new_v) :"cc" \
			); \
		return ret; \
	}


ATOMIC_FUNC_DECL0_0(inc, "addl %0, 1, %0", int, void, /* no return */ )
ATOMIC_FUNC_DECL0_0(dec, "subl %0, 1, %0", int, void, /* no return */ )
ATOMIC_FUNC_DECL03_0(and, "and %0, %3, %0", int, void, /* no return */ )
ATOMIC_FUNC_DECL03_0(or,  "bis %0, %3, %0", int, void, /* no return */ )
ATOMIC_FUNC_DECL0_1(inc_and_test, "addl %0, 1, %1", int, int, (ret+1)==0 )
ATOMIC_FUNC_DECL0_1(dec_and_test, "subl %0, 1, %1", int, int, (ret-1)==0 )
ATOMIC_FUNC_DECL01_1(get_and_set, "" /* nothing needed */, int, int, ret )
ATOMIC_CMPXCHG_DECL(cmpxchg, int )
ATOMIC_FUNC_DECL01_1(add, "addl %1, %0, %1", int, int, ret+v)

ATOMIC_FUNC_DECL0_0(inc, "addq %0, 1, %0", long, void, /* no return */ )
ATOMIC_FUNC_DECL0_0(dec, "subq %0, 1, %0", long, void, /* no return */ )
ATOMIC_FUNC_DECL03_0(and, "and %0, %3, %0", long, void, /* no return */ )
ATOMIC_FUNC_DECL03_0(or,  "bis %0, %3, %0", long, void, /* no return */ )
ATOMIC_FUNC_DECL0_1(inc_and_test, "addq %0, 1, %1", long, long, (ret+1)==0 )
ATOMIC_FUNC_DECL0_1(dec_and_test, "subq %0, 1, %1", long, long, (ret-1)==0 )
ATOMIC_FUNC_DECL01_1(get_and_set, "" /* nothing needed */, long, long, ret )
ATOMIC_CMPXCHG_DECL(cmpxchg, long )
ATOMIC_FUNC_DECL01_1(add, "addq %1, %0, %1", long, long, ret+v)


#define atomic_inc(var) atomic_inc_int(&(var)->val)
#define atomic_dec(var) atomic_dec_int(&(var)->val)
#define atomic_and(var, mask) atomic_and_int(&(var)->val, (mask))
#define atomic_or(var, mask)  atomic_or_int(&(var)->val, (mask))
#define atomic_dec_and_test(var) atomic_dec_and_test_int(&(var)->val)
#define atomic_inc_and_test(var) atomic_inc_and_test_int(&(var)->val)
#define atomic_get_and_set(var, i) atomic_get_and_set_int(&(var)->val, i)
#define atomic_cmpxchg(var, old, new_v) \
		atomic_cmpxchg_int(&(var)->val, old, new_v)
#define atomic_add(var, v) atomic_add_int(&(var)->val, (v))


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
