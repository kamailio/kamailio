/*
 * $Id$
 *
 */

/* 
  timer.c is where we implement TM timers. It has been designed
  for high performance using some techniques of which timer users
  need to be aware.

	One technique is "fixed-timer-length". We maintain separate 
	timer lists, all of them include elements of the same time
	to fire. That allows *appending* new events to the list as
	opposed to inserting them by time, which is costly due to
	searching time spent in a mutex. The performance benefit is
	noticeable. The limitation is you need a new timer list for
	each new timer length.

	Another technique is the timer process slices off expired elements
	from the list in a mutex, but executes the timer after the mutex
	is left. That saves time greatly as whichever process wants to
	add/remove a timer, it does not have to wait until the current
	list is processed. However, be aware the timers may hit in a delayed
	manner; you have no guarantee in your process that after resetting a timer, 
	it will no more hit. It might have been removed by timer process,
    and is waiting to be executed.  The following example shows it:

			PROCESS1				TIMER PROCESS

	0.								timer hits, it is removed from queue and
									about to be executed
	1.	process1 decides to
		reset the timer 
	2.								timer is executed now
	3.	if the process1 naively
		thinks the timer could not 
		have been executed after 
		resetting the timer, it is
		WRONG -- it was (step 2.)

	So be careful when writing the timer handlers. Currently defined timers 
	don't hurt if they hit delayed, I hope at least. Retransmission timer 
	may results in a useless retransmission -- not too bad. FR timer not too
	bad either as timer processing uses a REPLY mutex making it safe to other
	processing affecting transaction state. Wait timer not bad either -- processes
	putting a transaction on wait don't do anything with it anymore.

		Example when it does not hurt:

			P1						TIMER
	0.								RETR timer removed from list and
									scheduled for execution
	1. 200/BYE received->
	   reset RETR, put_on_wait
	2.								RETR timer executed -- too late but it does
									not hurt
	3.								WAIT handler executed

	The rule of thumb is don't touch data you put under a timer. Create data,
    put them under a timer, and let them live until they are safely destroyed from
    wait/delete timer.  The only safe place to manipulate the data is 
    from timer process in which delayed timers cannot hit (all timers are
    processed sequentially).

	A "bad example" -- rewriting content of retransmission buffer
	in an unprotected way is bad because a delayed retransmission timer might 
	hit. Thats why our reply retransmission procedure is enclosed in 
	a REPLY_LOCK.

*/


#include "config.h"
#include "h_table.h"
#include "timer.h"
#include "../../dprint.h"
#include "lock.h"

#include "t_stats.h"

int timer_group[NR_OF_TIMER_LISTS] = 
{
	TG_FR, TG_FR,
	TG_WT,
	TG_DEL,
	TG_RT, TG_RT, TG_RT, TG_RT
};

/* default values of timeouts for all the timer list
   (see timer.h for enumeration of timer lists)
*/
unsigned int timer_id2timeout[NR_OF_TIMER_LISTS] = {
	FR_TIME_OUT, 		/* FR_TIMER_LIST */
	INV_FR_TIME_OUT, 	/* FR_INV_TIMER_LIST */
	WT_TIME_OUT, 		/* WT_TIMER_LIST */
	DEL_TIME_OUT,		/* DELETE_LIST */
	RETR_T1, 			/* RT_T1_TO_1 */
	RETR_T1 << 1, 		/* RT_T1_TO_2 */
	RETR_T1 << 2, 		/* RT_T1_TO_3 */
	RETR_T2 			/* RT_T2 */
						/* NR_OF_TIMER_LISTS */
};


void reset_timer_list( struct s_table* hash_table, enum lists list_id)
{
	hash_table->timers[list_id].first_tl.next_tl =
		&(hash_table->timers[list_id].last_tl );
	hash_table->timers[list_id].last_tl.prev_tl =
		&(hash_table->timers[list_id].first_tl );
	hash_table->timers[list_id].first_tl.prev_tl =
		hash_table->timers[list_id].last_tl.next_tl = NULL;
	hash_table->timers[list_id].last_tl.time_out = -1;
}




void init_timer_list( struct s_table* hash_table, enum lists list_id)
{
	reset_timer_list( hash_table, list_id );
	init_timerlist_lock( hash_table, list_id );
}




void print_timer_list(struct s_table* hash_table, enum lists list_id)
{
	struct timer* timer_list=&(hash_table->timers[ list_id ]);
	struct timer_link *tl ;

	tl = timer_list->first_tl.next_tl;
	while (tl!=& timer_list->last_tl)
	{
		DBG("DEBUG: print_timer_list[%d]: %p, next=%p \n",
			list_id, tl, tl->next_tl);
		tl = tl->next_tl;
	}
}




void remove_timer_unsafe(  struct timer_link* tl )
{
#ifdef EXTRA_DEBUG
	if (tl && tl->timer_list &&
		tl->timer_list->last_tl.prev_tl==0) {
		LOG( L_CRIT,
		"CRITICAL : Oh no, zero link in trailing timer element\n");
		abort();
	};
#endif
	if (is_in_timer_list2( tl )) {
#ifdef EXTRA_DEBUG
		DBG("DEBUG: unlinking timer: tl=%p, timeout=%d, group=%d\n", 
			tl, tl->time_out, tl->tg);
#endif
		tl->prev_tl->next_tl = tl->next_tl;
		tl->next_tl->prev_tl = tl->prev_tl;
		tl->next_tl = 0;
		tl->prev_tl = 0;
		tl->timer_list = NULL;
	}
}




/* put a new cell into a list nr. list_id within a hash_table;
   set initial timeout */
void add_timer_unsafe( struct timer *timer_list, struct timer_link *tl,
	unsigned int time_out )
{
#ifdef EXTRA_DEBUG
	if (timer_list->last_tl.prev_tl==0) {
	LOG( L_CRIT,
		"CRITICAL : Oh no, zero link in trailing timer element\n");
		abort();
	};
#endif

	tl->time_out = time_out;
	tl->prev_tl = timer_list->last_tl.prev_tl;
	tl->next_tl = & timer_list->last_tl;
	timer_list->last_tl.prev_tl = tl;
	tl->prev_tl->next_tl = tl;
	tl->timer_list = timer_list;
#ifdef EXTRA_DEBUG
	if ( tl->tg != timer_group[ timer_list->id ] ) {
		LOG( L_CRIT, "CRITICAL error: changing timer group\n");
		abort();
	}
#endif
	DBG("DEBUG: add_to_tail_of_timer[%d]: %p\n",timer_list->id,tl);
}




/* detach items passed by the time from timer list */
struct timer_link  *check_and_split_time_list( struct timer *timer_list,
	int time )
{
	struct timer_link *tl , *end, *ret;

	/* quick check whether it is worth entering the lock */
	if (timer_list->first_tl.next_tl==&timer_list->last_tl ||
		timer_list->first_tl.next_tl->time_out > time )
			return NULL;

	/* the entire timer list is locked now -- noone else can manipulate it */
	lock(timer_list->mutex);

	end = &timer_list->last_tl;
	tl = timer_list->first_tl.next_tl;
	while( tl!=end && tl->time_out <= time) {
		tl->timer_list = NULL;
		tl=tl->next_tl;
	}

	/* nothing to delete found */
	if (tl->prev_tl==&(timer_list->first_tl)) {
		ret = NULL;
	} else { /* we did find timers to be fired! */
		/* the detached list begins with current beginning */
		ret = timer_list->first_tl.next_tl;
		/* and we mark the end of the split list */
		tl->prev_tl->next_tl = NULL;
		/* the shortened list starts from where we suspended */
		timer_list->first_tl.next_tl = tl;	
		tl->prev_tl = & timer_list->first_tl;
	}
#ifdef EXTRA_DEBUG
	if (timer_list->last_tl.prev_tl==0) {
		LOG( L_CRIT,
		"CRITICAL : Oh no, zero link in trailing timer element\n");
		abort();
	};
#endif
	/* give the list lock away */
	unlock(timer_list->mutex);

	return ret;
}



/* stop timer */
void reset_timer( struct s_table *hash_table,
	struct timer_link* tl )
{
	/* disqualify this timer from execution by setting its time_out
	   to zero; it will stay in timer-list until the timer process
	   starts removing outdated elements; then it will remove it
	   but not execute; there is a race condition, though -- see
	   timer.c for more details
	*/
	tl->time_out = TIMER_DELETED;
#ifdef EXTRA_DEBUG
	DBG("DEBUG: reset_timer (group %d, tl=%p)\n", tl->tg, tl );
#endif
#ifdef _OBSOLETED
	/* lock(timer_group_lock[ tl->tg ]); */
	/* hack to work arround this timer group thing*/
	lock(hash_table->timers[timer_group[tl->tg]].mutex);
	remove_timer_unsafe( tl );
	unlock(hash_table->timers[timer_group[tl->tg]].mutex);
	/*unlock(timer_group_lock[ tl->tg ]);*/
#endif
}




/* determine timer length and put on a correct timer list */
void set_timer( struct s_table *hash_table,
	struct timer_link *new_tl, enum lists list_id )
{
	unsigned int timeout;
	struct timer* list;


	if (list_id<FR_TIMER_LIST || list_id>=NR_OF_TIMER_LISTS) {
		LOG(L_CRIT, "ERROR: set_timer: unkown list: %d\n", list_id);
#ifdef EXTRA_DEBUG
		abort();
#endif
		return;
	}
	timeout = timer_id2timeout[ list_id ];
	list= &(hash_table->timers[ list_id ]);

	lock(list->mutex);
	/* make sure I'm not already on a list */
	remove_timer_unsafe( new_tl );
	add_timer_unsafe( list, new_tl, get_ticks()+timeout);
	unlock(list->mutex);
}

/* similar to set_timer, except it allows only one-time
   timer setting and all later attempts are ignored */
void set_1timer( struct s_table *hash_table,
	struct timer_link *new_tl, enum lists list_id )
{
	unsigned int timeout;
	struct timer* list;


	if (list_id<FR_TIMER_LIST || list_id>=NR_OF_TIMER_LISTS) {
		LOG(L_CRIT, "ERROR: set_timer: unkown list: %d\n", list_id);
#ifdef EXTRA_DEBUG
		abort();
#endif
		return;
	}
	timeout = timer_id2timeout[ list_id ];
	list= &(hash_table->timers[ list_id ]);

	lock(list->mutex);
	if (!(new_tl->time_out>TIMER_DELETED)) {
		/* make sure I'm not already on a list */
		/* remove_timer_unsafe( new_tl ); */
		add_timer_unsafe( list, new_tl, get_ticks()+timeout);

		/* set_1timer is used only by WAIT -- that's why we can
		   afford updating wait statistics; I admit its not nice
		   but it greatly utilizes existing lock 
		*/
		cur_stats->waiting++;acc_stats->waiting++;
	}
	unlock(list->mutex);
}

