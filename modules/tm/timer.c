/*
 * $Id$
 */


#include "config.h"
#include "h_table.h"
#include "timer.h"
#include "../../dprint.h"

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

static void remove_from_timer_list_dummy(  struct timer_link* tl )
{
	DBG("DEBUG: remove_from_timer[%d]: %p \n",tl->list->id,tl);
	tl->prev_tl->next_tl = tl->next_tl;
	tl->next_tl->prev_tl = tl->prev_tl;
    tl->next_tl = 0;
    tl->prev_tl = 0;
	tl->list = NULL;
}

/* put a new cell into a list nr. list_id within a hash_table;
  * set initial timeout
  */
void add_to_tail_of_timer_list( struct timer *timer_list, 
	struct timer_link *tl, unsigned int time_out )
{
	remove_from_timer_list( tl );
	/* the entire timer list is locked now -- noone else can manipulate it */
	lock( timer_list->mutex );
	tl->time_out = time_out;
	tl->prev_tl = timer_list->last_tl.prev_tl;
	tl->next_tl = & timer_list->last_tl;
	timer_list->last_tl.prev_tl = tl;
	tl->prev_tl->next_tl = tl;
	tl->list = timer_list;
	//print_timer_list(hash_table, list_id);
	/* give the list lock away */
	unlock( timer_list->mutex );
	DBG("DEBUG: add_to_tail_of_timer[%d]: %p\n",timer_list->id,tl);
}





/* remove a cell from a list nr. list_id within a hash_table;
*/
void remove_from_timer_list( struct timer_link* tl)
{
	ser_lock_t	m;

	if (is_in_timer_list2( tl )) {
		m=tl->list->mutex;
		/* the entire timer list is locked now -- noone else can manipulate it */
		lock( m );
		if ( is_in_timer_list2( tl )  ) remove_from_timer_list_dummy( tl );
		//print_timer_list(hash_table, list_id);
		/* give the list lock away */
		unlock( m );
	}
}




/*
	detach items passed by the time from timer list
*/
struct timer_link  *check_and_split_time_list( struct timer *timer_list, int time )

{
	struct timer_link *tl , *tmp , *end, *ret;

	//DBG("DEBUG : check_and_split_time_list: start\n");

	/* quick check whether it is worth entering the lock */
	if (timer_list->first_tl.next_tl==&timer_list->last_tl ||
		timer_list->first_tl.next_tl->time_out > time )
			return NULL;

	/* the entire timer list is locked now -- noone else can manipulate it */
	lock( timer_list->mutex );

	end = &timer_list->last_tl;
	tl = timer_list->first_tl.next_tl;
	while( tl!=end && tl->time_out <= time) tl=tl->next_tl;

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

   /* give the list lock away */
   unlock( timer_list->mutex );

   //DBG("DEBUG : check_and_split_time_list: done, returns %p\n",tl);
   //print_timer_list(hash_table, list_id);
   return ret;
}





void timer_routine(unsigned int ticks , void * attr)
{
	struct s_table       *hash_table = (struct s_table *)attr;
	struct timer*          timers= hash_table->timers;
	struct timer_link  *tl, *tmp_tl;
	int                           id;

	DBG("%d\n", ticks);

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
			tl->list = NULL;
			DBG("DEBUG: timer routine: timer[%d] , tl=%p next=%p\n",id,tl,tmp_tl);
			timers[id].timeout_handler( tl->payload );
			tl = tmp_tl;
		}
	}
}



/* deprecated -- too CPU expensive 
  */
/*
void insert_into_timer_list( struct s_table* hash_table , 
	struct timer_link* new_tl, enum lists list_id , unsigned int time_out )
{
   struct timer          *timer_list = &(hash_table->timers[ list_id ]);
   struct timer_link  *tl;

   // the entire timer list is locked now -- noone else can manipulate it 
   lock( timer_list->mutex );

   // if the element is already in list->first remove it 
   if ( is_in_timer_list( new_tl,list_id)  )
      remove_from_timer_list_dummy( hash_table , new_tl , list_id);

   new_tl->time_out = time_out ;
   DBG("DEBUG: insert_into_timer[%d]:%d, %p\n",list_id,new_tl->time_out,new_tl);
    // seeks the position for insertion 
   for( tl=timer_list->first_tl ; tl && tl->time_out<new_tl->time_out ; tl=tl->next_tl );

   // link it into list
    if ( tl )
    {  // insert before tl
       new_tl->prev_tl = tl->prev_tl;
       tl->prev_tl = new_tl;
    }
   else
    {  // at the end or empty list 
       new_tl->prev_tl = timer_list->last_tl;
       timer_list->last_tl = new_tl;
    }
    if (new_tl->prev_tl )
       new_tl->prev_tl->next_tl = new_tl;
    else
       timer_list->first_tl = new_tl;
    new_tl->next_tl = tl;
	tl->list_id = list_id;

   //print_timer_list(hash_table, list_id);
    // give the list lock away 

    unlock( timer_list->mutex );
}


*/
