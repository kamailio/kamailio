#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "h_table.h"

/* Uni*x permissions for IPC */
#define IPC_PERMISSIONS 0666

/* typedef to structure we use for mutexing; 
   currently, index to a semaphore set identifier now */
typedef int lock_t;

/* try to allocate as long set as possible;
   return set size or -1 on error
*/

int lock_initialize();

int lock_cleanup();

int lock( lock_t s );

int change_semaphore( int semaphore_id , int val );

init_cell_lock( struct cell *cell );
init_entry_lock( struct entry *entry );
init_timerlist_lock( struct timer *timerlist );

release_cell_lock( struct cell *cell );
release_entry_lock( struct entry *entry );
release_timerlist_lock( struct timer *timerlist );

#endif

