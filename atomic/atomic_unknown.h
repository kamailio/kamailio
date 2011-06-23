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
 * @brief Atomic operations and memory barriers implemented using locks
 *
 * Atomic operations and memory barriers implemented using locks
 * (for architectures not yet supported via inline assembler).
 *
 * \warning atomic ops do not include memory barriers, see atomic_ops.h
 * for more details 
 *
 * Config defines:
 * - NOSMP (membars are null in this case)
 * - HAVE_ASM_INLINE_MEMBAR (membars already defined => use them)
 * - HAVE_ASM_INLINE_ATOMIC_OPS (atomic ops already defined => don't
 *   redefine them)
 * @ingroup atomic
 */

/* 
 * History:
 * --------
 *  2006-03-08  created by andrei
 *  2007-05-11  added atomic_add and atomic_cmpxchg 
 *              use lock_set if lock economy is not needed (andrei)
 *  2007-05-29  added membar_depends(), membar_*_atomic_op and
 *                membar_*_atomic_setget (andrei)
 */


#ifndef _atomic_unknown_h
#define _atomic_unknown_h

#include "../lock_ops.h"



#ifndef HAVE_ASM_INLINE_MEMBAR

#ifdef NOSMP
#define membar() do {} while(0)
#else /* SMP */

#warning no native memory barrier implementations, falling back to slow lock \
	       based workarround

#define MEMBAR_USES_LOCK

extern gen_lock_t* __membar_lock; /* init in atomic_ops.c */
#define _membar_lock    lock_get(__membar_lock)
#define _membar_unlock  lock_release(__membar_lock)

/* memory barriers 
 *  not a known cpu -> fall back unlock/lock: safe but costly  (it should 
 *  include a memory barrier effect)
 *  lock/unlock does not imply a full memory barrier effect (it allows mixing
 *   operations from before the lock with operations after the lock _inside_
 *  the lock & unlock block; however in most implementations it is equivalent
 *  with at least membar StoreStore | StoreLoad | LoadStore => only LoadLoad
 *  is missing). On the other hand and unlock/lock will always be equivalent
 *  with a full memory barrier
 *  => to be safe we must use either unlock; lock or lock; unlock; lock; unlock
 *  --andrei*/
#define membar() \
	do{\
		_membar_unlock; \
		_membar_lock; \
	} while(0)
#endif /* NOSMP */


#define membar_write() membar()

#define membar_read()  membar()


#ifndef __CPU_alpha
#define membar_depends()  do {} while(0) /* really empty, not even a cc bar. */
#else
/* really slow */
#define membar_depends()  membar_read()
#endif

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

#endif /* HAVE_ASM_INLINE_MEMBAR */


#ifndef HAVE_ASM_INLINE_ATOMIC_OPS

#ifdef GEN_LOCK_SET_T_UNLIMITED
#ifndef ATOMIC_OPS_USE_LOCK_SET
#define ATOMIC_OPS_USE_LOCK_SET
#endif
#else
#ifndef ATOMIC_OPS_USE_LOCK
#define ATOMIC_OPS_USE_LOCK
#endif
#endif /* GEN_LOCK_SET_T_UNLIMITED */

#ifdef ATOMIC_OPS_USE_LOCK_SET 
#define _ATOMIC_LS_SIZE	256
/* hash after the variable address: ignore first 4 bits since
 * vars are generally alloc'ed at at least 16 bytes multiples */
#define _atomic_ls_hash(v)  ((((unsigned long)(v))>>4)&(_ATOMIC_LS_SIZE-1))
extern gen_lock_set_t* _atomic_lock_set;

#define atomic_lock(v)   lock_set_get(_atomic_lock_set, _atomic_ls_hash(v))
#define atomic_unlock(v) lock_set_release(_atomic_lock_set, _atomic_ls_hash(v))

#else
extern gen_lock_t* _atomic_lock; /* declared and init in ../atomic_ops.c */

#define atomic_lock(v)    lock_get(_atomic_lock)
#define atomic_unlock(v)  lock_release(_atomic_lock)

#endif /* ATOMIC_OPS_USE_LOCK_SET */

/* atomic ops */


/* OP can include var (function param), no other var. is declared */
#define ATOMIC_FUNC_DECL(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var) \
	{ \
		atomic_lock(var); \
		OP ; \
		atomic_unlock(var); \
		return RET_EXPR; \
	}


/* like above, but takes an extra param: v =>
 *  OP can use var and v (function params) */
#define ATOMIC_FUNC_DECL1(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
														P_TYPE v) \
	{ \
		atomic_lock(var); \
		OP ; \
		atomic_unlock(var); \
		return RET_EXPR; \
	}

/* OP can include var (function param), and ret (return)
 *  ( like ATOMIC_FUNC_DECL, but includes ret) */
#define ATOMIC_FUNC_DECL_RET(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var) \
	{ \
		P_TYPE ret; \
		atomic_lock(var); \
		OP ; \
		atomic_unlock(var); \
		return RET_EXPR; \
	}

/* like ATOMIC_FUNC_DECL1, but declares an extra variable: P_TYPE ret */
#define ATOMIC_FUNC_DECL1_RET(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
														P_TYPE v) \
	{ \
		P_TYPE ret; \
		atomic_lock(var); \
		OP ; \
		atomic_unlock(var); \
		return RET_EXPR; \
	}

/* like ATOMIC_FUNC_DECL1_RET, but takes an extra param */
#define ATOMIC_FUNC_DECL2_RET(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
														P_TYPE v1, P_TYPE v2)\
	{ \
		P_TYPE ret; \
		atomic_lock(var); \
		OP ; \
		atomic_unlock(var); \
		return RET_EXPR; \
	}


ATOMIC_FUNC_DECL(inc,      (*var)++, int, void, /* no return */ )
ATOMIC_FUNC_DECL(dec,      (*var)--, int, void, /* no return */ )
ATOMIC_FUNC_DECL1(and,     *var&=v, int, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,      *var|=v, int, void, /* no return */ )
ATOMIC_FUNC_DECL_RET(inc_and_test, ret=++(*var), int, int, (ret==0) )
ATOMIC_FUNC_DECL_RET(dec_and_test, ret=--(*var), int, int, (ret==0) )
ATOMIC_FUNC_DECL1_RET(get_and_set, ret=*var;*var=v , int, int,  ret)
ATOMIC_FUNC_DECL2_RET(cmpxchg, ret=*var;\
							*var=(((ret!=v1)-1)&v2)+(~((ret!=v1)-1)&ret),\
							int, int,  ret)
ATOMIC_FUNC_DECL1_RET(add, *var+=v;ret=*var, int, int, ret )

ATOMIC_FUNC_DECL(inc,      (*var)++, long, void, /* no return */ )
ATOMIC_FUNC_DECL(dec,      (*var)--, long, void, /* no return */ )
ATOMIC_FUNC_DECL1(and,     *var&=v, long, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,      *var|=v, long, void, /* no return */ )
ATOMIC_FUNC_DECL_RET(inc_and_test, ret=++(*var), long, long, (ret==0) )
ATOMIC_FUNC_DECL_RET(dec_and_test, ret=--(*var), long, long, (ret==0) )
ATOMIC_FUNC_DECL1_RET(get_and_set, ret=*var;*var=v , long, long,  ret)
ATOMIC_FUNC_DECL2_RET(cmpxchg, ret=*var;\
							*var=(((ret!=v1)-1)&v2)+(~((ret!=v1)-1)&ret),\
							long, long,  ret)
ATOMIC_FUNC_DECL1_RET(add, *var+=v;ret=*var, long, long, ret )


#define atomic_inc(var) atomic_inc_int(&(var)->val)
#define atomic_dec(var) atomic_dec_int(&(var)->val)
#define atomic_and(var, mask) atomic_and_int(&(var)->val, (mask))
#define atomic_or(var, mask)  atomic_or_int(&(var)->val, (mask))
#define atomic_dec_and_test(var) atomic_dec_and_test_int(&(var)->val)
#define atomic_inc_and_test(var) atomic_inc_and_test_int(&(var)->val)
#define atomic_get_and_set(var, i) atomic_get_and_set_int(&(var)->val, i)
#define atomic_cmpxchg(var, old, new_v) \
	atomic_cmpxchg_int(&(var)->val, old, new_v)
#define atomic_add(var, v) atomic_add_int(&(var)->val, v)


/* memory barrier versions, the same as "normal" versions (since the
 *  locks act as membars), *  except fot * the set/get 
 */

/* mb_atomic_{set,get} use membar() : if we're lucky we have membars
 * for the arch. (e.g. sparc32) => membar() might be cheaper then lock/unlock */
#define mb_atomic_set_int(v, i) \
	do{ \
		membar(); \
		atomic_set_int(v, i); \
	}while(0)

inline static int  mb_atomic_get_int(volatile int* v)
{
		membar();
		return atomic_get_int(v);
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


/* the rest are the same as the non membar version (the locks have a membar
 * effect) */
#define mb_atomic_inc_int(v)	atomic_inc_int(v)
#define mb_atomic_dec_int(v)	atomic_dec_int(v)
#define mb_atomic_or_int(v, m)	atomic_or_int(v, m)
#define mb_atomic_and_int(v, m)	atomic_and_int(v, m)
#define mb_atomic_inc_and_test_int(v)	atomic_inc_and_test_int(v)
#define mb_atomic_dec_and_test_int(v)	atomic_dec_and_test_int(v)
#define mb_atomic_get_and_set_int(v, i)	atomic_get_and_set_int(v, i)
#define mb_atomic_cmpxchg_int(v, o, n)	atomic_cmpxchg_int(v, o, n)
#define mb_atomic_add_int(v, i)	atomic_add_int(v, i)

#define mb_atomic_inc_long(v)	atomic_inc_long(v)
#define mb_atomic_dec_long(v)	atomic_dec_long(v)
#define mb_atomic_or_long(v, m)	atomic_or_long(v, m)
#define mb_atomic_and_long(v, m)	atomic_and_long(v, m)
#define mb_atomic_inc_and_test_long(v)	atomic_inc_and_test_long(v)
#define mb_atomic_dec_and_test_long(v)	atomic_dec_and_test_long(v)
#define mb_atomic_get_and_set_long(v, i)	atomic_get_and_set_long(v, i)
#define mb_atomic_cmpxchg_long(v, o, n)	atomic_cmpxchg_long(v, o, n)
#define mb_atomic_add_long(v, i)	atomic_add_long(v, i)

#define mb_atomic_inc(var) mb_atomic_inc_int(&(var)->val)
#define mb_atomic_dec(var) mb_atomic_dec_int(&(var)->val)
#define mb_atomic_and(var, mask) mb_atomic_and_int(&(var)->val, (mask))
#define mb_atomic_or(var, mask)  mb_atomic_or_int(&(var)->val, (mask))
#define mb_atomic_dec_and_test(var) mb_atomic_dec_and_test_int(&(var)->val)
#define mb_atomic_inc_and_test(var) mb_atomic_inc_and_test_int(&(var)->val)
#define mb_atomic_get_and_set(var, i) mb_atomic_get_and_set_int(&(var)->val, i)
#define mb_atomic_cmpxchg(v, o, n)	atomic_cmpxchg_int(&(v)->val, o, n)
#define mb_atomic_add(v, i)	atomic_add_int(&(v)->val, i)

#define mb_atomic_get(var)	mb_atomic_get_int(&(var)->val)
#define mb_atomic_set(var, i)	mb_atomic_set_int(&(var)->val, i)

#endif /* if HAVE_ASM_INLINE_ATOMIC_OPS */

#endif
