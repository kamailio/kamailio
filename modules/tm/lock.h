/*
 * $Id$
 */


#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>



#ifdef  FAST_LOCK
#include "../../fastlock.h"
#endif

#ifdef FAST_LOCK
#define ser_lock_t fl_lock_t
#else
/* typedef to structure we use for mutexing;
   currently, index to a semaphore set identifier now */
typedef struct {
	int semaphore_set;
	int semaphore_index;
} ser_lock_t;
#endif


enum timer_groups {
	TG_FR,
	TG_WT,
	TG_DEL,
	TG_RT,
	TG_NR
};


/* extern ser_lock_t timer_group_lock[TG_NR]; */


#include "h_table.h"
#include "timer.h"

/* Uni*x permissions for IPC */
#define IPC_PERMISSIONS 0666


int lock_initialize();
void lock_cleanup();
/*
#ifndef FAST_LOCK
static int init_semaphore_set( int size );
#endif
*/

#ifdef DBG_LOCK
int _lock( ser_lock_t* s , char *file, char *function, unsigned int line );
int _unlock( ser_lock_t* s, char *file, char *function, unsigned int line );
#define lock(_s) _lock( (_s), __FILE__, __FUNCTION__, __LINE__ )
#define unlock(_s) _unlock( (_s), __FILE__, __FUNCTION__, __LINE__ )
#else
int _lock( ser_lock_t* s );
int _unlock( ser_lock_t* s );
#define lock(_s) _lock( (_s) )
#define unlock(_s) _unlock( (_s) )
#endif

/*
#ifndef FAST_LOCK
static int change_semaphore( ser_lock_t s  , int val );
#endif
*/

int init_cell_lock( struct cell *cell );
int init_entry_lock( struct s_table* hash_table, struct entry *entry );
int init_timerlist_lock( struct s_table* hash_table, enum lists timerlist_id);


int release_cell_lock( struct cell *cell );
int release_entry_lock( struct entry *entry );
int release_timerlist_lock( struct timer *timerlist );

#endif

