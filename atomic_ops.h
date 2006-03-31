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
 *  atomic operations and memory barriers
 *  WARNING: atomic ops do not include memory barriers
 *  
 *  memory barriers:
 *  ----------------
 *
 *  void membar();       - memory barrier (load & store)
 *  void membar_read()   - load (read) memory barrier
 *  void membar_write()  - store (write) memory barrier
 *
 *  Note: properly using memory barriers is tricky, in general try not to 
 *        depend on them. Locks include memory barriers, so you don't need
 *        them for writes/load already protected by locks.
 *
 * atomic operations:
 * ------------------
 *  type: atomic_t
 *
 * not including memory barriers:
 *
 *  void atomic_set(atomic_t* v, int i)      - v->val=i
 *  int atomic_get(atomic_t* v)              - return v->val
 *  int atomic_get_and_set(atomic_t *v, i)   - return old v->val, v->val=i
 *  void atomic_inc(atomic_t* v)
 *  void atomic_dec(atomic_t* v)
 *  int atomic_inc_and_test(atomic_t* v)     - returns 1 if the result is 0
 *  int atomic_dec_and_test(atomic_t* v)     - returns 1 if the result is 0
 *  void atomic_or (atomic_t* v, int mask)   - v->val|=mask 
 *  void atomic_and(atomic_t* v, int mask)   - v->val&=mask
 * 
 * same ops, but with builtin memory barriers:
 *
 *  void mb_atomic_set(atomic_t* v, int i)      -  v->val=i
 *  int mb_atomic_get(atomic_t* v)              -  return v->val
 *  int mb_atomic_get_and_set(atomic_t *v, i)   -  return old v->val, v->val=i
 *  void mb_atomic_inc(atomic_t* v)
 *  void mb_atomic_dec(atomic_t* v)
 *  int mb_atomic_inc_and_test(atomic_t* v)  - returns 1 if the result is 0
 *  int mb_atomic_dec_and_test(atomic_t* v)  - returns 1 if the result is 0
 *  void mb_atomic_or(atomic_t* v, int mask - v->val|=mask 
 *  void mb_atomic_and(atomic_t* v, int mask)- v->val&=mask
 *
 *  Same operations are available for int and long. The functions are named
 *   after the following rules:
 *     - add an int or long  suffix to the correspondent atomic function
 *     -  volatile int* or volatile long* replace atomic_t* in the functions
 *        declarations
 *     -  long and int replace the parameter type (if the function has an extra
 *        parameter) and the return value
 *  E.g.:
 *    long atomic_get_long(volatile long* v)
 *    int atomic_get_int( volatile int* v)
 *    long atomic_get_and_set(volatile long* v, long l)
 *    int atomic_get_and_set(volatile int* v, int i)
 *
 * Config defines:   CC_GCC_LIKE_ASM  - the compiler support gcc style
 *                     inline asm
 *                   NOSMP - the code will be a little faster, but not SMP
 *                            safe
 *                   __CPU_i386, __CPU_x86_64, X86_OOSTORE - see 
 *                       atomic/atomic_x86.h
 *                   __CPU_mips, __CPU_mips2, __CPU_mips64, MIPS_HAS_LLSC - see
 *                       atomic/atomic_mip2.h
 *                   __CPU_ppc, __CPU_ppc64 - see atomic/atomic_ppc.h
 *                   __CPU_sparc - see atomic/atomic_sparc.h
 *                   __CPU_sparc64, SPARC64_MODE - see atomic/atomic_sparc64.h
 *                   __CPU_arm, __CPU_arm6 - see atomic/atomic_arm.h
 *                   __CPU_alpha - see atomic/atomic_alpha.h
 */
/* 
 * History:
 * --------
 *  2006-03-08  created by andrei
 */
#ifndef __atomic_ops
#define __atomic_ops

/* atomic_t defined as a struct to easily catch non atomic ops. on it,
 * e.g.  atomic_t  foo; foo++  will generate a compile error */
typedef struct{ volatile int val; } atomic_t; 


/* store and load operations are atomic on all cpus, note however that they
 * don't include memory barriers so if you want to use atomic_{get,set} 
 * to implement mutexes you must use the mb_* versions or explicitely use
 * the barriers */

#define atomic_set_int(pvar, i) (*(pvar)=i)
#define atomic_set_long(pvar, i) (*(pvar)=i)
#define atomic_get_int(pvar) (*(pvar))
#define atomic_get_long(pvar) (*(pvar))

#define atomic_set(at_var, value)	(atomic_set_int(&((at_var)->val), (value)))

inline static int atomic_get(atomic_t *v)
{
	return atomic_get_int(&(v->val));
}



#ifdef CC_GCC_LIKE_ASM

#if defined __CPU_i386 || defined __CPU_x86_64

#include "atomic/atomic_x86.h"

#elif defined __CPU_mips2 || defined __CPU_mips64 || \
	  ( defined __CPU_mips && defined MIPS_HAS_LLSC )

#include "atomic/atomic_mips2.h"

#elif defined __CPU_ppc || defined __CPU_ppc64

#include "atomic/atomic_ppc.h"

#elif defined __CPU_sparc64

#include "atomic/atomic_sparc64.h"

#elif defined __CPU_sparc

#include "atomic/atomic_sparc.h"

#elif defined __CPU_arm || defined __CPU_arm6

#include "atomic/atomic_arm.h"

#elif defined __CPU_alpha

#include "atomic/atomic_alpha.h"

#endif /* __CPU_xxx  => no known cpu */

#endif /* CC_GCC_LIKE_ASM */


#if  ! defined HAVE_ASM_INLINE_ATOMIC_OPS || ! defined HAVE_ASM_INLINE_MEMBAR

#include "atomic/atomic_unknown.h"

#endif /* if HAVE_ASM_INLINE_ATOMIC_OPS */

#endif
