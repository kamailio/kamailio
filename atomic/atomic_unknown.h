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
 *  atomic operations and memory barriers implemented using locks
 *  (for architectures not yet supported via inline asm)
 *
 *  WARNING: atomic ops do not include memory barriers
 *  see atomic_ops.h for more details 
 *
 *  Config defs: - NOSMP (membars are null in this case)
 *               - HAVE_ASM_INLINE_MEMBAR (membars arleady defined =>
 *                                          use them)
 *               - HAVE_ASM_INLINE_ATOMIC_OPS (atomic ops already defined
 *                                               => don't redefine them)
 *
 */
/* 
 * History:
 * --------
 *  2006-03-08  created by andrei
 */

#ifndef _atomic_unknown_h
#define _atomic_unknown_h

#include "../lock_ops.h"



#ifndef HAVE_ASM_INLINE_MEMBAR

#ifdef NOSMP
#define membar()
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

#endif /* HAVE_ASM_INLINE_MEMBAR */


#ifndef HAVE_ASM_INLINE_ATOMIC_OPS

#ifndef ATOMIC_OPS_USE_LOCK
#define ATOMIC_OPS_USE_LOCK
#endif

extern gen_lock_t* _atomic_lock; /* declared and init in ../atomic_ops.c */

#define atomic_lock    lock_get(_atomic_lock)
#define atomic_unlock  lock_release(_atomic_lock)


/* atomic ops */


/* OP can include var (function param), no other var. is declared */
#define ATOMIC_FUNC_DECL(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var) \
	{ \
		atomic_lock; \
		OP ; \
		atomic_unlock; \
		return RET_EXPR; \
	}


/* like above, but takes an extra param: v =>
 *  OP can use var and v (function params) */
#define ATOMIC_FUNC_DECL1(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
														P_TYPE v) \
	{ \
		atomic_lock; \
		OP ; \
		atomic_unlock; \
		return RET_EXPR; \
	}

/* OP can include var (function param), and ret (return)
 *  ( like ATOMIC_FUNC_DECL, but includes ret) */
#define ATOMIC_FUNC_DECL_RET(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var) \
	{ \
		P_TYPE ret; \
		atomic_lock; \
		OP ; \
		atomic_unlock; \
		return RET_EXPR; \
	}

/* like ATOMIC_FUNC_DECL1, but declares an extra variable: P_TYPE ret */
#define ATOMIC_FUNC_DECL1_RET(NAME, OP, P_TYPE, RET_TYPE, RET_EXPR) \
	inline static RET_TYPE atomic_##NAME##_##P_TYPE (volatile P_TYPE *var, \
														P_TYPE v) \
	{ \
		P_TYPE ret; \
		atomic_lock; \
		OP ; \
		atomic_unlock; \
		return RET_EXPR; \
	}

ATOMIC_FUNC_DECL(inc,      (*var)++, int, void, /* no return */ )
ATOMIC_FUNC_DECL(dec,      (*var)--, int, void, /* no return */ )
ATOMIC_FUNC_DECL1(and,     *var&=v, int, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,      *var|=v, int, void, /* no return */ )
ATOMIC_FUNC_DECL_RET(inc_and_test, ret=++(*var), int, int, (ret==0) )
ATOMIC_FUNC_DECL_RET(dec_and_test, ret=--(*var), int, int, (ret==0) )
ATOMIC_FUNC_DECL1_RET(get_and_set, ret=*var;*var=v , int, int,  ret)

ATOMIC_FUNC_DECL(inc,      (*var)++, long, void, /* no return */ )
ATOMIC_FUNC_DECL(dec,      (*var)--, long, void, /* no return */ )
ATOMIC_FUNC_DECL1(and,     *var&=v, long, void, /* no return */ )
ATOMIC_FUNC_DECL1(or,      *var|=v, long, void, /* no return */ )
ATOMIC_FUNC_DECL_RET(inc_and_test, ret=++(*var), long, long, (ret==0) )
ATOMIC_FUNC_DECL_RET(dec_and_test, ret=--(*var), long, long, (ret==0) )
ATOMIC_FUNC_DECL1_RET(get_and_set, ret=*var;*var=v , long, long,  ret)


#define atomic_inc(var) atomic_inc_int(&(var)->val)
#define atomic_dec(var) atomic_dec_int(&(var)->val)
#define atomic_and(var, mask) atomic_and_int(&(var)->val, (mask))
#define atomic_or(var, mask)  atomic_or_int(&(var)->val, (mask))
#define atomic_dec_and_test(var) atomic_dec_and_test_int(&(var)->val)
#define atomic_inc_and_test(var) atomic_inc_and_test_int(&(var)->val)
#define atomic_get_and_set(var, i) atomic_get_and_set_int(&(var)->val, i)


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

#define mb_atomic_inc_long(v)	atomic_inc_long(v)
#define mb_atomic_dec_long(v)	atomic_dec_long(v)
#define mb_atomic_or_long(v, m)	atomic_or_long(v, m)
#define mb_atomic_and_long(v, m)	atomic_and_long(v, m)
#define mb_atomic_inc_and_test_long(v)	atomic_inc_and_test_long(v)
#define mb_atomic_dec_and_test_long(v)	atomic_dec_and_test_long(v)
#define mb_atomic_get_and_set_long(v, i)	atomic_get_and_set_long(v, i)

#define mb_atomic_inc(var) mb_atomic_inc_int(&(var)->val)
#define mb_atomic_dec(var) mb_atomic_dec_int(&(var)->val)
#define mb_atomic_and(var, mask) mb_atomic_and_int(&(var)->val, (mask))
#define mb_atomic_or(var, mask)  mb_atomic_or_int(&(var)->val, (mask))
#define mb_atomic_dec_and_test(var) mb_atomic_dec_and_test_int(&(var)->val)
#define mb_atomic_inc_and_test(var) mb_atomic_inc_and_test_int(&(var)->val)
#define mb_atomic_get_and_set(var, i) mb_atomic_get_and_set_int(&(var)->val, i)

#define mb_atomic_get(var)	mb_atomic_get_int(&(var)->val)
#define mb_atomic_set(var, i)	mb_atomic_set_int(&(var)->val, i)

#endif /* if HAVE_ASM_INLINE_ATOMIC_OPS */

#endif
