/* 
 * Copyright (C) 2007 iptelorg GmbH
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
* \brief Kamailio core :: locks
* \author andrei
* \ingroup core
* Module: \ref core
 *
 * futex based lock (mutex) implementation  (linux 2.6+ only)
 * based on Ulrich Drepper implementation in "Futexes Are Tricky"
 * (http://people.redhat.com/drepper/futex.pdf)
 *
 * Implements:
 *   void futex_get(futex_lock_t* lock);     - mutex lock
 *   void futex_release(futex_lock_t* lock); - unlock
 *   int  futex_try(futex_lock_t* lock);     - tries to get lock, returns 0
 *                                              on success and !=0 on failure
 *                                              (1 or 2)
 *
 *  Config defines:
 */

#ifndef _futexlock_h
#define _futexlock_h


#include "atomic/atomic_common.h"
#include "atomic/atomic_native.h"

#ifdef HAVE_ASM_INLINE_ATOMIC_OPS
#define HAVE_FUTEX
#include <sys/types.h> /* hack to workaround some type conflicts 
                          between linux-libc-dev andlibc headers
                          in recent (6.08.2008) x86_64 debian sid
                          installations */
/* hack to work with old linux/futex.h versions, that depend on sched.h in
   __KERNEL__ mode (futex.h < 2.6.20) */
#include <linux/types.h>
typedef __u32 u32;
struct task_struct;
/* end of the hack */
/* another hack this time for OpenSuse 10.2:
   futex.h uses a __user attribute, which is defined in linux/compiler.h
   However linux/compiler.h is not part of the kernel headers package in
   most distributions. Instead they ship a modified linux/futex.h that does
   not include <linux/compile.h> and does not user __user.
*/
#ifndef __user
#define __user
#endif /* __user__*/
/* end of hack */
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "compiler_opt.h"

/* either syscall directly or #include <sys/linux/syscall.h> and use
 * sys_futex directly */
#define sys_futex(addr, op, val, timeout, addr2, val3) \
	syscall(__NR_futex , (addr), (op), (val), (timeout), (addr2), (val3))

typedef atomic_t futex_lock_t;

/* the mutex has 3 states: 0 - free/unlocked and nobody waiting
 *                         1 - locked and nobody waiting for it
 *                         2 - locked w/ 0 or more waiting processes/threads
 */

inline static futex_lock_t* futex_init(futex_lock_t* lock)
{
	atomic_set(lock, 0);
	return lock;
}


inline static void futex_get(futex_lock_t* lock)
{
	int v;
#ifdef ADAPTIVE_WAIT
	register int i=ADAPTIVE_WAIT_LOOPS;
	
retry:
#endif
	
	v=atomic_cmpxchg(lock, 0, 1); /* lock if 0 */
	if (likely(v==0)){  /* optimize for the uncontended case */
		/* success */
		membar_enter_lock();
		return;
	}else if (unlikely(v==2)){ /* if contended, optimize for the one waiter
								case */
		/* waiting processes/threads => add ourselves to the queue */
		do{
			sys_futex(&(lock)->val, FUTEX_WAIT, 2, 0, 0, 0);
			v=atomic_get_and_set(lock, 2);
		}while(v);
	}else{
		/* v==1 */
#ifdef ADAPTIVE_WAIT
		if (i>0){
			i--;
			goto retry;
		}
#endif
		v=atomic_get_and_set(lock, 2);
		while(v){
			sys_futex(&(lock)->val, FUTEX_WAIT, 2, 0, 0, 0);
			v=atomic_get_and_set(lock, 2);
		}
	}
	membar_enter_lock();
}


inline static void futex_release(futex_lock_t* lock)
{
	int v;
	
	membar_leave_lock();
	v=atomic_get_and_set(lock, 0);
	if (unlikely(v==2)){ /* optimize for the uncontended case */
		sys_futex(&(lock)->val, FUTEX_WAKE, 1, 0, 0, 0);
	}
}


static inline int futex_try(futex_lock_t* lock)
{
	int c;
	c=atomic_cmpxchg(lock, 0, 1);
	if (likely(c))
		membar_enter_lock();
	return c;
}


#else /*HAVE_ASM_INLINE_ATOMIC_OPS*/
#undef USE_FUTEX
#endif /*HAVE_ASM_INLINE_ATOMIC_OPS*/

#endif /* _futexlocks_h*/
