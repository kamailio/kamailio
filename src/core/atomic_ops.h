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

/*!
 * \file
 * \brief Kamailio core :: Atomic operations and memory barriers
 * Copyright (C) 2006 iptelorg GmbH
 * \ingroup core
 * Module: \ref core
 * See \ref atomic
 */

/**
 * \page atomicops  Atomic operations and memory barriers
 *
 *  WARNING: atomic ops do not include memory barriers
 *  
 *  memory barriers:
 *  ----------------
 *
 *  void membar();        - memory barrier (load & store)
 *  void membar_read()    - load (read) memory barrier
 *  void membar_write()   - store (write) memory barrier
 *  void membar_depends() - read depends memory barrier, needed before using
 *                          the contents of a pointer (for now is needed only
 *                          on Alpha so on all other CPUs it will be a no-op)
 *                          For more info see: 
 *                          http://lse.sourceforge.net/locking/wmbdd.html
 *                          http://www.linuxjournal.com/article/8212
 *
 *  void membar_enter_lock() - memory barrier function that should be 
 *                             called after a lock operation (where lock is
 *                             an asm inline function that uses atomic store
 *                             operation on the lock var.). It is at most
 *                             a StoreStore|StoreLoad barrier, but could also
 *                             be empty if an atomic op implies a memory 
 *                             barrier on the specific arhitecture.
 *                             Example usage: 
 *                               raw_lock(l); membar_enter_lock(); ...
 *  void membar_leave_lock() - memory barrier function that should be called 
 *                             before an unlock operation (where unlock is an
 *                             asm inline function that uses at least an atomic
 *                             store to on the lock var.). It is at most a 
 *                             LoadStore|StoreStore barrier (but could also be
 *                             empty, see above).
 *                             Example: raw_lock(l); membar_enter_lock(); ..
 *                                      ... critical section ...
 *                                      membar_leave_lock(); raw_unlock(l);
 *  void membar_atomic_op() - memory barrier that should be called if a memory
 *                            barrier is needed immediately after or 
 *                            immediately before an atomic operation
 *                            (for example: atomic_inc(&i); membar_atomic_op()
 *                               instead of atomic_inc(&i); membar()).
 *                            atomic_op means every atomic operation except get
 *                            and set (for them use membar_atomic_setget()).
 *                            Using membar_atomic_op() instead of membar() in
 *                            these cases will generate faster code on some
 *                            architectures (for now x86 and x86_64), where 
 *                            atomic operations act also as memory barriers.
 *                            Note that mb_atomic_<OP>(...) is equivalent to
 *                            membar_atomic_op(); atomic_<OP>(...) and in this
 *                            case the first form is preferred).
 * void membar_atomic_setget() - same as above but for atomic_set and 
 *                            atomic_get (and not for any other atomic op.,
 *                            including atomic_get_and_set, for them use
 *                            membar_atomic_op()).
 *                            Note that mb_atomic_{get,set}(&i) is equivalent 
 *                            and preferred to membar_atomic_setget(); 
 *                            atomic_{get,set}(&i) (it will generate faster
 *                            code on x86 and x86_64).
 * void membar_read_atomic_op() - like membar_atomic_op(), but acts only as
 *                             a read barrier.
 * void membar_read_atomic_setget() - like membar_atomic_setget() but acts only
 *                            as a read barrier.
 * void membar_write_atomic_op() - like membar_atomic_op(), but acts only as
 *                            a write barrier.
 * void membar_write_atomic_setget() - like membar_atomic_setget() but acts 
 *                            only as a write barrier.
 *
 *
 *  Note: - properly using memory barriers is tricky, in general try not to 
 *        depend on them. Locks include memory barriers, so you don't need
 *        them for writes/load already protected by locks.
 *        - membar_enter_lock() and membar_leave_lock() are needed only if
 *        you implement your own locks using atomic ops (ser locks have the
 *        membars included)
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
 *  int atomic_add(atomic_t* v, int i)       - v->val+=i; return v->val
 *  int atomic_cmpxchg(atomic_t* v, o, n)    - r=v->val; if (r==o) v->val=n;
 *                                             return r (old value)
 *
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
 *  int mb_atomic_add(atomic_t* v, int i)    - v->val+=i; return v->val
 *  int mb_atomic_cmpxchg(atomic_t* v, o, n) - r=v->val; if (r==o) v->val=n;
 *                                             return r (old value)
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
 *  2007-05-13  moved some of the decl. and includes into atomic_common.h and 
 *               atomic_native.h (andrei)
 */
#ifndef __atomic_ops
#define __atomic_ops

#include "atomic/atomic_common.h"

#include "atomic/atomic_native.h"

/*! \brief if no native operations, emulate them using locks */
#if  ! defined HAVE_ASM_INLINE_ATOMIC_OPS || ! defined HAVE_ASM_INLINE_MEMBAR

#include "atomic/atomic_unknown.h"

#endif /* if HAVE_ASM_INLINE_ATOMIC_OPS */

#endif
