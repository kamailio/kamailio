/*
 * $Id$
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



#include <errno.h>

#include "lock.h"
#include "timer.h"
#include "../../dprint.h"

#ifdef FAST_LOCK
#include "../../mem/shm_mem.h"
#endif


#ifndef FAST_LOCK
/* semaphore probing limits */
#define SEM_MIN		16
#define SEM_MAX		4096

/* we implement mutex here using System V semaphores; as number of
   sempahores is limited and number of synchronized elements
   high, we partition the sync'ed SER elements and share semaphores
   in each of the partitions; we try to use as many semaphores as OS
   gives us for finest granularity; perhaps later we will
   add some arch-dependent mutex code that will not have
   ipc's dimensioning limitations and will provide us with
   fast unlimited (=no sharing) mutexing

  UPDATE: we do have now arch-dependent locking (-DFAST_LOCK)

   we allocate the locks according to the following plans:

   1) transaction timer lists have each a semaphore in
      a semaphore set
   2) retransmission timer lists have each a semaphore
      in a semaphore set
   3) we allocate a semaphore set for hash_entries and
      try to use as many semaphores in it as OS allows;
      we partition the the hash_entries by available
      semaphores which are shared  in each partition
   4) cells get always the same semaphore as its hash
      entry in which they live

*/

/* keep the semaphore here */
static int
	entry_semaphore=0, 
	timer_semaphore=0, 
	reply_semaphore=0;
#ifdef _OBSOLETED
	ack_semaphore=0;
#endif
#ifdef _XWAIT
static int  wait_semaphore=0;
#endif
/* and the maximum number of semaphores in the entry_semaphore set */
static int sem_nr;
/* timer group locks */



/* return -1 if semget failed, -2 if semctl failed */
static int init_semaphore_set( int size )
{
	int new_semaphore, i;

	new_semaphore=semget ( IPC_PRIVATE, size, IPC_CREAT | IPC_PERMISSIONS );
	if (new_semaphore==-1) {
		DBG("DEBUG: init_semaphore_set(%d):  failure to allocate a"
					" semaphore: %s\n", size, strerror(errno));
		return -1;
	}
	for (i=0; i<size; i++) {

 		union semun {
			int val;
			struct semid_ds *buf;
			ushort *array;
		} argument;

		/* binary lock */
		argument.val = +1;
   		if (semctl( new_semaphore, i , SETVAL , argument )==-1) {
			DBG("DEBUG: init_semaphore_set:  failure to "
				"initialize a semaphore: %s\n", strerror(errno));
			/* bug cought -- courtesy of Bogdan; -Jiri */
			/* if (semctl( entry_semaphore, 0 , IPC_RMID , 0 )==-1) */
			if (semctl( new_semaphore, 0 , IPC_RMID , 0 )==-1) 
				DBG("DEBUG: init_semaphore_set:  failure to release"
					" a semaphore\n");
			return -2;
		}
	} /* for cycle */
	return new_semaphore;
}



int change_semaphore( ser_lock_t* s  , int val )
{
	struct sembuf pbuf;
	int r;

	pbuf.sem_num = s->semaphore_index ;
	pbuf.sem_op =val;
	pbuf.sem_flg = 0;

tryagain:
	r=semop( s->semaphore_set, &pbuf ,  1 /* just 1 op */ );

	if (r==-1) {
		if (errno==EINTR) {
			DBG("signal received in a semaphore\n");
			goto tryagain;
		} else {
			LOG(L_CRIT, "ERROR: change_semaphore(%x, %x, 1) : %s\n",
					s->semaphore_set, &pbuf,
					strerror(errno));
		}
	}
	return r;
}
#endif  /* !FAST_LOCK*/


static ser_lock_t* timer_group_lock; /* pointer to a TG_NR lock array,
								    it's safer if we alloc this in shared mem 
									( required for fast lock ) */

/* intitialize the locks; return 0 on success, -1 otherwise
*/
int lock_initialize()
{
	int i;
#ifndef FAST_LOCK
	int probe_run;
#endif

	/* first try allocating semaphore sets with fixed number of semaphores */
	DBG("DEBUG: lock_initialize: lock initialization started\n");

	timer_group_lock=shm_malloc(TG_NR*sizeof(ser_lock_t));
	if (timer_group_lock==0){
		LOG(L_CRIT, "ERROR: lock_initialize: out of shm mem\n");
		goto error;
	}
#ifdef FAST_LOCK
	for(i=0;i<TG_NR;i++) init_lock(timer_group_lock[i]);
#else
	/* transaction timers */
	if ((timer_semaphore= init_semaphore_set( TG_NR ) ) < 0) {
		LOG(L_CRIT, "ERROR: lock_initialize:  "
			"transaction timer semaphore initialization failure: %s\n",
				strerror(errno));
		goto error;
	}

	for (i=0; i<TG_NR; i++) {
		timer_group_lock[i].semaphore_set = timer_semaphore;
		timer_group_lock[i].semaphore_index = timer_group[ i ];	
	}


	i=SEM_MIN;
	/* probing phase: 0=initial, 1=after the first failure */
	probe_run=0;
again:
	do {
		if (entry_semaphore>0) /* clean-up previous attempt */
			semctl( entry_semaphore, 0 , IPC_RMID , 0 );
		if (reply_semaphore>0)
			semctl(reply_semaphore, 0 , IPC_RMID , 0 );
#ifdef _OBSOLETED
		if (ack_semaphore>0)
			semctl(reply_semaphore, 0 , IPC_RMID , 0 );
#endif
#ifdef _XWAIT
		if (wait_semaphore>0)
			semctl(wait_semaphore, 0 , IPC_RMID , 0 );
#endif


		if (i==0){
			LOG(L_CRIT, "lock_initialize: could not allocate semaphore"
					" sets\n");
			goto error;
		}

		entry_semaphore=init_semaphore_set( i );
		if (entry_semaphore==-1) {
			DBG("DEBUG: lock_initialize: entry semaphore "
					"initialization failure:  %s\n", strerror( errno ) );
			/* Solaris: EINVAL, Linux: ENOSPC */
                        if (errno==EINVAL || errno==ENOSPC ) {
                                /* first time: step back and try again */
                                if (probe_run==0) {
					DBG("DEBUG: lock_initialize: first time "
								"semaphore allocation failure\n");
                                        i--;
                                        probe_run=1;
                                        continue;
				/* failure after we stepped back; give up */
                                } else {
				 	DBG("DEBUG: lock_initialize:   second time sempahore allocation failure\n");
					goto error;
				}
                        }
			/* some other error occured: give up */
                        goto error;
                }
		/* allocation succeeded */
		if (probe_run==1) { /* if ok after we stepped back, we're done */
			break;
		} else { /* if ok otherwiese, try again with larger set */
			if (i==SEM_MAX) break;
			else {
				i++;
				continue;
			}
		}
	} while(1);
	sem_nr=i;	

	if ((reply_semaphore=init_semaphore_set( sem_nr ))<0){
		if (errno==EINVAL || errno==ENOSPC ) {
			DBG("DEBUG:lock_initialize: reply semaphore initialization"
				" failure: %s\n", strerror(errno));
			probe_run==1;
			i--;
			goto again;
		}else{
			LOG(L_CRIT, "ERROR:lock_initialize: reply semaphore initialization"
				" failure: %s\n", strerror(errno));
			goto error;
		}
	}
#ifdef _OBSOLETED	
	if ((ack_semaphore=init_semaphore_set(sem_nr))<0){
		if (errno==EINVAL || errno==ENOSPC ) {
			DBG( "DEBUG:lock_initialize: ack semaphore initialization"
				" failure: %s\n", strerror(errno));
			probe_run==1;
			i--;
			goto again;
		}else{
			LOG(L_CRIT, "ERROR:lock_initialize: ack semaphore initialization"
				" failure: %s\n", strerror(errno));
			goto error;
		}
	}
#endif

#ifdef _XWAIT
	if ((wait_semaphore=init_semaphore_set(sem_nr))<0){
		if (errno==EINVAL || errno==ENOSPC ) {
			DBG( "DEBUG:lock_initialize: wait semaphore initialization"
				" failure: %s\n", strerror(errno));
			probe_run==1;
			i--;
			goto again;
		}else{
			LOG(L_CRIT, "ERROR:lock_initialize: wait semaphore initialization"
				" failure: %s\n", strerror(errno));
			goto error;
		}
	}
#endif




	/* return success */
	LOG(L_INFO, "INFO: semaphore arrays of size %d allocated\n", sem_nr );
#endif /* FAST_LOCK*/
	return 0;
error:
	lock_cleanup();
	return -1;
}


#ifdef FAST_LOCK
void lock_cleanup()
{
	/* must check if someone uses them, for now just leave them allocated*/
}

#else

/* remove the semaphore set from system */
void lock_cleanup()
{
	/* that's system-wide; all othe processes trying to use
	   the semaphore will fail! call only if it is for sure
	   no other process lives 
	*/

	/* sibling double-check missing here; install a signal handler */

	if (entry_semaphore > 0 && 
	    semctl( entry_semaphore, 0 , IPC_RMID , 0 )==-1)
		LOG(L_ERR, "ERROR: lock_cleanup, entry_semaphore cleanup failed\n");
	if (timer_semaphore > 0 && 
	    semctl( timer_semaphore, 0 , IPC_RMID , 0 )==-1)
		LOG(L_ERR, "ERROR: lock_cleanup, timer_semaphore cleanup failed\n");
	if (reply_semaphore > 0 &&
	    semctl( reply_semaphore, 0 , IPC_RMID , 0 )==-1)
		LOG(L_ERR, "ERROR: lock_cleanup, reply_semaphore cleanup failed\n");
#ifdef _OBSOLETED
	if (ack_semaphore > 0 &&
	    semctl( ack_semaphore, 0 , IPC_RMID , 0 )==-1)
		LOG(L_ERR, "ERROR: lock_cleanup, ack_semaphore cleanup failed\n");
#endif
#ifdef _XWAIT
	if (wait_semaphore > 0 &&
		semctl( wait_semaphore, 0 , IPC_RMID , 0 )==-1)
		LOG(L_ERR, "ERROR: lock_cleanup, wait_semaphore cleanup failed\n");
#endif


	entry_semaphore = timer_semaphore = reply_semaphore 
#ifdef _OBSOLETED
		= ack_semaphore 
#endif
		= 0;
#ifdef _XWAIT
	wait_semaphore = 0;
#endif


}
#endif /*FAST_LOCK*/





int init_cell_lock( struct cell *cell )
{
#ifdef FAST_LOCK
	init_lock(cell->reply_mutex);
#ifdef _OBSOLETED
	init_lock(cell->ack_mutex);
#endif
#ifdef _XWAIT
	init_lock(cell->wait_mutex);
#endif
	return 0;
#else
	cell->reply_mutex.semaphore_set=reply_semaphore;
	cell->reply_mutex.semaphore_index = cell->hash_index % sem_nr;
#ifdef _OBSOLETED
	cell->ack_mutex.semaphore_set=ack_semaphore;
	cell->ack_mutex.semaphore_index = cell->hash_index % sem_nr;
#endif
#ifdef _XWAIT
	cell->wait_mutex.semaphore_set=wait_semaphore;
	cell->wait_mutex.semaphore_index = cell->hash_index % sem_nr;
#endif /* WAIT */
#endif /* FAST_LOCK */
	return 0;
}

int init_entry_lock( struct s_table* ht, struct entry *entry )
{
#ifdef FAST_LOCK
	init_lock(entry->mutex);
#else
	/* just advice which of the available semaphores to use;
	   specifically, all entries are partitioned into as
	   many partitions as number of available semaphors allows
        */
	entry->mutex.semaphore_set=entry_semaphore;
	entry->mutex.semaphore_index = ( ((void *)entry - (void *)(ht->entrys ) )
               / sizeof(struct entry) ) % sem_nr;
#endif
	return 0;
}



int release_cell_lock( struct cell *cell )
{
#ifndef FAST_LOCK
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



int release_timerlist_lock( struct timer *timerlist )
{
	/* the same as above */
	return 0;
}

int init_timerlist_lock( enum lists timerlist_id)
{
	get_timertable()->timers[timerlist_id].mutex=
		&(timer_group_lock[ timer_group[timerlist_id] ]);
	return 0;
}
