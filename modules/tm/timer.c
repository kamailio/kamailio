/*
 * $Id$
 */


#include "config.h"
#include "h_table.h"
#include "timer.h"
#include "../../dprint.h"

int timer_group[NR_OF_TIMER_LISTS] = { 
	TG_FR, TG_FR,
	TG_WT,
	TG_DEL,
	TG_RT, TG_RT, TG_RT, TG_RT
};

void reset_timer_list( struct s_table* hash_table, enum lists list_id)
{
	hash_table->timers[ list_id ].first_tl.next_tl = & (hash_table->timers[ list_id ].last_tl );
	hash_table->timers[ list_id ].last_tl.prev_tl = & (hash_table->timers[ list_id ].first_tl );
	hash_table->timers[ list_id ].first_tl.prev_tl = 
		hash_table->timers[ list_id ].last_tl.next_tl = NULL;
	hash_table->timers[ list_id ].last_tl.time_out = -1;
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
      DBG("DEBUG: print_timer_list[%d]: %p, next=%p \n",list_id, tl, tl->next_tl);
      tl = tl->next_tl;
   }
}

/* static void remove_from_timer_list_dummy(  struct timer_link* tl ) */
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
		tl->prev_tl->next_tl = tl->next_tl;
		tl->next_tl->prev_tl = tl->prev_tl;
		tl->next_tl = 0;
		tl->prev_tl = 0;
		tl->timer_list = NULL;
	}
}

/* put a new cell into a list nr. list_id within a hash_table;
  * set initial timeout
  */
void add_timer_unsafe( struct timer *timer_list,
	struct timer_link *tl, unsigned int time_out )
{
#ifdef EXTRA_DEBUG
	if (timer_list->last_tl.prev_tl==0) {
	LOG( L_CRIT,
		"CRITICAL : Oh no, zero link in trailing timer element\n");
		abort();
	};
#endif

	/*	remove_from_timer_list( tl ); */
	/* the entire timer list is locked now -- noone else can manipulate it */
	/* lock( timer_list->mutex ); */
	tl->time_out = time_out;
	tl->prev_tl = timer_list->last_tl.prev_tl;
	tl->next_tl = & timer_list->last_tl;
	timer_list->last_tl.prev_tl = tl;
	tl->prev_tl->next_tl = tl;
	tl->timer_list = timer_list;
#	ifdef EXTRA_DEBUG
		if ( tl->tg != timer_group[ timer_list->id ] ) {
			LOG( L_CRIT, "CRITICAL error: changing timer group\n");
			abort();
		}
#	endif
	/* give the list lock away */
	/* unlock( timer_list->mutex ); */
	DBG("DEBUG: add_to_tail_of_timer[%d]: %p\n",timer_list->id,tl);
}

/*
	detach items passed by the time from timer list
*/
struct timer_link  *check_and_split_time_list( struct timer *timer_list, int time )

{
	struct timer_link *tl , *tmp , *end, *ret;

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





void timer_routine(unsigned int ticks , void * attr)
{
	struct s_table       *hash_table = (struct s_table *)attr;
	struct timer*          timers= hash_table->timers;
	struct timer_link  *tl, *tmp_tl;
	int                           id;


	for( id=0 ; id<NR_OF_TIMER_LISTS ; id++ )
	{
		/* to waste as little time in lock as possible, detach list
		   with expired items and process them after leaving the
		   lock
		*/
		tl = check_and_split_time_list( & (hash_table->timers[ id ]), ticks );
		/* process items now */
		while (tl)
		{
			/* reset the timer list linkage */
			tmp_tl = tl->next_tl;
			tl->next_tl = tl->prev_tl =0 ; 
			DBG("DEBUG: timer routine: timer[%d] , tl=%p next=%p\n",id,tl,tmp_tl);
			timers[id].timeout_handler( tl->payload );
			tl = tmp_tl;
		}
	}
}



