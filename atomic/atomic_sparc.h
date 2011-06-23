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
 * @brief Memory barriers for SPARC32 ( version < v 9))
 * 
 * Memory barriers for SPARC32 ( version < v 9)), see atomic_ops.h for more
 * details.
 *
 * Config defines: 
 * - NOSMP
 * @ingroup atomic
 */

/* 
 * History:
 * --------
 *  2006-03-28  created by andrei
 *  2007-05-29  added membar_depends(), membar_*_atomic_op and
 *                membar_*_atomic_setget (andrei)
 */


#ifndef _atomic_sparc_h
#define _atomic_sparc_h

#define HAVE_ASM_INLINE_MEMBAR


#warning "sparc32 atomic operations support not tested"

#ifdef NOSMP
#define membar() asm volatile ("" : : : "memory") /* gcc do not cache barrier*/
#define membar_read()  membar()
#define membar_write() membar()
#define membar_depends()  do {} while(0) /* really empty, not even a cc bar. */
/* lock barrriers: empty, not needed for NOSMP; the lock/unlock should already
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
#else /* SMP */
#define membar_write() asm volatile ("stbar \n\t" : : : "memory") 
#define membar() membar_write()
#define membar_read() asm volatile ("" : : : "memory") 
#define membar_depends()  do {} while(0) /* really empty, not even a cc bar. */
#define membar_enter_lock() do {} while(0)
#define membar_leave_lock() asm volatile ("stbar \n\t" : : : "memory") 
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



#endif
