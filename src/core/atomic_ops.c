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
 * \brief Kamailio core :: atomic operations init
 * \ingroup core
 * Module: \ref core
 */


#include "atomic_ops_init.h"
#include "atomic_ops.h"

#if defined ATOMIC_OPS_USE_LOCK  || defined ATOMIC_OPS_USE_LOCK_SET || \
	defined MEMBAR_USES_LOCK
#include "locking.h"
#endif

#ifdef MEMBAR_USES_LOCK
gen_lock_t* __membar_lock=0; /* init in atomic_ops.c */
#endif

#ifdef ATOMIC_OPS_USE_LOCK_SET
gen_lock_set_t* _atomic_lock_set=0;
#elif defined ATOMIC_OPS_USE_LOCK
gen_lock_t* _atomic_lock=0;
#endif


/* returns 0 on success, -1 on error */
int init_atomic_ops()
{
	
#ifdef MEMBAR_USES_LOCK
	if ((__membar_lock=lock_alloc())==0){
		goto error;
	}
	if (lock_init(__membar_lock)==0){
		lock_dealloc(__membar_lock);
		__membar_lock=0;
		goto error;
	}
	_membar_lock; /* start with the lock "taken" so that we can safely use
					 unlock/lock sequences on it later */
#endif
#ifdef ATOMIC_OPS_USE_LOCK_SET
	if ((_atomic_lock_set=lock_set_alloc(_ATOMIC_LS_SIZE))==0){
		goto error;
	}
	if (lock_set_init(_atomic_lock_set)==0){
		lock_set_dealloc(_atomic_lock_set);
		_atomic_lock_set=0;
		goto error;
	}
#elif defined ATOMIC_OPS_USE_LOCK
	if ((_atomic_lock=lock_alloc())==0){
		goto error;
	}
	if (lock_init(_atomic_lock)==0){
		lock_dealloc(_atomic_lock);
		_atomic_lock=0;
		goto error;
	}
#endif /* ATOMIC_OPS_USE_LOCK_SET/ATOMIC_OPS_USE_LOCK */
	return 0;
#if defined MEMBAR_USES_LOCK || defined ATOMIC_OPS_USE_LOCK || \
	defined ATOMIC_OPS_USE_LOCK_SET
error:
	destroy_atomic_ops();
	return -1;
#endif
}



void destroy_atomic_ops()
{
#ifdef MEMBAR_USES_LOCK
	if (__membar_lock!=0){
		lock_destroy(__membar_lock);
		lock_dealloc(__membar_lock);
		__membar_lock=0;
	}
#endif
#ifdef ATOMIC_OPS_USE_LOCK_SET
	if (_atomic_lock_set!=0){
		lock_set_destroy(_atomic_lock_set);
		lock_set_dealloc(_atomic_lock_set);
		_atomic_lock_set=0;
	}
#elif defined ATOMIC_OPS_USE_LOCK
	if (_atomic_lock!=0){
		lock_destroy(_atomic_lock);
		lock_dealloc(_atomic_lock);
		_atomic_lock=0;
	}
#endif /* ATOMIC_OPS_USE_LOCK_SET / ATOMIC_OPS_USE_LOCK*/
}
