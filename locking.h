/* $Id$ */
/*
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 *   ser locking library
 *
 *  2002-12-16  created by andrei
 *  2003-02-20  s/gen_lock_t/gen_lock_t/ to avoid a type conflict 
 *               on solaris  (andrei)
 *  2003-03-05  lock set support added for FAST_LOCK & SYSV (andrei)
 *  2003-03-06  split in two: lock_ops.h & lock_alloc.h, to avoid
 *               shm_mem.h<->locking.h interdependency (andrei)
 *  2004-07-28  s/lock_set_t/gen_lock_set_t/ because of a type conflict
 *              on darwin (andrei)
 *
Implements (in lock_ops.h & lock_alloc.h):

	simple locks:
	-------------
	type: gen_lock_t
	gen_lock_t* lock_alloc();                - allocates a lock in shared mem.
	gen_lock_t* lock_init(gen_lock_t* lock); - inits the lock
	void    lock_destroy(gen_lock_t* lock);  - removes the lock (e.g sysv rmid)
	void    lock_dealloc(gen_lock_t* lock);  - deallocates the lock's shared m.
	void    lock_get(gen_lock_t* lock);      - lock (mutex down)
	void    lock_release(gen_lock_t* lock);  - unlock (mutex up)
	
	lock sets:
	----------
	type: gen_lock_set_t
	gen_lock_set_t* lock_set_alloc(no)               - allocs a lock set in shm.
	gen_lock_set_t* lock_set_init(gen_lock_set_t* set);  - inits the lock set
	void lock_set_destroy(gen_lock_set_t* s);        - removes the lock set
	void lock_set_dealloc(gen_lock_set_t* s);        - deallocs the lock set shm.
	void lock_set_get(gen_lock_set_t* s, int i);     - locks sem i from the set
	void lock_set_release(gen_lock_set_t* s, int i)  - unlocks sem i from the set

WARNING: - lock_set_init may fail for large number of sems (e.g. sysv). 
         - signals are not treated! (some locks are "awakened" by the signals)
*/

#ifndef _locking_h
#define _locking_h

/* the order is important */
#include "lock_ops.h"
#include "lock_alloc.h" 

#endif
