#include <errno.h>

#include "lock.h"
#include "globals.h"
#include "timer.h"

/* we implement mutex here using System V semaphores; as number of
   sempahores is limited and number of synchronized elements
   high, we partition the SER elements and share semaphores in
   each of them; we try to use as much semaphores as OS
   gives us for finest granularity; perhaps later we will
   add some arch-dependent mutex code that will not have
   ipc's dimensioning limitations and will provide us with
   fast unlimited mutexing

   we allocate the locks according to the following plans:

   1) we try to allocate as many semaphores as possible
   2) we grab first NR_OF_TIMER_LISTS semaphores for the
      timer list
   3) we take the remainder of semaphores R, and split
      the hash-table table_entries into R partitiones;
      within each partition, each entry shares the same
      semaphore
   4) every cell shares the same semaphore as its entry

*/

/* keep the semaphore here */
int semaphore;
/* and the number of semaphores */
int sem_nr;


/* intitialize the locks; return 0 for unlimited number of locks
   available, +int if number of locks limited, -1 on failure

  with semaphores, use a single set with maximum number
  of semaphores in it; return this number on success
*/


int lock_initialize()
{
	int i;
	
	sem_nr=12;
	/* probing should return if too big:
		Solaris: EINVAL
		Linux: ENOSPC
	*/
	semaphore=semget ( IPC_PRIVATE, sem_nr, IPC_CREAT | IPC_PERMISSIONS );
	/* if we failed to initialize, return */
	if (semaphore==-1) return -1;


	/* initialize semaphores */
	
	for (i=0; i<sem_nr; i++) {
		union semun {
			int val;
			struct semid_ds *buf;
			ushort *array;
		} argument;
		/* binary lock */
		argument.val = +1;
		if (semctl( semaphore , i , SETVAL , argument )==-1) {
			/* return semaphore */
			DBG("failed to initialize semaphore\n");
			lock_cleanup();
			return -1;
		}
	}
	
	/* return number of  sempahores in the set */
	return sem_nr;
}

/* remove the semaphore set from system */
int lock_cleanup()
{
	/* that's system-wide; all othe processes trying to use
	   the semaphore will fail! call only if it is for sure
	   no other process lives 
	*/

	DBG("clean-up still not implemented properly\n");
	/* sibling double-check missing here; install a signal handler */

	return  semctl( semaphore, 0 , IPC_RMID , 0 ) ;
}

/* lock sempahore s */
int lock( lock_t s )
{
	return change_sem( s, -1 );
}
	
int unlock( lock_t s )
{
	return change_sem( s, +1 );
}


int change_semaphore( int semaphore_id , int val )
{
   struct sembuf pbuf;
   int r;

   pbuf.sem_num = semaphore_id ;
   pbuf.sem_op =val;
   pbuf.sem_flg = 0;

tryagain:
   r=semop( semaphore , &pbuf ,  1 /* just 1 op */ );

   if (r==-1) {
	printf("ERROR occured in change_semaphore: %d, %d, %s\n", 
		semaphore_id, val, strerror(errno));
	if (errno=EINTR) {
		DBG("signal received in a semaphore\n");
		goto tryagain;
	}
    }
   return r;
}

int init_cell_lock( struct cell *cell )
{
	/* just advice which of the available semaphores to use;
	   specifically, all cells in an entry use the same one
        */
	cell->lock=cell->hash_index / sem_nr + NR_OF_TIMER_LISTS;
}

int init_entry_lock( struct entry *entry )
{
	/* just advice which of the available semaphores to use;
	   specifically, all entries are partitioned into as
	   many partitions as number of available semaphors allows
        */
	entry->lock= (entry - hash_table ) / sizeof(struct entry) + NR_OF_TIMER_LISTS;

}
int init_timerlist_lock( struct timer *timerlist )
{
	/* each timer list has its own semaphore */
	entry->lock=timerlist->id;
}

int release_cell_lock( struct cell *cell )
{
	/* don't do anything here -- the init_*_lock procedures
	   just advised on usage of shared semaphores but did not 
	   generate them
	*/
}
int release_entry_lock( struct entry *entry )
{
	/* the same as above */
}

release_timerlist_lock( struct timer *timerlist )
{
	/* the same as above */
}
