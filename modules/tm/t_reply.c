/*
 * $Id$
 *
 */


#include "hash_func.h"
#include "t_funcs.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "../../timer.h"

#include "t_hooks.h"




inline int check_for_no_response( struct cell *Trans ,int code, int relay)
{
	if ( code/100>=3 && Trans->uac[Trans->nr_of_outgoings].uri.s )
	{
		forward_serial_branch( Trans , Trans->nr_of_outgoings );
		return -1;
	}
	return relay;
}



/* Retransmits the last sent inbound reply.
 * input: p_msg==request for which I want to retransmit an associated reply
 * Returns  -1 - error
 *           1 - OK
 */
int t_retransmit_reply( /* struct sip_msg* p_msg    */ )
{
	static char b[BUF_SIZE];
	int len;

	if (!T->uas.response.buffer)
		return -1;

	if ( (len=T->uas.response.buffer_len)==0 || len>BUF_SIZE ) {
		UNLOCK_REPLIES( T );
		return -2;
	}
	memcpy( b, T->uas.response.buffer, len );
	UNLOCK_REPLIES( T );
	SEND_PR_BUFFER( & T->uas.response, b, len );
	return 1;
}





/* Force a new response into inbound response buffer.
  * returns 1 if everything was OK or -1 for error
  */
int t_send_reply( struct sip_msg* p_msg, unsigned int code, 
	char * text, unsigned int branch)
{
	unsigned int len, buf_len=0;
	char * buf;
	struct retr_buf *rb;
	int relay, save_clone;
	struct socket_info* send_sock;

	buf = build_res_buf_from_sip_req(code,text,T->uas.tag->s,
		T->uas.tag->len, T->uas.request,&len);
	DBG("DEBUG: t_send_reply: buffer computed\n");
	if (!buf)
	{
		DBG("DEBUG: t_send_reply: response building failed\n");
		goto error;
	}

	LOCK_REPLIES( T );
	relay = t_should_relay_response(T, code, branch, &save_clone);

	if (save_clone)
	{
		T->uac[branch].status = code;
	}

	rb = & T->uas.response;
	if (relay >=0 && (relay=check_for_no_response(T,code,relay))>=0 )
	{
		if (!rb->buffer) {
			/* initialize retransmission structure */
			if (update_sock_struct_from_via(  &(rb->to),  p_msg->via1 )==-1)
			{
				UNLOCK_REPLIES( T );
				LOG(L_ERR,"ERROR: t_send_reply: cannot lookup reply dst: %s\n",
					p_msg->via1->host.s );
				goto error2;
			}
			send_sock=get_send_socket(&rb->to);
			if (send_sock==0) {
				LOG(L_ERR, "ERROR: t_send_reply: cannot fwd to af %d "
					"no socket\n", rb->to.s.sa_family);
				ser_error=E_NO_SOCKET;
				goto error2;
			}
			rb->send_sock=send_sock;
			/* rb->to.sin_family = AF_INET; */
			rb->activ_type = code;
			buf_len = len + REPLY_OVERBUFFER_LEN;
		}else{
			buf_len = len;
		}
		/* puts the reply's buffer to uas.response */
		if (! (rb->buffer = (char*)shm_resize( rb->buffer, buf_len )))
		{
			UNLOCK_REPLIES( T );
			LOG(L_ERR, "ERROR: t_send_reply: cannot allocate shmem buffer\n");
			goto error2;
		}
		rb->buffer_len = len ;
		memcpy( rb->buffer , buf , len );
		T->uas.status = code;
		/* needs to be protected too because what timers are set depends
		   on current transactions status */
		t_update_timers_after_sending_reply( rb );
	} /* if realy */

	UNLOCK_REPLIES( T );

	if (relay>=0) SEND_PR_BUFFER( rb, buf, len );
	pkg_free( buf ) ;
	DBG("DEBUG: t_send_reply: finished\n");
	return 1;

error2:
	pkg_free ( buf );
error:
	return -1;
}



#if 0
/* Push a previously stored reply from UA Client to UA Server
 * and send it out */
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
#endif


/*  This function is called whenever a reply for our module is received; 
  * we need to register  this function on module initialization;
  *  Returns :   0 - core router stops
  *              1 - core router relay statelessly
  */
int t_on_reply( struct sip_msg  *p_msg )
{
	int branch, msg_status, msg_class, save_clone;
	int local_cancel;
	int relay;
	int start_fr = 0;
	int is_invite;
	/* retransmission structure of outbound reply and request */
	struct retr_buf *rb=0;
	char *buf=0, *ack=0;
	/* length of outbound reply */
	unsigned int res_len, ack_len;
	/* buffer length (might be somewhat larger than message size */
	unsigned int alloc_len;
	str *str_foo;
	struct socket_info* send_sock;


	/* make sure we know the assosociated tranaction ... */
	if (t_check( p_msg  , &branch , &local_cancel)==-1)
		return 1;
	/* ... if there is no such, tell the core router to forward statelessly */
	if ( T<=0 ) return 1;

	DBG("DEBUG: t_on_reply: org. status uas=%d, uac[%d]=%d loca_cancel=%d)\n",
		T->uas.status, branch, T->uac[branch].status, local_cancel);

	/* special cases (local cancel reply) -bogdan */
	if (local_cancel==1)
	{
		reset_timer( hash_table, &(T->uac[branch].request.retr_timer));
		if ( p_msg->REPLY_STATUS>=200 )
			reset_timer(hash_table,&(T->uac[branch].request.fr_timer));
		goto error;
	}

	/* do we have via2 ? - maybe we'll need it for forwarding -bogdan*/
	if ((p_msg->via2==0) || (p_msg->via2->error!=VIA_PARSE_OK)){
		/* no second via => error */
		LOG(L_ERR, "ERROR: t_on_reply: no 2nd via found in reply\n");
		goto error;
	}

	msg_status=p_msg->REPLY_STATUS;
	msg_class=REPLY_CLASS(p_msg);
	is_invite= T->uas.request->REQ_METHOD==METHOD_INVITE;

#ifdef _DONT_DO_IT_MAN
/*  generate the retrans buffer, make a simplified
	assumption everything but 100 will be fwd-ed;
	sometimes it will result in useless CPU cycles
	but mostly the assumption holds and allows the
	work to be done out of criticial lock region */
	if (msg_status==100 && T->uac[branch].status)
		buf=0;
	else {
		/* buf maybe allo'ed*/
		buf = build_res_buf_from_sip_res ( p_msg, &res_len);
		if (!buf) {
			LOG(L_ERR, "ERROR: t_on_reply_received: "
			"no mem for outbound reply buffer\n");
			goto error;
		}
	}
#endif

	/* *** stop timers *** */
	/* stop retransmission */
	reset_timer( hash_table, &(T->uac[branch].request.retr_timer));
	/* stop final response timer only if I got a final response */
	if ( msg_class>1 )
		reset_timer( hash_table, &(T->uac[branch].request.fr_timer));

	LOCK_REPLIES( T );
	/* if a got the first prov. response for an INVITE ->
	   change FR_TIME_OUT to INV_FR_TIME_UT */
	start_fr = !T->uac[branch].rpl_received && msg_class==1 && is_invite;

	/* *** store and relay message as needed *** */
	relay = t_should_relay_response( T , msg_status, branch, &save_clone );
	DBG("DEBUG: t_on_reply: branch=%d, save=%d, relay=%d\n",
		branch, save_clone, relay );

	if (save_clone)
	{
		str_foo = &(T->uac[branch].tag);
		str_foo->s = shm_resize(str_foo->s, (str_foo?0:TAG_OVERBUFFER_LEN) +
			get_to(p_msg)->tag_value.len);
		if (!str_foo->s)
		{
			LOG( L_ERR , "ERROR: t_on_reply: connot alocate memory!\n");
			goto error1;
		}
		/* when forking, replies greater then 300 are saved */
		if ((T->nr_of_outgoings>1 || T->uac[T->nr_of_outgoings].uri.s)
			&& msg_status>=300 )
		{
			DBG("DEBUG: t_on_reply: saving reply! \n");
			str_foo = &(T->uac[branch].rpl_buffer);
			str_foo->s = shm_resize(str_foo->s, res_len+
				(str_foo->s?0:REPLY_OVERBUFFER_LEN) );
			if (!str_foo->s)
			{
				LOG( L_ERR , "ERROR: t_on_reply: connot alocate memory!\n");
				goto error1;
			}
			memcpy(str_foo->s,buf,res_len);
			str_foo->len = res_len;
		}
		/*copy the TO tag from reply*/
		T->uac[branch].tag.len = get_to(p_msg)->tag_value.len;
		memcpy( T->uac[branch].tag.s, get_to(p_msg)->tag_value.s,
			T->uac[branch].tag.len );
		T->uac[branch].rpl_received = 1;
		T->uac[branch].status = msg_status;
	}

	rb = & T->uas.response;
	if (relay >= 0  && (relay=check_for_no_response(T,msg_status,relay))>=0 ) {

		buf = build_res_buf_from_sip_res ( p_msg, &res_len);
		if (!buf) {
			UNLOCK_REPLIES( T );
			start_fr = 1;
			LOG(L_ERR, "ERROR: t_on_reply_received: "
				"no mem for outbound reply buffer\n");
			goto error1;
		}
		callback_event( TMCB_REPLY_IN, T, p_msg );

		if (relay!=branch)
		{
			str_foo = &(T->uac[relay].rpl_buffer);
			if (buf) pkg_free(buf);
			buf = (char*)pkg_malloc(str_foo->len);
			if (!buf)
			{
				UNLOCK_REPLIES( T );
				start_fr = 1;
				LOG(L_ERR, "ERROR: t_on_reply: cannot alloc pkg mem\n");
				goto error1;
			}
			memcpy( buf , str_foo->s , str_foo->len );
			res_len = str_foo->len;
		}
		/* if there is no reply yet, initialize the structure */
		if ( ! rb->buffer ) {
			/*init retrans buffer*/
			if (update_sock_struct_from_via( &(rb->to),p_msg->via2 )==-1) {
				UNLOCK_REPLIES( T );
				start_fr = 1;
				LOG(L_ERR, "ERROR: t_on_reply: cannot lookup reply dst: %s\n",
					p_msg->via2->host.s );
				goto error1;
			}
			send_sock=get_send_socket(&rb->to);
			if (send_sock==0) {
				UNLOCK_REPLIES( T );
				LOG(L_ERR, "ERROR: t_on_reply: cannot fwd to af %d "
					"no socket\n", rb->to.s.sa_family);
				start_fr=1;
				goto error1;
			}
			/* rb->to.sin_family = AF_INET; */
			rb->send_sock=send_sock;
			rb->activ_type = p_msg->REPLY_STATUS;
			/* allocate something more for the first message;
			   subsequent messages will be longer and buffer
			   reusing will save us a malloc lock */
			alloc_len = res_len + REPLY_OVERBUFFER_LEN ;
		}else{
			alloc_len = res_len;
		}
		/* puts the reply's buffer to uas.response */
		if (! (rb->buffer = (char*)shm_resize( rb->buffer, alloc_len ))) {
			UNLOCK_REPLIES( T );
			start_fr = 1;
			LOG(L_ERR, "ERROR: t_on_reply: cannot alloc shmem\n");
			goto error1;
		}
		rb->buffer_len = res_len;
		memcpy( rb->buffer, buf, res_len );
		/* update the status ... */
		T->uas.status = p_msg->REPLY_STATUS;
		T->uas.tag=&(T->uac[relay].tag);
		if (T->uas.status >=200 && T->relaied_reply_branch==-1 )
				T->relaied_reply_branch = relay;
	}; /* if relay ... */

	UNLOCK_REPLIES( T );

	if (relay >= 0) {
		SEND_PR_BUFFER( rb, buf, res_len );
		t_update_timers_after_sending_reply( rb );
		callback_event( TMCB_REPLY, T, p_msg );
	}

	/* *** ACK handling *** */
	if ( is_invite ) {
		if ( T->uac[branch].request.ack_len )
		{   /*retransmit*/
			/* I don't need any additional syncing here -- after ack
			   is introduced it's never changed */
			DBG("DEBUG: localy cached ACK retranmitted\n");
			SEND_ACK_BUFFER( &(T->uac[branch].request) );
		} else if (msg_class>2 ) {
			/*on a non-200 reply to INVITE*/
			DBG("DEBUG: t_on_reply_received: >=3xx reply to INVITE:"
				"send ACK\n");
			ack = build_ack( p_msg, T, branch , &ack_len);
			if (ack) {
				SEND_PR_BUFFER( &(T->uac[branch].request), ack, ack_len );
				/* append to transaction structure */
				attach_ack( T, branch, ack , ack_len );
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
		rb->retr_list = RT_T2;
		set_timer( hash_table, &(rb->retr_timer), RT_T2 );
	}
error1:
	if (start_fr)
		set_timer( hash_table, &(rb->fr_timer), FR_INV_TIMER_LIST );
	if (buf) pkg_free( buf );
error:
	T_UNREF( T );
	/* don't try to relay statelessly on error; on troubles, simply do nothing;
	   that will make the other party to retransmit; hopefuly, we'll then 
	   be better off */
	return 0;
}


/* Checks if the new reply (with new_code status) should be sent or not
 *  based on the current
 * transactin status.
 * Returns 	- branch number (0,1,...) which should be relayed
 *         -1 if nothing to be relayed
 */
int t_should_relay_response( struct cell *Trans , int new_code,
									int branch , int *should_store )
{
	//int T_code;
	int b, lowest_b, lowest_s;

	//if (Trans->uas.request->REQ_METHOD==METHOD_INVITE)
	//	T_code = Trans->uac[branch].status;
	//else
	//T_code = Trans->uas.status;

	/* note: this code never lets replies to CANCEL go through;
	   we generate always a local 200 for CANCEL; 200s are
	   not relayed because it's not an INVITE transaction;
	   >= 300 are not relayed because 200 was already sent
	   out
	*/
	DBG("->>>>>>>>> T_code=%d, new_code=%d\n",Trans->uas.status,new_code);
	/* if final response sent out, allow only INVITE 2xx  */
	if ( Trans->uas.status >= 200 ) {
		if (new_code>=200 && new_code < 300  && 
			Trans->uas.request->REQ_METHOD==METHOD_INVITE) {
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
			/* dirty hack by Jiri -- subject to clean up as all the
			   reply_processing crap; if there are no branches at
			   all, I guess TM wants to reply itself and allow that
			*/
			if (Trans->nr_of_outgoings==0)
				return 0;
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
				if ( Trans->uac[b].status<200 )
					return -1;
				if ( Trans->uac[b].status<lowest_s )
				{
					lowest_b =b;
					lowest_s = T->uac[b].status;
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
			if (!T->uac[branch].rpl_received) *should_store=1;
				else *should_store=0;
			if (Trans->uas.status==0) return branch;
				else return -1;
		}
	}

	LOG(L_CRIT, "ERROR: Oh my gooosh! We don't know whether to relay\n");
	abort();
}

