/*
 * $Id$
 */


#include "timer.h"
#include "../../dprint.h"


void remove_from_timer_list_dummy( struct s_table* hash_table , struct timer_link* tl , int list_id)
{
   struct timer* timer_list=&(hash_table->timers[ list_id ]);
   DBG("DEBUG: remove_from_timer[%d]: %d, %p \n",list_id,tl->time_out,tl);

   if ( tl->prev_tl )
       tl->prev_tl->next_tl = tl->next_tl;
    else
         timer_list->first_tl = tl->next_tl;

   if ( tl->next_tl )
         tl->next_tl->prev_tl = tl->prev_tl;
    else
         timer_list->last_tl = tl->prev_tl;

    tl->next_tl = 0;
    tl->prev_tl = 0;
}








/* put a new cell into a list nr. list_id within a hash_table;
  * set initial timeout
  */
void add_to_tail_of_timer_list( struct s_table* hash_table , struct timer_link* tl, int list_id , unsigned int time_out )
{
   struct timer* timer_list = &(hash_table->timers[ list_id ]);

   /* the entire timer list is locked now -- noone else can manipulate it */
   lock( timer_list->mutex );

   /* if the element is already in list->first remove it */
   if ( is_in_timer_list( tl,list_id)  )
      remove_from_timer_list_dummy( hash_table , tl , list_id);

   tl->time_out = time_out;
   tl->next_tl= 0;

   DBG("DEBUG: add_to_tail_of_timer[%d]: %d, %p\n",list_id,tl->time_out,tl);
   /* link it into list */
   if (timer_list->last_tl)
   {
       tl->prev_tl=timer_list->last_tl;
       timer_list->last_tl->next_tl = tl;
       timer_list->last_tl = tl ;
   } else {
       tl->prev_tl = 0;
       timer_list->first_tl = tl;
       timer_list->last_tl = tl;
   }
   /* give the list lock away */
   unlock( timer_list->mutex );
}




/*
  */
void insert_into_timer_list( struct s_table* hash_table , struct timer_link* new_tl, int list_id , unsigned int time_out )
{
   struct timer          *timer_list = &(hash_table->timers[ list_id ]);
   struct timer_link  *tl;

   /* the entire timer list is locked now -- noone else can manipulate it */
   lock( timer_list->mutex );

   /* if the element is already in list->first remove it */
   if ( is_in_timer_list( new_tl,list_id)  )
      remove_from_timer_list_dummy( hash_table , new_tl , list_id);

   new_tl->time_out = time_out ;
   DBG("DEBUG: insert_into_timer[%d]: %d, %p\n",list_id,new_tl->time_out,new_tl);
    /*seeks the position for insertion */
   for( tl=timer_list->first_tl ; tl && tl->time_out<new_tl->time_out ; tl=tl->next_tl );

   /* link it into list */
    if ( tl )
    {  /* insert before tl*/
       new_tl->prev_tl = tl->prev_tl;
       tl->prev_tl = new_tl;
    }
   else
    {  /* at the end or empty list */
       new_tl->prev_tl = timer_list->last_tl;
       timer_list->last_tl = new_tl;
    }
    if (new_tl->prev_tl )
       new_tl->prev_tl->next_tl = new_tl;
    else
       timer_list->first_tl = new_tl;
    new_tl->next_tl = tl;

   /* give the list lock away */
    unlock( timer_list->mutex );
}




/* remove a cell from a list nr. list_id within a hash_table;
*/
void remove_from_timer_list( struct s_table* hash_table , struct timer_link* tl , int list_id)
{
   struct timer* timer_list=&(hash_table->timers[ list_id ]);

   /* the entire timer list is locked now -- noone else can manipulate it */
   lock( timer_list->mutex );

   /* if the element is already in list->first remove it */
   if ( is_in_timer_list( tl,list_id)  )
   {
      remove_from_timer_list_dummy( hash_table , tl , list_id);
   }

   /* give the list lock away */
   unlock( timer_list->mutex );
}




/*
*/
struct timer_link  *check_and_split_time_list( struct s_table* hash_table, int list_id ,int time)
{
   struct timer* timer_list=&(hash_table->timers[ list_id ]);
   struct timer_link *tl ;

   /* the entire timer list is locked now -- noone else can manipulate it */
   lock( timer_list->mutex );

   tl = timer_list->first_tl;
   if ( !tl )
      goto exit;

   for(  ; tl && tl->time_out <= time ; tl = tl->next_tl );

    /*if I don't have to remove anything*/
    if ( tl==timer_list->first_tl )
    {
      tl =0;
      goto exit;
    }

    /*if I have to remove everything*/
    if (tl==0)
    {
      tl = timer_list->first_tl;
      timer_list->first_tl = timer_list->last_tl = 0;
     goto exit;
    }

    /*I have to split it somewhere in the middle */
    tl->prev_tl->next_tl=0;
    tl->prev_tl = 0;
    timer_list->first_tl = tl;
    tl = timer_list->first_tl;

exit:
   /* give the list lock away */
   unlock( timer_list->mutex );

   return tl;
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
      tl = check_and_split_time_list( hash_table, id , ticks );
      while (tl)
      {
         tmp_tl = tl->next_tl;
         tl->next_tl = tl->prev_tl =0 ;
         timers[id].timeout_handler( tl->payload );
         tl = tmp_tl;
      }
   }
}



