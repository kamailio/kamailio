/*
 * $Id$
 *
 */

#include "hash_func.h"
#include "t_funcs.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser_f.h"
#include "../../ut.h"
#include "../../timer.h"


struct cell         *T;
unsigned int     global_msg_id;
struct s_table*  hash_table;




/* determine timer length and put on a correct timer list */
static inline void set_timer( struct s_table *hash_table,
	struct timer_link *new_tl, enum lists list_id )
{
	unsigned int timeout;
	static enum lists to_table[NR_OF_TIMER_LISTS] =
		{	FR_TIME_OUT, INV_FR_TIME_OUT, WT_TIME_OUT, DEL_TIME_OUT,
			RETR_T1, RETR_T1 << 1, 	RETR_T1 << 2, RETR_T2 };

	if (list_id<FR_TIMER_LIST || list_id>=NR_OF_TIMER_LISTS) {
		LOG(L_CRIT, "ERROR: set_timer: unkown list: %d\n", list_id);
#ifdef EXTRA_DEBUG
		abort();
#endif
		return;
	}
	timeout = to_table[ list_id ];
	add_to_tail_of_timer_list( &(hash_table->timers[ list_id ]),
		new_tl,get_ticks()+timeout);
}

/* remove from timer list */
static inline void reset_timer( struct s_table *hash_table,
	struct timer_link* tl )
{
	remove_from_timer_list( tl );
}

static inline void reset_retr_timers( struct s_table *h_table,
	struct cell *p_cell )
{
	int ijk; 
	struct retrans_buff *rb;

	DBG("DEBUG:stop_RETR_and_FR_timers : start \n");
	reset_timer( h_table, &(p_cell->outbound_response.retr_timer));
	reset_timer( h_table, &(p_cell->outbound_response.fr_timer));

	for( ijk=0 ; ijk<(p_cell)->nr_of_outgoings ; ijk++ )  { 
			if ( rb = p_cell->outbound_request[ijk] ) {
				reset_timer(h_table, &(rb->retr_timer));
				reset_timer(h_table, &(rb->fr_timer));
			}
		} 
	DBG("DEBUG:stop_RETR_and_FR_timers : stop\n");
}

int tm_startup()
{
   /* building the hash table*/
   hash_table = init_hash_table();
   if (!hash_table)
      return -1;

#define init_timer(_id,_handler) \
	hash_table->timers[(_id)].timeout_handler=(_handler); \
	hash_table->timers[(_id)].id=(_id);

   init_timer( RT_T1_TO_1, retransmission_handler );
   init_timer( RT_T1_TO_2, retransmission_handler );
   init_timer( RT_T1_TO_3, retransmission_handler );
   init_timer( RT_T2, retransmission_handler );
   init_timer( FR_TIMER_LIST, final_response_handler );
   init_timer( FR_INV_TIMER_LIST, final_response_handler );
   init_timer( WT_TIMER_LIST, wait_handler );
   init_timer( DELETE_LIST, delete_handler );

   /* register the timer function */
   register_timer( timer_routine , hash_table , 1 );

   /*first msg id*/
   global_msg_id = 0;
   T = T_UNDEFINED;

   return 0;
}




void tm_shutdown()
{
    struct timer_link  *tl, *end, *tmp;
    int i;

    DBG("DEBUG: tm_shutdown : start\n");
    /*remember the DELETE LIST */
    tl = hash_table->timers[DELETE_LIST].first_tl.next_tl;
	end = & hash_table->timers[DELETE_LIST].last_tl;
    /*unlink the lists*/
    for( i=0; i<NR_OF_TIMER_LISTS ; i++ )
    {
       //lock( hash_table->timers[i].mutex );
		reset_timer_list( hash_table, i );
       //unlock( hash_table->timers[i].mutex );
    }

    DBG("DEBUG: tm_shutdown : empting DELETE list\n");
    /* deletes all cells from DELETE_LIST list (they are no more accessible from enrys) */
	while (tl!=end) {
		tmp=tl->next_tl;
		free_cell((struct cell*)tl->payload);
		 tl=tmp;
	}

    /* destroy the hash table */
    DBG("DEBUG: tm_shutdown : empting hash table\n");
    free_hash_table( hash_table );
    DBG("DEBUG: tm_shutdown : removing semaphores\n");
	lock_cleanup();
    DBG("DEBUG: tm_shutdown : done\n");
}




/* function returns:
 *       1 - a new transaction was created
 *      -1 - error, including retransmission
 */
int t_add_transaction( struct sip_msg* p_msg, char* foo, char* bar )
{
   struct cell*    new_cell;

   DBG("DEBUG: t_add_transaction: adding......\n");

   /* sanity check: ACKs can never establish a transaction */
   if ( p_msg->REQ_METHOD==METHOD_ACK )
   {
       LOG(L_ERR, "ERROR: add_transaction: ACK can't be used to add transaction\n");
      return -1;
   }

   /* it's about the same transaction or not?*/
	if (t_check( p_msg , 0 )==-1) return -1;

   /* if the lookup's result is not 0 means that it's a retransmission */
   if ( T )
   {
      LOG(L_ERR,"ERROR: t_add_transaction: won't add a retransmission\n");
      return -1;
   }

   /* creates a new transaction */
   new_cell = build_cell( p_msg ) ;
   DBG("DEBUG: t_add_transaction: new transaction created %p\n", new_cell);
   if  ( !new_cell ){
	   LOG(L_ERR, "ERROR: add_transaction: out of mem:\n");
	   sh_status();
      return -1;
	}
   /*insert the transaction into hash table*/
   insert_into_hash_table( hash_table , new_cell );
   DBG("DEBUG: t_add_transaction: new transaction inserted, hash: %d\n", new_cell->hash_index );

   T = new_cell;
	T_REF(T);
   return 1;
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
   	char               *buf, *shbuf;
	struct retrans_buff *rb;


	buf=NULL;
	shbuf = NULL;
	branch = 0;	/* we don't do any forking right now */

	/* it's about the same transaction or not? */
	if (t_check( p_msg , 0 )==-1) return -1;

	/*if T hasn't been found after all -> return not found (error) */
	if ( !T )
	{
		DBG("DEBUG: t_forward: no transaction found for request forwarding\n");
		return -1;
	}

	/*if it's an ACK and the status is not final or is final, but error the
	ACK is not forwarded*/
	if ( p_msg->REQ_METHOD==METHOD_ACK  && (T->status/100)!=2 ) {
		DBG("DEBUG: t_forward: local ACK; don't forward\n");
		return 1;
	}

	/* if it's forwarded for the first time ; else the request is retransmited
	 * from the transaction buffer
	 * when forwarding an ACK, this condition will be all the time false because
	 * the forwarded INVITE is in the retransmission buffer */
	if ( T->outbound_request[branch]==NULL )
	{
		DBG("DEBUG: t_forward: first time forwarding\n");
		/* special case : CANCEL */
		if ( p_msg->REQ_METHOD==METHOD_CANCEL  )
		{
			DBG("DEBUG: t_forward: it's CANCEL\n");
			/* find original cancelled transaction; if found, use its
			   next-hops; otherwise use those passed by script */
			if ( T->T_canceled==T_UNDEFINED )
				T->T_canceled = t_lookupOriginalT( hash_table , p_msg );
			/* if found */
			if ( T->T_canceled!=T_NULL )
			{
				T->T_canceled->T_canceler = T;
				/* if in 1xx status, send to the same destination */
				if ( (T->T_canceled->status/100)==1 )
				{
					DBG("DEBUG: t_forward: it's CANCEL and I will send "
						"to the same place where INVITE went\n");
					dest_ip=T->T_canceled->outbound_request[branch]->
						to.sin_addr.s_addr;
					dest_port = T->T_canceled->outbound_request[branch]->
						to.sin_port;
				} else { /* transaction exists, but nothing to cancel */
               				DBG("DEBUG: t_forward: it's CANCEL but "
					"I have nothing to cancel here\n");
				/* continue forwarding CANCEL as a stand-alone transaction */
				}
			} else { /* transaction does not exists  */
				DBG("DEBUG: t_forward: canceled request not found! "
					"nothing to CANCEL\n");
			}
		}/* end special case CANCEL*/

		if ( add_branch_label( T, T->inbound_request , branch )==-1)
			goto error;
		if ( add_branch_label( T, p_msg , branch )==-1)
			goto error;
		if ( !(buf = build_req_buf_from_sip_req  ( p_msg, &len)))
			goto error;

		/* allocates a new retrans_buff for the outbound request */
		DBG("DEBUG: t_forward: building outbound request\n");
		shm_lock();
		T->outbound_request[branch] = rb =
			(struct retrans_buff*)shm_malloc_unsafe( sizeof(struct retrans_buff)  );
		if (!rb)
		{
			LOG(L_ERR, "ERROR: t_forward: out of shmem\n");
			shm_unlock();
			goto error;
		}
		shbuf = (char *) shm_malloc_unsafe( len );
		if (!shbuf)
		{
			LOG(L_ERR, "ERROR: t_forward: out of shmem buffer\n");
			shm_unlock();
			goto error;
		}
		shm_unlock();
		memset( rb , 0 , sizeof (struct retrans_buff) );
		rb->retr_buffer = shbuf;
		rb->retr_timer.payload =  rb;
		rb->fr_timer.payload =  rb;
		rb->to.sin_family = AF_INET;
		rb->my_T =  T;
		T->nr_of_outgoings = 1;
		rb->bufflen = len ;
		memcpy( rb->retr_buffer , buf , len );
		free( buf ) ; buf=NULL;

		DBG("DEBUG: t_forward: starting timers (retrans and FR) %d\n",get_ticks() );
		/*sets and starts the FINAL RESPONSE timer */
		set_timer( hash_table, &(rb->fr_timer), FR_TIMER_LIST );

		/* sets and starts the RETRANS timer */
		rb->retr_list = RT_T1_TO_1;
		set_timer( hash_table, &(rb->retr_timer), RT_T1_TO_1 );
	}/* end for the first time */

	/* if we are forwarding an ACK*/
	if (  p_msg->REQ_METHOD==METHOD_ACK &&
		T->relaied_reply_branch>=0 &&
		T->relaied_reply_branch<=T->nr_of_outgoings)
	{
		DBG("DEBUG: t_forward: forwarding ACK [%d]\n",T->relaied_reply_branch);
		t_build_and_send_ACK( T, branch ,
			T->inbound_response[T->relaied_reply_branch] );
		T->inbound_request_isACKed = 1;
		return 1;
	} else /* if we are forwarding a CANCEL*/
	if (  p_msg->REQ_METHOD==METHOD_CANCEL )
	{
		DBG("DEBUG: t_forward: forwarding CANCEL\n");
		/* if no transaction to CANCEL
		  or if the canceled transaction has a final status -> drop the CANCEL*/
		if ( T->T_canceled==T_NULL || T->T_canceled->status>=200)
		{
			reset_timer( hash_table, &(rb->fr_timer ));
			reset_timer( hash_table, &(rb->retr_timer ));
		return 1;
		}
	}

	/* send the request */
	/* known to be in network order */
	rb->to.sin_port     =  dest_port;
	rb->to.sin_addr.s_addr =  dest_ip;
	rb->to.sin_family = AF_INET;

	SEND_BUFFER( rb );

   return 1;

error:
	if (shbuf) shm_free(shbuf);
	if (rb) {
		shm_free(rb);
		T->outbound_request[branch]=NULL;
	}
	if (buf) free( buf );

	return -1;

}


/* Forwards the inbound request to dest. from via.  Returns:
 *       1 - forward successfull
 *      -1 - error during forward
 */
int t_forward_uri( struct sip_msg* p_msg, char* foo, char* bar  )
{
   unsigned int     ip, port;

   if ( get_ip_and_port_from_uri( p_msg , &ip, &port)<0 )
   {
      LOG( L_ERR , "ERROR: t_forward_uri: unable to extarct ip and port from uri!\n" );
      return -1;
   }

   return t_forward( p_msg , ip , port );
}




/*  This function is called whenever a reply for our module is received; we need to register
  *  this function on module initialization;
  *  Returns :   0 - core router stops
  *                    1 - core router relay statelessly
  */
int t_on_reply_received( struct sip_msg  *p_msg )
{
	unsigned int  branch,len, msg_status, msg_class;
	struct sip_msg *clone;
	int relay;
	struct retrans_buff *rb;

	clone=NULL;

	/* if a reply received which has not all fields we might want to
	   have for stateul forwarding, give the stateless router
	   a chance for minimum routing; parse only what's needed
	   for MPLS-ize reply matching
	*/
	if (t_check( p_msg  , &branch )==-1) return 1;

	/* if no T found ->tell the core router to forward statelessly */
	if ( T<=0 )
		return 1;
	DBG("DEBUG: t_on_reply_received: Original status =%d\n",T->status);

	/* we were not able to process the response due to memory
	   shortage; simply drop it; hopefuly, we will have more
	memory on the next try */
	msg_status=p_msg->REPLY_STATUS;
	msg_class=REPLY_CLASS(p_msg);
	relay = t_should_relay_response( T , msg_status );

	if (relay && !(clone=sip_msg_cloner( p_msg ))) {
		T_UNREF( T );
		return 0;
	}

	rb=T->outbound_request[branch];

	/* stop retransmission */
	reset_timer( hash_table, &(rb->retr_timer));

	/* stop final response timer only if I got a final response */
	if ( msg_class>1 )
		reset_timer( hash_table, &(rb->fr_timer));
	/* if a got the first prov. response for an INVITE ->
	   change FR_TIME_OUT to INV_FR_TIME_UT */
	if (!T->inbound_response[branch] && msg_class==1
	 && T->inbound_request->REQ_METHOD==METHOD_INVITE )
		set_timer( hash_table, &(rb->fr_timer), FR_INV_TIMER_LIST );

	/* get response for INVITE */
	if ( T->inbound_request->REQ_METHOD==METHOD_INVITE )
	{
		if ( T->outbound_request_isACKed[branch] )
		{	/*retransmit the last ACK*/
			DBG("DEBUG: t_on_reply_received: retransmitting ACK!!!!!!!!!!!!!!!!!!+!+!+!!\n");
			SEND_BUFFER( T->outbound_request[branch] );
		} else if (msg_class>2 ) {   /*on a non-200 reply to INVITE*/
			DBG("DEBUG: t_on_reply_received: >=3xx reply to INVITE: send ACK\n");
			if ( t_build_and_send_ACK( T , branch , p_msg )==-1)
			{
				LOG( L_ERR , "ERROR: t_on_reply_received: unable to send ACK\n" );
				if (clone ) sip_msg_free( clone );
				T_UNREF( T );
				return 0;
			}
		}
	}

#	ifdef FORKING
   	/* skipped for the moment*/
#	endif

	/* if the incoming response code is not reliable->drop it*/
	if (!relay) {
		T_UNREF( T );
		return 0;
	}

	/* restart retransmission if provisional response came for a non_INVITE ->
		retrasmit at RT_T2*/
	if ( msg_class==1 && T->inbound_request->REQ_METHOD!=METHOD_INVITE )
	{
		rb->retr_list = RT_T2;
		set_timer( hash_table, &(rb->retr_timer), RT_T2 );
	}

	/*store the inbound reply - if there is a previous reply, replace it */
	if ( T->inbound_response[branch] ) {
		sip_msg_free( T->inbound_response[branch] ) ;
		DBG("DEBUG: t_store_incoming_reply: previous inbound reply freed....\n");
	}
	T->inbound_response[branch] = clone;

	if ( msg_class>=3 && msg_class<=5 )
	{
		if ( t_all_final(T) && relay_lowest_reply_upstream( T , p_msg )==-1 && clone )
			goto error;
	} else {
		if (push_reply_from_uac_to_uas( T , branch )==-1 && clone )
			goto error;
	}

	/* nothing to do for the ser core */
	/* t_unref( p_msg, NULL, NULL ); */
	T_UNREF( T );
	return 0;

error:
	/* t_unref( p_msg, NULL, NULL ); */
	T_UNREF( T );
	T->inbound_response[branch]=NULL;
	sip_msg_free( clone );
	/* don't try to relay statelessly on error */
	return 0;
}


int t_on_request_received( struct sip_msg  *p_msg , unsigned int ip , unsigned int port)
{
	if ( t_check( p_msg , 0 ) )
	{
		if ( p_msg->first_line.u.request.method_value==METHOD_ACK )
		{
			LOG( L_INFO , "SER: ACK received -> t_release\n");
			if ( !t_forward( p_msg , ip , port ) )
			{
				LOG( L_WARN, "SER: WARNING: bad forward\n");
			}
			if ( !t_release_transaction( p_msg ) )
			{
				LOG( L_WARN ,"SER: WARNING: bad t_release\n");
			}
		}
		else
		{
			if ( !t_retransmit_reply( p_msg , 0, 0) )
			{
				LOG( L_WARN, "SER: WARNING: bad t_retransmit_reply\n");
			}
			LOG( L_INFO, "SER: yet another annoying retranmission\n");
		}
		t_unref( p_msg,0,0 );
	} else {
		if ( p_msg->first_line.u.request.method_value==METHOD_ACK )
		{
			LOG( L_INFO , "SER: forwarding ACK  statelessly\n");
			/* no established transaction ... forward ACK just statelessly*/
			forward_request( p_msg , mk_proxy_from_ip(ip,port) );
		}
		else
		{
			/* establish transaction*/
			if ( !t_add_transaction(p_msg,0,0) )
			{
				LOG( L_ERR , "ERROR in ser: t_add_transaction\n");
			}
			/* reply */
			if ( p_msg->first_line.u.request.method_value==METHOD_CANCEL)
			{
				LOG( L_INFO, "SER: new CANCEL\n");
				if ( !t_send_reply( p_msg , 200, "glad to cancel") )
				{
					LOG( L_ERR ,"SER:ERROR: t_send_reply\n");
				}
			} else {
				LOG( L_INFO, "SER: new transaction\n");
				if ( !t_send_reply( p_msg , 100 , "trying -- your call is important to us") )
				{
					LOG( L_ERR, "SER: ERROR: t_send_reply (100)\n");
				}
			}
			if ( !t_forward( p_msg, ip, port ) )
			{
				LOG( L_ERR , "SER:ERROR: t_forward \n");
			}
			t_unref( p_msg , 0 , 0);
		}
	}

}




int t_on_request_received_uri( struct sip_msg  *p_msg )
{
   unsigned int     ip, port;

   if ( get_ip_and_port_from_uri( p_msg , &ip, &port)<0 )
   {
      LOG( L_ERR , "ERROR: t_on_request_received_uri: unable to extract ip and port from uri!\n" );
      return -1;
   }

   return t_on_request_received( p_msg , ip , port );
}




/*   returns 1 if everything was OK or -1 for error
  */
int t_release_transaction( struct sip_msg* p_msg)
{
	if (t_check( p_msg  , 0 )==-1) return 1;

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
	if (t_check( p_msg  , 0 )==-1) return 1;

   /* if no transaction exists or no reply to be resend -> out */
   if ( T )
   {
	SEND_BUFFER( & T->outbound_response );
	return 1;
   }

  /* no transaction found */
   return -1;
}




int t_unref( struct sip_msg* p_msg, char* foo, char* bar )
{
	if (T==T_UNDEFINED || T==T_NULL)
		return -1;
	T_UNREF( T );
	T=T_UNDEFINED;
	return 1;
}




/* Force a new response into inbound response buffer.
  * returns 1 if everything was OK or -1 for erro
  */
int t_send_reply(  struct sip_msg* p_msg , unsigned int code , char * text )
{
	unsigned int len, buf_len;
	char * buf;
	struct retrans_buff *rb;

	DBG("DEBUG: t_send_reply: entered\n");
	if (t_check( p_msg , 0 )==-1) return -1;

	if (!T)
	{
		LOG(L_ERR, "ERROR: t_send_reply: cannot send a t_reply to a message "
			"for which no T-state has been established\n");
		return -1;
	}

	rb = & T->outbound_response;
	if (!rb->retr_buffer) {
		/* initialize retransmission structure */
		memset( rb , 0 , sizeof (struct retrans_buff) );
		if (update_sock_struct_from_via(  &(rb->to),  p_msg->via1 )==-1)
		{
			LOG(L_ERR, "ERROR: t_send_reply: cannot lookup reply dst: %s\n",
				p_msg->via1->host.s );
			goto error;
		}

		rb->retr_timer.payload = rb;
		rb->fr_timer.payload = rb;
		rb->to.sin_family = AF_INET;
		rb->my_T = T;
	}

	buf = build_res_buf_from_sip_req( code , text , T->inbound_request , &len );
	DBG("DEBUG: t_send_reply: buffer computed\n");
	if (!buf)
	{
		DBG("DEBUG: t_send_reply: response building failed\n");
		goto error;
	}

	/* if this is a first reply (?100), longer replies will probably follow;
       try avoiding shm_resize by higher buffer size
    */
	buf_len = rb->retr_buffer ? len : len + REPLY_OVERBUFFER_LEN;

	if (! (rb->retr_buffer = (char*)shm_resize( rb->retr_buffer, buf_len )))
	{
		LOG(L_ERR, "ERROR: t_send_reply: cannot allocate shmem buffer\n");
		goto error2;
	}
	rb->bufflen = len ;
	memcpy( rb->retr_buffer , buf , len );
	free( buf ) ;
	T->status = code;

	/* start/stops the proper timers*/
	DBG("DEBUG: t_send_reply: update timers\n");
	t_update_timers_after_sending_reply( rb );

	DBG("DEBUG: t_send_reply: send reply\n");
	/* t_retransmit_reply( p_msg, 0 , 0); */
	SEND_BUFFER( rb );

	return 1;

error2:
	free ( buf );
error:
	return -1;
}



/* Push a previously stored reply from UA Client to UA Server
  * and send it out
  */
int push_reply_from_uac_to_uas( struct cell* trans , unsigned int branch )
{
	char *buf;
	unsigned int len, buf_len;
	struct retrans_buff *rb;

	DBG("DEBUG: push_reply_from_uac_to_uas: start\n");
	rb= & trans->outbound_response;
	/* if there is a reply, release the buffer (everything else stays same) */
	if ( ! rb->retr_buffer ) {
		/*init retrans buffer*/
		memset( rb , 0 , sizeof (struct retrans_buff) );
		if (update_sock_struct_from_via(  &(rb->to),
			trans->inbound_response[branch]->via2 )==-1) {
				LOG(L_ERR, "ERROR: push_reply_from_uac_to_uas: "
					"cannot lookup reply dst: %s\n",
				trans->inbound_response[branch]->via2->host.s );
				goto error;
		}
		rb->retr_timer.payload = rb;
		rb->fr_timer.payload =  rb;
		rb->to.sin_family = AF_INET;
		rb->my_T = trans;

	} else {
		reset_timer( hash_table, &(rb->retr_timer));
		reset_timer( hash_table, &(rb->fr_timer));
	}

	/*  generate the retrans buffer */
	buf = build_res_buf_from_sip_res ( trans->inbound_response[branch], &len);
	if (!buf) {
		LOG(L_ERR, "ERROR: push_reply_from_uac_to_uas: "
			"no shmem for outbound reply buffer\n");
		goto error;
	}

	/* if this is a first reply (?100), longer replies will probably follow;
	try avoiding shm_resize by higher buffer size */
	buf_len = rb->retr_buffer ? len : len + REPLY_OVERBUFFER_LEN;
	if (! (rb->retr_buffer = (char*)shm_resize( rb->retr_buffer, buf_len )))
	{
		LOG(L_ERR, "ERROR: t_send_reply: cannot allocate shmem buffer\n");
		goto error1;
	}
	rb->bufflen = len ;
	memcpy( rb->retr_buffer , buf , len );
	free( buf ) ;

	/* update the status*/
	trans->status = trans->inbound_response[branch]->REPLY_STATUS;
	if ( trans->inbound_response[branch]->REPLY_STATUS>=200 &&
	trans->relaied_reply_branch==-1 )
		trans->relaied_reply_branch = branch;

	/* start/stops the proper timers*/
	t_update_timers_after_sending_reply( rb );

	/*send the reply*/
	/* t_retransmit_reply( trans->inbound_response[branch], 0 , 0 ); */
	SEND_BUFFER( rb );
	return 1;

error1:
	free( buf );
error:
	return -1;
}





/* ----------------------------HELPER FUNCTIONS-------------------------------- */




/*  Checks if all the transaction's outbound request has a final response.
  *  Return   1 - all are final
  *                0 - some waitting
  */
int t_all_final( struct cell *Trans )
{
   unsigned int i;

	for( i=0 ; i<Trans->nr_of_outgoings ; i++  )
		if (  !Trans->inbound_response[i] ||
		Trans->inbound_response[i]->REPLY_STATUS<=200 )
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
	   T->inbound_response[i]->REPLY_STATUS>=200 &&
	   T->inbound_response[i]->REPLY_STATUS<lowest_v )
      {
         lowest_i =i;
         lowest_v = T->inbound_response[i]->REPLY_STATUS;
      }

   DBG("DEBUG: relay_lowest_reply_upstream: lowest reply [%d]=%d\n",lowest_i,lowest_v);

   if ( lowest_i != -1 && push_reply_from_uac_to_uas( T ,lowest_i ) == -1 )
	return -1;

   return lowest_i;
}




/*
  */
int t_update_timers_after_sending_reply( struct retrans_buff *rb )
{
	struct cell *Trans = rb->my_T;

	/* make sure that if we send something final upstream, everything else
	   will be cancelled */
	if (Trans->status>=300 && Trans->inbound_request->REQ_METHOD==METHOD_INVITE )
	{
		rb->retr_list = RT_T1_TO_1;
		set_timer( hash_table, &(rb->retr_timer), RT_T1_TO_1 );
		set_timer( hash_table, &(rb->fr_timer), FR_TIMER_LIST );
   	} else if ( Trans->inbound_request->REQ_METHOD==METHOD_CANCEL ) {
		if ( Trans->T_canceled==T_UNDEFINED )
			Trans->T_canceled = t_lookupOriginalT( hash_table ,
				Trans->inbound_request );
      		if ( Trans->T_canceled==T_NULL )
            		return 1;
      		Trans->T_canceled->T_canceler = Trans;
     		/* put CANCEL transaction on wait only if canceled transaction already
        	   is in final status and there is nothing to cancel;
     		*/
     		if ( Trans->T_canceled->status>=200)
            		t_put_on_wait( Trans );
   	} else if (Trans->status>=200)
            t_put_on_wait( Trans );
   return 1;
}




/* Checks if the new reply (with new_code status) should be sent or not
 *  based on the current
  * transactin status.
  * Returns 1 - the response can be sent
  *         0 - is not indicated to sent
  */
int t_should_relay_response( struct cell *Trans , int new_code )
{
	int T_code;

	T_code = Trans->status;

	if ( T_code >= 200 ) { /* if final response sent out ... */
		if (new_code>=200 && new_code < 300  && /* relay only 2xx */
			Trans->inbound_request->REQ_METHOD==METHOD_INVITE) {
			DBG("DBG: t_should_relay: 200 INV after final sent\n");
			return 1;
		}
	} else { /* no final response sent yet */
		if (new_code!=100) { /* all but "100 trying" */
			DBG("DBG: t_should_relay: !=100 -> relay\n");
			return 1;
		}
	}
	DBG("DBG: t_should_relay: not to be relayed\n");
	return 0;
}




/*
  */
int t_put_on_wait(  struct cell  *Trans  )
{
	struct timer_link *tl;
	unsigned int i;
	if (is_in_timer_list2( &(Trans->wait_tl)))
  	{
		DBG("DEBUG: t_put_on_wait: already on wait\n");
		return 1;
	}

	DBG("DEBUG: t_put_on_wait: stopping timers (FR and RETR)\n");
	/**/
	for( i=0 ; i<Trans->nr_of_outgoings ; i++ )
		if ( Trans->inbound_response[i] && 
		REPLY_CLASS(Trans->inbound_response[i])==1)
		t_cancel_branch(i);

	/* make double-sure we have finished everything */
	/* remove from  retranssmision  and  final response   list */
	reset_retr_timers(hash_table,Trans) ;
	/* adds to Wait list*/
	set_timer( hash_table, &(Trans->wait_tl), WT_TIMER_LIST );
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
   struct hdr_field *hdr;
   char *ack_buf, *p, *via;
   unsigned int len, via_len;
   int n;

   ack_buf = 0;
   via =0;

   p_msg = Trans->inbound_request;
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
    via = via_builder( p_msg , &via_len );
    if (!via)
    {
	LOG(L_ERR, "ERROR: t_build_and_send_ACK: no via header got from builder\n");
	goto error;
    }
    len+= via_len;
    /*headers*/
   for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
      if ( hdr->type==HDR_FROM || hdr->type==HDR_CALLID || hdr->type==HDR_CSEQ )
                 len += ((hdr->body.s+hdr->body.len ) - hdr->name.s ) + CRLF_LEN ;
      else if ( hdr->type==HDR_TO )
                 len += ((r_msg->to->body.s+r_msg->to->body.len ) - r_msg->to->name.s ) + CRLF_LEN ;
      /*else if ( hdr->type==HDR_)*/

   /* CSEQ method : from INVITE-> ACK */
   len -= 3  ;
   /* end of message */
   len += CRLF_LEN; /*new line*/

   ack_buf = (char *)pkg_malloc( len +1);
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
   memcpy( p , via , via_len );
   p += via_len;

   /*other headers*/
   for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
   {
      if ( hdr->type==HDR_FROM || hdr->type==HDR_CALLID  )
	{
		memcpy( p , p_msg->orig+(hdr->name.s-p_msg->buf) ,
			((hdr->body.s+hdr->body.len ) - hdr->name.s ) );
		p += ((hdr->body.s+hdr->body.len ) - hdr->name.s );
		memcpy( p, CRLF, CRLF_LEN );
		p+=CRLF_LEN;
	}
      else if ( hdr->type==HDR_TO )
	{
		memcpy( p , r_msg->orig+(r_msg->to->name.s-r_msg->buf) ,
			((r_msg->to->body.s+r_msg->to->body.len ) - r_msg->to->name.s ) );
		p += ((r_msg->to->body.s+r_msg->to->body.len ) - r_msg->to->name.s );
		memcpy( p, CRLF, CRLF_LEN );
		p+=CRLF_LEN;
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
   udp_send( ack_buf, p-ack_buf, (struct sockaddr*)&(Trans->outbound_request[branch]->to) , sizeof(struct sockaddr_in) );

   /* registering the ACK as received, processed and send */
   Trans->outbound_request_isACKed[branch] = 1;
   if ( (Trans->outbound_request[branch]->retr_buffer =
      (char*)shm_resize( Trans->outbound_request[branch]->retr_buffer, p-ack_buf) ))
   {
       memcpy ( Trans->outbound_request[branch]->retr_buffer , ack_buf , p-ack_buf);
       Trans->outbound_request[branch]->bufflen = p-ack_buf;
   }
   else
       Trans->outbound_request[branch]->bufflen = 0;


   DBG("DEBUG: t_build_and_send_ACK: ACK sent\n");

   /* free mem*/
   if (ack_buf) pkg_free( ack_buf );
   if (via) pkg_free(via );
   return 0;

error:
   if (ack_buf) free( ack_buf );
   if (via) pkg_free(via );
   return -1;
}


void delete_cell( struct cell *p_cell )
{
#ifdef EXTRA_DEBUG
	int i;

	if (is_in_timer_list2(& p_cell->wait_tl )) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and still on WAIT\n",
			p_cell);
		abort();
	}
	if (is_in_timer_list2(& p_cell->outbound_response.retr_timer )) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and still on RETR (rep)\n",
			p_cell);
		abort();
	}
	if (is_in_timer_list2(& p_cell->outbound_response.fr_timer )) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and still on FR (rep)\n",
			p_cell);
		abort();
	}
	for (i=0; i<p_cell->nr_of_outgoings; i++) {
		if (is_in_timer_list2(& p_cell->outbound_request[i]->retr_timer)) {
			LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and still on RETR (req %d)\n",
			p_cell, i);
			abort();
		}
		if (is_in_timer_list2(& p_cell->outbound_request[i]->fr_timer)) {
			LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and still on FR (req %d)\n",
			p_cell, i);
			abort();
		}
	}
#endif
	/* still in use ... don't delete */
	if ( T_IS_REFED(p_cell) ) {
#ifdef	EXTRA_DEBUG
		if (T_REFCOUNTER(p_cell)>1) {
			DBG("DEBUG: while debugging with a single process, ref_count > 1\n");
			DBG("DEBUG: transaction =%p\n", p_cell );
			abort();
		}
#endif
		DBG("DEBUG: delete_cell: t=%p post for delete (refbitmap %x, refcount %d)\n",
			p_cell,p_cell->ref_bitmap, T_REFCOUNTER(p_cell));
		/* it's added to del list for future del */
		set_timer( hash_table, &(p_cell->dele_tl), DELETE_LIST );
	} else {
		DBG("DEBUG: delete_handler : delete transaction %p\n", p_cell );
		free_cell( p_cell );
	}
}


/* Returns  -1 = error
                    0 = OK
*/
int get_ip_and_port_from_uri( struct sip_msg* p_msg , unsigned int *param_ip, unsigned int *param_port)
{
   struct hostent  *nhost;
   unsigned int     ip, port;
   struct sip_uri    parsed_uri;
   str                      uri;
   int                      err;

   /* the original uri has been changed? */
   if (p_msg->new_uri.s==0 || p_msg->new_uri.len==0)
     uri = p_msg->first_line.u.request.uri;
   else
     uri = p_msg->new_uri;

   /* parsing the request uri in order to get host and port */
   if (parse_uri( uri.s , uri.len , &parsed_uri )<0)
   {
        LOG(L_ERR, "ERROR: get_ip_and_port_from_uri: unable to parse destination uri\n");
        return  -1;
   }

   /* getting host address*/
   nhost = gethostbyname( parsed_uri.host.s );

   if ( !nhost )
   {
      LOG(L_ERR, "ERROR: get_ip_and_port_from_uri: cannot resolve host\n");
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
           LOG(L_ERR, "ERROR: get_ip_and_port_from_uri: converting port from str to int failed; using default SIP port\n");
           port = SIP_PORT;
       }
   }
   port = htons( port );

   free_uri( &parsed_uri );

   *param_ip = ip;
   *param_port = port;
   return 0;
}






/*---------------------TIMEOUT HANDLERS--------------------------*/


void retransmission_handler( void *attr)
{
	struct retrans_buff* r_buf ;
	enum lists id;

	r_buf = (struct retrans_buff*)attr;
#ifdef EXTRA_DEBUG
	if (r_buf->my_T->damocles) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and called from RETR timer\n",
			r_buf->my_T);
		abort();
	}	
#endif


	/*the transaction is already removed from RETRANSMISSION_LIST by timer*/

	/* retransmision */
	DBG("DEBUG: retransmission_handler : resending (t=%p)\n", r_buf->my_T);
	SEND_BUFFER( r_buf );

	id = r_buf->retr_list;
	r_buf->retr_list = id < RT_T2 ? id + 1 : RT_T2;

	set_timer( hash_table, &(r_buf->retr_timer), id < RT_T2 ? id + 1 : RT_T2 );

	DBG("DEBUG: retransmission_handler : done\n");
}




void final_response_handler( void *attr)
{
	struct retrans_buff* r_buf = (struct retrans_buff*)attr;

#ifdef EXTRA_DEBUG
	if (r_buf->my_T->damocles) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and called from FR timer\n",
			r_buf->my_T);
		abort();
	}	
#endif

	/* the transaction is already removed from FR_LIST by the timer */
	/* send a 408 */
	if ( r_buf->my_T->status<200)
	{
		DBG("DEBUG: final_response_handler:stop retransmission and send 408 (t=%p)\n", r_buf->my_T);
		reset_timer( hash_table, &(r_buf->retr_timer) );
		/* dirty hack: t_send_reply would increase ref_count which would indeed
		   result in refcount++ which would not -- until timer processe's
		   T changes again; currently only on next call to t_send_reply from
		   FR timer; thus I fake the values now to avoid recalculating T
		   and refcount++

			-jku
	    */
		T=r_buf->my_T;
		global_msg_id=T->inbound_request->id;

		t_send_reply( r_buf->my_T->inbound_request , 408 , "Request Timeout" );
	} else {
		/* put it on WT_LIST - transaction is over */
		DBG("DEBUG: final_response_handler:cancel transaction->put on wait (t=%p)\n", r_buf->my_T);
		t_put_on_wait(  r_buf->my_T );
	}
	DBG("DEBUG: final_response_handler : done\n");
}




void wait_handler( void *attr)
{
	struct cell *p_cell = (struct cell*)attr;

#ifdef EXTRA_DEBUG
	if (p_cell->damocles) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and called from WAIT timer\n",
			p_cell);
		abort();
	}	
#endif

	/* the transaction is already removed from WT_LIST by the timer */
	/* the cell is removed from the hash table */
	DBG("DEBUG: wait_handler : removing %p from table \n", p_cell );
	remove_from_hash_table( hash_table, p_cell );
	DBG("DEBUG: wait_handler : stopping all timers\n");
	reset_retr_timers(hash_table,p_cell) ;
	/* put it on DEL_LIST - sch for del */
#ifdef EXTRA_DEBUG
	p_cell->damocles = 1;
#endif
	delete_cell( p_cell );
	DBG("DEBUG: wait_handler : done\n");
}


void delete_handler( void *attr)
{
	struct cell *p_cell = (struct cell*)attr;

	DBG("DEBUG: delete_handler : removing %p \n", p_cell );
#ifdef EXTRA_DEBUG
	if (p_cell->damocles==0) {
		LOG( L_ERR, "ERROR: transaction %p not scheduled for deletion and called from DELETE timer\n",
			p_cell);
		abort();
	}	
#endif
	delete_cell( p_cell );
    DBG("DEBUG: delete_handler : done\n");
}
