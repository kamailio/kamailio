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


#include <errno.h>

#include "lock.h"
#include "../../dprint.h"



#ifndef GEN_LOCK_T_PREFERED 
/* semaphore probing limits */
#define SEM_MIN		16
#define SEM_MAX		4096

/* we implement mutex here using lock sets; as the number of
   semaphores may be limited (e.g. sysv) and number of synchronized 
   elements high, we partition the synced SER elements and share 
   semaphores in each of the partitions; we try to use as many 
   semaphores as OS gives us for finest granularity. 

   we allocate the locks according to the following plans:

   1) we allocate a semaphore set for hash_entries and
      try to use as many semaphores in it as OS allows;
      we partition the hash_entries by available
      semaphores which are shared  in each partition
   2) cells get always the same semaphore as its hash
      entry in which they live

*/

/* and the maximum number of semaphores in the entry_semaphore set */
static int sem_nr;
gen_lock_set_t* entry_semaphore=0;
gen_lock_set_t* reply_semaphore=0;
#ifdef ENABLE_ASYNC_MUTEX
gen_lock_set_t* async_semaphore=0;
#endif
#endif


/* initialize the locks; return 0 on success, -1 otherwise
*/
int lock_initialize()
{
#ifndef GEN_LOCK_T_PREFERED
	int i;
	int probe_run;
#endif

	/* first try allocating semaphore sets with fixed number of semaphores */
	DBG("DEBUG: lock_initialize: lock initialization started\n");

#ifndef GEN_LOCK_T_PREFERED
	i=SEM_MIN;
	/* probing phase: 0=initial, 1=after the first failure */
	probe_run=0;
again:
	do {
		if (entry_semaphore!=0){ /* clean-up previous attempt */
			lock_set_destroy(entry_semaphore);
			lock_set_dealloc(entry_semaphore);
		}
		if (reply_semaphore!=0){
			lock_set_destroy(reply_semaphore);
			lock_set_dealloc(reply_semaphore);
		}
#ifdef ENABLE_ASYNC_MUTEX
		if (async_semaphore!=0){
			lock_set_destroy(async_semaphore);
			lock_set_dealloc(async_semaphore);
		}
#endif
		if (i==0){
			LOG(L_CRIT, "lock_initialize: could not allocate semaphore"
					" sets\n");
			goto error;
		}
		
		if (((entry_semaphore=lock_set_alloc(i))==0)||
			(lock_set_init(entry_semaphore)==0)) {
			DBG("DEBUG: lock_initialize: entry semaphore "
					"initialization failure:  %s\n", strerror( errno ) );
			if (entry_semaphore){
				lock_set_dealloc(entry_semaphore);
				entry_semaphore=0;
			}
			/* first time: step back and try again */
			if (probe_run==0) {
					DBG("DEBUG: lock_initialize: first time "
								"semaphore allocation failure\n");
					i--;
					probe_run=1;
					continue;
				/* failure after we stepped back; give up */
			} else {
					DBG("DEBUG: lock_initialize:   second time semaphore"
							" allocation failure\n");
					goto error;
			}
		}
		/* allocation succeeded */
		if (probe_run==1) { /* if ok after we stepped back, we're done */
			break;
		} else { /* if ok otherwise, try again with larger set */
			if (i==SEM_MAX) break;
			else {
				i++;
				continue;
			}
		}
	} while(1);
	sem_nr=i;

	if (((reply_semaphore=lock_set_alloc(i))==0)||
		(lock_set_init(reply_semaphore)==0)){
			if (reply_semaphore){
				lock_set_dealloc(reply_semaphore);
				reply_semaphore=0;
			}
			DBG("DEBUG:lock_initialize: reply semaphore initialization"
				" failure: %s\n", strerror(errno));
			probe_run=1;
			i--;
			goto again;
	}
#ifdef ENABLE_ASYNC_MUTEX
	i++;
	if (((async_semaphore=lock_set_alloc(i))==0)||
		(lock_set_init(async_semaphore)==0)){
			if (async_semaphore){
				lock_set_dealloc(async_semaphore);
				async_semaphore=0;
			}
			DBG("DEBUG:lock_initialize: async semaphore initialization"
				" failure: %s\n", strerror(errno));
			probe_run=1;
			i--;
			goto again;
	}
#endif

	/* return success */
	LOG(L_INFO, "INFO: semaphore arrays of size %d allocated\n", sem_nr );
#endif /* GEN_LOCK_T_PREFERED*/
	return 0;
#ifndef GEN_LOCK_T_PREFERED
error:
	lock_cleanup();
	return -1;
#endif
}


#ifdef GEN_LOCK_T_PREFERED
void lock_cleanup()
{
	/* must check if someone uses them, for now just leave them allocated*/
}

#else

/* remove the semaphore set from system */
void lock_cleanup()
{
	/* that's system-wide; all other processes trying to use
	   the semaphore will fail! call only if it is for sure
	   no other process lives 
	*/

	/* sibling double-check missing here; install a signal handler */

	if (entry_semaphore !=0){
		lock_set_destroy(entry_semaphore);
		lock_set_dealloc(entry_semaphore);
	};
	if (reply_semaphore !=0) {
		lock_set_destroy(reply_semaphore);
		lock_set_dealloc(reply_semaphore);
	};
#ifdef ENABLE_ASYNC_MUTEX
	if (async_semaphore !=0) {
		lock_set_destroy(async_semaphore);
		lock_set_dealloc(async_semaphore);
	}
	async_semaphore = 0;
#endif
	entry_semaphore =  reply_semaphore = 0;

}
#endif /*GEN_LOCK_T_PREFERED*/





int init_cell_lock( struct cell *cell )
{
#ifdef GEN_LOCK_T_PREFERED
	lock_init(&cell->reply_mutex);
#else
	cell->reply_mutex.semaphore_set=reply_semaphore;
	cell->reply_mutex.semaphore_index = cell->hash_index % sem_nr;
#endif /* GEN_LOCK_T_PREFERED */
	return 0;
}

int init_entry_lock( struct s_table* ht, struct entry *entry )
{
#ifdef GEN_LOCK_T_PREFERED
	lock_init(&entry->mutex);
#else
	/* just advice which of the available semaphores to use;
	   specifically, all entries are partitioned into as
	   many partitions as number of available semaphores allows
        */
	entry->mutex.semaphore_set=entry_semaphore;
	entry->mutex.semaphore_index = ( ((char *)entry - (char *)(ht->entries ) )
               / sizeof(struct entry) ) % sem_nr;
#endif
	return 0;
}

int init_async_lock( struct cell *cell )
{
#ifdef ENABLE_ASYNC_MUTEX

#ifdef GEN_LOCK_T_PREFERED
	lock_init(&cell->async_mutex);
#else
	cell->async_mutex.semaphore_set=async_semaphore;
	cell->async_mutex.semaphore_index = cell->hash_index % sem_nr;
#endif /* GEN_LOCK_T_PREFERED */

#endif /* ENABLE_ASYNC_MUTEX */

	return 0;
}

int release_cell_lock( struct cell *cell )
{
#ifndef GEN_LOCK_T_PREFERED
	/* don't do anything here -- the init_*_lock procedures
	   just advised on usage of shared semaphores but did not
	   generate them
	*/
#endif
	return 0;
}



int release_entry_lock( struct entry *entry )
{
	/* the same as above */
	return 0;
}
