/*
 * Copyright (C) 2016 kamailio.org
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

#include <stdlib.h>
#include <string.h>

#include "mem/shm.h"

#include "locking.h"

/**
 *
 */
rec_lock_t* rec_lock_alloc()
{
	return shm_malloc(sizeof(rec_lock_t));
}

/**
 *
 */
rec_lock_t* rec_lock_init(rec_lock_t* rlock)
{
	memset(rlock, 0, sizeof(rec_lock_t));
	if(lock_init(&rlock->lock)==0) {
		return NULL;
	}
	return rlock;
}

/**
 *
 */
void rec_lock_destroy(rec_lock_t* rlock)
{
	lock_destroy(&rlock->lock);
	memset(rlock, 0, sizeof(rec_lock_t));
}

/**
 *
 */
void rec_lock_dealloc(rec_lock_t* rlock)
{
	shm_free(rlock);
}

/**
 *
 */
void rec_lock_get(rec_lock_t* rlock)
{
	int mypid;

	mypid = my_pid();
	if (likely(atomic_get(&rlock->locker_pid) != mypid)) {
		lock_get(&rlock->lock);
		atomic_set(&rlock->locker_pid, mypid);
	} else {
		/* locked within the same process that executed us */
		rlock->rec_lock_level++;
	}
}

/**
 *
 */
void rec_lock_release(rec_lock_t* rlock)
{
	if (likely(rlock->rec_lock_level == 0)) {
		atomic_set(&rlock->locker_pid, 0);
		lock_release(&rlock->lock);
	} else  {
		/* recursive locked => decrease lock count */
		rlock->rec_lock_level--;
	}
}

/**
 *
 */
rec_lock_set_t* rec_lock_set_alloc(int n)
{
	rec_lock_set_t* ls;
	ls=(rec_lock_set_t*)shm_malloc(sizeof(rec_lock_set_t)+n*sizeof(rec_lock_t));
	if (ls==0){
		SHM_MEM_CRITICAL;
	}else{
		ls->locks=(rec_lock_t*)((char*)ls+sizeof(rec_lock_set_t));
		ls->size=n;
	}
	return ls;
}

/**
 *
 */
rec_lock_set_t* rec_lock_set_init(rec_lock_set_t* lset)
{
	int r;
	for (r=0; r<lset->size; r++) if (rec_lock_init(&lset->locks[r])==0) return 0;
	return lset;
}

/**
 *
 */
void rec_lock_set_destroy(rec_lock_set_t* lset)
{
	return;
}

/**
 *
 */
void rec_lock_set_dealloc(rec_lock_set_t* lset)
{
	shm_free((void*)lset);
	return;
}

/**
 *
 */
void rec_lock_set_get(rec_lock_set_t* lset, int i)
{
	rec_lock_get(&lset->locks[i]);
	return;
}

/**
 *
 */
void rec_lock_set_release(rec_lock_set_t* lset, int i)
{
	rec_lock_release(&lset->locks[i]);
	return;
}
