/*
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
* \file
* \brief Kamailio core :: Kamailio locking library
* \ingroup core
* \author andrei
* Module: \ref core
 *
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

	recursive locks:
	-------------
	type: rec_lock_t
	rec_lock_t* rec_lock_alloc();                - allocates a lock in shared mem.
	rec_lock_t* rec_lock_init(rec_lock_t* lock); - inits the lock
	void    rec_lock_destroy(rec_lock_t* lock);  - removes the lock (e.g sysv rmid)
	void    rec_lock_dealloc(rec_lock_t* lock);  - deallocates the lock's shared m.
	void    rec_lock_get(rec_lock_t* lock);      - lock (mutex down)
	void    rec_lock_release(rec_lock_t* lock);  - unlock (mutex up)

WARNING: - lock_set_init may fail for large number of sems (e.g. sysv).
         - signals are not treated! (some locks are "awakened" by the signals)
*/

#ifndef _locking_h
#define _locking_h

/* the order is important */
#include "lock_ops.h"
#include "lock_alloc.h"
#include "atomic_ops.h"

typedef struct rec_lock {
	gen_lock_t lock;     /* mutex to access items in the slot */
	atomic_t locker_pid; /* pid of the process that holds the lock */
	int rec_lock_level;  /* recursive lock count */
} rec_lock_t;

rec_lock_t* rec_lock_alloc();
rec_lock_t* rec_lock_init(rec_lock_t* lock);
void rec_lock_destroy(rec_lock_t* lock);
void rec_lock_dealloc(rec_lock_t* lock);
void rec_lock_get(rec_lock_t* lock);
void rec_lock_release(rec_lock_t* lock);

typedef struct rec_lock_set {
	int size;
	rec_lock_t *locks;
} rec_lock_set_t;

rec_lock_set_t* rec_lock_set_alloc(int n);
rec_lock_set_t* rec_lock_set_init(rec_lock_set_t* lset);
void rec_lock_set_destroy(rec_lock_set_t* lset);
void rec_lock_set_dealloc(rec_lock_set_t* lset);
void rec_lock_set_get(rec_lock_set_t* lset, int i);
void rec_lock_set_release(rec_lock_set_t* lset, int i);

#endif
