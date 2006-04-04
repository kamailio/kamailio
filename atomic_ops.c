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
 *  atomic operations init
 */
/* 
 * History:
 * --------
 *  2006-03-08  created by andrei
 */

#include "atomic_ops_init.h"
#include "atomic_ops.h"

#if defined ATOMIC_OPS_USE_LOCK  || defined MEMBAR_USES_LOCK
#include "locking.h"
#endif

#ifdef MEMBAR_USES_LOCK
gen_lock_t* __membar_lock=0; /* init in atomic_ops.c */
#endif

#ifdef ATOMIC_OPS_USE_LOCK
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
#ifdef ATOMIC_OPS_USE_LOCK
	if ((_atomic_lock=lock_alloc())==0){
		goto error;
	}
	if (lock_init(_atomic_lock)==0){
		lock_dealloc(_atomic_lock);
		_atomic_lock=0;
		goto error;
	}
#endif
	return 0;
#if defined MEMBAR_USES_LOCK || defined ATOMIC_OPS_USE_LOCK
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
#ifdef ATOMIC_OPS_USE_LOCK
	if (_atomic_lock!=0){
		lock_destroy(_atomic_lock);
		lock_dealloc(_atomic_lock);
		_atomic_lock=0;
	}
#endif
}
