#include "h_table.h"

struct cell      *T;
unsigned int  global_msg_id;
struct s_table*  hash_table;

void free_cell( struct cell* dead_cell )
{
   int i;

   /* inbound_request */
   sh_free( dead_cell->transaction.inbound_request->orig );
   sh_free( dead_cell->transaction.inbound_request->buf );
   sh_free( dead_cell->transaction.inbound_request );
   /* inbound_response */
   if ( dead_cell->transaction.inbound_response )
   {
      sh_free( dead_cell->transaction.inbound_response->buffer );
      sh_free( dead_cell->transaction.inbound_response );
   }

   for ( i =0 ; i<dead_cell->transaction.nr_of_outgoings;  i++ )
   {
      /* outbound requests*/
      sh_free( dead_cell->transaction.outbound_request[i]->buffer );
      sh_free( dead_cell->transaction.outbound_request[i] );
      /* outbound requests*/
      if ( dead_cell -> transaction.outbound_response[i] )
      {
         sh_free( dead_cell->transaction.outbound_response[i]->orig );
         sh_free( dead_cell->transaction.outbound_response[i]->buf );
         sh_free( dead_cell->transaction.outbound_response[i] );
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
         sh_free( hash_table->entrys );
      }

     /* delete all cells on the to-be-deleted list */
      for( p_cell = hash_table->timers[DELETE_LIST].first_cell; p_cell; p_cell = tmp_cell )
      {
          tmp_cell = p_cell->transaction.tl[DELETE_LIST].timer_next_cell;
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
	goto error;

   /*inits the time*/
   hash_table->time = 0;


   /* try first allocating all the structures needed for syncing */
   if (lock_initialize()==-1)
	goto error;

   /* allocs the entry's table */
    hash_table->entrys  = sh_malloc( TABLE_ENTRIES * sizeof( struct entry )  );
    if ( !hash_table->entrys )
	goto error;

   /* allocs the transaction timer's table */
    hash_table->timers  = sh_malloc( NR_OF_TIMER_LISTS * sizeof( struct timer )  );
    if ( !hash_table->timers )
	goto error;

    /* allocs the retransmission timer's table */
    hash_table->retr_timers = sh_malloc( NR_OF_RT_LISTS * sizeof (struct timer ) );
    if ( !hash_table->retr_timers )
	goto error;

    /* inits the entrys */
    for(  i=0 ; i<TABLE_ENTRIES; i++ )
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

   /* init the retransmission timers */
   for ( i=0; i<NR_OF_RT_LISTS; i++)
   {
       hash_table->timers[i].first_cell = 0;
       hash_table->timers[i].last_cell = 0;
       //init_retr_timer_lock( hash_table, (hash_table->timers)+i );
   }

#ifdef THREAD
   /* starts the timer thread/ process */
   pthread_create( &(hash_table->timer_thread_id), NULL, timer_routine, hash_table );
#endif

   return  hash_table;

error:
   free_hash_table( hash_table );
   lock_cleanup();
   return 0;
}



void ref_transaction( struct cell* p_cell)
{
   lock( p_cell->mutex );
   p_cell->ref_counter++;
   unlock( p_cell->mutex );
}


void unref_transaction( struct cell* p_cell)
{
   lock( p_cell->mutex );
   p_cell->ref_counter--;
   unlock( p_cell->mutex );
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

   /* it's about the same transaction or not?*/
   if ( global_msg_id != p_msg->msg_id )
   {
      T = (struct cell*)-1;
      global_msg_id = p_msg->msg_id;
   }

    /* if the transaction is not found yet we are tring to look for it*/
   if ( (int)T==-1 )
      /* if the lookup's result is not 0 means that it's a retransmission */
      if ( t_lookup_request( hash_table , p_msg ) )
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
   new_cell->transaction.hash_index = hash_index;
   /* mutex */
   init_cell_lock(  new_cell );
   /* ref counter is 0 */
   /* all pointers from timers list tl are NULL */

   /* inbound request */
   new_cell->transaction.inbound_request = p_msg;
   /* inbound response is NULL*/
   /* status is 0 */
   /* nr of outbound requests is 0 */
   /* all pointers from outbound_request array are NULL */
   /* all pointers from outbound_response array are NULL */


   /* critical region - inserting the cell at the end of the list */
   lock( match_entry->mutex );
   new_cell->transaction.label = match_entry->next_label++;
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


/* function returns:
 *       0 - transaction wasn't found
 *       1 - transaction found
 */
int t_lookup_request(  struct s_table* hash_table , struct sip_msg* p_msg )
{
   struct cell*  p_cell;
   struct cell* tmp_cell;
   int                hash_index=0;

   /* it's about the same transaction or not?*/
   if ( global_msg_id != p_msg->msg_id )
   {
      T = (struct cell*)-1;
      global_msg_id = p_msg->msg_id;
   }

    /* if  T is previous found -> return found */
   if ( (int)T !=-1 && T )
      return 1;

    /* if T was previous searched and not found -> return not found*/
   if ( !T )
      return 0;

   /* start searching into the table */
   hash_index = hash( p_msg->call_id , p_msg->cseq_nr ) ;

   /* all the transactions from the entry are compared */
   p_cell     = hash_table->entrys[hash_index].first_cell;
   tmp_cell = 0;
   while( p_cell )
   {
      /* the transaction is referenceted for reading */
      ref_transaction( p_cell );

      /* is the wanted transaction ? */
     // if ( p_cell->from_length == from_len && p_cell->to_length == to_len && p_cell->req_tag_length == tag_len && p_cell->call_id_length == call_id_len && p_cell->cseq_nr_length == cseq_nr_len && p_cell->cseq_method_length == cseq_method_len  )
      //   if ( !strcmp(p_cell->from,from) && !strcmp(p_cell->to,to) && !strncmp(p_cell->req_tag,tag,tag_len) && !strcmp(p_cell->call_id,call_id) && !strcmp(p_cell->cseq_nr,cseq_nr) && !strcmp(p_cell->cseq_method,cseq_method)  )
       //       return p_cell;
      /* next transaction */
      tmp_cell = p_cell;
      p_cell = p_cell->next_cell;

      /* the transaction is dereferenceted */
      unref_transaction ( tmp_cell );
   }

   /* no transaction found */
   return 0;
}
