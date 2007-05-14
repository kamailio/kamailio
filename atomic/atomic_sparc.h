/* 
 * $Id$
 * 
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
/*
 *  memory barriers for sparc32 ( version < v 9))
 *  see atomic_ops.h for more details 
 *
 * Config defines: NOSMP
 */
/* 
 * History:
 * --------
 *  2006-03-28  created by andrei
 */


#ifndef _atomic_sparc_h
#define _atomic_sparc_h

#define HAVE_ASM_INLINE_MEMBAR


#warning "sparc32 atomic operations support not tested"

#ifdef NOSMP
#define membar() asm volatile ("" : : : "memory") /* gcc do not cache barrier*/
#define membar_read()  membar()
#define membar_write() membar()
/* lock barrriers: empty, not needed for NOSMP; the lock/unlock should already
 * contain gcc barriers*/
#define membar_enter_lock() 
#define membar_leave_lock()
#else /* SMP */
#define membar_write() asm volatile ("stbar \n\t" : : : "memory") 
#define membar() membar_write()
#define membar_read() asm volatile ("" : : : "memory") 
#define membar_enter_lock() 
#define membar_leave_lock() asm volatile ("stbar \n\t" : : : "memory") 

#endif /* NOSMP */



#endif
