#include "h_table.h"
#include "../../dprint.h"


/*   Frees the all the containes of a cell and the cell's body itself
  */
void free_cell( struct cell* dead_cell )
{
   int i;

   /* UA Server */ 
   if ( dead_cell->inbound_request )
      sip_msg_free( dead_cell->inbound_request );
   if ( dead_cell->outbound_response )
   {
      sh_free( dead_cell->outbound_response->retr_buffer );
      sh_free( dead_cell->outbound_response );
   }

  /* UA Clients */
   for ( i =0 ; i<dead_cell->nr_of_outgoings;  i++ )
   {
      /* outbound requests*/
      if ( dead_cell->outbound_request[i] )
      {
         sh_free( dead_cell->outbound_request[i]->retr_buffer );
         sh_free( dead_cell->outbound_request[i] );
      }
      /* outbound requests*/
      if ( dead_cell -> inbound_response[i] )
         sip_msg_free( dead_cell->inbound_response[i] );
   }
   /* mutex */
   release_cell_lock( dead_cell );
   /* the cell's body */
   sh_free( dead_cell );
}




/* Release all the data contained by the hash table. All the aux. structures
  *  as sems, lists, etc, are also released
  */
void free_hash_table( struct s_table *hash_table )
{
   struct cell* p_cell;
   struct cell* tmp_cell;
   struct timer_link *tl;
   int   i;

   if (hash_table)
   {
      /* remove the data contained by each entry */
      for( i = 0 ; i<TABLE_ENTRIES; i++)
      {
         release_entry_lock( (hash_table->entrys)+i );
         /* delete all synonyms at hash-collision-slot i */
         for( p_cell = hash_table->entrys[i].first_cell; p_cell; p_cell = tmp_cell )
         {
            tmp_cell = p_cell->next_cell;
            free_cell( p_cell );
         }
      }

      /* deletes all cells from DELETE_LIST list (they are no more accessible from enrys) */
      while( (tl=remove_from_timer_list_from_head( hash_table, DELETE_LIST ))!=0 )
         free_cell( p_cell ) ;

      /* the mutexs for sync the lists are released*/
      for ( i=0 ; i<NR_OF_TIMER_LISTS ; i++ )
         release_timerlist_lock( &(hash_table->timers[i]) );

      sh_free( hash_table );
   }
}




/*
  */
struct s_table* init_hash_table()
{
   struct s_table*  hash_table;
   pthread_t *thread;
   int       i;

   /*allocs the table*/
   hash_table = sh_malloc(  sizeof( struct s_table ) );
   if ( !hash_table )
      goto error;

   memset( hash_table, 0, sizeof (struct s_table ) );

   /* try first allocating all the structures needed for syncing */
   if (lock_initialize()==-1)
     goto error;

   /* inits the entrys */
   for(  i=0 ; i<TABLE_ENTRIES; i++ )
      init_entry_lock( hash_table , (hash_table->entrys)+i );

   /* inits the timers*/
   for(  i=0 ; i<NR_OF_TIMER_LISTS ; i++ )
      init_timerlist_lock( hash_table, i );

#ifdef THREAD
   /* starts the timer thread/ process */
   pthread_create( thread, NULL, timer_routine, hash_table );
#endif

   return  hash_table;

error:
   free_hash_table( hash_table );
   lock_cleanup();
   return 0;
}



struct cell*  build_cell( struct sip_msg* p_msg )
{
   struct cell*  new_cell;
   int                i;

    /* do we have the source for the build process? */
   if (!p_msg)
      return 0;

   /* allocs a new cell */
   new_cell = sh_malloc( sizeof( struct cell ) );
   if  ( !new_cell )
      return 0;

   /* filling with 0 */
   memset( new_cell, 0, sizeof( struct cell ) );
   /* hash index of the entry */
   new_cell->hash_index = hash( p_msg->callid->body , get_cseq(p_msg)->number );
   /* mutex */
   init_cell_lock(  new_cell );
   /* ref counter is 0 */
   /* all pointers from timers list tl are NULL */
   new_cell->wait_tl.payload = new_cell;
   new_cell->dele_tl.payload = new_cell;

   /* inbound request */
   /* force parsing all the needed headers*/
   parse_headers(p_msg, HDR_VIA|HDR_TO|HDR_FROM|HDR_CALLID|HDR_CSEQ );
   new_cell->inbound_request =  sip_msg_cloner(p_msg) ;
   /* inbound response is NULL*/
   /* status is 0 */
   /* tag pointer is NULL */
   //if ( p_msg->tag )      TO DO !!!!!!!!!!!!!!!!!!!!!!
   //   new_cell->tag  =  &(new_cell->inbound_request->tag->body);
   /* nr of outbound requests is 0 */
   /* all pointers from outbound_request array are NULL */
   /* all pointers from outbound_response array are NULL */

   return new_cell;
}




/*  Takes an already created cell and links it into hash table on the
  *  appropiate entry.
  */
void    insert_into_hash_table( struct s_table *hash_table,  struct cell * p_cell )
{
   struct entry* p_entry;

   /* do we have or not something to insert? */
   if (!p_cell)
      return;

   /* locates the apropiate entry */
   p_entry = &hash_table->entrys[ p_cell->hash_index ];

   /* critical region - inserting the cell at the end of the list */
   lock( p_entry->mutex );

   p_cell->label = p_entry->next_label++;
   if ( p_entry->last_cell )
   {
      p_entry->last_cell->next_cell = p_cell;
      p_cell->prev_cell = p_entry->last_cell;
   }
   else
      p_entry->first_cell = p_cell;
   p_entry->last_cell = p_cell;

   unlock( p_entry->mutex );
}




/*  Un-link a  cell from hash_table, but the cell itself is not released
  */
void remove_from_hash_table( struct s_table *hash_table,  struct cell * p_cell )
{
   struct entry*  p_entry  = &(hash_table->entrys[p_cell->hash_index]);

   lock( p_entry->mutex );

   if ( p_cell->prev_cell )
      p_cell->prev_cell->next_cell = p_cell->next_cell;
   else
      p_entry->first_cell = p_cell->next_cell;

   if ( p_cell->next_cell )
      p_cell->next_cell->prev_cell = p_cell->prev_cell;
   else
      p_entry->last_cell = p_cell->prev_cell;

   unlock( p_entry->mutex );
}


void ref_cell( struct cell* p_cell)
{
   lock( p_cell->mutex );
   p_cell->ref_counter++;
   unlock( p_cell->mutex );
}


void unref_cell( struct cell* p_cell)
{
   lock( p_cell->mutex );
   p_cell->ref_counter--;
   unlock( p_cell->mutex );
}




