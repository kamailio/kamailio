/*
 * $Id$
 */


#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>


/* typedef to structure we use for mutexing;
   currently, index to a semaphore set identifier now */
typedef struct {
	int semaphore_set;
	int semaphore_index;
} ser_lock_t;


#include "h_table.h"
#include "timer.h"

/* Uni*x permissions for IPC */
#define IPC_PERMISSIONS 0666


int lock_initialize();
int init_semaphore_set( int size );
void lock_cleanup();

int lock( ser_lock_t s );
int unlock( ser_lock_t s );
int change_semaphore( ser_lock_t s  , int val );

int init_cell_lock( struct cell *cell );
int init_entry_lock( struct s_table* hash_table, struct entry *entry );
// int init_timerlist_lock( struct s_table* hash_table, enum lists timerlist_id);
//int init_retr_timer_lock( struct s_table* hash_table, enum retransmission_lists list_id );

int release_cell_lock( struct cell *cell );
int release_entry_lock( struct entry *entry );
int release_timerlist_lock( struct timer *timerlist );

#endif

