#include "h_table.h"

struct cell      *T;
unsigned int  global_msg_id;
struct s_table*  hash_table;


int t_reply_matching( struct s_table* , struct sip_msg* , struct cell** , unsigned int*  );
int t_store_incoming_reply( struct cell* , unsigned int , struct sip_msg* );
int t_relay_reply( struct cell* , unsigned int , struct sip_msg* );
int t_check( struct s_table* , struct sip_msg*  );
int t_all_final( struct cell * );
int relay_lowest_reply_upstream( struct cell *Trans , struct sip_msg *p_msg );
int push_reply_from_uac_to_uas( struct sip_msg * , unsigned int );



void free_cell( struct cell* dead_cell )      // TO DO UPDATE
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
      if ( dead_cell ->outbound_response[i] )
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
	/* Oooops! Error here */
         sh_free( hash_table->entrys );
      }

 	if (hash_table->timers ) {
		/* for each of the timer lists ... */
		for( i=0; i<NR_OF_TIMER_LISTS ; i++ ) {
     				/* ... delete all cells on the timer list */
      				for( p_cell = hash_table->timers[i].first_cell; p_cell; p_cell = tmp_cell )
      				{
          				tmp_cell = p_cell->tl[i].timer_next_cell;
          				remove_timer_from_head( hash_table, p_cell, i );
          				free_cell( p_cell );
				}
				release_timerlist_lock( &(hash_table->timers[i]) );
      		}
		sh_free( hash_table->timers );
	} /* if (hash_table->timers ) */
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

   memset( hash_table, 0, sizeof (struct s_table ) );

   /*inits the time*/
   hash_table->time = 0;


   /* try first allocating all the structures needed for syncing */
   if (lock_initialize()==-1)
	goto error;

   /* allocs the entry's table */
    hash_table->entrys  = sh_malloc( TABLE_ENTRIES * sizeof( struct entry )  );
    if ( !hash_table->entrys )
	goto error;
    /* inits the entrys */
    for(  i=0 ; i<TABLE_ENTRIES; i++ )
    {
       hash_table->entrys[i].first_cell = 0;
       hash_table->entrys[i].last_cell = 0;
       hash_table->entrys[i].next_label = 0;
       init_entry_lock( hash_table , (hash_table->entrys)+i );
    }

   /* allocs the transaction timer's table */
    hash_table->timers  = sh_malloc( NR_OF_TIMER_LISTS * sizeof( struct timer )  );
    if ( !hash_table->timers )
	goto error;
   /* inits the timers*/
    for(  i=0 ; i<NR_OF_TIMER_LISTS ; i++ )
    {
       hash_table->timers[i].first_cell = 0;
       hash_table->timers[i].last_cell = 0;
       //init_timerlist_lock( hash_table, (hash_table->timers)+i );
       init_timerlist_lock( hash_table, i );
    }

    /* allocs the retransmission timer's table */
    hash_table->retr_timers = sh_malloc( NR_OF_RT_LISTS * sizeof (struct timer ) );
    if ( !hash_table->retr_timers )
	goto error;
   /* init the retransmission timers */
   for ( i=0; i<NR_OF_RT_LISTS; i++)
   {
       hash_table->timers[i].first_cell = 0;
       hash_table->timers[i].last_cell = 0;
       //init_retr_timer_lock( hash_table, (hash_table->timers)+i );
       init_retr_timer_lock( hash_table, i );
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
 *       0 - a new transaction was created
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
   hash_index   = hash( p_msg->callid , get_cseq(p_msg)->number );
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
   new_cell->inbound_request =  sip_msg_cloner(p_msg) ;
   /* inbound response is NULL*/
   /* status is 0 */
   /* tag pointer is NULL */
   //if ( p_msg->tag )      TO DO !!!!!!!!!!!!!!!!!!!!!!
   //   new_cell->tag  =  &(new_cell->inbound_request->tag->body);
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
   hash_index = hash( p_msg->callid , get_cseq(p_msg)->number ) ;
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
         if ( /*from length*/ p_cell->inbound_request->from->body.len == p_msg->from->body.len )
            if ( /*to length*/ p_cell->inbound_request->to->body.len == p_msg->to->body.len )
                //if ( /*tag length*/ (!p_cell->inbound_request->tag && !p_msg->tag) || (p_cell->inbound_request->tag && p_msg->tag && p_cell->inbound_request->tag->body.len == p_msg->tag->body.len) )
                  if ( /*callid length*/ p_cell->inbound_request->callid->body.len == p_msg->callid->body.len )
                     if ( /*cseq length*/ p_cell->inbound_request->cseq->body.len == p_msg->cseq->body.len )
                            /* so far the lengths are the same -> let's check the contents */
                            if ( /*from*/ !memcmp( p_cell->inbound_request->from->body.s , p_msg->from->body.s , p_msg->from->body.len ) )
                               if ( /*to*/ !memcmp( p_cell->inbound_request->to->body.s , p_msg->to->body.s , p_msg->to->body.len)  )
                                  //if ( /*tag*/ (!p_cell->inbound_request->tag && !p_msg->tag) || (p_cell->inbound_request->tag && p_msg->tag && !memcmp( p_cell->inbound_request->tag->body.s , p_msg->tag->body.s , p_msg->tag->body.len )) )
                                     if ( /*callid*/ !memcmp( p_cell->inbound_request->callid->body.s , p_msg->callid->body.s , p_msg->callid->body.len ) )
                                        if ( /*cseq*/ !memcmp( p_cell->inbound_request->cseq->body.s , p_msg->cseq->body.s , p_msg->cseq->body.len ) )
                                              { /* WE FOUND THE GOLDEN EGG !!!! */
                                                 T = p_cell;
                                                 unref_transaction ( p_cell );
                                                 return 1;
                                              }
      }
      else
      { /* it's a ACK request*/
         /* first only the length are checked */
         if ( /*from length*/ p_cell->inbound_request->from->body.len == p_msg->from->body.len )
            if ( /*to length*/ p_cell->inbound_request->to->body.len == p_msg->to->body.len )
               if ( /*callid length*/ p_cell->inbound_request->callid->body.len == p_msg->callid->body.len )
                  if ( /*cseq_nr length*/ get_cseq(p_cell->inbound_request)->number.len == get_cseq(p_msg)->number.len )
                      if ( /*cseq_method length*/ get_cseq(p_cell->inbound_request)->method.len == 6 /*INVITE*/ )
                         //if ( /*tag length*/ p_cell->tag &&  p_cell->tag->len==p_msg->tag->body.len )
                            /* so far the lengths are the same -> let's check the contents */
                            if ( /*from*/ !memcmp( p_cell->inbound_request->from->body.s , p_msg->from->body.s , p_msg->from->body.len ) )
                               if ( /*to*/ !memcmp( p_cell->inbound_request->to->body.s , p_msg->to->body.s , p_msg->to->body.len)  )
                                  //if ( /*tag*/ !memcmp( p_cell->tag->s , p_msg->tag->body.s , p_msg->tag->body.len ) )
                                     if ( /*callid*/ !memcmp( p_cell->inbound_request->callid->body.s , p_msg->callid->body.s , p_msg->callid->body.len ) )
                                        if ( /*cseq_nr*/ !memcmp( get_cseq(p_cell->inbound_request)->number.s , get_cseq(p_msg)->number.s , get_cseq(p_msg)->number.len ) )
                                           if ( /*cseq_method*/ !memcmp( get_cseq(p_cell->inbound_request)->method.s , "INVITE" , 6 ) )
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
   T = 0;
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
   hash_index = hash( p_msg->callid , get_cseq(p_msg)->number  ) ;

   /* all the transactions from the entry are compared */
   p_cell     = hash_table->entrys[hash_index].first_cell;
   tmp_cell = 0;
   while( p_cell )
   {
      /* the transaction is referenceted for reading */
      ref_transaction( p_cell );

      /* is it the wanted transaction ? */
      /* first only the length are checked */
      if ( /*from length*/ p_cell->inbound_request->from->body.len == p_msg->from->body.len )
         if ( /*to length*/ p_cell->inbound_request->to->body.len == p_msg->to->body.len )
            //if ( /*tag length*/ (!p_cell->inbound_request->tag && !p_msg->tag) || (p_cell->inbound_request->tag && p_msg->tag && p_cell->inbound_request->tag->body.len == p_msg->tag->body.len) )
               if ( /*callid length*/ p_cell->inbound_request->callid->body.len == p_msg->callid->body.len )
                  if ( /*cseq_nr length*/ get_cseq(p_cell->inbound_request)->number.len == get_cseq(p_msg)->number.len )
                      //if ( /*cseq_method length*/ p_cell->inbound_request->cseq_method->body.len != 6 /*CANCEL*/ )
                         if ( /*req_uri length*/ p_cell->inbound_request->first_line.u.request.uri.len == p_msg->first_line.u.request.uri.len )
                             /* so far the lengths are the same -> let's check the contents */
                             if ( /*from*/ !memcmp( p_cell->inbound_request->from->body.s , p_msg->from->body.s , p_msg->from->body.len ) )
                                if ( /*to*/ !memcmp( p_cell->inbound_request->to->body.s , p_msg->to->body.s , p_msg->to->body.len)  )
                                   //if ( /*tag*/ (!p_cell->inbound_request->tag && !p_msg->tag) || (p_cell->inbound_request->tag && p_msg->tag && !memcmp( p_cell->inbound_request->tag->body.s , p_msg->tag->body.s , p_msg->tag->body.len )) )
                                      if ( /*callid*/ !memcmp( p_cell->inbound_request->callid->body.s , p_msg->callid->body.s , p_msg->callid->body.len ) )
                                          if ( /*cseq_nr*/ !memcmp( get_cseq(p_cell->inbound_request)->number.s , get_cseq(p_msg)->number.s , get_cseq(p_msg)->number.len ) )
                                             if ( /*cseq_method*/ strcmp( get_cseq(p_cell->inbound_request)->method.s , "CANCEL" ) )
                                                if ( /*req_uri*/ memcmp( p_cell->inbound_request->first_line.u.request.uri.s , p_msg->first_line.u.request.uri.s , p_msg->first_line.u.request.uri.len ) )
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
   T = 0;
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
      global_msg_id = p_msg->id;
   }

   /* if  T hasn't been previous searched -> search for it */
   if ( (int)T !=-1 )
      t_lookup_request( hash_table , p_msg );

   /*if T hasn't been found after all -> return not found (error) */
   if ( !T )
      return -1;

   /*if it's an ACK and the status is not final or is final, but error the ACK is not forwarded*/
   if ( !memcmp(p_msg->first_line.u.request.method.s,"ACK",3) && (T->status/100)!=2 )
      return 0;

   /* if it's forwarded for the first time ; else the request is retransmited from the transaction buffer */
   if ( T->outbound_request[0]==NULL )
   {
      unsigned int dest_ip     = dest_ip_param;
      unsigned int dest_port  = dest_port_param;
      unsigned int len;
      char              *buf;

      /* allocates a new retrans_buff for the outbound request */
      T->outbound_request[0] = (struct retrans_buff*)sh_malloc( sizeof(struct retrans_buff) );
      T->nr_of_outgoings = 1;

      /* special case : CANCEL */
      if ( p_msg->first_line.u.request.method_value==METHOD_CANCEL  )
      {
         struct cell  *T2;
         /* find original cancelled transaction; if found, use its next-hops; otherwise use those passed by script */
         T2 = t_lookupOriginalT( hash_table , p_msg );
         /* if found */
         if (T2)
         {  /* if in 1xx status, send to the same destination */
            if ( (T2->status/100)==1 )
            {
               dest_ip    = T2->outbound_request[0]->dest_ip;
               dest_port = T2->outbound_request[0]->dest_port;
            }
            else
               /* transaction exists, but nothing to cancel */
             return 0;
         }
      }/* end special case CANCEL*/

      /* store */
      T->outbound_request[0]->dest_ip    = dest_ip;
      T->outbound_request[0]->dest_port = dest_port;
      // buf = build_message( p_mesg , &len );
      // T->outbound_request[0]->bufflen     = len ;
      // memcpy( T->outbound_request[0]->buffer , buf , len );
   }/* end for the first time */

   /* send the request */
   // send ( T->outbound_request.buffer , T->outbound_request.dest_ip , T->outbound_request.dest_ip );

}





/*  This function is called whenever a reply for our module is received; we need to register
  *  this function on module initialization;
  *  Returns :   1 - core router stops
  *                    0 - core router relay statelessly
  */
int t_on_reply_received( struct s_table  *hash_table , struct sip_msg  *p_msg )
{
   unsigned int  branch;

   global_msg_id = p_msg->id;

   /* we use label-matching to lookup for T */
   t_reply_matching( hash_table , p_msg , &T , &branch  );

   /* if no T found ->tell the core router to forward statelessly */
   if ( !T )
      return 0;

   /* on a non-200 reply to INVITE, generate local ACK and stop retransmission of the INVITE */
   if ( T->inbound_request->first_line.u.request.method_value==METHOD_INVITE && p_msg->first_line.u.reply.statusclass>2 )
   {
      // sendACK   TO DO !!!!!!!!!!
      // remove_from_retransmission_list( T->outbound_request[branch] );     TO DO !!!!!
      t_store_incoming_reply( T , branch , p_msg );
   }

   #ifdef FORKING
   /* skipped for the moment*/
   #endif

   /*let's check the current inbound response status (is final or not) */
   if ( T->inbound_response && (T->status/100)>1 )
   {   /*a final reply was already sent upstream */
       /* alway relay 2xx immediately ; drop anything else */
      if ( p_msg->first_line.u.reply.statusclass==2 )
          t_relay_reply( T , branch , p_msg );
       /* nothing to do for the ser core */
      return 1;
   }
   else
   {  /* no reply sent yet or sent but not final*/

      /* stops the request's retransmission*/
      // remove_from_retransmission_list( T->outbound_request[branch] );     TO DO !!!!!
      /* restart retransmission if provisional response came for a non_INVITE -> retrasmit at RT_T2*/
      if ( p_msg->first_line.u.reply.statusclass==1 && T->inbound_request->first_line.u.request.method_value!=METHOD_INVITE )
         /*put_in_retransmission_list( T->trasaction.outbound_request[branch] , RT_T2 ) TO DO !!!!!!!*/ ;


      /* relay ringing and OK immediately */
      if ( p_msg->first_line.u.reply.statusclass ==1 || p_msg->first_line.u.reply.statusclass ==2  )
      {
           if ( p_msg->first_line.u.reply.statuscode > T->status )
              t_relay_reply( T , branch , p_msg );
         return 1;
      }

      /* error final responses are only stored */
      if ( p_msg->first_line.u.reply.statusclass>=3 && p_msg->first_line.u.reply.statusclass<=5 )
      {
         t_store_incoming_reply( T , branch , p_msg );
         if ( t_all_final(T) )
              relay_lowest_reply_upstream( T , p_msg );
         /* nothing to do for the ser core */
         return 1;
      }

      /* global failure responses relay immediately */
     if ( p_msg->first_line.u.reply.statusclass==6 )
      {
         t_relay_reply( T , branch , p_msg );
         /* nothing to do for the ser core */
         return 1;
      }
   }
}




/* Retransmits the last sent inbound reply.
  * Returns  -1 -error
  *                0 - OK
  */
int t_retransmit_reply( struct s_table *hash_table , struct sip_msg* p_msg )
{
   t_check( hash_table, p_msg );

   /* if no transaction exists or no reply to be resend -> out */
   if ( T  && T->inbound_response )
   {
       //sendto(  );   TO DO !!!!!!
      return 0;
   }

   return -1;
}




/* ----------------------------HELPER FUNCTIONS-------------------------------- */


/* Returns 0 - nothing found
  *              1  - T found
  */
int t_reply_matching( struct s_table *hash_table , struct sip_msg *p_msg , struct cell **p_Trans , unsigned int *p_branch )
{
   struct cell*  p_cell;
   struct cell* tmp_cell;
   unsigned int hash_index = 0;
   unsigned int entry_label  = 0;
   unsigned int branch_id    = 0;

   /* getting the hash_index from the brach param , via header*/
   // hash_index = get_hash_index( p_msg );   TO DO !!!!

   /*if the hash index is corect */
   if  ( hash_index>=0 && hash_index<TABLE_ENTRIES-1 )
   {
      /* getting the entry label value */
      // entry_label =  get_entry_label( p_msg );   TO DO !!!!
      /* if the entry label also is corect */
      if  ( entry_label>=0 )
      {
         /* getting the branch_id value */
         // entry_label =  get_branch_id( p_msg );   TO DO !!!!
         /* if the entry label also is corect */
          if  ( branch_id>=0 )
          {
             /*all the cells from the entry are scan to detect an entry_label matching */
             p_cell     = hash_table->entrys[hash_index].first_cell;
             tmp_cell = 0;
            while( p_cell )
             {
                /* the transaction is referenceted for reading */
                ref_Transaction( p_cell );
                /* is it the cell with the wanted entry_label? */
                if ( p_cell->label = entry_label )
                   /* has the transaction the wanted branch? */
                   if ( p_cell->nr_of_outgoings>branch_id && p_cell->outbound_request[branch_id] )
                    {/* WE FOUND THE GOLDEN EGG !!!! */
                       p_Trans = &p_cell;
                       *p_branch = branch_id;
                       unref_Transaction( p_cell );
                       return 1;
                    }

               /* next cell */
               tmp_cell = p_cell;
               p_cell = p_cell->next_cell;

               /* the transaction is dereferenceted */
               unref_Transaction( tmp_cell );
             }
          }
      }
   }

   /* nothing found */
   *p_branch = -1;
   return 0;
}




/* We like this incoming reply, so, let's store it, we'll decide
  * later what to d with that
  */
int t_store_incoming_reply( struct cell* Trans, unsigned int branch, struct sip_msg* p_msg )
{
   /* if there is a previous reply, replace it */
   if ( Trans->outbound_response[branch] )
      free_sip_msg( Trans->outbound_response[branch] );
   Trans->outbound_response[branch] = sip_msg_cloner( p_msg );
}




/*  We like this incoming reply and we want ot store it and
  *  to relay it upstream
  */
int t_relay_reply( struct cell* Trans, unsigned int branch, struct sip_msg* p_msg )
{
   t_store_incoming_reply( Trans , branch, p_msg );
   push_reply_from_uac_to_uas( p_msg , branch );
}




/* Functions update T (T gets either a valid pointer in it or it equals zero) if no transaction
  * for current message exists;
  * Returns 1 if T was modified or 0 if not.
  */
int t_check( struct s_table *hash_table , struct sip_msg* p_msg )
{
   unsigned int branch;

   /* is T still up-to-date ? */
   if ( p_msg->id != global_msg_id || (int)T==-1 )
   {
      global_msg_id = p_msg->id;
      /* transaction lookup */
     if ( p_msg->first_line.type=SIP_REQUEST )
         t_lookup_request(  hash_table , p_msg );
      else
         t_reply_matching( hash_table , p_msg , &T , &branch );

      return 1;
   }

   return 0;
}




/*  Checks if all the transaction's outbound request has a final response.
  *  Return   1 - all are final
  *                0 - some waitting
  */
int t_all_final( struct cell *Trans )
{
   unsigned int i;

   for( i=0 ; i<Trans->nr_of_outgoings ; i++  )
      if (  !Trans->outbound_response[i] || (Trans->outbound_response[i]) && Trans->outbound_response[i]->first_line.u.reply.statuscode<200 )
         return 0;

   return 1;
}




/* Picks the lowest code reply and send it upstream.
  *  Returns -1 if no lowest find reply found (all provisional)
  */
int relay_lowest_reply_upstream( struct cell *Trans , struct sip_msg *p_msg )
{
   unsigned int i            =0 ;
   unsigned int lowest_i = -1;
   int                 lowest_v = 999;

   for(  ; i<T->nr_of_outgoings ; i++ )
      if ( T->outbound_response[i] && T->outbound_response[i]->first_line.u.reply.statuscode>=200 && T->outbound_response[i]->first_line.u.reply.statuscode<lowest_v )
      {
         lowest_i =i;
         lowest_v = T->outbound_response[i]->first_line.u.reply.statuscode;
      }

   if ( lowest_i != -1 )
      push_reply_from_uac_to_uas( p_msg ,lowest_i );

   return lowest_i;
}




/* Push a previously stored reply from UA Client to UA Server
  * and send it out
  */
int push_reply_from_uac_to_uas( struct sip_msg *p_msg , unsigned int branch )
{
   /* if there is a reply, release the buffer (everything else stays same) */
   if ( T->inbound_response )
   {
      sh_free( T->inbound_response->buffer );
      //release_retransmision.... ????
   }


}


