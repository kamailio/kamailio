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
   if ( global_msg_id != p_msg->id )
   {
      T = (struct cell*)-1;
      global_msg_id = p_msg->id;
   }

    /* if the transaction is not found yet we are tring to look for it*/
   if ( (int)T==-1 )
      /* if the lookup's result is not 0 means that it's a retransmission */
      if ( t_lookup_request( hash_table , p_msg ) )
         return -1;

   /* creates a new transaction */
   hash_index   = hash( p_msg->callid , p_msg->cseq_nr );
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
   /* tag pointer is NULL */
   if ( p_msg->tag )
      new_cell->transaction.tag  =  &(new_cell->transaction.inbound_request->tag->body);
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
   struct cell      *p_cell;
   struct cell      *tmp_cell;
   unsigned int  hash_index=0;
   unsigned int  isACK = 0;

   /* it's about the same transaction or not?*/
   if ( global_msg_id != p_msg->id )
   {
      T = (struct cell*)-1;
      global_msg_id = p_msg->id;
   }

    /* if  T is previous found -> return found */
   if ( (int)T !=-1 && T )
      return 1;

    /* if T was previous searched and not found -> return not found*/
   if ( !T )
      return 0;

   /* start searching into the table */
   hash_index = hash( p_msg->callid , p_msg->cseq_nr ) ;
   if ( p_msg->first_line.u.request.method_value==METHOD_ACK  )
      isACK = 1;

   /* all the transactions from the entry are compared */
   p_cell     = hash_table->entrys[hash_index].first_cell;
   tmp_cell = 0;
   while( p_cell )
   {
      /* the transaction is referenceted for reading */
      ref_transaction( p_cell );

      /* is it the wanted transaction ? */
      if ( !isACK )
      { /* is not an ACK request */
         /* first only the length are checked */
         if ( /*from length*/ p_cell->transaction.inbound_request->from->body.len == p_msg->from->body.len )
            if ( /*to length*/ p_cell->transaction.inbound_request->to->body.len == p_msg->to->body.len )
                if ( /*tag length*/ (!p_cell->transaction.inbound_request->tag && !p_msg->tag) || (p_cell->transaction.inbound_request->tag && p_msg->tag && p_cell->transaction.inbound_request->tag->body.len == p_msg->tag->body.len) )
                  if ( /*callid length*/ p_cell->transaction.inbound_request->callid->body.len == p_msg->callid->body.len )
                     if ( /*cseq_nr length*/ p_cell->transaction.inbound_request->cseq_nr->body.len == p_msg->cseq_nr->body.len )
                         if ( /*cseq_method length*/ p_cell->transaction.inbound_request->cseq_method->body.len == p_msg->cseq_method->body.len )
                            /* so far the lengths are the same -> let's check the contents */
                            if ( /*from*/ !memcmp( p_cell->transaction.inbound_request->from->body.s , p_msg->from->body.s , p_msg->from->body.len ) )
                               if ( /*to*/ !memcmp( p_cell->transaction.inbound_request->to->body.s , p_msg->to->body.s , p_msg->to->body.len)  )
                                if ( /*tag*/ (!p_cell->transaction.inbound_request->tag && !p_msg->tag) || (p_cell->transaction.inbound_request->tag && p_msg->tag && !memcmp( p_cell->transaction.inbound_request->tag->body.s , p_msg->tag->body.s , p_msg->tag->body.len )) )
                                     if ( /*callid*/ !memcmp( p_cell->transaction.inbound_request->callid->body.s , p_msg->callid->body.s , p_msg->callid->body.len ) )
                                        if ( /*cseq_nr*/ !memcmp( p_cell->transaction.inbound_request->cseq_nr->body.s , p_msg->cseq_nr->body.s , p_msg->cseq_nr->body.len ) )
                                           if ( /*cseq_method*/ !memcmp( p_cell->transaction.inbound_request->cseq_method->body.s , p_msg->cseq_method->body.s , p_msg->cseq_method->body.len ) )
                                              { /* WE FOUND THE GOLDEN EGG !!!! */
                                                 T = p_cell;
                                                 unref_transaction ( p_cell );
                                                 return 1;
                                              }
      }
      else
      { /* it's a ACK request*/
         /* first only the length are checked */
         if ( /*from length*/ p_cell->transaction.inbound_request->from->body.len == p_msg->from->body.len )
            if ( /*to length*/ p_cell->transaction.inbound_request->to->body.len == p_msg->to->body.len )
               if ( /*callid length*/ p_cell->transaction.inbound_request->callid->body.len == p_msg->callid->body.len )
                  if ( /*cseq_nr length*/ p_cell->transaction.inbound_request->cseq_nr->body.len == p_msg->cseq_nr->body.len )
                      if ( /*cseq_method length*/ p_cell->transaction.inbound_request->cseq_method->body.len == 6 /*INVITE*/ )
                         if ( /*tag length*/ p_cell->transaction.tag &&  p_cell->transaction.tag->len==p_msg->tag->body.len )
                            /* so far the lengths are the same -> let's check the contents */
                            if ( /*from*/ !memcmp( p_cell->transaction.inbound_request->from->body.s , p_msg->from->body.s , p_msg->from->body.len ) )
                               if ( /*to*/ !memcmp( p_cell->transaction.inbound_request->to->body.s , p_msg->to->body.s , p_msg->to->body.len)  )
                                  if ( /*tag*/ !memcmp( p_cell->transaction.tag->s , p_msg->tag->body.s , p_msg->tag->body.len ) )
                                     if ( /*callid*/ !memcmp( p_cell->transaction.inbound_request->callid->body.s , p_msg->callid->body.s , p_msg->callid->body.len ) )
                                        if ( /*cseq_nr*/ !memcmp( p_cell->transaction.inbound_request->cseq_nr->body.s , p_msg->cseq_nr->body.s , p_msg->cseq_nr->body.len ) )
                                           if ( /*cseq_method*/ !memcmp( p_cell->transaction.inbound_request->cseq_method->body.s , "INVITE" , 6 ) )
                                              { /* WE FOUND THE GOLDEN EGG !!!! */
                                                 T = p_cell;
                                                 unref_transaction ( p_cell );
                                                 return 1;
                                              }
      } /* end if is ACK or not*/
      /* next transaction */
      tmp_cell = p_cell;
      p_cell = p_cell->next_cell;

      /* the transaction is dereferenceted */
      unref_transaction ( tmp_cell );
   }

   /* no transaction found */
   return 0;
}



/* function returns:
 *       0 - transaction wasn't found
 *       T - transaction found
 */
struct cell* t_lookupOriginalT(  struct s_table* hash_table , struct sip_msg* p_msg )
{
   struct cell      *p_cell;
   struct cell      *tmp_cell;
   unsigned int  hash_index=0;

   /* it's a CANCEL request for sure */

   /* start searching into the table */
   hash_index = hash( p_msg->callid , p_msg->cseq_nr ) ;

   /* all the transactions from the entry are compared */
   p_cell     = hash_table->entrys[hash_index].first_cell;
   tmp_cell = 0;
   while( p_cell )
   {
      /* the transaction is referenceted for reading */
      ref_transaction( p_cell );

      /* is it the wanted transaction ? */
      /* first only the length are checked */
      if ( /*from length*/ p_cell->transaction.inbound_request->from->body.len == p_msg->from->body.len )
         if ( /*to length*/ p_cell->transaction.inbound_request->to->body.len == p_msg->to->body.len )
            if ( /*tag length*/ (!p_cell->transaction.inbound_request->tag && !p_msg->tag) || (p_cell->transaction.inbound_request->tag && p_msg->tag && p_cell->transaction.inbound_request->tag->body.len == p_msg->tag->body.len) )
               if ( /*callid length*/ p_cell->transaction.inbound_request->callid->body.len == p_msg->callid->body.len )
                  if ( /*cseq_nr length*/ p_cell->transaction.inbound_request->cseq_nr->body.len == p_msg->cseq_nr->body.len )
                      if ( /*cseq_method length*/ p_cell->transaction.inbound_request->cseq_method->body.len != 6 /*CANCEL*/ )
                         if ( /*req_uri length*/ p_cell->transaction.inbound_request->first_line.u.request.uri.len == p_msg->first_line.u.request.uri.len )
                             /* so far the lengths are the same -> let's check the contents */
                             if ( /*from*/ !memcmp( p_cell->transaction.inbound_request->from->body.s , p_msg->from->body.s , p_msg->from->body.len ) )
                                if ( /*to*/ !memcmp( p_cell->transaction.inbound_request->to->body.s , p_msg->to->body.s , p_msg->to->body.len)  )
                                   if ( /*tag*/ (!p_cell->transaction.inbound_request->tag && !p_msg->tag) || (p_cell->transaction.inbound_request->tag && p_msg->tag && !memcmp( p_cell->transaction.inbound_request->tag->body.s , p_msg->tag->body.s , p_msg->tag->body.len )) )
                                      if ( /*callid*/ !memcmp( p_cell->transaction.inbound_request->callid->body.s , p_msg->callid->body.s , p_msg->callid->body.len ) )
                                          if ( /*cseq_nr*/ !memcmp( p_cell->transaction.inbound_request->cseq_nr->body.s , p_msg->cseq_nr->body.s , p_msg->cseq_nr->body.len ) )
                                             if ( /*cseq_method*/ strcmp( p_cell->transaction.inbound_request->cseq_method->body.s , "CANCEL" ) )
                                                if ( /*req_uri*/ memcmp( p_cell->transaction.inbound_request->first_line.u.request.uri.s , p_msg->first_line.u.request.uri.s , p_msg->first_line.u.request.uri.len ) )
                                                { /* WE FOUND THE GOLDEN EGG !!!! */
                                                   unref_transaction ( p_cell );
                                                   return p_cell;
                                                }
      /* next transaction */
      tmp_cell = p_cell;
      p_cell = p_cell->next_cell;

      /* the transaction is dereferenceted */
      unref_transaction ( tmp_cell );
   }

   /* no transaction found */
   return 0;


   return 0;
}


/* function returns:
 *       0 - forward successfull
 *      -1 - error during forward
 */
int t_forward( struct s_table* hash_table , struct sip_msg* p_msg , unsigned int dest_ip_param , unsigned int dest_port_param )
{
   /* it's about the same transaction or not? */
   if ( global_msg_id != p_msg->id )
   {
      T = (struct cell*)-1;
      global_msg_id = p_msg->msg_id;
   }

   /* if  T hasn't been previous searched -> search for it */
   if ( (int)T !=-1 )
      t_lookup_request( hash_table , p_msg );

   /*if T hasn't been found after all -> return not found (error) */
   if ( !T )
      return -1;

   /*if it's an ACK and the status is not final or is final, but error the ACK is not forwarded*/
   if ( !memcmp(p_msg->first_line.u.request.method.s,"ACK",3) && (T->transaction.status/100)!=2 )
      return 0;

   /* if it's forwarded for the first time ; else the request is retransmited from the transaction buffer */
   if ( T->transaction.outbound_request[0]==NULL )
   {
      unsigned int dest_ip     = dest_ip_param;
      unsigned int dest_port  = dest_port_param;
      unsigned int len;
     char               *buf;

      /* special case : CANCEL */
      if ( p_msg->first_line.u.request.method_value==METHOD_CANCEL  )
      {
         struct cell  *T2;
         /* find original cancelled transaction; if found, use its next-hops; otherwise use those passed by script */
         T2 = t_lookupOriginalT( hash_table , p_msg );
         /* if found */
         if (T2)
         {  /* if in 1xx status, send to the same destination */
            if ( (T2->transaction.status/100)==1 )
            {
               dest_ip    = T2->transaction.outbound_request[0]->dest_ip;
               dest_port = T2->transaction.outbound_request[0]->dest_port;
            }
            else
               /* transaction exists, but nothing to cancel */
             return 0;
         }
      }/* end special case CANCEL*/

      /* store */
      T->transaction.outbound_request[0]->dest_ip    = dest_ip;
      T->transaction.outbound_request[0]->dest_port = dest_port;
      // buf = build_message( p_mesg , &len );
      // T->transaction.outbound_request[0]->bufflen     = len ;
      // memcpy( T->transaction.outbound_request[0]->buffer , buf , len );
   }/* end for the first time */

   /* send the request */
   // send ( T->transaction.outbound_request.buffer , T->transaction.outbound_request.dest_ip , T->transaction.outbound_request.dest_ip );

}


