void free_cell( struct cell* dead_cell );


/*
*   The cell is inserted at the end of the FINAL RESPONSE timer list.
*   The expire time is given by the current time plus the FINAL RESPONSE timeout - FR_TIME_OUT
*/
void start_FR_timer( table hash_table, struct cell* p_cell )
{
   struct timer* timers= hash_table->timers;

   /* adds the cell int FINAL RESPONSE timer list*/
   change_sem( timers[0].sem , -1  );
   p_cell -> time_out    = FR_TIME_OUT + hash_table->time;
   if ( timers[0].last_cell )
   {
      p_cell -> timer_next_cell = 0;
      p_cell->timer_prev_cell = timers[0].last_cell;
      timers[0].last_cell->timer_next_cell = p_cell;
      timers[0].last_cell = p_cell;
   }
   else
   {
      p_cell->timer_prev_cell = 0;
      p_cell->timer_next_cell = 0;
      timers[0].first_cell = p_cell;
      timers[0].last_cell = p_cell;
   }

   change_sem( timers[0].sem , +1  );
}



/*
*   The cell is inserted at the end of the WAIT timer list. Before adding to the WT list, it's verify if the cell is
*   or not in the FR list (normally it should be there). If it is, it's first removed from FR list and after that
*   added to the WT list.
*   The expire time is given by the current time plus the WAIT timeout - WT_TIME_OUT
*/

void start_WT_timer( table hash_table, struct cell* p_cell )
{
   struct timer* timers= hash_table->timers;

   //if is in FR list -> first it must be removed from there
   if ( p_cell->timer_next_cell || p_cell->timer_prev_cell || (!p_cell->timer_next_cell && !p_cell->timer_prev_cell && p_cell==timers[0].first_cell)  )
   {
      change_sem( timers[0].sem , -1  );
      if ( p_cell->timer_prev_cell )
         p_cell->timer_prev_cell->timer_next_cell = p_cell->timer_next_cell;
      else
         timers[0].first_cell = p_cell->timer_next_cell;
      if ( p_cell->timer_next_cell )
         p_cell->timer_next_cell->timer_prev_cell = p_cell->timer_prev_cell;
      else
         timers[0].last_cell = p_cell->timer_prev_cell;
      change_sem( timers[0].sem , +1  );
   }

   /* adds the cell int WAIT timer list*/
   change_sem( timers[1].sem , -1  );
   p_cell -> time_out = WT_TIME_OUT + hash_table->time;
   if ( timers[1].last_cell )
   {
      p_cell -> timer_next_cell = 0;
      p_cell ->timer_prev_cell = timers[1].last_cell;
      timers[1].last_cell->timer_next_cell = p_cell;
      timers[1].last_cell = p_cell;
   }
   else
   {
      p_cell->timer_prev_cell = 0;
      p_cell->timer_next_cell = 0;
      timers[1].first_cell = p_cell;
      timers[1].last_cell = p_cell;
   }

   change_sem( timers[1].sem , +1  );
}




/*
*   prepare for del a transaction ; the transaction is first removed from the hash entry list (oniy the links from
*   cell to list are deleted in order to make the cell unaccessible from the list and in the same time to keep the
*   list accessible from the cell for process that are currently reading the cell). If no process is reading the cell
*   (ref conter is 0) the cell is immediatly deleted. Otherwise it is put in a waitting list (del_hooker list) for
*   future del. This list is veify by the the timer every sec and the cell that finaly have ref_counter 0 are del.
*/
void del_Transaction( table hash_table , struct cell * p_cell )
{
   int      ref_counter         = 0;
   int      hash_index         = 0;
   char*  p                         = p_cell->via_label;
   struct entry*  p_entry  = 0 ;

    /* gets the entry number from the label */
   for(  ; p && *p>='0' && *p<='9' ; p++ )
      hash_index = hash_index * 10 + (*p - '0')  ;
    p_entry = &(hash_table->entrys[hash_index]);

    /* the cell is removed from the list */
    change_sem( p_entry->sem , -1  );
    if ( p_cell->prev_cell )
         p_cell->prev_cell->next_cell = p_cell->next_cell;
      else
         p_entry->first_cell = p_cell->next_cell;
      if ( p_cell->next_cell )
         p_cell->next_cell->prev_cell = p_cell->prev_cell;
      else
         p_entry->last_cell = p_cell->prev_cell;
    change_sem( p_entry->sem , +1  );

    /* gets the cell's ref counter*/
    change_sem( p_cell->sem , -1  );
    ref_counter = p_cell->ref_counter;
    change_sem( p_cell->sem , +1  );

    /* if is not refenceted -> is deleted*/
    if ( ref_counter==0 )
       free_cell( p_cell );
     /* else it's added to del hooker list for future del */
    else
        if ( hash_table->last_del_hooker )
         {
            p_cell -> timer_next_cell = 0;
            p_cell ->timer_prev_cell = hash_table->last_del_hooker;
            hash_table->last_del_hooker->timer_next_cell = p_cell;
            hash_table->last_del_hooker = p_cell;
         }
        else
         {
            p_cell->timer_prev_cell = 0;
            p_cell->timer_next_cell = 0;
            hash_table->first_del_hooker = p_cell;
            hash_table->last_del_hooker = p_cell;
         }
}





void * timer_routine(void * attr)
{
   table                  hash_table = (table)attr;
   struct timer*    timers= hash_table->timers;
   struct timeval  a_sec;
   struct cell*       p_cell;
   struct cell*       tmp_cell;
   int unsigned*    time =  &(hash_table->time);


   while (1)
   {
      a_sec.tv_sec   = 1;
      a_sec.tv_usec = 0;
      select( 0 , 0 , 0 ,0 , &a_sec );
      (*time)++;
      printf("%d\n", *time);


      /* del hooker list */
      for( p_cell = hash_table->first_del_hooker ; p_cell ;  p_cell = tmp_cell )
      {
         int ref_counter;
         /*gets the cell's ref counter*/
         change_sem( p_cell->sem , -1  );
         ref_counter = p_cell->ref_counter;
         change_sem( p_cell->sem , +1  );
         /*if the ref counter is 0 (nobody is reading the cell any more) -> remove from list and del*/
        if (ref_counter==0)
         {
             tmp_cell = p_cell->timer_next_cell;
             /*remove from the list*/
           if ( p_cell->timer_prev_cell )
                 p_cell->timer_prev_cell->timer_next_cell = p_cell->timer_next_cell;
            else
                 hash_table->first_del_hooker = p_cell->timer_next_cell;
            if ( p_cell->timer_next_cell )
                 p_cell->timer_next_cell->timer_prev_cell = p_cell->timer_prev_cell;
            else
                 hash_table->last_del_hooker = p_cell->timer_prev_cell;
             /* del the cell*/
             free_cell( p_cell );
         }
         else
            tmp_cell = p_cell->timer_next_cell;

       }


       /* final response timer list*/
      for( p_cell = timers[0].first_cell ; p_cell ;  p_cell=tmp_cell )
           /* if the time out expired -> send a 408 and move the trans in the WAIT timer queue */
           if ( (p_cell->time_out == *time) )
           {
              // TO DO - send r0 = 408
              printf("FR timeout !!!\n");
              /* next cell in FR list */
              tmp_cell = p_cell->timer_next_cell;
              /* and added to WT timer list */
              start_WT_timer( hash_table, p_cell );
           }
           /* the search stops - no further matched found*/
         else
           {
              tmp_cell = 0;
           }


       /* wait timer list */
       for( p_cell = timers[1].first_cell ; p_cell ;  p_cell = tmp_cell )
          /* if the timeout expired - > release the tranzactin*/
        if ( p_cell->time_out == *time)
          {
              printf("WT timeout !!!\n");
              /* next cell in WT list */
              tmp_cell = p_cell->timer_next_cell;
              /* the cell is removed from the WT timer list */
              change_sem( timers[1].sem , -1  );
              timers[1].first_cell = p_cell->timer_next_cell;
              if ( !timers[1].first_cell ) timers[1].last_cell = 0;
              change_sem( timers[1].sem , +1  );
              /* release the trasaction*/
              del_Transaction( hash_table , p_cell );
          }
           /* the search stops - no further matched found*/
          else
          {
             tmp_cell = 0;
          }

    }
}






