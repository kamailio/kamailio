#include "lock.h"

/* we implement locking using semaphores here; we generate a single
   semaphore set, try to allocate as many semaphores in it as OS
   supports and keep them in the semphore set; it's application's
   reponsibility to decide how to distribute semaphores across
   its data set; if we are happy and have many semaphores, every
   mutexed piece of data will get its own semaphore; not likely
   though, as typically this number is low and number of sync'ed
   data items is very high
*/

/* keep the semaphore here */
int semaphore;

/* after initialization I will certainly want to
   dimension partitioning; I need now hash_table_size
   + number_of_lists locks; the customer of this
   procedure should adapt hash_table_size
   accordingly

        probing should return ENOSPC
*/



/* intitialize the locks; return 0 for unlimited number of locks
   available, +int if number of locks limited, -1 on failure

  with semaphores, use a single set with maximum number
  of semaphores in it; return this number on success
*/


int lock_initialize()
{
	int i;
	int probe=12;
	/* probing should return ENOSPC */
	semaphore=semget ( IPC_PRIVATE, probe, IPC_CREAT | IPC_PERMISSIONS );
	/* if we failed to initialize, return */
	if (semaphore==-1) return -1;

	/* initialize semaphores */
	
	for (i=0; i<probe; i++) {
		union semun {
			int val;
			struct semid_ds *buf;
			ushort *array;
		} argument;
		/* binary lock */
		argument.val = +1;
		if (semctl( semaphore , i , SETVAL , argument )==-1) {
			/* return semaphore */
			lock_cleanup();
			return -1;
		}
	}
	
	/* return number of  sempahores in the set */
	return probe;
}

/* remove the semaphore set from system */
int lock_cleanup()
{

	/* that's system-wide; all othe processes trying to use
	   the semaphore will fail! call only if it is for sure
	   no other process lives 
	*/

	/* sibling double-check missing here */
	return  semctl( semaphore, 0 , IPC_RMID , 0 ) ;
}

/* lock sempahore s */
int lock( lock_t s )
{
/* don't forget EINTER */
	return change_sem( s, -1 );
}

int unlock( lock_t s )
{
	return change_sem( s, 1 );
	
}


int change_sem( int semaphore_id , int val )
{
   struct sembuf pbuf;

   pbuf.sem_num = semaphore_id ;
   pbuf.sem_op =val;
   pbuf.sem_flg = 0;

   return semop( semaphore , &pbuf ,  1 /* just 1 op */ );
}

struct s_table;
struct timer;
struct cell;


init_cell_lock( struct cell *cell )
{}
init_entry_lock( struct entry *entry )
{}
init_timerlist_lock( struct timer *timerlist )
{}

release_cell_lock( struct cell *cell )
{}
release_entry_lock( struct entry *entry )
{}

release_timerlist_lock( struct timer *timerlist )
{}



