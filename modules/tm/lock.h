/*
 * $Id$
 */


#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "../../dprint.h"



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
#define lock(_s) _lock( (_s), __FILE__, __FUNCTION__, __LINE__ )
#define unlock(_s) _unlock( (_s), __FILE__, __FUNCTION__, __LINE__ )
#else
#define lock(_s) _lock( (_s) )
#define unlock(_s) _unlock( (_s) )
#endif


int init_cell_lock( struct cell *cell );
int init_entry_lock( struct s_table* ht, struct entry *entry );


int release_cell_lock( struct cell *cell );
int release_entry_lock( struct entry *entry );
int release_timerlist_lock( struct timer *timerlist );


#ifndef FAST_LOCK
int change_semaphore( ser_lock_t* s  , int val );
#endif


/* lock semaphore s */
#ifdef DBG_LOCK
static inline int _lock( ser_lock_t* s , char *file, char *function,
							unsigned int line )
#else
static inline int _lock( ser_lock_t* s )
#endif
{
#ifdef DBG_LOCK
	DBG("DEBUG: lock : entered from %s , %s(%d)\n", function, file, line );
#endif
#ifdef FAST_LOCK
	get_lock(s);
	return 0;
#else
	return change_semaphore( s, -1 );
#endif
}



#ifdef DBG_LOCK
static inline int _unlock( ser_lock_t* s, char *file, char *function,
		unsigned int line )
#else
static inline int _unlock( ser_lock_t* s )
#endif
{
#ifdef DBG_LOCK
	DBG("DEBUG: unlock : entered from %s, %s:%d\n", file, function, line );
#endif
#ifdef FAST_LOCK
	release_lock(s);
	return 0;
#else
	return change_semaphore( s, +1 );
#endif
}

int init_timerlist_lock(  enum lists timerlist_id);


#endif

