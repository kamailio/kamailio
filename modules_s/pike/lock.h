/* 
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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


#ifndef _PIKE_LOCK_H
#define _PIKE_LOCK_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#ifdef  FAST_LOCK
#include "../../fastlock.h"
#endif



#ifdef FAST_LOCK
#define pike_lock fl_lock_t
#else
typedef struct {
	int semaphore_set;
	int semaphore_index;
} pike_lock;
#endif



#ifndef FAST_LOCK
int change_semaphore( pike_lock *s  , int val );
#endif

pike_lock* create_semaphores(int nr);
void destroy_semaphores(pike_lock *sem_set);



/* lock semaphore s */
static inline int lock( pike_lock *s )
{
#ifdef FAST_LOCK
	get_lock(s);
	return 0;
#else
	return change_semaphore( s, -1 );
#endif
}



static inline int unlock( pike_lock *s )
{
#ifdef FAST_LOCK
	release_lock(s);
	return 0;
#else
	return change_semaphore( s, +1 );
#endif
}



#endif
