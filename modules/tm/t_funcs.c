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


#define  append_mem_block(_d,_s,_len) \
		do{\
			memcpy((_d),(_s),(_len));\
			(_d) += (_len);\
		}while(0);
#define  req_line(_msg) \
		((_msg)->first_line.u.request)



struct cell         *T;
unsigned int     global_msg_id;
struct s_table*  hash_table;



/* determine timer length and put on a correct timer list */
inline void set_timer( struct s_table *hash_table,
	struct timer_link *new_tl, enum lists list_id )
{
	unsigned int timeout;
	struct timer* list;
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
	list= &(hash_table->timers[ list_id ]);
/*
	add_to_tail_of_timer_list( &(hash_table->timers[ list_id ]),
		new_tl,get_ticks()+timeout);
*/
	lock(list->mutex);
	/* make sure I'm not already on a list */
	remove_timer_unsafe( new_tl );
	add_timer_unsafe( list, new_tl, get_ticks()+timeout);
	unlock(list->mutex);
}

/* remove from timer list */
inline void reset_timer( struct s_table *hash_table,
	struct timer_link* tl )
{
	/* lock(timer_group_lock[ tl->tg ]); */
	/* hack to work arround this timer group thing*/
	lock(hash_table->timers[timer_group[tl->tg]].mutex);
	remove_timer_unsafe( tl );
	unlock(hash_table->timers[timer_group[tl->tg]].mutex);
	/*unlock(timer_group_lock[ tl->tg ]);*/
}

static inline void reset_retr_timers( struct s_table *h_table,
	struct cell *p_cell )
{
	int ijk;
	struct retrans_buff *rb;

	DBG("DEBUG:stop_RETR_and_FR_timers : start \n");
	/* lock the first timer list of the FR group -- all other
	   lists share the same lock*/
	lock(hash_table->timers[RT_T1_TO_1].mutex);
	remove_timer_unsafe( & p_cell->outbound_response.retr_timer );
	for( ijk=0 ; ijk<(p_cell)->nr_of_outgoings ; ijk++ )  {
			if ( rb = p_cell->outbound_request[ijk] ) {
				remove_timer_unsafe( & rb->retr_timer );
			}
		}
	unlock(hash_table->timers[RT_T1_TO_1].mutex);

	lock(hash_table->timers[FR_TIMER_LIST].mutex);
	remove_timer_unsafe( & p_cell->outbound_response.fr_timer );
	for( ijk=0 ; ijk<(p_cell)->nr_of_outgoings ; ijk++ )  {
			if ( rb = p_cell->outbound_request[ijk] ) {
				remove_timer_unsafe( & rb->fr_timer );
			}
		}
	unlock(hash_table->timers[FR_TIMER_LIST].mutex);
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
int t_add_transaction( struct sip_msg* p_msg )
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
	/* if (t_check( p_msg , 0 )==-1) return -1; */

   /* if the lookup's result is not 0 means that it's a retransmission */
   /* if ( T )
   {
      LOG(L_ERR,"ERROR: t_add_transaction: won't add a retransmission\n");
      return -1;
   } */

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



#ifdef _OBSOLETED_TM
/* function returns:
 *       1 - forward successfull
 *      -1 - error during forward
 */
int t_forward( struct sip_msg* p_msg , unsigned int dest_ip_param , unsigned int dest_port_param )
{
	unsigned int  dest_ip     = dest_ip_param;
	unsigned int  dest_port  = dest_port_param;
	int                  branch;
	unsigned int  len;
	char               *buf, *shbuf;
	struct retrans_buff  *rb;
	struct cell      *T_source = T;

	buf=NULL;
	shbuf = NULL;
	branch = 0;	/* we don't do any forking right now */

	/* it's about the same transaction or not? */
	/* if (t_check( p_msg , 0 )==-1) return -1; */

	/*if T hasn't been found after all -> return not found (error) */
	/*
	if ( !T )
	{
		DBG("DEBUG: t_forward: no transaction found for request forwarding\n");
		return -1;
	}
	*/

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
			if (T->T_canceled==T_UNDEFINED)
				T->T_canceled = t_lookupOriginalT( hash_table , p_msg );
			/* if found */
			if ( T->T_canceled!=T_NULL )
			{
				/* if in 1xx status, send to the same destination */
				if ( (T->T_canceled->status/100)==1 )
				{
					DBG("DEBUG: t_forward: it's CANCEL and I will send "
						"to the same place where INVITE went\n");
					dest_ip=T->T_canceled->outbound_request[branch]->
						to.sin_addr.s_addr;
					dest_port = T->T_canceled->outbound_request[branch]->
						to.sin_port;
#ifdef USE_SYNONIM
					T_source = T->T_canceled;
					T->label  = T->T_canceled->label;
#endif
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

		if ( add_branch_label( T_source, T->inbound_request , branch )==-1)
			goto error;
		if ( add_branch_label( T_source, p_msg , branch )==-1)
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
		rb->retr_timer.tg=TG_RT;
		rb->fr_timer.tg=TG_FR;
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
		DBG("DEBUG: t_forward: forwarding CANCEL \n");
		/* if no transaction to CANCEL
		    or if the canceled transaction has a final status -> drop the CANCEL*/
		if ( T->T_canceled!=T_NULL && T->T_canceled->status>=200)
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
int t_forward_uri( struct sip_msg* p_msg  )
{
   unsigned int     ip, port;

   if ( get_ip_and_port_from_uri( p_msg , &ip, &port)<0 )
   {
      LOG( L_ERR , "ERROR: t_forward_uri: unable to parse uri!\n");
      return -1;
   }

   return t_forward( p_msg , ip , port );
}
#endif


#ifdef _OBSOLETED_TM
int t_on_request_received( struct sip_msg  *p_msg , 
	unsigned int ip , unsigned int port)
{
	if ( t_check( p_msg , 0 ) )
	{
		if ( p_msg->first_line.u.request.method_value==METHOD_ACK )
		{
			DBG( "SER: ACK received -> t_release\n");
			if ( !t_forward( p_msg , ip , port ) )
			{
				DBG( "SER: WARNING: bad forward\n");
			}
			if ( !t_release_transaction( p_msg ) )
			{
				DBG( "SER: WARNING: bad t_release\n");
			}
		}
		else
		{
			if ( !t_retransmit_reply( p_msg ) )
			{
				DBG( "SER: WARNING: bad t_retransmit_reply\n");
			}
			DBG( "SER: yet another annoying retranmission\n");
		}
		t_unref( /* p_msg */ );
	} else {
		if ( p_msg->first_line.u.request.method_value==METHOD_ACK )
		{
			DBG( "SER: forwarding ACK  statelessly\n");
			/* no established transaction ... forward ACK just statelessly*/
			forward_request( p_msg , mk_proxy_from_ip(ip,port) );
		}
		else
		{
			/* establish transaction*/
			if ( !t_add_transaction(p_msg) )
			{
				DBG( "SER: ERROR in ser: t_add_transaction\n");
			}
			/* reply */
			if ( p_msg->first_line.u.request.method_value==METHOD_CANCEL)
			{
				DBG( "SER: new CANCEL\n");
				if ( !t_send_reply( p_msg , 200, "glad to cancel") )
				{
					DBG( "SER:ERROR: t_send_reply\n");
				}
			} else {
				DBG( "SER: new transaction\n");
				if ( !t_send_reply( p_msg , 100 , "trying -- your call is important to us") )
				{
					DBG( "SER: ERROR: t_send_reply (100)\n");
				}
			}
			if ( !t_forward( p_msg, ip, port ) )
			{
				DBG( "SER:ERROR: t_forward \n");
			}
			t_unref( /* p_msg */ );
		}
	}

}




int t_on_request_received_uri( struct sip_msg  *p_msg )
{
	unsigned int     ip, port;

	if ( get_ip_and_port_from_uri( p_msg , &ip, &port)<0 )
	{
		LOG( L_ERR , "ERROR: t_on_request_received_uri: \
		    unable to extract ip and port from uri!\n" );
		return -1;
	}

	return t_on_request_received( p_msg , ip , port );
}

#endif


/*   returns 1 if everything was OK or -1 for error
  */
int t_release_transaction( struct sip_msg* p_msg)
{
/*
	if (t_check( p_msg  , 0 )==-1) return 1;

   if ( T && T!=T_UNDEFINED )
*/
      return t_put_on_wait( T );

/*   return 1; */
}







int t_unref( /* struct sip_msg* p_msg */ )
{
	if (T==T_UNDEFINED || T==T_NULL)
		return -1;
	T_UNREF( T );
	T=T_UNDEFINED;
	return 1;
}







/* ----------------------------HELPER FUNCTIONS-------------------------------- */





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
		/* put CANCEL transaction on wait only if canceled transaction already
		    is in final status and there is nothing to cancel; */
		if ( Trans->T_canceled->status>=200)
			t_put_on_wait( Trans );
	} else if (Trans->status>=200)
		t_put_on_wait( Trans );
   return 1;
}




/* Checks if the new reply (with new_code status) should be sent or not
 *  based on the current
  * transactin status.
  * Returns 	- branch number (0,1,...) which should be relayed
		- -1 if nothing to be relayed
  */
int t_should_relay_response( struct cell *Trans , int new_code, 
	int branch , int *should_store )
{
	int T_code;
	int b, lowest_b, lowest_s;

	T_code = Trans->status;

	/* note: this code never lets replies to CANCEL go through;
	   we generate always a local 200 for CANCEL; 200s are
	   not relayed because it's not an INVITE transaction;
	   >= 300 are not relayed because 200 was already sent
	   out
	*/

	/* if final response sent out, allow only INVITE 2xx  */
	if ( T_code >= 200 ) { 
		if (new_code>=200 && new_code < 300  && 
			Trans->inbound_request->REQ_METHOD==METHOD_INVITE) {
			DBG("DBG: t_should_relay: 200 INV after final sent\n");
			*should_store=1;
			return branch;
		} else {
			*should_store=0;
			return -1;
		}
	} else { /* no final response sent yet */

		/* negative replies subject to fork picking */
		if (new_code >=300 ) {
			*should_store=1;
			/* if all_final return lowest */
			lowest_b=-1; lowest_s=999;
			for ( b=0; b<Trans->nr_of_outgoings ; b++ ) {
				/* "fake" for the currently processed branch */
				if (b==branch) {
					if (new_code<lowest_s) {
						lowest_b=b;
						lowest_s=new_code;
					}
					continue;
				}
				/* there is still an unfinished UAC transaction; wait now! */
				if ( !Trans->inbound_response[b] ||
					Trans->inbound_response[b]->REPLY_STATUS<200 )
					return -1;
				if ( Trans->inbound_response[b]->REPLY_STATUS<lowest_s )
      				{
         				lowest_b =b;
         				lowest_s = T->inbound_response[b]->REPLY_STATUS;
      				}
			}
			return lowest_b;

		/* 1xx except 100 and 2xx will be relayed */
		} else if (new_code>100) {
			*should_store=1;
			return branch;
		}
		/* 100 won't be relayed */
		else {
			if (!T->inbound_response[branch]) *should_store=1; 
			else *should_store=0;
			return -1;
		}
	}

	LOG(L_CRIT, "ERROR: Oh my gooosh! We don't know whether to relay\n");
	abort();
}


/*
  */
int t_put_on_wait(  struct cell  *Trans  )
{
	struct timer_link *tl;
	unsigned int i;
	struct retrans_buff* rb;

#ifndef WAIT
	if (is_in_timer_list2( &(Trans->wait_tl)))
  	{
		DBG("DEBUG: t_put_on_wait: already on wait\n");
		return 1;
	}
#else
	/* have some race conditons occured and we already
	  entered/passed the wait status previously?
	  if so, exit now
	*/

	LOCK_WAIT(T);
	if (Trans->on_wait)
	{
		DBG("DEBUG: t_put_on_wait: already on wait\n");
		UNLOCK_WAIT(T);
		return 1;
	} else {
		Trans->on_wait=1;
		UNLOCK_WAIT(T);
	};
#endif


	/* remove from  retranssmision  and  final response   list */
	DBG("DEBUG: t_put_on_wait: stopping timers (FR and RETR)\n");
	reset_retr_timers(hash_table,Trans) ;

	/* cancel pending client transactions, if any */
	for( i=0 ; i<Trans->nr_of_outgoings ; i++ )
		if ( Trans->inbound_response[i] && 
		REPLY_CLASS(Trans->inbound_response[i])==1)
		t_cancel_branch(i);


	/* we don't need outbound requests anymore -- let's save
	   memory and junk them right now!
	*/
/*
	shm_lock();
	for ( i =0 ; i<Trans->nr_of_outgoings;  i++ )
	{
		if ( rb=Trans->outbound_request[i] )
		{
			if (rb->retr_buffer) shm_free_unsafe( rb->retr_buffer );
			Trans->outbound_request[i] = NULL;
			shm_free_unsafe( rb );
		}
	}
	shm_unlock();
*/

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
  * to same address */
int t_build_and_send_ACK(struct cell *Trans,unsigned int branch,
														struct sip_msg* rpl)
{
	struct sip_msg      *p_msg , *r_msg;
	struct hdr_field    *hdr;
	char                *ack_buf, *p, *via;
	unsigned int         len, via_len;
	int                  n;
	struct retrans_buff *srb;

	ack_buf = 0;
	via =0;
	p_msg = Trans->inbound_request;
	r_msg = rpl;

	if ( parse_headers(rpl,HDR_TO)==-1 || !rpl->to )
	{
		LOG(L_ERR, "ERROR: t_build_and_send_ACK: "
			"cannot generate a HBH ACK if key HFs in reply missing\n");
		goto error;
	}

	len = 0;
	/*first line's len */
	len += 4/*reply code and one space*/+
		p_msg->first_line.u.request.version.len+CRLF_LEN;
	/*uri's len*/
	if (p_msg->new_uri.s)
		len += p_msg->new_uri.len +1;
	else
		len += p_msg->first_line.u.request.uri.len +1;
	/*via*/
	via = via_builder( p_msg , &via_len );
	if (!via)
	{
		LOG(L_ERR, "ERROR: t_build_and_send_ACK: "
			"no via header got from builder\n");
		goto error;
	}
	len+= via_len;
	/*headers*/
	for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
		if (hdr->type==HDR_FROM||hdr->type==HDR_CALLID||hdr->type==HDR_CSEQ)
			len += ((hdr->body.s+hdr->body.len ) - hdr->name.s ) + CRLF_LEN ;
		else if ( hdr->type==HDR_TO )
			len += ((r_msg->to->body.s+r_msg->to->body.len ) -
				r_msg->to->name.s ) + CRLF_LEN ;
	/* CSEQ method : from INVITE-> ACK */
	len -= 3  ;
	/* end of message */
	len += CRLF_LEN; /*new line*/

	srb=(struct retrans_buff*)sh_malloc(sizeof(struct retrans_buff)+len+1);
	if (!srb)
	{
		LOG(L_ERR, "ERROR: t_build_and_send_ACK: cannot allocate memory\n");
		goto error1;
	}
	ack_buf = (char *) srb + sizeof(struct retrans_buff);
	p = ack_buf;

	/* first line */
	memcpy( p , "ACK " , 4);
	p += 4;
	/* uri */
	if ( p_msg->new_uri.s )
	{
		memcpy(p,p_msg->orig+(p_msg->new_uri.s-p_msg->buf),p_msg->new_uri.len);
		p +=p_msg->new_uri.len;
	}else{
		memcpy(p,p_msg->orig+(p_msg->first_line.u.request.uri.s-p_msg->buf),
			p_msg->first_line.u.request.uri.len );
		p += p_msg->first_line.u.request.uri.len;
	}
	/* SIP version */
	*(p++) = ' ';
	memcpy(p,p_msg->orig+(p_msg->first_line.u.request.version.s-p_msg->buf),
		p_msg->first_line.u.request.version.len );
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
				((r_msg->to->body.s+r_msg->to->body.len)-r_msg->to->name.s));
			p+=((r_msg->to->body.s+r_msg->to->body.len)-r_msg->to->name.s);
			memcpy( p, CRLF, CRLF_LEN );
			p+=CRLF_LEN;
		}
		else if ( hdr->type==HDR_CSEQ )
		{
			memcpy( p , p_msg->orig+(hdr->name.s-p_msg->buf) ,
				((((struct cseq_body*)hdr->parsed)->method.s)-hdr->name.s));
			p+=((((struct cseq_body*)hdr->parsed)->method.s)-hdr->name.s);
			memcpy( p , "ACK" CRLF, 3+CRLF_LEN );
			p += 3+CRLF_LEN;
		}
	}

	/* end of message */
	memcpy( p , CRLF , CRLF_LEN );
	p += CRLF_LEN;

	send_ack( T, branch, srb, p-ack_buf );
	pkg_free( via );
	DBG("DEBUG: t_build_and_send_ACK: ACK sent\n");
	return 0;

error1:
	pkg_free(via );
error:
	return -1;
}





/* Builds a CANCEL request based on an INVITE request. CANCEL is send
 * to same address */
int t_build_and_send_CANCEL(struct cell *Trans,unsigned int branch)
{
	struct sip_msg      *p_msg;
	struct hdr_field    *hdr;
	char                *cancel_buf, *p, *via;
	unsigned int         len, via_len;
	int                  n;
	struct retrans_buff *srb;

	cancel_buf = 0;
	via = 0;
	p_msg = Trans->inbound_request;

	len = 0;
	/*first line's len - CANCEL and INVITE has the same lenght */
	len += ( req_line(p_msg).version.s+req_line(p_msg).version.len)-
		req_line(p_msg).method.s+CRLF_LEN;
	/*check if the REQ URI was override */
	if (p_msg->new_uri.s)
		len += p_msg->new_uri.len - req_line(p_msg).uri.len;
	/*via*/
	via = via_builder( p_msg , &via_len );
	if (!via)
	{
		LOG(L_ERR, "ERROR: t_build_and_send_CANCEL: "
			"no via header got from builder\n");
		goto error;
	}
	len+= via_len;
	/*headers*/
	for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
		if (hdr->type==HDR_FROM || hdr->type==HDR_CALLID || 
			hdr->type==HDR_CSEQ || hdr->type==HDR_TO )
			len += ((hdr->body.s+hdr->body.len ) - hdr->name.s ) + CRLF_LEN ;
	/* User Agent header*/
	len += USER_AGENT_LEN + CRLF_LEN;
	/* Content Lenght heder*/
	len += CONTENT_LEN_LEN + CRLF_LEN;
	/* end of message */
	len += CRLF_LEN;

	srb=(struct retrans_buff*)sh_malloc(sizeof(struct retrans_buff)+len+1);
	if (!srb)
	{
		LOG(L_ERR, "ERROR: t_build_and_send_CANCEL: cannot allocate memory\n");
		goto error;
	}
	cancel_buf = (char*) srb + sizeof(struct retrans_buff);
	p = cancel_buf;

	/* first line -> do we have a new URI? */
	if (p_msg->new_uri.s)
	{
		append_mem_block(p,req_line(p_msg).method.s,
			req_line(p_msg).uri.s-req_line(p_msg).method.s);
		append_mem_block(p,p_msg->new_uri.s,p_msg->new_uri.len);
		append_mem_block(p,req_line(p_msg).uri.s+req_line(p_msg).uri.len,
			req_line(p_msg).version.s+req_line(p_msg).version.len-
			(req_line(p_msg).uri.s+req_line(p_msg).uri.len))
	}else{
	append_mem_block(p,req_line(p_msg).method.s,
		req_line(p_msg).version.s+req_line(p_msg).version.len-
		req_line(p_msg).method.s);
	}
	/* changhing method name*/
	memcpy(cancel_buf,"CANCEL",6);
	append_mem_block(p,CRLF,CRLF_LEN);
	/* insert our via */
	append_mem_block(p,via,via_len);

	/*other headers*/
	for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
	{
		if(hdr->type==HDR_FROM||hdr->type==HDR_CALLID||hdr->type==HDR_TO)
		{
			append_mem_block(p,hdr->name.s,
				((hdr->body.s+hdr->body.len)-hdr->name.s) );
			append_mem_block(p, CRLF, CRLF_LEN );
		}else if ( hdr->type==HDR_CSEQ )
		{
			append_mem_block(p,hdr->name.s,
				((((struct cseq_body*)hdr->parsed)->method.s)-hdr->name.s));
			append_mem_block(p,"CANCEL" CRLF, 6+CRLF_LEN );
		}
}

	/* User Agent header */
	append_mem_block(p,USER_AGENT,USER_AGENT_LEN);
	append_mem_block(p,CRLF,CRLF_LEN);
	/* Content Lenght header*/
	append_mem_block(p,CONTENT_LEN,CONTENT_LEN_LEN);
	append_mem_block(p,CRLF,CRLF_LEN);
	/* end of message */
	append_mem_block(p,CRLF,CRLF_LEN);
	*p=0;
	

	DBG("LOCAL CANCEL = \n%s\n",cancel_buf);

	pkg_free(via);
	return 1;
error:
	if (via) pkg_free(via);
	return -1;
}






void delete_cell( struct cell *p_cell )
{
#ifdef EXTRA_DEBUG
	int i;

	if (is_in_timer_list2(& p_cell->wait_tl )) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" still on WAIT\n", p_cell);
		abort();
	}
	/*
	if (is_in_timer_list2(& p_cell->outbound_response.retr_timer )) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" still on RETR (rep)\n",
			p_cell);
		abort();
	}
	if (is_in_timer_list2(& p_cell->outbound_response.fr_timer )) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" still on FR (rep)\n", p_cell);
		abort();
	}
	for (i=0; i<p_cell->nr_of_outgoings; i++) {
		if (is_in_timer_list2(& p_cell->outbound_request[i]->retr_timer)) {
			LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
				" still on RETR (req %d)\n", p_cell, i);
			abort();
		}
		if (is_in_timer_list2(& p_cell->outbound_request[i]->fr_timer)) {
			LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
				" still on FR (req %d)\n", p_cell, i);
			abort();
		}
	}
	*/
	reset_retr_timers( hash_table, p_cell );
#endif
	/* still in use ... don't delete */
	if ( T_IS_REFED(p_cell) ) {
#ifdef	EXTRA_DEBUG
		if (T_REFCOUNTER(p_cell)>1) {
			DBG("DEBUG: while debugging with a single process, ref_count>1\n");
			DBG("DEBUG: transaction =%p\n", p_cell );
			abort();
		}
#endif
		DBG("DEBUG: delete_cell: t=%p post for delete (refbitmap %x,"
			" refcount %d)\n",p_cell,p_cell->ref_bitmap,T_REFCOUNTER(p_cell));
		/* it's added to del list for future del */
		set_timer( hash_table, &(p_cell->dele_tl), DELETE_LIST );
	} else {
		DBG("DEBUG: delete transaction %p\n", p_cell );
		free_cell( p_cell );
	}
}




/* Returns  -1 = error
                    0 = OK
*/
int get_ip_and_port_from_uri( struct sip_msg* p_msg , unsigned int *param_ip, unsigned int *param_port)
{
	struct hostent  *nhost;
	unsigned int      ip, port;
	struct sip_uri    parsed_uri;
	str                      uri;
	int                      err;
#ifdef DNS_IP_HACK
	int                      len;
#endif

	/* the original uri has been changed? */
	if (p_msg->new_uri.s==0 || p_msg->new_uri.len==0)
		uri = p_msg->first_line.u.request.uri;
	else
		uri = p_msg->new_uri;

	/* parsing the request uri in order to get host and port */
	if (parse_uri( uri.s , uri.len , &parsed_uri )<0)
	{
		LOG(L_ERR, "ERROR: get_ip_and_port_from_uri: "
		   "unable to parse destination uri: %.*s\n", uri.len, uri.s );
		goto error;
	}

	/* getting the port */
	if ( parsed_uri.port.s==0 || parsed_uri.port.len==0 )
		port = SIP_PORT;
	else{
		port = str2s( parsed_uri.port.s , parsed_uri.port.len , &err );
		if ( err<0 ){
			LOG(L_ERR, "ERROR: get_ip_and_port_from_uri: converting port "
				"from str to int failed; using default SIP port\n\turi:%.*s\n",
				uri.len, uri.s );
			port = SIP_PORT;
		}
	}
	port = htons( port );

	/* getting host address*/
#ifdef DNS_IP_HACK
	len=strlen( parsed_uri.host.s );
	ip=str2ip(parsed_uri.host.s, len, &err);
	if (err==0)
		goto success;
#endif
	/* fail over to normal lookup */
	nhost = gethostbyname( parsed_uri.host.s );
	if ( !nhost )
	{
		LOG(L_ERR, "ERROR: get_ip_and_port_from_uri: "
		  "cannot resolve host in uri: %.*s\n", uri.len, uri.s );
		free_uri(&parsed_uri);
		goto error;
	}
	memcpy(&ip, nhost->h_addr_list[0], sizeof(unsigned int));


success:
	free_uri(&parsed_uri);
	*param_ip = ip;
	*param_port = port;
	return 0;

error:
	*param_ip = 0;
	*param_port = 0;
	return -1;
}






/*---------------------TIMEOUT HANDLERS--------------------------*/


void retransmission_handler( void *attr)
{
	struct retrans_buff* r_buf ;
	enum lists id;

	r_buf = (struct retrans_buff*)attr;
#ifdef EXTRA_DEBUG
	if (r_buf->my_T->damocles) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" called from RETR timer\n",r_buf->my_T);
		abort();
	}	
#endif


	/*the transaction is already removed from RETRANSMISSION_LIST by timer*/

	/* retransmision */
	DBG("DEBUG: retransmission_handler : resending (t=%p)\n", r_buf->my_T);
	if (r_buf->reply) {
		T=r_buf->my_T;
/*
		LOCK_REPLIES( r_buf->my_T );
		SEND_BUFFER( r_buf );
		UNLOCK_REPLIES( r_buf->my_T );
*/
		t_retransmit_reply();
	}else{
		SEND_BUFFER( r_buf );
	}

	id = r_buf->retr_list;
	r_buf->retr_list = id < RT_T2 ? id + 1 : RT_T2;

	set_timer(hash_table,&(r_buf->retr_timer),id < RT_T2 ? id + 1 : RT_T2 );

	DBG("DEBUG: retransmission_handler : done\n");
}




void final_response_handler( void *attr)
{
	struct retrans_buff* r_buf = (struct retrans_buff*)attr;

#ifdef EXTRA_DEBUG
	if (r_buf->my_T->damocles) 
	{
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" called from FR timer\n",r_buf->my_T);
		abort();
	}
#endif

	/* the transaction is already removed from FR_LIST by the timer */
	/* send a 408 */
	if ( r_buf->my_T->status<200)
	{
		DBG("DEBUG: final_response_handler:stop retransmission and"
			" send 408 (t=%p)\n", r_buf->my_T);
		reset_timer( hash_table, &(r_buf->retr_timer) );
		t_build_and_send_CANCEL( r_buf->my_T ,0);
		/* dirty hack:t_send_reply would increase ref_count which would indeed
		result in refcount++ which would not -- until timer processe's
		T changes again; currently only on next call to t_send_reply from
		FR timer; thus I fake the values now to avoid recalculating T
		and refcount++ JKU */
		T=r_buf->my_T;
		global_msg_id=T->inbound_request->id;

		t_send_reply( r_buf->my_T->inbound_request,408,"Request Timeout" );
	}else{
		/* put it on WT_LIST - transaction is over */
		DBG("DEBUG: final_response_handler:cancel transaction->put on wait"
			" (t=%p)\n", r_buf->my_T);
		t_put_on_wait(  r_buf->my_T );
	}
	DBG("DEBUG: final_response_handler : done\n");
}




void wait_handler( void *attr)
{
	struct cell *p_cell = (struct cell*)attr;

#ifdef EXTRA_DEBUG
	if (p_cell->damocles) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" called from WAIT timer\n",p_cell);
		abort();
	}	
#endif

	/* the transaction is already removed from WT_LIST by the timer */
	/* the cell is removed from the hash table */
	DBG("DEBUG: wait_handler : removing %p from table \n", p_cell );
	remove_from_hash_table( hash_table, p_cell );
	/* jku: no more here -- we do it when we put a transaction on wait */
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
		LOG( L_ERR, "ERROR: transaction %p not scheduled for deletion"
			" and called from DELETE timer\n",p_cell);
		abort();
	}	
#endif
	delete_cell( p_cell );
    DBG("DEBUG: delete_handler : done\n");
}
