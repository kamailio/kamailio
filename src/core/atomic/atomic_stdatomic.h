/*
 * Copyright (C) 2025 Viktor Litvinov
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
 * @brief Atomic operations and memory barriers using C11 stdatomic.h
 *
 * Atomic operations and memory barriers implemented using C11 standard
 * atomic library (stdatomic.h). This provides portable atomic operations
 * for platforms supporting C11 or newer.
 *
 * \warning atomic ops do not include memory barriers unless using mb_* variants
 * @ingroup atomic
 */

#ifndef _atomic_stdatomic_h
#define _atomic_stdatomic_h

#include <stdatomic.h>

#define HAVE_ASM_INLINE_ATOMIC_OPS
#define HAVE_ASM_INLINE_MEMBAR

/* memory barriers using C11 atomic thread fence */
#define membar() atomic_thread_fence(memory_order_seq_cst)
#define membar_write() atomic_thread_fence(memory_order_release)
#define membar_read() atomic_thread_fence(memory_order_acquire)

#ifndef __CPU_alpha
#define membar_depends() \
	do {                 \
	} while(0) /* empty on non-Alpha */
#else
#define membar_depends() membar_read()
#endif

#define membar_enter_lock() atomic_thread_fence(memory_order_acquire)
#define membar_leave_lock() atomic_thread_fence(memory_order_release)

/* membars after or before atomic_ops */
#define membar_atomic_op() membar()
#define membar_atomic_setget() membar()
#define membar_write_atomic_op() membar_write()
#define membar_write_atomic_setget() membar_write()
#define membar_read_atomic_op() membar_read()
#define membar_read_atomic_setget() membar_read()

/* atomic operations for int */

inline static void atomic_inc_int(volatile int *var)
{
	atomic_fetch_add_explicit((_Atomic int *)var, 1, memory_order_relaxed);
}

inline static void atomic_dec_int(volatile int *var)
{
	atomic_fetch_sub_explicit((_Atomic int *)var, 1, memory_order_relaxed);
}

inline static void atomic_and_int(volatile int *var, int mask)
{
	atomic_fetch_and_explicit((_Atomic int *)var, mask, memory_order_relaxed);
}

inline static void atomic_or_int(volatile int *var, int mask)
{
	atomic_fetch_or_explicit((_Atomic int *)var, mask, memory_order_relaxed);
}

inline static int atomic_inc_and_test_int(volatile int *var)
{
	return atomic_fetch_add_explicit(
				   (_Atomic int *)var, 1, memory_order_relaxed)
		   == 1;
}

inline static int atomic_dec_and_test_int(volatile int *var)
{
	return atomic_fetch_sub_explicit(
				   (_Atomic int *)var, 1, memory_order_relaxed)
		   == -1;
}

inline static int atomic_get_and_set_int(volatile int *var, int v)
{
	return atomic_exchange_explicit(
			(_Atomic int *)var, v, memory_order_relaxed);
}

inline static int atomic_cmpxchg_int(
		volatile int *var, int old_val, int new_val)
{
	int expected = old_val;
	atomic_compare_exchange_strong_explicit((_Atomic int *)var, &expected,
			new_val, memory_order_relaxed, memory_order_relaxed);
	return expected;
}

inline static int atomic_add_int(volatile int *var, int v)
{
	return atomic_fetch_add_explicit(
				   (_Atomic int *)var, v, memory_order_relaxed)
		   + v;
}


/* atomic operations for long */

inline static void atomic_inc_long(volatile long *var)
{
	atomic_fetch_add_explicit((_Atomic long *)var, 1, memory_order_relaxed);
}

inline static void atomic_dec_long(volatile long *var)
{
	atomic_fetch_sub_explicit((_Atomic long *)var, 1, memory_order_relaxed);
}

inline static void atomic_and_long(volatile long *var, long mask)
{
	atomic_fetch_and_explicit((_Atomic long *)var, mask, memory_order_relaxed);
}

inline static void atomic_or_long(volatile long *var, long mask)
{
	atomic_fetch_or_explicit((_Atomic long *)var, mask, memory_order_relaxed);
}

inline static long atomic_inc_and_test_long(volatile long *var)
{
	return atomic_fetch_add_explicit(
				   (_Atomic long *)var, 1, memory_order_relaxed)
		   == 1;
}

inline static long atomic_dec_and_test_long(volatile long *var)
{
	return atomic_fetch_sub_explicit(
				   (_Atomic long *)var, 1, memory_order_relaxed)
		   == -1;
}

inline static long atomic_get_and_set_long(volatile long *var, long v)
{
	return atomic_exchange_explicit(
			(_Atomic long *)var, v, memory_order_relaxed);
}

inline static long atomic_cmpxchg_long(
		volatile long *var, long old_val, long new_val)
{
	long expected = old_val;
	atomic_compare_exchange_strong_explicit((_Atomic long *)var, &expected,
			new_val, memory_order_relaxed, memory_order_relaxed);
	return expected;
}

inline static long atomic_add_long(volatile long *var, long v)
{
	return atomic_fetch_add_explicit(
				   (_Atomic long *)var, v, memory_order_relaxed)
		   + v;
}


/* atomic_t wrapper macros */

#define atomic_inc(var) atomic_inc_int(&(var)->val)
#define atomic_dec(var) atomic_dec_int(&(var)->val)
#define atomic_and(var, mask) atomic_and_int(&(var)->val, (mask))
#define atomic_or(var, mask) atomic_or_int(&(var)->val, (mask))
#define atomic_dec_and_test(var) atomic_dec_and_test_int(&(var)->val)
#define atomic_inc_and_test(var) atomic_inc_and_test_int(&(var)->val)
#define atomic_get_and_set(var, i) atomic_get_and_set_int(&(var)->val, i)
#define atomic_cmpxchg(var, old, new_v) \
	atomic_cmpxchg_int(&(var)->val, old, new_v)
#define atomic_add(var, v) atomic_add_int(&(var)->val, v)


/* memory barrier versions with sequential consistency */

inline static void mb_atomic_inc_int(volatile int *var)
{
	atomic_fetch_add_explicit((_Atomic int *)var, 1, memory_order_seq_cst);
}

inline static void mb_atomic_dec_int(volatile int *var)
{
	atomic_fetch_sub_explicit((_Atomic int *)var, 1, memory_order_seq_cst);
}

inline static void mb_atomic_and_int(volatile int *var, int mask)
{
	atomic_fetch_and_explicit((_Atomic int *)var, mask, memory_order_seq_cst);
}

inline static void mb_atomic_or_int(volatile int *var, int mask)
{
	atomic_fetch_or_explicit((_Atomic int *)var, mask, memory_order_seq_cst);
}

inline static int mb_atomic_inc_and_test_int(volatile int *var)
{
	return atomic_fetch_add_explicit(
				   (_Atomic int *)var, 1, memory_order_seq_cst)
		   == 1;
}

inline static int mb_atomic_dec_and_test_int(volatile int *var)
{
	return atomic_fetch_sub_explicit(
				   (_Atomic int *)var, 1, memory_order_seq_cst)
		   == -1;
}

inline static int mb_atomic_get_and_set_int(volatile int *var, int v)
{
	return atomic_exchange_explicit(
			(_Atomic int *)var, v, memory_order_seq_cst);
}

inline static int mb_atomic_cmpxchg_int(
		volatile int *var, int old_val, int new_val)
{
	int expected = old_val;
	atomic_compare_exchange_strong_explicit((_Atomic int *)var, &expected,
			new_val, memory_order_seq_cst, memory_order_seq_cst);
	return expected;
}

inline static int mb_atomic_add_int(volatile int *var, int v)
{
	return atomic_fetch_add_explicit(
				   (_Atomic int *)var, v, memory_order_seq_cst)
		   + v;
}

inline static int mb_atomic_get_int(volatile int *v)
{
	return atomic_load_explicit((_Atomic int *)v, memory_order_seq_cst);
}

inline static void mb_atomic_set_int(volatile int *v, int i)
{
	atomic_store_explicit((_Atomic int *)v, i, memory_order_seq_cst);
}


/* memory barrier versions for long */

inline static void mb_atomic_inc_long(volatile long *var)
{
	atomic_fetch_add_explicit((_Atomic long *)var, 1, memory_order_seq_cst);
}

inline static void mb_atomic_dec_long(volatile long *var)
{
	atomic_fetch_sub_explicit((_Atomic long *)var, 1, memory_order_seq_cst);
}

inline static void mb_atomic_and_long(volatile long *var, long mask)
{
	atomic_fetch_and_explicit((_Atomic long *)var, mask, memory_order_seq_cst);
}

inline static void mb_atomic_or_long(volatile long *var, long mask)
{
	atomic_fetch_or_explicit((_Atomic long *)var, mask, memory_order_seq_cst);
}

inline static long mb_atomic_inc_and_test_long(volatile long *var)
{
	return atomic_fetch_add_explicit(
				   (_Atomic long *)var, 1, memory_order_seq_cst)
		   == 1;
}

inline static long mb_atomic_dec_and_test_long(volatile long *var)
{
	return atomic_fetch_sub_explicit(
				   (_Atomic long *)var, 1, memory_order_seq_cst)
		   == -1;
}

inline static long mb_atomic_get_and_set_long(volatile long *var, long v)
{
	return atomic_exchange_explicit(
			(_Atomic long *)var, v, memory_order_seq_cst);
}

inline static long mb_atomic_cmpxchg_long(
		volatile long *var, long old_val, long new_val)
{
	long expected = old_val;
	atomic_compare_exchange_strong_explicit((_Atomic long *)var, &expected,
			new_val, memory_order_seq_cst, memory_order_seq_cst);
	return expected;
}

inline static long mb_atomic_add_long(volatile long *var, long v)
{
	return atomic_fetch_add_explicit(
				   (_Atomic long *)var, v, memory_order_seq_cst)
		   + v;
}

inline static long mb_atomic_get_long(volatile long *v)
{
	return atomic_load_explicit((_Atomic long *)v, memory_order_seq_cst);
}

inline static void mb_atomic_set_long(volatile long *v, long i)
{
	atomic_store_explicit((_Atomic long *)v, i, memory_order_seq_cst);
}


/* mb_atomic_t wrapper macros */

#define mb_atomic_inc(var) mb_atomic_inc_int(&(var)->val)
#define mb_atomic_dec(var) mb_atomic_dec_int(&(var)->val)
#define mb_atomic_and(var, mask) mb_atomic_and_int(&(var)->val, (mask))
#define mb_atomic_or(var, mask) mb_atomic_or_int(&(var)->val, (mask))
#define mb_atomic_dec_and_test(var) mb_atomic_dec_and_test_int(&(var)->val)
#define mb_atomic_inc_and_test(var) mb_atomic_inc_and_test_int(&(var)->val)
#define mb_atomic_get_and_set(var, i) mb_atomic_get_and_set_int(&(var)->val, i)
#define mb_atomic_cmpxchg(v, o, n) mb_atomic_cmpxchg_int(&(v)->val, o, n)
#define mb_atomic_add(v, i) mb_atomic_add_int(&(v)->val, i)
#define mb_atomic_get(var) mb_atomic_get_int(&(var)->val)
#define mb_atomic_set(var, i) mb_atomic_set_int(&(var)->val, i)

#endif /* _atomic_stdatomic_h */
