#include "t_funcs.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser_f.h"
#include "../../ut.h"
#include "../../timer.h"

#define stop_RETR_and_FR_timers(h_table,p_cell)    \
           { int ijk; \
           if ( p_cell->outbound_response )  {  \
               remove_from_timer_list( h_table , (&(p_cell->outbound_response->tl[RETRASMISSIONS_LIST])) , RETRASMISSIONS_LIST ); \
               remove_from_timer_list( h_table , (&(p_cell->outbound_response->tl[FR_TIMER_LIST])) , FR_TIMER_LIST ); } \
           for( ijk=0 ; ijk<p_cell->nr_of_outgoings ; ijk++ )  { \
               remove_from_timer_list( h_table , (&(p_cell->outbound_request[ijk]->tl[RETRASMISSIONS_LIST])) , RETRASMISSIONS_LIST ); \
               remove_from_timer_list( h_table , (&(p_cell->outbound_request[ijk]->tl[FR_TIMER_LIST])) , FR_TIMER_LIST ); } \
           }

#define insert_into_timer(hash_table,new_tl,list_id,time_out) \
             {\
                if ( is_in_timer_list(new_tl,list_id)  )\
                   remove_from_timer_list( hash_table , new_tl , list_id);\
                insert_into_timer_list(hash_table,new_tl,list_id,get_ticks()+time_out);\
             }

#define add_to_tail_of_timer(hash_table,new_tl,list_id,time_out) \
             {\
                if ( is_in_timer_list(new_tl,list_id)  )\
                   remove_from_timer_list( hash_table , new_tl , list_id);\
                add_to_tail_of_timer_list(hash_table,new_tl,list_id,get_ticks()+time_out);\
             }

#define remove_from_timer(hash_table,tl,list_id) \
             {\
                if ( !is_in_timer_list(tl,list_id)  )\
                   remove_from_timer_list( hash_table , tl , list_id);\
             }





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

   /* register the timer function */
   register_timer( timer_routine , hash_table , 1 );

   /*first msg id*/
   global_msg_id = 0;
   T = T_UNDEFINED;

   return 0;
}




void tm_shutdown()
{
    int i;

    /*unlink the lists*/
    for( i=NR_OF_TIMER_LISTS ; i>=0 ; i-- )
       hash_table->timers[ i ].first_tl = hash_table->timers[ i ].last_tl = 0;

    /* destroy the hash table */
    free_hash_table( hash_table );
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
      T = T_UNDEFINED;
      global_msg_id = p_msg->id;
   }

    /* if the transaction is not found yet we are tring to look for it*/
   if ( T==T_UNDEFINED )
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
   /*insert the transaction into hash table*/
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
      T = T_UNDEFINED;
      global_msg_id = p_msg->id;
   }

    /* if  T is previous found -> return found */
   if ( T!=T_UNDEFINED && T )	{
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
	   T=T_NULL;
	   return -1;
   }
   /* start searching into the table */
   hash_index = hash( p_msg->callid->body , get_cseq(p_msg)->number ) ;
   if ( p_msg->first_line.u.request.method_value==METHOD_ACK  )
      isACK = 1;
   DBG("t_lookup_request: continue searching;  hash=%d, isACK=5d\n",hash_index,isACK);

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
            //if ( /*to length*/ p_cell->inbound_request->to->body.len == p_msg->to->body.len )
               if ( /*callid length*/ p_cell->inbound_request->callid->body.len == p_msg->callid->body.len )
                  if ( /*cseq_nr length*/ get_cseq(p_cell->inbound_request)->number.len == get_cseq(p_msg)->number.len )
                      if ( /*cseq_method type*/ p_cell->inbound_request->first_line.u.request.method_value == METHOD_INVITE  )
                         //if ( /*tag length*/ p_cell->tag &&  p_cell->tag->len==p_msg->tag->body.len )
                            /* so far the lengths are the same -> let's check the contents */
                            if ( /*from*/ !memcmp( p_cell->inbound_request->from->body.s , p_msg->from->body.s , p_msg->from->body.len ) )
                               //if ( /*to*/ !memcmp( p_cell->inbound_request->to->body.s , p_msg->to->body.s , p_msg->to->body.len)  )
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
   int	branch;
   unsigned int len;
   char               *buf;

   buf=NULL;
   branch = 0;	/* we don't do any forking right now */

   /* it's about the same transaction or not? */
   if ( global_msg_id != p_msg->id )
   {
      T = T_UNDEFINED;
      global_msg_id = p_msg->id;
   }

   /* if  T hasn't been previous searched -> search for it */
   if ( T == T_UNDEFINED )
      t_lookup_request( p_msg, 0 , 0 );

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
      DBG("DEBUG: t_forward: first time forwarding\n");
      /* special case : CANCEL */
      if ( p_msg->first_line.u.request.method_value==METHOD_CANCEL  )
      {
         DBG("DEBUG: t_forward: it's CANCEL\n");
         /* find original cancelled transaction; if found, use its next-hops; otherwise use those passed by script */
         if ( T->T_canceled==T_UNDEFINED )
            T->T_canceled = t_lookupOriginalT( hash_table , p_msg );
         /* if found */
         if ( T->T_canceled!=T_NULL )
         {
            T->T_canceled->T_canceler = T;
            /* if in 1xx status, send to the same destination */
            if ( (T->T_canceled->status/100)==1 )
            {
               DBG("DEBUG: t_forward: it's CANCEL and I will send to the same place where INVITE went\n");
               dest_ip    = T->T_canceled->outbound_request[branch]->to.sin_addr.s_addr;
               dest_port = T->T_canceled->outbound_request[branch]->to.sin_port;
            }
            else
            {
               /* transaction exists, but nothing to cancel */
               DBG("DEBUG: t_forward: it's CANCEL but I have nothing to cancel here\n");
               return 1;
            }
         }
         else
         {
            /* transaction doesnot exists  */
            DBG("DEBUG: t_forward: canceled request not found! nothing to CANCEL\n");
            return 1;
         }
      }/* end special case CANCEL*/

      /* allocates a new retrans_buff for the outbound request */
      DBG("DEBUG: t_forward: building outbound request\n");
      T->outbound_request[branch] = (struct retrans_buff*)sh_malloc( sizeof(struct retrans_buff) );
      if (!T->outbound_request[branch]) {
        LOG(L_ERR, "ERROR: t_forward: out of shmem\n");
	goto error;
      }
      memset( T->outbound_request[branch] , 0 , sizeof (struct retrans_buff) );
      T->outbound_request[branch]->tl[RETRASMISSIONS_LIST].payload =  T->outbound_request[branch];
      T->outbound_request[branch]->tl[FR_TIMER_LIST].payload =  T->outbound_request[branch];
      T->outbound_request[branch]->to.sin_family = AF_INET;
      T->outbound_request[branch]->my_T =  T;
      T->nr_of_outgoings = 1;

      if (add_branch_label( T, p_msg , branch )==-1) return -1;
      if ( !(buf = build_req_buf_from_sip_req  ( p_msg, &len))) goto error;
      T->outbound_request[branch]->bufflen = len ;
      if ( !(T->outbound_request[branch]->retr_buffer   = (char*)sh_malloc( len ))) {
	LOG(L_ERR, "ERROR: t_forward: shmem allocation failed\n");
	goto error;
      }
      memcpy( T->outbound_request[branch]->retr_buffer , buf , len );
      free( buf ) ; buf=NULL;

      DBG("DEBUG: t_forward: starting timers (retrans and FR)\n");
      /*sets and starts the FINAL RESPONSE timer */
      insert_into_timer( hash_table , (&(T->outbound_request[branch]->tl[FR_TIMER_LIST])) , FR_TIMER_LIST, FR_TIME_OUT );

      /* sets and starts the RETRANS timer */
      T->outbound_request[branch]->timeout_ceiling  = RETR_T2;
      T->outbound_request[branch]->timeout_value    = RETR_T1;
      insert_into_timer( hash_table , (&(T->outbound_request[branch]->tl[RETRASMISSIONS_LIST])), RETRASMISSIONS_LIST , RETR_T1 );
   }/* end for the first time */

    /* if we are forwarding a CANCEL*/
   if (  p_msg->first_line.u.request.method_value==METHOD_CANCEL )
   {
       /* if no transaction to CANCEL */
      /* or if the canceled transaction has a final status -> drop the CANCEL*/
      if ( T->T_canceled==T_NULL || T->T_canceled->status>=200)
       {
           remove_from_timer( hash_table , (&(T->outbound_request[branch]->tl[FR_TIMER_LIST])) , FR_TIMER_LIST);
           remove_from_timer( hash_table , (&(T->outbound_request[branch]->tl[RETRASMISSIONS_LIST])), RETRASMISSIONS_LIST );
           return 1;
       }
   }

   /* send the request */
   /* known to be in network order */
   T->outbound_request[branch]->to.sin_port     =  dest_port;
   T->outbound_request[branch]->to.sin_addr.s_addr =  dest_ip;
   T->outbound_request[branch]->to.sin_family = AF_INET;

   udp_send( T->outbound_request[branch]->retr_buffer , T->outbound_request[branch]->bufflen ,
                    (struct sockaddr*)&(T->outbound_request[branch]->to) , sizeof(struct sockaddr_in) );

   return 1;

error:
	if (T->outbound_request[branch]) free(T->outbound_request[branch]);
	if (buf) free( buf );

	return -1;

}




/* Forwards the inbound request to dest. from via.  Returns:
 *       1 - forward successfull
 *      -1 - error during forward
 */
int t_forward_uri( struct sip_msg* p_msg, char* foo, char* bar  )
{
   struct hostent  *nhost;
   unsigned int     ip, port;
   struct sip_uri    parsed_uri;
   str                      uri;
   int                      err;

   /* it's about the same transaction or not? */
   if ( global_msg_id != p_msg->id )
   {
      T = T_UNDEFINED;
      global_msg_id = p_msg->id;
   }

   /* if  T hasn't been previous searched -> search for it */
   if ( T==T_UNDEFINED )
      t_lookup_request( p_msg, 0 , 0 );

   /*if T hasn't been found after all -> return not found (error) */
   if ( !T )
      return -1;

   /* the original uri has been changed? */
   if (p_msg->new_uri.s==0 || p_msg->new_uri.len==0)
     uri = p_msg->first_line.u.request.uri;
   else
     uri = p_msg->new_uri;

   /* parsing the request uri in order to get host and port */
   if (parse_uri( uri.s , uri.len , &parsed_uri )<0)
   {
        LOG(L_ERR, "ERROR: t_forward_uri: unable to parse destination uri\n");
        return  -1;
   }

   /* getting host address*/
   nhost = gethostbyname( parsed_uri.host.s );
   if ( !nhost )
   {
      LOG(L_ERR, "ERROR: t_forward_uri: cannot resolve host\n");
      return -1;
   }
   memcpy(&ip, nhost->h_addr_list[0], sizeof(unsigned int));

   /* getting the port */
   if ( parsed_uri.port.s==0 || parsed_uri.port.len==0 )
      port = SIP_PORT;
   else
   {
       port = str2s( parsed_uri.port.s , parsed_uri.port.len , &err );
       if ( err<0 )
       {
           LOG(L_ERR, "ERROR: t_forward_uri: converting port from str to int failed; using default SIP port\n");
           port = SIP_PORT;
       }
   }
   port = htons( port );

   free_uri( &parsed_uri );

   return t_forward( p_msg , ip , port );
}




/*  This function is called whenever a reply for our module is received; we need to register
  *  this function on module initialization;
  *  Returns :   0 - core router stops
  *                    1 - core router relay statelessly
  */
int t_on_reply_received( struct sip_msg  *p_msg )
{
   unsigned int  branch,len;

   global_msg_id = p_msg->id;

   parse_headers( p_msg , HDR_EOH ); /*????*/
   /* we use label-matching to lookup for T */
   t_reply_matching( hash_table , p_msg , &T , &branch  );

   /* if no T found ->tell the core router to forward statelessly */
   if ( T<=0 )
      return 1;
   DBG("DEBUG: t_on_reply_received: Original status =%d\n",T->status);

   /* stop retransmission */
   remove_from_timer( hash_table , (&(T->outbound_request[branch]->tl[RETRASMISSIONS_LIST])) , RETRASMISSIONS_LIST );
   /* stop final response timer only if I got a final response */
   if ( p_msg->first_line.u.reply.statusclass>1 )
      remove_from_timer( hash_table , (&(T->outbound_request[branch]->tl[FR_TIMER_LIST])) , FR_TIMER_LIST );
   /* if a got the first prov. response for an INVITE -> change FR_TIME_OUT to INV_FR_TIME_UT */
   if ( !T->inbound_response[branch] && p_msg->first_line.u.reply.statusclass==1 && T->inbound_request->first_line.u.request.method_value==METHOD_INVITE )
      insert_into_timer( hash_table , (&(T->outbound_request[branch]->tl[FR_TIMER_LIST])) , FR_TIMER_LIST , INV_FR_TIME_OUT);

   /* on a non-200 reply to INVITE, generate local ACK */
   if ( T->inbound_request->first_line.u.request.method_value==METHOD_INVITE && p_msg->first_line.u.reply.statusclass>2 )
   {
      DBG("DEBUG: t_on_reply_received: >=3xx reply to INVITE: send ACK\n");
      t_build_and_send_ACK( T , branch , p_msg );
   }

   #ifdef FORKING
   /* skipped for the moment*/
   #endif

   /* if the incoming response code is not reliable->drop it*/
   if ( !t_should_relay_response( T , p_msg->first_line.u.reply.statuscode ) )
      return 0;

   /* restart retransmission if provisional response came for a non_INVITE -> retrasmit at RT_T2*/
   if ( p_msg->first_line.u.reply.statusclass==1 && T->inbound_request->first_line.u.request.method_value!=METHOD_INVITE )
   {
      T->outbound_request[branch]->timeout_value = RETR_T2;
      insert_into_timer( hash_table , (&(T->outbound_request[branch]->tl[RETRASMISSIONS_LIST])) , RETRASMISSIONS_LIST , RETR_T2 );
   }

   /*store the inbound reply*/
   t_store_incoming_reply( T , branch , p_msg );
   if ( p_msg->first_line.u.reply.statusclass>=3 && p_msg->first_line.u.reply.statusclass<=5 )
   {
      if ( t_all_final(T) )
           relay_lowest_reply_upstream( T , p_msg );
   }
   else
   {
      push_reply_from_uac_to_uas( T , branch );
   }

   /* nothing to do for the ser core */
    return 0;
}




/*   returns 1 if everything was OK or -1 for error
  */
int t_release_transaction( struct sip_msg* p_msg)
{
   t_check( hash_table , p_msg );

   if ( T && T!=T_UNDEFINED )
      return t_put_on_wait( T );

   return 1;
}




/* Retransmits the last sent inbound reply.

  * input: p_msg==request for which I want to retransmit an associated
    reply
  * Returns  -1 -error
  *                1 - OK
  */
int t_retransmit_reply( struct sip_msg* p_msg, char* foo, char* bar  )
{
   t_check( hash_table, p_msg );

   /* if no transaction exists or no reply to be resend -> out */
   if ( T  && T->outbound_response )
   {
      udp_send( T->outbound_response->retr_buffer , T->outbound_response->bufflen ,
	(struct sockaddr*)&(T->outbound_response->to) , sizeof(struct sockaddr_in) );
      return 1;
   }

  /* no transaction found */
   return -1;
}




/* Force a new response into inbound response buffer.
  * returns 1 if everything was OK or -1 for erro
  */
int t_send_reply(  struct sip_msg* p_msg , unsigned int code , char * text )
{
   unsigned int len;
   char * buf = NULL;
   struct hostent  *nhost;
   unsigned int      ip, port;
   char foo;
   int err;
   struct retrans_buff* rb = NULL;
   char *b;

   DBG("DEBUG: t_send_reply: entered\n");
   t_check( hash_table, p_msg );

   if (!T)
   {
      LOG(L_ERR, "ERROR: cannot send a t_reply to a message for which no T-state has been established\n");
     return -1;
   }

   /* if the incoming response code is not reliable->drop it*/
   if ( !t_should_relay_response( T , code ) )
      return 1;

   if ( T->outbound_response)
   {
      if (  T->outbound_response->retr_buffer )
       {
	  b = T->outbound_response->retr_buffer;
          T->outbound_response->retr_buffer = NULL;
          sh_free( b );
	  rb = NULL;
       }
   }
   else
   {
      rb = (struct retrans_buff*)sh_malloc( sizeof(struct retrans_buff) );
     if (!rb)
      {
        LOG(L_ERR, "ERROR: t_send_reply: cannot allocate shmem for retransmission buffer\n");
       goto error;
      }
      T->outbound_response = rb;
      memset( T->outbound_response , 0 , sizeof (struct retrans_buff) );

      /* initialize retransmission structure */
     if (update_sock_struct_from_via(  &(T->outbound_response->to),  p_msg->via1 )==-1)
      {
         LOG(L_ERR, "ERROR: t_send_reply: cannot lookup reply dst: %s\n",
                  p_msg->via1->host.s );
        goto error;
      }

      T->outbound_response->tl[RETRASMISSIONS_LIST].payload = T->outbound_response;
      T->outbound_response->tl[FR_TIMER_LIST].payload = T->outbound_response;
      T->outbound_response->to.sin_family = AF_INET;
      T->outbound_response->my_T = T;
   }

   buf = build_res_buf_from_sip_req( code , text , T->inbound_request , &len );
   DBG("DEBUG: t_send_reply: buffer computed\n");

   if (!buf)
   {
      DBG("DEBUG: t_send_reply: response building failed\n");
     goto error;
   }

   T->outbound_response->bufflen = len ;
   T->outbound_response->retr_buffer   = (char*)sh_malloc( len );
   if (!T->outbound_response->retr_buffer)
   {
      LOG(L_ERR, "ERROR: t_send_reply: cannot allocate shmem buffer\n");
     goto error;
   }
   memcpy( T->outbound_response->retr_buffer , buf , len );
   free( buf ) ;
   T->status = code;

   /* start/stops the proper timers*/
   DBG("DEBUG: t_send_reply: update timers\n");
   t_update_timers_after_sending_reply( T->outbound_response );

   DBG("DEBUG: t_send_reply: send reply\n");
   t_retransmit_reply( p_msg, 0 , 0);

   return 1;

error:
   if (rb) { sh_free(rb); T->outbound_response = rb = NULL;}
   if ( buf ) free ( buf );
   return -1;
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
   hash_index = hash( p_msg->callid->body , get_cseq(p_msg)->number  ) ;
   DBG("DEBUG: t_lookupOriginalT: searching on hash entry %d\n",hash_index );

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
                                             if ( /*req_uri*/ !memcmp( p_cell->inbound_request->first_line.u.request.uri.s , p_msg->first_line.u.request.uri.s , p_msg->first_line.u.request.uri.len ) )
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

   if (! ( p_msg->via1 && p_msg->via1->branch && p_msg->via1->branch->value.s) )
	goto nomatch;

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
      if ( p_cell->label == entry_label )
      /* has the transaction the wanted branch? */
      if ( p_cell->nr_of_outgoings>branch_id && p_cell->outbound_request[branch_id] )
      {/* WE FOUND THE GOLDEN EGG !!!! */
		*p_Trans = p_cell;
		*p_branch = branch_id;
		unref_cell( p_cell );
                              DBG("DEBUG: t_reply_matching: reply matched!\n");
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
   DBG("DEBUG: t_store_incoming_reply: starting [%d]....\n",branch);
   /* force parsing all the needed headers*/
   if ( parse_headers(p_msg, HDR_VIA1|HDR_VIA2|HDR_TO|HDR_CSEQ )==-1 ||
        !p_msg->via1 || !p_msg->via2 || !p_msg->to || !p_msg->cseq )
   {
      LOG( L_ERR , "ERROR: t_store_incoming_reply: unable to parse headers !\n"  );
      return -1;
   }
   /* if there is a previous reply, replace it */
   if ( Trans->inbound_response[branch] ) {
      sip_msg_free( Trans->inbound_response[branch] ) ;
      DBG("DEBUG: t_store_incoming_reply: sip_msg_free done....\n");
   }

   Trans->inbound_response[branch] = sip_msg_cloner( p_msg );
   if (!Trans->inbound_response[branch])
	return -1;
   Trans->status = p_msg->first_line.u.reply.statuscode;
   DBG("DEBUG: t_store_incoming_reply: reply stored\n");
   return 1;
}




/* Functions update T (T gets either a valid pointer in it or it equals zero) if no transaction
  * for current message exists;
  * Returns 1 if T was modified or 0 if not.
  */
int t_check( struct s_table *hash_table , struct sip_msg* p_msg )
{
   unsigned int branch;

   /* is T still up-to-date ? */
   DBG("DEBUG: t_check : msg id=%d , global msg id=%d , T=%p\n", p_msg->id,global_msg_id,T);
   if ( p_msg->id != global_msg_id || T==T_UNDEFINED )
   {
      global_msg_id = p_msg->id;
      T = T_UNDEFINED;
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
      if (  !Trans->inbound_response[i] ||  Trans->inbound_response[i]->first_line.u.reply.statuscode<=200 )
         return 0;

  DBG("DEBUG: t_all_final: final state!!!!:)) \n");
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
      if ( T->inbound_response[i] &&
	   T->inbound_response[i]->first_line.u.reply.statuscode>=200 &&
	   T->inbound_response[i]->first_line.u.reply.statuscode<lowest_v )
      {
         lowest_i =i;
         lowest_v = T->inbound_response[i]->first_line.u.reply.statuscode;
      }

   DBG("DEBUG: relay_lowest_reply_upstream: lowest reply [%d]=%d\n",lowest_i,lowest_v);

   if ( lowest_i != -1 )
      push_reply_from_uac_to_uas( T ,lowest_i );

   return lowest_i;
}




/* Push a previously stored reply from UA Client to UA Server
  * and send it out
  */
int push_reply_from_uac_to_uas( struct cell* trans , unsigned int branch )
{
   char *buf;
   unsigned int len;

   DBG("DEBUG: push_reply_from_uac_to_uas: start\n");
   /* if there is a reply, release the buffer (everything else stays same) */
   if ( trans->outbound_response )
   {
      sh_free( trans->outbound_response->retr_buffer );
      remove_from_timer( hash_table , (&(trans->outbound_response->tl[RETRASMISSIONS_LIST])) , RETRASMISSIONS_LIST );
      remove_from_timer( hash_table , (&(trans->outbound_response->tl[FR_TIMER_LIST])) , FR_TIMER_LIST );
   }
   else
   {
      struct hostent  *nhost;
     char foo;

      trans->outbound_response = (struct retrans_buff*)sh_malloc( sizeof(struct retrans_buff) );
      if (!trans->outbound_response) {
	LOG(L_ERR, "ERROR: push_reply_from_uac_to_uas: no more shmem\n");
	trans->outbound_response = NULL;
	return -1;
      }
      /*init retrans buffer*/
      memset( trans->outbound_response , 0 , sizeof (struct retrans_buff) );
      trans->outbound_response->tl[RETRASMISSIONS_LIST].payload = trans->outbound_response;
      trans->outbound_response->tl[FR_TIMER_LIST].payload = trans->outbound_response;
      trans->outbound_response->to.sin_family = AF_INET;
      trans->outbound_response->my_T = trans;

      if (update_sock_struct_from_via(  &(trans->outbound_response->to),  trans->inbound_response[branch]->via2 )==-1) {
	LOG(L_ERR, "ERROR: push_reply_from_uac_to_uas: cannot lookup reply dst: %s\n",
		trans->inbound_response[branch]->via2->host.s );
	sh_free(  T->outbound_response );
	T->outbound_response = NULL;
	return -1;
      }
   }

   /*  */
   buf = build_res_buf_from_sip_res ( trans->inbound_response[branch], &len);
   if (!buf) {
	LOG(L_ERR, "ERROR: push_reply_from_uac_to_uas: no shmem for outbound reply buffer\n");
        return -1;
   }
   trans->outbound_response->bufflen = len ;
   trans->outbound_response->retr_buffer   = (char*)sh_malloc( len );
   memcpy( trans->outbound_response->retr_buffer , buf , len );
   free( buf ) ;

   /* start/stops the proper timers*/
   t_update_timers_after_sending_reply( T->outbound_response );

   /*send the reply*/
   t_retransmit_reply( trans->inbound_response[branch], 0 , 0 );

   return 1;
}




/*
  */
int t_update_timers_after_sending_reply( struct retrans_buff *rb )
{
   struct cell *Trans = rb->my_T;

   /* make sure that if we send something final upstream, everything else will be cancelled */
   if ( Trans->status>=300 &&  Trans->inbound_request->first_line.u.request.method_value==METHOD_INVITE )
   {
            rb->timeout_ceiling  = RETR_T2;
            rb->timeout_value    = RETR_T1;
            insert_into_timer( hash_table , (&(rb->tl[RETRASMISSIONS_LIST])) , RETRASMISSIONS_LIST , RETR_T1 );
            insert_into_timer( hash_table , (&(rb->tl[FR_TIMER_LIST])) , FR_TIMER_LIST , FR_TIME_OUT );
   }
   else if ( Trans->inbound_request->first_line.u.request.method_value==METHOD_CANCEL )
   {
      if ( Trans->T_canceled==T_UNDEFINED )
            Trans->T_canceled = t_lookupOriginalT( hash_table , Trans->inbound_request );
      if ( Trans->T_canceled==T_NULL )
            return 1;
      Trans->T_canceled->T_canceler = Trans;
     /* put CANCEL transaction on wait only if canceled transaction already
        is in final status and there is nothing to cancel; 
     */
     if ( Trans->T_canceled->status>=200)
            t_put_on_wait( Trans );
   }
   else if (Trans->status>=200)
            t_put_on_wait( Trans );

   return 1;
}




/* Checks if the new reply (with new_code status) should be sent or not based on the current
  * transactin status. Returns 1 - the response can be sent
  *                                            0 - is not indicated to sent
  */
int t_should_relay_response( struct cell *Trans , int new_code )
{
   int T_code;

   T_code = Trans->status;

   /* have we already sent something? */
   if ( !Trans->outbound_response )
   {
      DBG("DEBUG: t_should_relay_response: %d response relayed (no previous response sent)\n",new_code);
      return 1;
   }

   /* have we sent a final response? */
   if ( (T_code/100)>1 )
   {  /*final response was sent*/
      if ( new_code==200 && Trans->inbound_request->first_line.u.request.method_value==METHOD_INVITE )
      {
         DBG("DEBUG: t_should_relay_response: %d response relayed (final satus, but 200 to an INVITE)\n",new_code);
         return 0;
      }
   }
   else
   { /* provisional response was sent */
      if ( new_code>T_code )
      {
         DBG("DEBUG: t_should_relay_response: %d response relayed (higher provisional response)\n",new_code);
         return 1;
      }
   }

   DBG("DEBUG: t_should_relay_response: %d response not relayed\n",new_code);
   return 0;
}




/*
  */
int t_put_on_wait(  struct cell  *Trans  )
{
   struct timer_link *tl;
   unsigned int i;

  if ( is_in_timer_list( (&(Trans->wait_tl)) , WT_TIMER_LIST) )
  {
     DBG("DEBUG: t_put_on_wait: already on wait\n");
     return -1;
  }

   DBG("DEBUG: t_put_on_wait: stopping timers (FR and RETR)\n");
   /**/
   for( i=0 ; i<Trans->nr_of_outgoings ; i++ )
      if ( Trans->inbound_response[i] && Trans->inbound_response[i]->first_line.u.reply.statusclass==1)
          t_cancel_branch(i);

   /* make double-sure we have finished everything */
   /* remove from  retranssmision  and  final response   list */
   stop_RETR_and_FR_timers(hash_table,Trans) ;
   /* adds to Wait list*/
   add_to_tail_of_timer( hash_table, (&(Trans->wait_tl)), WT_TIMER_LIST, WT_TIME_OUT );
   return 1;
}




/*
  */
int t_cancel_branch(unsigned int branch)
{
	LOG(L_ERR, "ERROR: t_cancel_branch: NOT IMPLEMENTED YET\n");
}




/* Builds an ACK request based on an INVITE request. ACK is send
  * to same address
  */
int t_build_and_send_ACK( struct cell *Trans, unsigned int branch, struct sip_msg* rpl)
{
   struct sip_msg* p_msg , *r_msg;
    struct via_body *via;
    struct hdr_field *hdr;
    char *ack_buf, *p;
    unsigned int len;
    int n;


   p_msg = T->inbound_request;
   r_msg = rpl;

   if ( parse_headers(rpl,HDR_TO)==-1 || !rpl->to )
   {
	LOG(L_ERR, "ERROR: t_build_and_send_ACK: cannot generate a HBH ACK if key HFs in INVITE missing\n");
	goto error;
   }

    len = 0;
    /*first line's len */
    len += 4+p_msg->first_line.u.request.uri.len+1+p_msg->first_line.u.request.version.len+CRLF_LEN;
    /*via*/
    len+= MY_VIA_LEN + names_len[0] + 1+ port_no_str_len + MY_BRANCH_LEN + 3*sizeof(unsigned int) /*branch*/ + CRLF_LEN;
    /*headers*/
   for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
      if ( hdr->type==HDR_FROM || hdr->type==HDR_CALLID || hdr->type==HDR_CSEQ )
                 len += ((hdr->body.s+hdr->body.len ) - hdr->name.s ) ;
      else if ( hdr->type==HDR_TO )
                 len += ((r_msg->to->body.s+r_msg->to->body.len ) - r_msg->to->name.s ) ;

   /* CSEQ method : from INVITE-> ACK*/
   len -= 3;
   /* end of message */
   len += CRLF_LEN; /*new line*/

   ack_buf = (char *)malloc( len +1);
   if (!ack_buf)
   {
       LOG(L_ERR, "ERROR: t_build_and_send_ACK: cannot allocate memory\n");
       goto error;
   }

   p = ack_buf;
   DBG("DEBUG: t_build_and_send_ACK: len = %d \n",len);

   /* first line */
   memcpy( p , "ACK " , 4);
   p += 4;

   memcpy( p , p_msg->orig+(p_msg->first_line.u.request.uri.s-p_msg->buf) , p_msg->first_line.u.request.uri.len );
   p += p_msg->first_line.u.request.uri.len;

   *(p++) = ' ';

   memcpy( p , p_msg->orig+(p_msg->first_line.u.request.version.s-p_msg->buf) , p_msg->first_line.u.request.version.len );
   p += p_msg->first_line.u.request.version.len;

   memcpy( p, CRLF, CRLF_LEN );
   p+=CRLF_LEN;

   /* insert our via */
   memcpy( p , MY_VIA , MY_VIA_LEN );
   p += MY_VIA_LEN;

   memcpy( p , names[0] , names_len[0] );
   p += names_len[0];

  // *(p++) = ':';

   memcpy( p , port_no_str , port_no_str_len );
   p += port_no_str_len;

   memcpy( p, MY_BRANCH, MY_BRANCH_LEN );
   p+=MY_BRANCH_LEN;

   n=sprintf( p /*, ack_buf + MAX_ACK_LEN - p*/, ".%x.%x.%x%s",
                 Trans->hash_index, Trans->label, branch, CRLF );

   if (n==-1) {
	LOG(L_ERR, "ERROR: t_build_and_send_ACK: unable to generate branch\n");
	goto error;
   }
   p+=n;



   for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
   {
      if ( hdr->type==HDR_FROM || hdr->type==HDR_CALLID  )
	{
		memcpy( p , p_msg->orig+(hdr->name.s-p_msg->buf) ,
			((hdr->body.s+hdr->body.len ) - hdr->name.s ) );
		p += ((hdr->body.s+hdr->body.len ) - hdr->name.s );
	}
      else if ( hdr->type==HDR_TO )
	{
		memcpy( p , r_msg->orig+(r_msg->to->name.s-r_msg->buf) ,
			((r_msg->to->body.s+r_msg->to->body.len ) - r_msg->to->name.s ) );
		p += ((r_msg->to->body.s+r_msg->to->body.len ) - r_msg->to->name.s );
	}
       else if ( hdr->type==HDR_CSEQ )
	{
		memcpy( p , p_msg->orig+(hdr->name.s-p_msg->buf) ,
			(( ((struct cseq_body*)hdr->parsed)->method.s ) - hdr->name.s ) );
		p += (( ((struct cseq_body*)hdr->parsed)->method.s ) - hdr->name.s );
		memcpy( p , "ACK" CRLF, 3+CRLF_LEN );
		p += 3+CRLF_LEN;
	}
    }

    memcpy( p , CRLF , CRLF_LEN );
    p += CRLF_LEN;

   /* sends the ACK message to the same destination as the INVITE */
   udp_send( ack_buf, p-ack_buf, (struct sockaddr*)&(T->outbound_request[branch]->to) , sizeof(struct sockaddr_in) );
   DBG("DEBUG: t_build_and_send_ACK: ACK sent\n");

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

   /* computs the new timeout. */
   if ( r_buf->timeout_value<r_buf->timeout_ceiling )
      r_buf->timeout_value *=2;

   /* retransmision */
   DBG("DEBUG: retransmission_handler : resending\n");
   udp_send( r_buf->retr_buffer, r_buf->bufflen, (struct sockaddr*)&(r_buf->to) , sizeof(struct sockaddr_in) );

   /* re-insert into RETRASMISSIONS_LIST */
   insert_into_timer( hash_table , (&(r_buf->tl[RETRASMISSIONS_LIST])) , RETRASMISSIONS_LIST , r_buf->timeout_value );
}




void final_response_handler( void *attr)
{
   struct retrans_buff* r_buf = (struct retrans_buff*)attr;

   /* the transaction is already removed from FR_LIST by the timer */
   /* send a 408 */
   if ( r_buf->my_T->status<200)
   {
      DBG("DEBUG: final_response_handler : stop retransmission and send 408\n");
      remove_from_timer( hash_table , (&(r_buf->tl[RETRASMISSIONS_LIST])) , RETRASMISSIONS_LIST );
      t_send_reply( r_buf->my_T->inbound_request , 408 , "Request Timeout" );
   }
   else
   {
      /* put it on WT_LIST - transaction is over */
      DBG("DEBUG: final_response_handler : cansel transaction->put on wait\n");
      t_put_on_wait(  r_buf->my_T );
   }
}




void wait_handler( void *attr)
{
   struct cell *p_cell = (struct cell*)attr;

   /* the transaction is already removed from WT_LIST by the timer */
   /* the cell is removed from the hash table */
   DBG("DEBUG: wait_handler : removing from table ans stopping all timers\n");
   remove_from_hash_table( hash_table, p_cell );
   stop_RETR_and_FR_timers(hash_table,p_cell) ;
   /* put it on DEL_LIST - sch for del */
    add_to_tail_of_timer( hash_table, (&(p_cell->dele_tl)), DELETE_LIST, DEL_TIME_OUT );
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
       add_to_tail_of_timer( hash_table, (&(p_cell->dele_tl)), DELETE_LIST, DEL_TIME_OUT );
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
	if (n==-1) {
		LOG(L_ERR, "ERROR: add_branch_label: too small branch buffer\n");
		return -1;
	} else {
		p_msg->add_to_branch_len += n;
		DBG("DEBUG: XXX branch label created now: %*s (%d)\n", p_msg->add_to_branch_len,
			p_msg->add_to_branch_s );
		return 0;
	}
}




