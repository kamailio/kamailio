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

#ifndef SRL
/*  This function is called whenever a reply for our module is received; 
  * we need to register  this function on module initialization;
  *  Returns :   0 - core router stops
  *              1 - core router relay statelessly
  */
int t_on_reply_received( struct sip_msg  *p_msg )
{
	unsigned int  branch,len, msg_status, msg_class, save_clone;
	unsigned int local_cancel;
	struct sip_msg *clone, *backup;
	int relay;
	int start_fr;
	int is_invite;
	struct retrans_buff *rb;


	/* make sure we know the assosociated tranaction ... */
	if (t_check( p_msg  , &branch , &local_cancel)==-1) return 1;
	/* ... if there is no such, tell the core router to forward statelessly */
	if ( T<=0 ) return 1;

	DBG("DEBUG: t_on_reply_received: Original status=%d (%d,%d)\n",
		T->status,branch,local_cancel);

	/* special cases (local cancel reply and another 100 reply!)*/
	if (p_msg->REPLY_STATUS==100 && T->status==100)
		return 0;
	if (local_cancel==1)
	{
		reset_timer( hash_table, &(T_>outbound_cancel[branch]->retr_timer));
		if ( p_msg->REPLY_STATUS>=200 )
			reset_timer( hash_table, &(T_>outbound_cancel[branch]->fr_timer));
		return 0;
	}

	/* it can take quite long -- better do it now than later 
	   inside a reply_lock */
	if (!(clone=sip_msg_cloner( p_msg ))) {
		goto error;
	}
	msg_status=p_msg->REPLY_STATUS;
	msg_class=REPLY_CLASS(p_msg);
	is_invite= T->inbound_request->REQ_METHOD==METHOD_INVITE;

	/* *** stop timers *** */
	rb=T->outbound_request[branch];
	/* stop retransmission */
	reset_timer( hash_table, &(rb->retr_timer));
	/* stop final response timer only if I got a final response */
	if ( msg_class>1 )
		reset_timer( hash_table, &(rb->fr_timer));

	LOCK_REPLIES( T );
   	/* if a got the first prov. response for an INVITE ->
	   change FR_TIME_OUT to INV_FR_TIME_UT */
	start_fr = !T->inbound_response[branch] && msg_class==1 && is_invite;

	/* *** store and relay message as needed *** */
	relay = t_should_relay_response( T , msg_status, branch, &save_clone );

	if (save_clone) {
		/* release previously hold message */
		backup = T->inbound_response[branch];
		T->inbound_response[branch] = clone;
		T->tag=&(get_to(clone)->tag_value);
	} else {
		backup = NULL;
		sip_msg_free( clone );
	}

	if (relay>=0 &&  
	push_reply_from_uac_to_uas( T, relay  ) == -1 ) {
		/* restore original state first */
		if (save_clone) T->inbound_response[branch] = backup;
		/* restart FR */
		start_fr=1;
		goto cleanup;
	}


	/* *** ACK handling *** */
	if ( is_invite )
	{
		if ( T->outbound_ack[branch] )
		{   /*retransmit*/
			SEND_BUFFER( T->outbound_ack[branch] );
		}else if (msg_class>2 ) {   
			/*on a non-200 reply to INVITE*/
			DBG("DEBUG: t_on_reply_received: >=3xx reply to INVITE:"
				" send ACK\n");
			if ( t_build_and_send_ACK( T , branch , p_msg )==-1)
			{
				LOG( L_ERR , "ERROR: t_on_reply_received:"
					" unable to send ACK\n" );
				/* restart FR */
				start_fr=1;
			}
		}
	}
cleanup:
	UNLOCK_REPLIES( T );
	if (backup) sip_msg_free(backup);
	if (start_fr) set_timer( hash_table, &(rb->fr_timer), FR_INV_TIMER_LIST );
   	/* restart retransmission if a provisional response came for 
	   a non_INVITE -> retrasmit at RT_T2*/
	if ( msg_class==1 && !is_invite )
	{
		rb->retr_list = RT_T2;
		set_timer( hash_table, &(rb->retr_timer), RT_T2 );
	}
error:
	T_UNREF( T );
	/* don't try to relay statelessly on error; on troubles, simply do nothing;
           that will make the other party to retransmit; hopefuly, we'll then 
           be better off */
	return 0;
}
#endif



/* Retransmits the last sent inbound reply.
 * input: p_msg==request for which I want to retransmit an associated reply
 * Returns  -1 - error
 *           1 - OK
 */
int t_retransmit_reply( /* struct sip_msg* p_msg    */ )
{

#ifdef SRL
	void *b;
	int len;
#endif
	LOCK_REPLIES( T );

#ifdef SRL
	if (!(b=pkg_malloc( len=T->outbound_response.bufflen ))) {
		UNLOCK_REPLIES( T );
		return -1;
	};
	memcpy( b, T->outbound_response.retr_buffer, len );
#else
	SEND_BUFFER( & T->outbound_response );
#endif
	UNLOCK_REPLIES( T );

#ifdef SRL
	SEND_PR_BUFFER( & T->outbound_response, b, len );
	pkg_free( b );
#endif
	return 1;
}





/* Force a new response into inbound response buffer.
  * returns 1 if everything was OK or -1 for erro
  */
int t_send_reply(  struct sip_msg* p_msg , unsigned int code , char * text )
{
	unsigned int len, buf_len;
	char * buf, *shbuf;
	struct retrans_buff *rb;

	buf = build_res_buf_from_sip_req(code,text,0,0,T->inbound_request,&len);
	DBG("DEBUG: t_send_reply: buffer computed\n");
	if (!buf)
	{
		DBG("DEBUG: t_send_reply: response building failed\n");
		goto error;
	}

	LOCK_REPLIES( T );

	rb = & T->outbound_response;
	if (!rb->retr_buffer) {
		/* initialize retransmission structure */
		memset( rb , 0 , sizeof (struct retrans_buff) );
		if (update_sock_struct_from_via(  &(rb->to),  p_msg->via1 )==-1)
		{
			UNLOCK_REPLIES( T );
			LOG(L_ERR, "ERROR: t_send_reply: cannot lookup reply dst: %s\n",
				p_msg->via1->host.s );
			goto error2;
		}

		rb->retr_timer.tg=TG_RT;
		rb->fr_timer.tg=TG_FR;
		rb->retr_timer.payload = rb;
		rb->fr_timer.payload = rb;
		rb->to.sin_family = AF_INET;
		rb->my_T = T;
		rb->status = code;
	}

	/* if this is a first reply (?100), longer replies will probably follow;
	   try avoiding shm_resize by higher buffer size */
	buf_len = rb->retr_buffer ? len : len + REPLY_OVERBUFFER_LEN;

	if (! (rb->retr_buffer = (char*)shm_resize( rb->retr_buffer, buf_len )))
	{
		UNLOCK_REPLIES( T );
		LOG(L_ERR, "ERROR: t_send_reply: cannot allocate shmem buffer\n");
		goto error2;
	}
	rb->bufflen = len ;
	memcpy( rb->retr_buffer , buf , len );
	T->status = code;
#ifndef SRL
	SEND_BUFFER( rb );
#endif
	/* needs to be protected too because what timers are set depends
	   on current transactions status
	*/
	t_update_timers_after_sending_reply( rb );
	UNLOCK_REPLIES( T );

#ifdef SRL
	SEND_PR_BUFFER( rb, buf, len );
#endif

	free( buf ) ;
	/* start/stops the proper timers*/

	DBG("DEBUG: t_send_reply: finished\n");

	return 1;

error2:
	free ( buf );
error:
	return -1;
}




/* Push a previously stored reply from UA Client to UA Server
  * and send it out
  */
static int push_reply_from_uac_to_uas( struct cell* trans , unsigned int branch 
#ifdef SRL
	, char *buf, unsigned int len
#endif
	)
{
	unsigned int buf_len;
	struct retrans_buff *rb;
#ifndef SRL
	char *buf;
	unsigned int len;
#endif

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
		rb->retr_timer.tg=TG_RT;
		rb->fr_timer.tg=TG_FR;
		rb->retr_timer.payload = rb;
		rb->fr_timer.payload =  rb;
		rb->to.sin_family = AF_INET;
		rb->my_T = trans;
		rb->status = trans->inbound_response[branch]->REPLY_STATUS;

	} else {
#ifndef SRL
		reset_timer( hash_table, &(rb->retr_timer));
		reset_timer( hash_table, &(rb->fr_timer));
#endif
	}

#ifndef SRL
	/*  generate the retrans buffer */
	buf = build_res_buf_from_sip_res ( trans->inbound_response[branch], &len);
	if (!buf) {
		LOG(L_ERR, "ERROR: push_reply_from_uac_to_uas: "
			"no shmem for outbound reply buffer\n");
		goto error;
	}
#endif

	/* if this is a first reply (?100), longer replies will probably follow;
	try avoiding shm_resize by higher buffer size */
	buf_len = rb->retr_buffer ? len : len + REPLY_OVERBUFFER_LEN;
	if (! (rb->retr_buffer = (char*)shm_resize( rb->retr_buffer, buf_len )))
	{
		LOG(L_ERR, "ERROR: t_push: cannot allocate shmem buffer\n");
		goto error1;
	}
	rb->bufflen = len ;
	memcpy( rb->retr_buffer , buf , len );
#ifndef SRL
	free( buf ) ;
#endif

	/* update the status*/
	trans->status = trans->inbound_response[branch]->REPLY_STATUS;
	if ( trans->inbound_response[branch]->REPLY_STATUS>=200 &&
		trans->relaied_reply_branch==-1 ) {

		memcpy( & trans->ack_to, & trans->outbound_request[ branch ]->to,
			sizeof( struct sockaddr_in ) );
		trans->relaied_reply_branch = branch;
	}

#ifndef SRL
	/* start/stops the proper timers*/
	t_update_timers_after_sending_reply( rb );
#endif

	/*send the reply*/
	SEND_BUFFER( rb );
	return 1;

error1:
#ifndef SRL
	free( buf );
#endif
error:
	return -1;
}

/* Push a previously stored reply from UA Client to UA Server
  * and send it out
  */
static int push_reply( struct cell* trans , unsigned int branch , 
	char *buf, unsigned int len)
{
	unsigned int buf_len;
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
		rb->retr_timer.tg=TG_RT;
		rb->fr_timer.tg=TG_FR;
		rb->retr_timer.payload = rb;
		rb->fr_timer.payload =  rb;
		rb->to.sin_family = AF_INET;
		rb->my_T = trans;
		rb->status = trans->inbound_response[branch]->REPLY_STATUS;
	};

	/* if this is a first reply (?100), longer replies will probably follow;
	try avoiding shm_resize by higher buffer size */
	buf_len = rb->retr_buffer ? len : len + REPLY_OVERBUFFER_LEN;
	if (! (rb->retr_buffer = (char*)shm_resize( rb->retr_buffer, buf_len )))
	{
		LOG(L_ERR, "ERROR: t_push: cannot allocate shmem buffer\n");
		goto error1;
	}
	rb->bufflen = len ;
	memcpy( rb->retr_buffer , buf , len );

	/* update the status*/
	trans->status = trans->inbound_response[branch]->REPLY_STATUS;
	if ( trans->inbound_response[branch]->REPLY_STATUS>=200 &&
		trans->relaied_reply_branch==-1 ) {

		memcpy( & trans->ack_to, & trans->outbound_request[ branch ]->to,
			sizeof( struct sockaddr_in ) );
		trans->relaied_reply_branch = branch;
	}

	/*send the reply*/
	SEND_BUFFER( rb );
	return 1;

error1:
error:
	return -1;
}

#ifdef SRL
/*  This function is called whenever a reply for our module is received; 
  * we need to register  this function on module initialization;
  *  Returns :   0 - core router stops
  *              1 - core router relay statelessly
  */
int t_on_reply( struct sip_msg  *p_msg )
{
	unsigned int branch,len, msg_status, msg_class, save_clone;
	unsigned int local_cancel;
	struct sip_msg *clone;
	int relay;
	int start_fr;
	int is_invite;
	/* retransmission structure of outbound reply and request */
	struct retrans_buff *orq_rb, *orp_rb, *ack_rb;
	char *buf;
	/* length of outbound reply */
	unsigned int orp_len;
	/* buffer length (might be somewhat larger than message size */
	unsigned int alloc_len;


	/* make sure we know the assosociated tranaction ... */
	if (t_check( p_msg  , &branch , &local_cancel)==-1) return 1;
	/* ... if there is no such, tell the core router to forward statelessly */
	if ( T<=0 ) return 1;

	DBG("DEBUG: t_on_reply_received: Original status=%d (%d,%d)\n",
		T->status,branch,local_cancel);

	/* special cases (local cancel reply and another 100 reply!)*/
	if (p_msg->REPLY_STATUS==100 && T->status==100)
		return 0;
	if (local_cancel==1)
	{
		reset_timer( hash_table, &(T->outbound_cancel[branch]->retr_timer));
		if ( p_msg->REPLY_STATUS>=200 )
			reset_timer( hash_table, &(T->outbound_cancel[branch]->fr_timer));
		return 0;
	}

	/* it can take quite long -- better do it now than later 
	   inside a reply_lock */
													/* CLONE alloc'ed */
	if (!(clone=sip_msg_cloner( p_msg ))) {
		goto error;
	}
	msg_status=p_msg->REPLY_STATUS;
	msg_class=REPLY_CLASS(p_msg);
	is_invite= T->inbound_request->REQ_METHOD==METHOD_INVITE;

	/*  generate the retrans buffer, make a simplified
	assumption everything but 100 will be fwd-ed;
	sometimes it will result in useless CPU cycles
	but mostly the assumption holds and allows the
	work to be done out of criticial lock region */
	if (msg_status==100) buf=0;
	else {
												/* buf maybe allo'ed*/

		buf = build_res_buf_from_sip_res ( p_msg, &orp_len);
		if (!buf) {
			LOG(L_ERR, "ERROR: t_on_reply_received: "
			"no mem for outbound reply buffer\n");
			goto error1;
		}
	}

	/* *** stop timers *** */
	orq_rb=T->outbound_request[branch];
	/* stop retransmission */
												/* timers reset */

	reset_timer( hash_table, &(orq_rb->retr_timer));
	/* stop final response timer only if I got a final response */
	if ( msg_class>1 )
		reset_timer( hash_table, &(orq_rb->fr_timer));

	LOCK_REPLIES( T );
   	/* if a got the first prov. response for an INVITE ->
	   change FR_TIME_OUT to INV_FR_TIME_UT */
	start_fr = !T->inbound_response[branch] && msg_class==1 && is_invite;

	/* *** store and relay message as needed *** */
	relay = t_should_relay_response( T , msg_status, branch, &save_clone );

	if (relay >= 0 ) {
		orp_rb= & T->outbound_response;
		/* if there is no reply yet, initialize the structure */
		if ( ! orp_rb->retr_buffer ) {
			/*init retrans buffer*/
			memset( orp_rb , 0 , sizeof (struct retrans_buff) );
			if (update_sock_struct_from_via(  &(orp_rb->to), p_msg->via2 )==-1) {
					UNLOCK_REPLIES( T );
					start_fr = 1;
					LOG(L_ERR, "ERROR: push_reply_from_uac_to_uas: "
						"cannot lookup reply dst: %s\n",
						p_msg->via2->host.s );
					save_clone = 0;
					goto error2;
			}
			orp_rb->retr_timer.tg=TG_RT;
			orp_rb->fr_timer.tg=TG_FR;
			orp_rb->retr_timer.payload = orp_rb;
			orp_rb->fr_timer.payload =  orp_rb;
			orp_rb->to.sin_family = AF_INET;
			orp_rb->my_T = T;
			orp_rb->status = p_msg->REPLY_STATUS;
			/* allocate something more for the first message;
			   subsequent messages will be longer and buffer
			   reusing will save us a malloc lock */
			alloc_len = orp_len + REPLY_OVERBUFFER_LEN ;
		} else {
			alloc_len = orp_len;
		};

		if (! (orp_rb->retr_buffer = (char *) shm_resize( orp_rb->retr_buffer, alloc_len ))) {
			UNLOCK_REPLIES( T );
			start_fr = 1;
			save_clone = 0;
			LOG(L_ERR, "ERROR: t_on_reply: cannot alloc shmem\n");
			goto error2;
		};

		orp_rb->bufflen=orp_len;
		memcpy( orp_rb->retr_buffer, buf, orp_len );
	}; /* if relay ... */

	if (save_clone) {
		T->inbound_response[branch]=clone;
		T->tag=&(get_to(clone)->tag_value);
	}

	/* update the status ... */
	if ((T->status = p_msg->REPLY_STATUS) >=200 &&
	/* ... and dst for a possible ACK if we are sending final downstream */
		T->relaied_reply_branch==-1 ) {
			memcpy( & T->ack_to, & T->outbound_request[ branch ]->to,
			sizeof( struct sockaddr_in ) );
   			T->relaied_reply_branch = branch;
	}

cleanup:
	UNLOCK_REPLIES( T );
	if (relay >= 0) {
		SEND_PR_BUFFER( orp_rb, buf, orp_len );
		t_update_timers_after_sending_reply( orp_rb );
	}

	/* *** ACK handling *** */
	if ( is_invite )
	{
		if ( T->outbound_ack[branch] )
		{   /*retransmit*/
			/* I don't need any additional syncing here -- after ack
			   is introduced it's never changed */
			SEND_BUFFER( T->outbound_ack[branch] );
		} else if (msg_class>2 ) {   /*on a non-200 reply to INVITE*/
           		DBG("DEBUG: t_on_reply_received: >=3xx reply to INVITE: send ACK\n");
				ack_rb = build_ack( p_msg, T, branch );
				if (ack_rb) {
					SEND_BUFFER( ack_rb );
					/* append to transaction structure */
					attach_ack( T, branch, ack_rb );
				} else {
					/* restart FR */
					start_fr=1;
					DBG("ERROR: t_on_reply: build_ack failed\n");
				}
		}
	} /* is_invite */

   	/* restart retransmission if a provisional response came for 
	   a non_INVITE -> retrasmit at RT_T2*/
	if ( msg_class==1 && !is_invite )
	{
		orq_rb->retr_list = RT_T2;
		set_timer( hash_table, &(orq_rb->retr_timer), RT_T2 );
	}
error2:
	if (start_fr) set_timer( hash_table, &(orq_rb->fr_timer), FR_INV_TIMER_LIST );
	if (buf) free( buf );
error1:
	if (!save_clone) sip_msg_free( clone );
error:
	T_UNREF( T );
	/* don't try to relay statelessly on error; on troubles, simply do nothing;
           that will make the other party to retransmit; hopefuly, we'll then 
           be better off */
	return 0;
}

#endif
