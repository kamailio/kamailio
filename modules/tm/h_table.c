#include "h_table.h"

int table_entries;


void free_cell( struct cell* dead_cell )
{
   int i;

   /* inbound_request */
   sh_free( dead_cell->inbound_request->orig );
   sh_free( dead_cell->inbound_request->buf );
   sh_free( dead_cell->inbound_request );
   /* inbound_response */
   if ( dead_cell->inbound_response )
   {
      sh_free( dead_cell->inbound_response->buffer );
      sh_free( dead_cell->inbound_response );
   }

   for ( i =0 ; i<dead_cell->nr_of_outgoings;  i++ )
   {
      /* outbound requests*/
      sh_free( dead_cell->outbound_request[i]->buffer );
      sh_free( dead_cell->outbound_request[i] );
      /* outbound requests*/
      if ( dead_cell -> outbound_response[i] )
      {
         sh_free( dead_cell->outbound_response[i]->orig );
         sh_free( dead_cell->outbound_response[i]->buf );
         sh_free( dead_cell->outbound_response[i] );
      }
   }
   /* mutex */
   release_cell_lock( dead_cell );
   sh_free( dead_cell );
}


void free_hash_table( struct s_table *hash_table )
{
   struct cell* p_cell;
   struct cell* tmp_cell;
   int   i;

   if (hash_table)
   {
      if ( hash_table->entrys )
      {
         /* remove the hash table entry by entry */
         for( i = 0 ; i<table_entries; i++)
         {
             release_entry_lock( (hash_table->entrys)+i );
             /* delete all synonyms at hash-collision-slot i */
              for( p_cell = hash_table->entrys[i].first_cell; p_cell; p_cell = tmp_cell )
              {
                tmp_cell = p_cell->next_cell;
                free_cell( p_cell );
              }
         }
         sh_free( hash_table->entrys );
      }

     /* delete all cells on the to-be-deleted list */
      for( p_cell = hash_table->timers[DELETE_LIST].first_cell; p_cell; p_cell = tmp_cell )
      {
          tmp_cell = p_cell->tl[DELETE_LIST].timer_next_cell;
          //remove_timer_from_head( hash_table, p_cell, DELETE_LIST );
          free_cell( p_cell );
      }

     /* delete all the hash table's timers*/
      if ( hash_table->timers)
         sh_free( hash_table->timers );

      sh_free( hash_table );
   }
}



struct s_table* init_hash_table()
{
   struct s_table*  hash_table;
   int       i;

   /*allocs the table*/
   hash_table = sh_malloc(  sizeof( struct s_table ) );
   if ( !hash_table )
   {
       free_hash_table( hash_table );
      return 0;
   }

   /*inits the time*/
   hash_table->time = 0;

   /* allocs the entry's table */
    table_entries = TABLE_ENTRIES;
    hash_table->entrys  = sh_malloc( table_entries * sizeof( struct entry )  );
    if ( !hash_table->entrys )
    {
        free_hash_table( hash_table );
       return 0;
    }

   /* allocs the timer's table */
    hash_table->timers  = sh_malloc( NR_OF_TIMER_LISTS * sizeof( struct timer )  );
    if ( !hash_table->timers )
    {
        free_hash_table( hash_table );
       return 0;
    }

    /* inits the entrys */
    for(  i=0 ; i<table_entries; i++ )
    {
       hash_table->entrys[i].first_cell = 0;
       hash_table->entrys[i].last_cell = 0;
       hash_table->entrys[i].next_label = 0;
       init_entry_lock( hash_table , (hash_table->entrys)+i );
    }

   /* inits the timers*/
    for(  i=0 ; i<NR_OF_TIMER_LISTS ; i++ )
    {
       hash_table->timers[i].first_cell = 0;
       hash_table->timers[i].last_cell = 0;
       //init_timerlist_lock( hash_table, (hash_table->timers)+i );
    }

#ifdef THREAD
   /* starts the timer thread/ process */
   pthread_create( &(hash_table->timer_thread_id), NULL, timer_routine, hash_table );
#endif

   return  hash_table;
}



/* function returns:
 *       0 - a new transaction was created -> the proxy core don't have to release the p_msg structure
 *      -1 - retransmission
 *      -2 - error
 */
int t_add_transaction( struct s_table* hash_table , struct sip_msg* p_msg )
{
   struct cell*    new_cell;
   struct entry* match_entry;
   int                  hash_index;


    /* if the transaction is not found yet we are tring to look for it*/
   if ( (int)T==-1 )
      T = t_lookup_request( hash_table , p_msg );

   /* if T is not 0 means that it's a retransmission */
   if ( T )
      return -1;

   /* creates a new transaction */
   hash_index   = hash( p_msg->call_id , p_msg->cseq_nr );
   match_entry = &hash_table->entrys[hash_index];

   new_cell = sh_malloc( sizeof( struct cell ) );
   if  ( !new_cell )
      return -2;

   /* filling with 0 */
   memset( new_cell, 0, sizeof( struct cell ) );
   /* hash index of the entry */
   new_cell->hash_index = hash_index;
   /* mutex */
   init_cell_lock(  new_cell );
   /* ref counter is 0 */
   /* all pointers from timers list tl are NULL */

   /* inbound request */
   new_cell->inbound_request = p_msg;
   /* inbound response is NULL*/
   /* status is 0 */
   /* nr of outbound requests is 0 */
   /* all pointers from outbound_request array are NULL */
   /* all pointers from outbound_response array are NULL */


   /* critical region - inserting the cell at the end of the list */
   lock( match_entry->mutex );
   new_cell->label = match_entry->next_label++;
   if ( match_entry->last_cell )
   {
      match_entry->last_cell->next_cell = new_cell;
      new_cell->prev_cell = match_entry->last_cell;
   }
   else
      match_entry->first_cell = new_cell;
   match_entry->last_cell = new_cell;
   unlock( match_entry->mutex );

   T = new_cell;
   return 0;
}









