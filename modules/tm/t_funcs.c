#include "t_funcs.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser_f.h"

struct cell         *T;
unsigned int     global_msg_id;
struct s_table*  hash_table;


int tm_startup()
{
   /* building the hash table*/
   hash_table = init_hash_table();
   if (!hash_table)
      return -1;

   /* installing handlers for timers */
   hash_table->timers[RETRASMISSIONS_LIST].timeout_handler = retransmission_handler;
   hash_table->timers[FR_TIMER_LIST].timeout_handler              = final_response_handler;
   hash_table->timers[WT_TIMER_LIST].timeout_handler             = wait_handler;
   hash_table->timers[DELETE_LIST].timeout_handler                 = delete_handler;

   /*first msg id*/
   global_msg_id = 0;
   T = (struct cell*)-1;

   return 0;
}




int tm_shutdown()
{
    /* destroy the hash table */
    free_hash_table( hash_table );

   return 0;
}




/* function returns:
 *       1 - a new transaction was created
 *      -1 - error, including retransmission
 */
int t_add_transaction( struct sip_msg* p_msg, char* foo, char* bar )
{
   struct cell*    new_cell;

   DBG("DEBUG: t_add_transaction: adding......\n");
   /* it's about the same transaction or not?*/
   if ( global_msg_id != p_msg->id )
   {
      T = (struct cell*)-1;
      global_msg_id = p_msg->id;
   }

    /* if the transaction is not found yet we are tring to look for it*/
   if ( (int)T==-1 )
      /* if the lookup's result is not 0 means that it's a retransmission */
      if ( t_lookup_request( p_msg, foo, bar ) )
      {
         DBG("DEBUG: t_add_transaction: won't add a retransmission\n");
         return -1;
      }

   /* creates a new transaction */
   new_cell = build_cell( p_msg ) ;
   if  ( !new_cell )
      return -1;
   insert_into_hash_table( hash_table , new_cell );
   DBG("DEBUG: t_add_transaction: new transaction inserted, hash: %d\n", new_cell->hash_index );

   T = new_cell;
   return 1;
}




/* function returns:
 *      -1 - transaction wasn't found
 *       1  - transaction found
 */
int t_lookup_request( struct sip_msg* p_msg, char* foo, char* bar  )
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
   if ( (int)T !=-1 && T )	{
      DBG("DEBUG: t_lookup_request: T already exists\n");
      return 1;
   }

    /* if T was previous searched and not found -> return not found*/
   if ( !T )	{
	DBG("DEBUG: t_lookup_request: T previously sought and not found\n");
      return -1;
   }

   DBG("t_lookup_request: start searching\n");
   /* parse all*/
   if (check_transaction_quadruple(p_msg)==0) {
	   LOG(L_ERR, "ERROR: TM module: t_lookup_request: too few headers\n");
	   T=0;
	   return -1;
   }
   /* start searching into the table */
   hash_index = hash( p_msg->callid->body , get_cseq(p_msg)->number ) ;
   DBG("hash_index=%d\n", hash_index);
   if ( p_msg->first_line.u.request.method_value==METHOD_ACK  )
      isACK = 1;
   DBG("t_lookup_request: 1.continue searching\n");

   /* all the transactions from the entry are compared */
   p_cell     = hash_table->entrys[hash_index].first_cell;
   tmp_cell = 0;
   while( p_cell )
   {
      /* the transaction is referenceted for reading */
      ref_cell( p_cell );

      /* is it the wanted transaction ? */
      if ( !isACK )
      { /* is not an ACK request */
         /* first only the length are checked */
         if ( /*from length*/ p_cell->inbound_request->from->body.len == p_msg->from->body.len )
            if ( /*to length*/ p_cell->inbound_request->to->body.len == p_msg->to->body.len )
               if ( /*callid length*/ p_cell->inbound_request->callid->body.len == p_msg->callid->body.len )
                  if ( /*cseq length*/ p_cell->inbound_request->cseq->body.len == p_msg->cseq->body.len )
                     /* so far the lengths are the same -> let's check the contents */
                        if ( /*from*/ !memcmp( p_cell->inbound_request->from->body.s , p_msg->from->body.s , p_msg->from->body.len ) )
                           if ( /*to*/ !memcmp( p_cell->inbound_request->to->body.s , p_msg->to->body.s , p_msg->to->body.len)  )
                               if ( /*callid*/ !memcmp( p_cell->inbound_request->callid->body.s , p_msg->callid->body.s , p_msg->callid->body.len ) )
                                  if ( /*cseq*/ !memcmp( p_cell->inbound_request->cseq->body.s , p_msg->cseq->body.s , p_msg->cseq->body.len ) )
                                     { /* WE FOUND THE GOLDEN EGG !!!! */
                                        DBG("DEBUG: t_lookup_request: non-ACK found\n");
                                        T = p_cell;
                                        unref_cell( p_cell );
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
                      if ( /*cseq_method type*/ p_cell->inbound_request->first_line.u.request.method_value == METHOD_INVITE  )
                         //if ( /*tag length*/ p_cell->tag &&  p_cell->tag->len==p_msg->tag->body.len )
                            /* so far the lengths are the same -> let's check the contents */
                            if ( /*from*/ !memcmp( p_cell->inbound_request->from->body.s , p_msg->from->body.s , p_msg->from->body.len ) )
                               if ( /*to*/ !memcmp( p_cell->inbound_request->to->body.s , p_msg->to->body.s , p_msg->to->body.len)  )
                                  //if ( /*tag*/ !memcmp( p_cell->tag->s , p_msg->tag->body.s , p_msg->tag->body.len ) )
                                     if ( /*callid*/ !memcmp( p_cell->inbound_request->callid->body.s , p_msg->callid->body.s , p_msg->callid->body.len ) )
                                        if ( /*cseq_nr*/ !memcmp( get_cseq(p_cell->inbound_request)->number.s , get_cseq(p_msg)->number.s , get_cseq(p_msg)->number.len ) )
                                           { /* WE FOUND THE GOLDEN EGG !!!! */
                                              DBG("DEBUG: t_lookup_request: ACK found\n");
                                              T = p_cell;
                                              unref_cell( p_cell );
                                               return 1;
                                           }
      } /* end if is ACK or not*/
      /* next transaction */
      tmp_cell = p_cell;
      p_cell = p_cell->next_cell;

      /* the transaction is dereferenceted */
      unref_cell( tmp_cell );
   }

   /* no transaction found */
   DBG("DEBUG: t_lookup_request: no transaction found\n");
   T = 0;
   return -1;
}




/* function returns:
 *       1 - forward successfull
 *      -1 - error during forward
 */
int t_forward( struct sip_msg* p_msg , unsigned int dest_ip_param , unsigned int dest_port_param )
{
   unsigned int dest_ip     = dest_ip_param;
   unsigned int dest_port  = dest_port_param;
   int	branch = 0;	/* we don't do any forking right now */

   /* it's about the same transaction or not? */
   if ( global_msg_id != p_msg->id )
   {
      T = (struct cell*)-1;
      global_msg_id = p_msg->id;
   }

   DBG("t_forward: 1. T=%x\n", T);
   /* if  T hasn't been previous searched -> search for it */
   if ( (int)T ==-1 )
      t_lookup_request( p_msg, 0 , 0 );

   DBG("t_forward: 2. T=%x\n", T);
   /*if T hasn't been found after all -> return not found (error) */
   if ( !T )
      return -1;

   /*if it's an ACK and the status is not final or is final, but error the ACK is not forwarded*/
   if ( p_msg->first_line.u.request.method_value==METHOD_ACK  && (T->status/100)!=2 ) {
      DBG("DEBUG: t_forward: local ACK; don't forward\n");
      return 1;
   }

   /* if it's forwarded for the first time ; else the request is retransmited from the transaction buffer */
   if ( T->outbound_request[branch]==NULL )
   {
      unsigned int len;
      char               *buf;

      DBG("DEBUG: t_forward: first time forwarding\n");
      /* allocates a new retrans_buff for the outbound request */
      T->outbound_request[branch] = (struct retrans_buff*)sh_malloc( sizeof(struct retrans_buff) );
      memset( T->outbound_request[branch] , 0 , sizeof (struct retrans_buff) );
      T->nr_of_outgoings = 1;

      /* special case : CANCEL */
      if ( p_msg->first_line.u.request.method_value==METHOD_CANCEL  )
      {
         struct cell  *T2;
         DBG("DEBUG: t_forward: it's CANCEL\n");
         /* find original cancelled transaction; if found, use its next-hops; otherwise use those passed by script */
         T2 = t_lookupOriginalT( hash_table , p_msg );
         /* if found */
         if (T2)
         {  /* if in 1xx status, send to the same destination */
            if ( (T2->status/100)==1 )
            {
               DBG("DEBUG: t_forward: it's CANCEL and I will send to the same place where INVITE went\n");
               dest_ip    = T2->outbound_request[branch]->dest_ip;
               dest_port = T2->outbound_request[branch]->dest_port;
            }
            else
            {
               /* transaction exists, but nothing to cancel */
               DBG("DEBUG: t_forward: it's CANCEL but I have nothing to cancel here\n");
               return 1;
            }
         }
      }/* end special case CANCEL*/

      /* store */
      DBG("DEBUG: t_forward: building outbound request\n");
      T->outbound_request[branch]->tl[RETRASMISSIONS_LIST].payload = &(T->outbound_request[branch]);
      T->outbound_request[branch]->to.sin_family = AF_INET;

      if (add_branch_label( T, p_msg , branch )==-1) return -1;
      buf = build_req_buf_from_sip_req  ( p_msg, &len);
      if (!buf)
         return -1;
      T->outbound_request[branch]->bufflen = len ;
      if ( !(T->outbound_request[branch]->buffer   = (char*)sh_malloc( len ))) {
	LOG(L_ERR, "ERROR: t_forward: shmem allocation failed\n");
	free( buf );
	return -1;
      }
      memcpy( T->outbound_request[branch]->buffer , buf , len );
      free( buf ) ;

      DBG("DEBUG: t_forward: starting timers (retrans and FR)\n");
      /*sets and starts the FINAL RESPONSE timer */
      //add_to_tail_of_timer_list( hash_table , &(T->outbound_request[branch]->tl[FR_TIMER_LIST]) , FR_TIMER_LIST, FR_TIME_OUT );

      /* sets and starts the RETRANS timer */
      T->outbound_request[branch]->timeout_ceiling  = RETR_T2;
      T->outbound_request[branch]->timeout_value    = RETR_T1;
      insert_into_timer_list( hash_table , &(T->outbound_request[branch]->tl[RETRASMISSIONS_LIST]), RETRASMISSIONS_LIST , RETR_T1 );
   }/* end for the first time */

   DBG("DEBUG: t_forward: sending outbund request from buffer (%d bytes):\n%*s\n", 
	T->outbound_request[branch]->bufflen, T->outbound_request[branch]->bufflen,
	 T->outbound_request[branch]->buffer);
   /* send the request */
   T->outbound_request[branch]->dest_ip         = dest_ip;
   T->outbound_request[branch]->dest_port      = dest_port;
   T->outbound_request[branch]->to.sin_port     =  dest_port;
   T->outbound_request[branch]->to.sin_addr.s_addr =  dest_ip;
   udp_send( T->outbound_request[branch]->buffer , T->outbound_request[branch]->bufflen ,
                    (struct sockaddr*)&(T->outbound_request[branch]->to) , sizeof(struct sockaddr_in) );
   return 1;
}




/* Forwards the inbound request to dest. from via.  Returns:
 *       1 - forward successfull
 *      -1 - error during forward
 */
int t_forward_uri( struct sip_msg* p_msg, char* foo, char* bar  )
{
   struct hostent  *nhost;
   unsigned int     ip, port;
   char                  backup;
   str                      uri;


   /* it's about the same transaction or not? */
   if ( global_msg_id != p_msg->id )
   {
      T = (struct cell*)-1;
      global_msg_id = p_msg->id;
   }

   DBG("DEBUG: t_forward: 1. T=%x\n", T);
   /* if  T hasn't been previous searched -> search for it */
   if ( (int)T ==-1 )
      t_lookup_request( p_msg, 0 , 0 );

   DBG("DEBUG: t_forward: 2. T=%x\n", T);
   /*if T hasn't been found after all -> return not found (error) */
   if ( !T )
      return -1;

   /* the original uri has been changed? */
   if (p_msg->new_uri.s==0 || p_msg->new_uri.len==0)
     uri = p_msg->first_line.u.request.uri;
   else
     uri = p_msg->new_uri;

   /*some dirty trick to get the port and ip of destination */
   backup = *((uri.s)+(uri.len));
   *((uri.s)+(uri.len)) = 0;
   nhost = gethostbyname( uri.s );
   *((uri.s)+(uri.len)) = backup;
   if ( !nhost )
   {
      DBG("DEBUG: t_forward_uri: cannot resolve host\n");
      return -1;
   }
   /*IP address*/
   memcpy(&ip, nhost->h_addr_list[0], sizeof(unsigned int));
   /* port */
   port = 5060;

   return t_forward( p_msg , ip , port );
}




/*  This function is called whenever a reply for our module is received; we need to register
  *  this function on module initialization;
  *  Returns :   0 - core router stops
  *                    1 - core router relay statelessly
  */
int t_on_reply_received( struct sip_msg  *p_msg )
{
   unsigned int  branch;

   global_msg_id = p_msg->id;

   /* we use label-matching to lookup for T */
   t_reply_matching( hash_table , p_msg , &T , &branch  );

   /* if no T found ->tell the core router to forward statelessly */
   if ( !T )
      return 1;

   /* stop retransmission */
   remove_from_timer_list( hash_table , &(T->outbound_request[branch]->tl[RETRASMISSIONS_LIST]) , RETRASMISSIONS_LIST );

   /* on a non-200 reply to INVITE, generate local ACK */
   if ( T->inbound_request->first_line.u.request.method_value==METHOD_INVITE && p_msg->first_line.u.reply.statusclass>2 )
   {
      DBG("DEBUG: t_on_reply_received: >=3xx reply to INVITE: send ACK\n");
      t_build_and_send_ACK( T , branch );
      t_store_incoming_reply( T , branch , p_msg );
   }

   #ifdef FORKING
   /* skipped for the moment*/
   #endif

   /*let's check the current inbound response status (is final or not) */
   if ( T->inbound_response && (T->status/100)>1 )
   {  /*a final reply was already sent upstream */
      /* alway relay 2xx immediately ; drop anything else */
      DBG("DEBUG: t_on_reply_received: something final had been relayed\n");
      if ( p_msg->first_line.u.reply.statusclass==2 )
          t_relay_reply( T , branch , p_msg );
      /* nothing to do for the ser core */
      return 0;
   }
   else
   {  /* no reply sent yet or sent but not final*/
      DBG("DEBUG: t_on_reply_received: no reply sent yet or sent but not final\n");

      /* restart retransmission if provisional response came for a non_INVITE -> retrasmit at RT_T2*/
      if ( p_msg->first_line.u.reply.statusclass==1 && T->inbound_request->first_line.u.request.method_value!=METHOD_INVITE )
      {
         T->outbound_request[branch]->timeout_value = RETR_T2;
         insert_into_timer_list( hash_table , &(T->outbound_request[branch]->tl[RETRASMISSIONS_LIST]) , RETRASMISSIONS_LIST , RETR_T2 );
      }


      /* relay ringing and OK immediately */
      if ( p_msg->first_line.u.reply.statusclass ==1 || p_msg->first_line.u.reply.statusclass ==2  )
      {
	   DBG("DEBUG: t_on_reply_received:  relay ringing and OK immediately \n");
           if ( p_msg->first_line.u.reply.statuscode > T->status )
              t_relay_reply( T , branch , p_msg );
         return 0;
      }

      /* error final responses are only stored */
      if ( p_msg->first_line.u.reply.statusclass>=3 && p_msg->first_line.u.reply.statusclass<=5 )
      {
	   DBG("DEBUG: t_on_reply_received:  error final responses are only stored  \n");
         t_store_incoming_reply( T , branch , p_msg );
         if ( t_all_final(T) )
              relay_lowest_reply_upstream( T , p_msg );
         /* nothing to do for the ser core */
         return 0;
      }

      /* global failure responses relay immediately */
     if ( p_msg->first_line.u.reply.statusclass==6 )
      {
	DBG("DEBUG: t_on_reply_received: global failure responses relay immediately (not 100 per cent compliant)\n");
         t_relay_reply( T , branch , p_msg );
         /* nothing to do for the ser core */
         return 0;
      }
   }
}




/*   returns 1 if everything was OK or -1 for error
  */
int t_put_on_wait(  struct sip_msg  *p_msg  )
{
   unsigned int i;

   t_check( hash_table , p_msg );
   /* do we have something to release? */
   if (T==0)
      return -1;

   /**/
  for( i=0 ; i<T->nr_of_outgoings ; i++ )
      if ( T->outbound_response[i] && T->outbound_response[i]->first_line.u.reply.statusclass==1)
          t_cancel_branch(i);

   /* make double-sure we have finished everything */
   /* remove from  retranssmision  and  final response   list */
   remove_from_timer_list( hash_table , &(T->inbound_response->tl[RETRASMISSIONS_LIST]) , RETRASMISSIONS_LIST );
   remove_from_timer_list( hash_table , &(T->inbound_response->tl[FR_TIMER_LIST]) , FR_TIMER_LIST );
   for( i=0 ; i<T->nr_of_outgoings ; i++ )
   {
      remove_from_timer_list( hash_table , &(T->outbound_request[i]->tl[RETRASMISSIONS_LIST]) , RETRASMISSIONS_LIST );
      remove_from_timer_list( hash_table , &(T->outbound_request[i]->tl[FR_TIMER_LIST]) , FR_TIMER_LIST );
   }
   /* adds to Wait list*/
   add_to_tail_of_timer_list( hash_table, &(T->wait_tl), WT_TIMER_LIST, WT_TIME_OUT );
   return 1;
}




/* Retransmits the last sent inbound reply.
  * Returns  -1 -error
  *                1 - OK
  */
int t_retransmit_reply( struct sip_msg* p_msg, char* foo, char* bar  )
{
   t_check( hash_table, p_msg );

   /* if no transaction exists or no reply to be resend -> out */
   if ( T  && T->inbound_response )
   {
      udp_send( T->inbound_response->buffer , T->inbound_response->bufflen , (struct sockaddr*)&(T->inbound_response->to) , sizeof(struct sockaddr_in) );
      return 1;
   }

   return -1;
}




/* Force a new response into inbound response buffer.
  * returns 1 if everything was OK or -1 for erro
  */
int t_send_reply(  struct sip_msg* p_msg , unsigned int code , char * text )
{
   t_check( hash_table, p_msg );

   if (T)
   {
      unsigned int len;
      char * buf;

      buf = build_res_buf_from_sip_req( code , text , T->inbound_request , &len );
      if (!buf)
      {
         DBG("DEBUG: t_send_reply: response building failed\n");
         return -1;
      }

     if ( T->inbound_response )
      {
         sh_free( T->inbound_response->buffer );
      }
      else
      {
         struct hostent  *nhost;
         char foo;

          T->inbound_response = (struct retrans_buff*)sh_malloc( sizeof(struct retrans_buff) );
          memset( T->inbound_response , 0 , sizeof (struct retrans_buff) );
          T->inbound_response->tl[RETRASMISSIONS_LIST].payload = &(T->inbound_response);
          /*some dirty trick to get the port and ip of destination */
          foo = *((p_msg->via1->host.s)+(p_msg->via1->host.len));
          *((p_msg->via1->host.s)+(p_msg->via1->host.len)) = 0;
          nhost = gethostbyname( p_msg->via1->host.s );
          *((p_msg->via1->host.s)+(p_msg->via1->host.len)) = foo;
          if ( !nhost )
             return -1;
          memcpy( &(T->inbound_response->to.sin_addr) , &(nhost->h_addr) , nhost->h_length );
          T->inbound_response->dest_ip         = htonl(T->inbound_response->to.sin_addr.s_addr);
          T->inbound_response->dest_port      = htonl(T->inbound_response->to.sin_port);
          T->inbound_response->to.sin_family = AF_INET;
      }
      T->status = code;
      T->inbound_response->bufflen = len ;
      T->inbound_response->buffer   = (char*)sh_malloc( len );
      memcpy( T->inbound_response->buffer , buf , len );
      free( buf ) ;

      /* make sure that if we send something final upstream, everything else will be cancelled */
      if ( code>=200 )
         t_put_on_wait( p_msg );

      /* if the code is 3,4,5,6 class for an INVITE -> starts retrans and FR timer*/
      if ( p_msg->first_line.u.request.method_value==METHOD_INVITE && code>=300)
      {
         remove_from_timer_list( hash_table , &(T->inbound_response->tl[RETRASMISSIONS_LIST]) , RETRASMISSIONS_LIST );
         remove_from_timer_list( hash_table , &(T->inbound_response->tl[FR_TIMER_LIST]) , FR_TIMER_LIST );
         insert_into_timer_list( hash_table , &(T->inbound_response->tl[RETRASMISSIONS_LIST]) , RETRASMISSIONS_LIST , RETR_T1 );
         insert_into_timer_list( hash_table , &(T->inbound_response->tl[FR_TIMER_LIST]) , FR_TIMER_LIST , FR_TIME_OUT );
      }

      t_retransmit_reply( p_msg, 0 , 0);
   }

   return 1;
}








/* ----------------------------HELPER FUNCTIONS-------------------------------- */


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
      ref_cell( p_cell );

      /* is it the wanted transaction ? */
      /* first only the length are checked */
      if ( /*from length*/ p_cell->inbound_request->from->body.len == p_msg->from->body.len )
         if ( /*to length*/ p_cell->inbound_request->to->body.len == p_msg->to->body.len )
            //if ( /*tag length*/ (!p_cell->inbound_request->tag && !p_msg->tag) || (p_cell->inbound_request->tag && p_msg->tag && p_cell->inbound_request->tag->body.len == p_msg->tag->body.len) )
               if ( /*callid length*/ p_cell->inbound_request->callid->body.len == p_msg->callid->body.len )
                  if ( /*cseq_nr length*/ get_cseq(p_cell->inbound_request)->number.len == get_cseq(p_msg)->number.len )
                      if ( /*cseq_method type*/ p_cell->inbound_request->first_line.u.request.method_value!=METHOD_CANCEL )
                         if ( /*req_uri length*/ p_cell->inbound_request->first_line.u.request.uri.len == p_msg->first_line.u.request.uri.len )
                             /* so far the lengths are the same -> let's check the contents */
                             if ( /*from*/ !memcmp( p_cell->inbound_request->from->body.s , p_msg->from->body.s , p_msg->from->body.len ) )
                                if ( /*to*/ !memcmp( p_cell->inbound_request->to->body.s , p_msg->to->body.s , p_msg->to->body.len)  )
                                   //if ( /*tag*/ (!p_cell->inbound_request->tag && !p_msg->tag) || (p_cell->inbound_request->tag && p_msg->tag && !memcmp( p_cell->inbound_request->tag->body.s , p_msg->tag->body.s , p_msg->tag->body.len )) )
                                      if ( /*callid*/ !memcmp( p_cell->inbound_request->callid->body.s , p_msg->callid->body.s , p_msg->callid->body.len ) )
                                          if ( /*cseq_nr*/ !memcmp( get_cseq(p_cell->inbound_request)->number.s , get_cseq(p_msg)->number.s , get_cseq(p_msg)->number.len ) )
                                             if ( /*req_uri*/ memcmp( p_cell->inbound_request->first_line.u.request.uri.s , p_msg->first_line.u.request.uri.s , p_msg->first_line.u.request.uri.len ) )
                                             { /* WE FOUND THE GOLDEN EGG !!!! */
                                                unref_cell( p_cell );
                                                return p_cell;
                                             }
      /* next transaction */
      tmp_cell = p_cell;
      p_cell = p_cell->next_cell;

      /* the transaction is dereferenceted */
      unref_cell( tmp_cell );
   }

   /* no transaction found */
   T = 0;
   return 0;


   return 0;
}

/* converts a string with positive hexadecimal number to an integer;
   if a non-hexadecimal character encountered within 'len', -1 is
   returned
*/
int str_unsigned_hex_2_int( char *c, int len )
{
	int r=0;
	int i;
	int d;

	for (i=0; i<len; i++ ) {
		if (c[i]>='0' && c[i]<='9') d=c[i]-'0'; else
		if (c[i]>='a' && c[i]<='f') d=c[i]-'a'+10; else
		if (c[i]>='A' && c[i]<='F') d=c[i]-'A'+10; else
		return -1;
		r = (r<<4) + d;
	}
	return r;
}

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
   char  *hashi, *syni, *branchi, *p, *n;
   int hashl, synl, branchl;
   int scan_space;

   /* split the branch into pieces: loop_detection_check(ignored),
      hash_table_id, synonym_id, branch_id
   */
   p=p_msg->via1->branch->value.s;
   scan_space=p_msg->via1->branch->value.len;

   /* loop detection ... ignore */ 
   n=eat_token2_end( p, p+scan_space, '.');
   scan_space-=n-p;
   if (n==p || scan_space<2 || *n!='.') goto nomatch;
   p=n+1; scan_space--;

   /* hash_id */
   n=eat_token2_end( p, p+scan_space, '.'); 
   hashl=n-p;
   scan_space-=hashl;
   if (!hashl || scan_space<2 || *n!='.') goto nomatch;
   hashi=p;
   p=n+1;scan_space--;


   /* sequence id */
   n=eat_token2_end( p, p+scan_space, '.');
   synl=n-p;
   scan_space-=synl;
   if (!synl || scan_space<2 || *n!='.') goto nomatch;
   syni=p;
   p=n+1;scan_space--;

   /* branch id */  /*  should exceed the scan_space */
   n=eat_token_end( p, p+scan_space );
   branchl=n-p;
   if (!branchl ) goto nomatch; 
   branchi=p;


   hash_index=str_unsigned_hex_2_int(hashi, hashl);
   entry_label=str_unsigned_hex_2_int(syni, synl);
   branch_id=str_unsigned_hex_2_int(branchi, branchl);

   DBG("DEBUG: t_reply_matching: hash %d label %d branch %d\n", 
	hash_index, entry_label, branch_id );

   /* sanity check */
   if (hash_index==-1 || hash_index >=TABLE_ENTRIES ||
       entry_label==-1 || branch_id==-1 ||
	branch_id>=MAX_FORK )
		goto nomatch;


   /*all the cells from the entry are scan to detect an entry_label matching */
   p_cell     = hash_table->entrys[hash_index].first_cell;
   tmp_cell = 0;
   while( p_cell )
   {
      /* the transaction is referenceted for reading */
      ref_cell( p_cell );
      /* is it the cell with the wanted entry_label? */
      if ( p_cell->label = entry_label )
      /* has the transaction the wanted branch? */
      if ( p_cell->nr_of_outgoings>branch_id && p_cell->outbound_request[branch_id] )
      {/* WE FOUND THE GOLDEN EGG !!!! */
		*p_Trans = p_cell;
		*p_branch = branch_id;
		unref_cell( p_cell );
		return 1;
	}
      /* next cell */
      tmp_cell = p_cell;
      p_cell = p_cell->next_cell;

      /* the transaction is dereferenceted */
      unref_cell( tmp_cell );
   } /* while p_cell */

   /* nothing found */
   DBG("DEBUG: t_reply_matching: no matching transaction exists\n");

nomatch:
   DBG("DEBUG: t_reply_matching: failure to match a transaction\n");
   *p_branch = -1;
   *p_Trans = NULL;
   return 0;
}




/* We like this incoming reply, so, let's store it, we'll decide
  * later what to d with that
  */
int t_store_incoming_reply( struct cell* Trans, unsigned int branch, struct sip_msg* p_msg )
{
   /* if there is a previous reply, replace it */
   if ( Trans->outbound_response[branch] )
      free_sip_msg( Trans->outbound_response[branch] ) ;
   /* force parsing all the needed headers*/
   parse_headers(p_msg, HDR_VIA|HDR_TO|HDR_FROM|HDR_CALLID|HDR_CSEQ );
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
         t_lookup_request( p_msg, 0, 0 );
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
   char *buf;
   unsigned int len;

   /* if there is a reply, release the buffer (everything else stays same) */
   if ( T->inbound_response )
   {
      sh_free( T->inbound_response->buffer );
      remove_from_timer_list( hash_table , &(T->inbound_response->tl[RETRASMISSIONS_LIST]) , RETRASMISSIONS_LIST );
   }
   else
   {
      struct hostent  *nhost;
     char foo;

      T->inbound_response = (struct retrans_buff*)sh_malloc( sizeof(struct retrans_buff) );
      memset( T->inbound_response , 0 , sizeof (struct retrans_buff) );
      T->inbound_response->tl[RETRASMISSIONS_LIST].payload = &(T->inbound_response);
      /*some dirty trick to get the port and ip of destination */
      foo = *((p_msg->via2->host.s)+(p_msg->via2->host.len));
      *((p_msg->via2->host.s)+(p_msg->via2->host.len)) = 0;
      nhost = gethostbyname( p_msg->via2->host.s );
      *((p_msg->via2->host.s)+(p_msg->via2->host.len)) = foo;
      if ( !nhost )
         return -1;
      memcpy( &(T->inbound_response->to.sin_addr) , &(nhost->h_addr) , nhost->h_length );
      T->inbound_response->dest_ip         = htonl(T->inbound_response->to.sin_addr.s_addr);
      T->inbound_response->dest_port      = ntohl(T->inbound_response->to.sin_port);
      T->inbound_response->to.sin_family = AF_INET;
   }

   /*  */
   buf = build_res_buf_from_sip_res ( p_msg, &len);
   if (!buf)
        return -1;
   T->inbound_response->bufflen = len ;
   T->inbound_response->buffer   = (char*)sh_malloc( len );
   memcpy( T->inbound_response->buffer , buf , len );
   free( buf ) ;

   /* make sure that if we send something final upstream, everything else will be cancelled */
   if (T->outbound_response[branch]->first_line.u.reply.statusclass>=2 )
      t_put_on_wait( p_msg );

   /* if the code is 3,4,5,6 class for an INVITE-> starts retrans timer*/
   if ( T->inbound_request->first_line.u.request.method_value==METHOD_INVITE &&
         T->outbound_response[branch]->first_line.u.reply.statusclass>=300)
         {
            remove_from_timer_list( hash_table , &(T->inbound_response->tl[RETRASMISSIONS_LIST]) , RETRASMISSIONS_LIST );
            remove_from_timer_list( hash_table , &(T->inbound_response->tl[FR_TIMER_LIST]) , FR_TIMER_LIST );
            insert_into_timer_list( hash_table , &(T->inbound_response->tl[RETRASMISSIONS_LIST]) , RETRASMISSIONS_LIST , RETR_T1 );
            insert_into_timer_list( hash_table , &(T->inbound_response->tl[FR_TIMER_LIST]) , FR_TIMER_LIST , FR_TIME_OUT );
         }

   t_retransmit_reply( p_msg, 0 , 0 );
}




/*
  */
int t_cancel_branch(unsigned int branch)
{
	LOG(L_ERR, "ERROR: t_cancel_branch: NOT IMPLEMENTED YET\n");
}


/* copy a header field to an output buffer if space allows */
int copy_hf( char **dst, struct hdr_field* hf, char *bumper )
{
   int n;
   n=hf->body.len+2+hf->name.len+CRLF_LEN;
   if (*dst+n >= bumper ) return -1;
   memcpy(*dst, hf->name.s, hf->name.len );
   *dst+= hf->name.len ;
   **dst = ':'; (*dst)++;
   **dst = ' '; (*dst)++;
   memcpy(*dst, hf->body.s, hf->body.len);
   *dst+= hf->body.len;
   memcpy( *dst, CRLF, CRLF_LEN );
   *dst+=CRLF_LEN;
   return 0;
}
  



/* Builds an ACK request based on an INVITE request. ACK is send
  * to same address
  */
int t_build_and_send_ACK( struct cell *Trans, unsigned int branch)
{
    struct sip_msg* p_msg = T->inbound_request;
    struct via_body *via;
    struct hdr_field *hdr;
    char *ack_buf=NULL, *p;
    unsigned int len;
    int n;

   /* enough place for first line and Via ? */
   if ( 4 + p_msg->first_line.u.request.uri.len + 1 + p_msg->first_line.u.request.version.len +
	CRLF_LEN + MY_VIA_LEN + names_len[0] + 1 + port_no_str_len + MY_BRANCH_LEN  < MAX_ACK_LEN ) {
		LOG( L_ERR, "ERROR: t_build_and_send_ACK: no place for FL/Via\n");
		goto error;
   }

   ack_buf = (char *)malloc( MAX_ACK_LEN );
   p = ack_buf;

   /* first line */
   memcpy( p , "ACK " , 4);
   p += 4;

   memcpy( p , p_msg->first_line.u.request.uri.s , p_msg->first_line.u.request.uri.len );
   p += p_msg->first_line.u.request.uri.len;

   *(p++) = ' ';

   memcpy( p , p_msg->first_line.u.request.version.s , p_msg->first_line.u.request.version.len );
   p += p_msg->first_line.u.request.version.len;

   memcpy( p, CRLF, CRLF_LEN );
   p+=CRLF_LEN;

   /* insert our via */
   memcpy( p , MY_VIA , MY_VIA_LEN );
   p += MY_VIA_LEN;

   memcpy( p , names[0] , names_len[0] );
   p += names_len[0];

   *(p++) = ':';

   memcpy( p , port_no_str , port_no_str_len );
   p += port_no_str_len;

   memcpy( p, MY_BRANCH, MY_BRANCH_LEN );
   p+=MY_BRANCH_LEN;

   n=snprintf( p, ack_buf + MAX_ACK_LEN - p,
                 ".%x.%x.%x%s",
                 Trans->hash_index, Trans->label, branch, CRLF );

   if (n==-1) {
	LOG(L_ERR, "ERROR: t_build_and_send_ACK: not enough memory for branch\n");
	goto error;
   }
   p+=n;

   if (!check_transaction_quadruple( p_msg )) {
	LOG(L_ERR, "ERROR: t_build_and_send_ACK: can't generate a HBH ACK if key HFs in INVITE missing\n");
	goto error;
   }

   /* To */
   if (copy_hf( &p, p_msg->to , ack_buf + MAX_ACK_LEN )==-1) {
	LOG(L_ERR, "ERROR: t_build_and_send_ACK: no place for To\n");
	goto error;
   }
   /* From */
   if (copy_hf( &p, p_msg->from, ack_buf + MAX_ACK_LEN )==-1) {
	LOG(L_ERR, "ERROR: t_build_and_send_ACK: no place for From\n");
	goto error;
   }
   /* CallId */
   if (copy_hf( &p, p_msg->callid, ack_buf + MAX_ACK_LEN )==-1) {
	LOG(L_ERR, "ERROR: t_build_and_send_ACK: no place for callid\n");
	goto error;
   }
   /* CSeq, EoH */
   n=snprintf( p, ack_buf + MAX_ACK_LEN - p,
                 "Cseq: %*s ACK%s%s", get_cseq(p_msg)->number.len,
		get_cseq(p_msg)->number.s, CRLF, CRLF );
   if (n==-1) {
	LOG(L_ERR, "ERROR: t_build_and_send_ACK: no enough memory for Cseq\n");
	goto error;
   }
   p+=n;


   /* sends the ACK message to the same destination as the INVITE */
   udp_send( ack_buf, p-ack_buf, (struct sockaddr*)&(T->outbound_request[branch]->to) , sizeof(struct sockaddr_in) );

   /* free mem*/
   if (ack_buf) free( ack_buf );
   return 0;

error:
   	if (ack_buf) free( ack_buf );
	return -1;
}



/*---------------------------------TIMEOUT HANDLERS--------------------------------------*/


void retransmission_handler( void *attr)
{
   struct retrans_buff* r_buf = (struct retrans_buff*)attr;

   /* the transaction is already removed from RETRANSMISSION_LIST by the timer */
   DBG("DEBUG: retransmission_handler : payload received=%p\n",attr);

   /* computs the new timeout. */
   if ( r_buf->timeout_value<r_buf->timeout_ceiling )
      r_buf->timeout_value *=2;

   /* retransmision */
   DBG("DEBUG: retransmission_handler : resending\n");
   //udp_send( r_buf->buffer, r_buf->bufflen, (struct sockaddr*)&(r_buf->to) , sizeof(struct sockaddr_in) );

   /* re-insert into RETRASMISSIONS_LIST */
   DBG("DEBUG: retransmission_handler : before insert\n");
   insert_into_timer_list( hash_table , &(r_buf->tl[RETRASMISSIONS_LIST]) , RETRASMISSIONS_LIST , r_buf->timeout_value );
   DBG("DEBUG: retransmission_handler : after insert\n");
}




void final_response_handler( void *attr)
{
   struct cell *p_cell = (struct cell*)attr;

   /* the transaction is already removed from FR_LIST by the timer */
   /* send a 408 */
   DBG("DEBUG: final_response_handler : sending 408\n");
   t_send_reply( p_cell->inbound_request , 408 , "Request Timeout" );
   /* put it on WT_LIST - transaction is over */
   t_put_on_wait(  p_cell->inbound_request );
}




void wait_handler( void *attr)
{
   struct cell *p_cell = (struct cell*)attr;

   /* the transaction is already removed from WT_LIST by the timer */
   /* the cell is removed from the hash table */
   DBG("DEBUG: wait_handler : removing from table\n");
    remove_from_hash_table( hash_table, p_cell );
   /* put it on DEL_LIST - sch for del */
    add_to_tail_of_timer_list( hash_table, &(p_cell->dele_tl), DELETE_LIST, DEL_TIME_OUT );
}




void delete_handler( void *attr)
{
   struct cell *p_cell = (struct cell*)attr;

   /* the transaction is already removed from DEL_LIST by the timer */
    /* if is not refenceted -> is deleted*/
    if ( p_cell->ref_counter==0 )
    {
       DBG("DEBUG: delete_handler : delete transaction\n");
       free_cell( p_cell );
    }
    else
    {
       DBG("DEBUG: delete_handler : re post for delete\n");
       /* else it's readded to del list for future del */
       add_to_tail_of_timer_list( hash_table, &(p_cell->dele_tl), DELETE_LIST, DEL_TIME_OUT );
    }
}




/* append appropriate branch labels for fast reply-transaction matching
   to outgoing requests
*/
int add_branch_label( struct cell *trans, struct sip_msg *p_msg, int branch )
{
	char *c;
	short n;

	n=snprintf( p_msg->add_to_branch_s+p_msg->add_to_branch_len,
		  MAX_BRANCH_PARAM_LEN - p_msg->add_to_branch_len,
		 ".%x.%x.%x",
		 trans->hash_index, trans->label, branch );
	DBG("DEBUG: branch created now: %*s (%d)\n", n, p_msg->add_to_branch_s+p_msg->add_to_branch_len );
	if (n==-1) {
		LOG(L_ERR, "ERROR: add_branch_label: too small branch buffer\n");
		return -1;
	} else return 0;
}
