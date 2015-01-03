/*
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

#include "defs.h"


#ifndef __lock_h
#define __lock_h

#include "../../dprint.h"
#include "../../locking.h"



#ifdef GEN_LOCK_T_PREFERED
#define ser_lock_t gen_lock_t
#else
/* typedef to structure we use for mutexing;
   currently, index to a semaphore set identifier now */
typedef struct {
	gen_lock_set_t* semaphore_set;
	int semaphore_index;
} ser_lock_t;
#endif



#include "h_table.h"



int lock_initialize(void);
void lock_cleanup(void);

#ifdef DBG_LOCK
#define lock(_s) _lock( (_s), __FILE__, __FUNCTION__, __LINE__ )
#define unlock(_s) _unlock( (_s), __FILE__, __FUNCTION__, __LINE__ )
#else
#define lock(_s) _lock( (_s) )
#define unlock(_s) _unlock( (_s) )
#endif


int init_cell_lock( struct cell *cell );
int init_entry_lock( struct s_table* ht, struct entry *entry );
int init_async_lock( struct cell *cell );


int release_cell_lock( struct cell *cell );
int release_entry_lock( struct entry *entry );



/* lock semaphore s */
#ifdef DBG_LOCK
static inline void _lock( ser_lock_t* s , char *file, char *function,
							unsigned int line )
#else
static inline void _lock( ser_lock_t* s )
#endif
{
#ifdef DBG_LOCK
	DBG("DEBUG: lock : entered from %s , %s(%d)\n", function, file, line );
#endif
#ifdef GEN_LOCK_T_PREFERED 
	lock_get(s);
#else
	lock_set_get(s->semaphore_set, s->semaphore_index);
#endif
}



#ifdef DBG_LOCK
static inline void _unlock( ser_lock_t* s, char *file, char *function,
		unsigned int line )
#else
static inline void _unlock( ser_lock_t* s )
#endif
{
#ifdef DBG_LOCK
	DBG("DEBUG: unlock : entered from %s, %s:%d\n", file, function, line );
#endif
#ifdef GEN_LOCK_T_PREFERED
	lock_release(s);
#else
	lock_set_release( s->semaphore_set, s->semaphore_index );
#endif
}



#endif

