/*
 * $Id$
 *
 */


#include "../../hash_func.h"
#include "t_funcs.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "../../timer.h"
#include "../../error.h"
#include "../../action.h"
#include "../../dset.h"

#include "t_hooks.h"
#include "t_funcs.h"
#include "t_reply.h"
#include "t_cancel.h"
#include "t_msgbuilder.h"
#include "t_lookup.h"
#include "t_fwd.h"
#include "fix_lumps.h"
#include "t_stats.h"

/* where to go if there is no positive reply */
static int goto_on_negative=0;

/* we store the reply_route # in private memory which is
   then processed during t_relay; we cannot set this value
   before t_relay creates transaction context or after
   t_relay when a reply may arrive after we set this
   value; that's why we do it how we do it, i.e.,
   *inside*  t_relay using hints stored in private memory
   before t_reay is called
*/
  
  
int t_on_negative( unsigned int go_to )
{
	goto_on_negative=go_to;
	return 1;
}


unsigned int get_on_negative()
{
	return goto_on_negative;
}

static void update_reply_stats( int code ) {
	if (code>=600) {
		acc_stats->completed_6xx++;
	} else if (code>=500) {
		acc_stats->completed_5xx++;
	} else if (code>=400) {
		acc_stats->completed_4xx++;
	} else if (code>=300) {
		acc_stats->completed_3xx++;
	} else if (code>=200) {
		acc_stats->completed_2xx++;
	}
}


/* the main code of stateful replying */
static int _reply( struct cell *t, struct sip_msg* p_msg, unsigned int code,
    char * text, int lock );

/* Retransmits the last sent inbound reply.
 * input: p_msg==request for which I want to retransmit an associated reply
 * Returns  -1 - error
 *           1 - OK
 */
int t_retransmit_reply( struct cell *t )
{
	static char b[BUF_SIZE];
	int len;

	/* first check if we managed to resolve topmost Via -- if
	   not yet, don't try to retransmit
	*/
	if (!t->uas.response.send_sock) {
		LOG(L_ERR, "ERROR: no resolved dst to retransmit\n");
		return -1;
	}

	/* we need to lock the transaction as messages from
	   upstream may change it continuously
	*/
	LOCK_REPLIES( t );

	if (!t->uas.response.buffer) {
		DBG("DBG: t_retransmit_reply: nothing to retransmit\n");
		goto error;
	}

	len=t->uas.response.buffer_len;
	if ( len==0 || len>BUF_SIZE )  {
		DBG("DBG: t_retransmit_reply: "
			"zero length or too big to retransmit: %d\n", len);
		goto error;
	}
	memcpy( b, t->uas.response.buffer, len );
	UNLOCK_REPLIES( t );
	SEND_PR_BUFFER( & t->uas.response, b, len );
	DBG("DEBUG: reply retransmitted. buf=%p: %.9s..., shmem=%p: %.9s\n", 
		b, b, t->uas.response.buffer, t->uas.response.buffer );
	return 1;

error:
	UNLOCK_REPLIES(t);
	return -1;
}



int t_reply( struct cell *t, struct sip_msg* p_msg, unsigned int code, 
	char * text )
{
	return _reply( t, p_msg, code, text, 1 /* lock replies */ );
}

int t_reply_unsafe( struct cell *t, struct sip_msg* p_msg, unsigned int code, 
	char * text )
{
	return _reply( t, p_msg, code, text, 0 /* don't lock replies */ );
}



/* send a UAS reply
 * returns 1 if everything was OK or -1 for error
 */
static int _reply( struct cell *trans, struct sip_msg* p_msg, 
	unsigned int code, char * text, int lock )
{
	unsigned int len, buf_len=0;
	char * buf;
	struct retr_buf *rb;

	branch_bm_t cancel_bitmap;

	if (code>=200) trans->kr|=REQ_RPLD;
	/*
	buf = build_res_buf_from_sip_req(code,text,trans->uas.tag->s,
		trans->uas.tag->len, trans->uas.request,&len);
	*/
	cancel_bitmap=0;
	/* compute the buffer in private memory prior to entering lock */
	buf = build_res_buf_from_sip_req(code,text, 0,0, /* no to-tag */
		p_msg,&len);
	DBG("DEBUG: t_reply: buffer computed\n");
	if (!buf)
	{
		DBG("DEBUG: t_reply: response building failed\n");
		/* determine if there are some branches to be cancelled */
		if (trans->is_invite) {
			if (lock) LOCK_REPLIES( trans );
			which_cancel(trans, &cancel_bitmap );
			if (lock) UNLOCK_REPLIES( trans );
		}
		/* and clean-up, including cancellations, if needed */
		goto error;
	}

	if (lock) LOCK_REPLIES( trans );
	if (trans->is_invite) which_cancel(trans, &cancel_bitmap );
	if (trans->uas.status>=200) {
		LOG( L_ERR, "ERROR: t_reply: can't generate replies"
			"when a final was sent out\n");
		goto error2;
	}
	rb = & trans->uas.response;
	rb->activ_type=code;

	trans->uas.status = code;
	buf_len = rb->buffer ? len : len + REPLY_OVERBUFFER_LEN;
	rb->buffer = (char*)shm_resize( rb->buffer, buf_len );
	/* puts the reply's buffer to uas.response */
	if (! rb->buffer ) {
			LOG(L_ERR, "ERROR: t_reply: cannot allocate shmem buffer\n");
			goto error2;
	}
	rb->buffer_len = len ;
	memcpy( rb->buffer , buf , len );
	/* needs to be protected too because what timers are set depends
	   on current transactions status */
	/* t_update_timers_after_sending_reply( rb ); */
	update_reply_stats( code );
	acc_stats->replied_localy++;
	if (lock) UNLOCK_REPLIES( trans );
	
	/* do UAC cleanup procedures in case we generated
	   a final answer whereas there are pending UACs */
	if (code>=200) {
		cleanup_uac_timers( trans );
		if (trans->is_invite) cancel_uacs( trans, cancel_bitmap );
		set_final_timer( /* hash_table, */ trans );
	}

	/* send it out */
	/* first check if we managed to resolve topmost Via -- if
	   not yet, don't try to retransmit
	*/
	if (!trans->uas.response.send_sock) {
		LOG(L_ERR, "ERROR: _reply: no resolved dst to send reply to\n");
	} else {
		SEND_PR_BUFFER( rb, buf, len );
		DBG("DEBUG: reply sent out. buf=%p: %.9s..., shmem=%p: %.9s\n", 
			buf, buf, rb->buffer, rb->buffer );
	}
	pkg_free( buf ) ;
	DBG("DEBUG: t_reply: finished\n");
	return 1;

error2:
	if (lock) UNLOCK_REPLIES( trans );
	pkg_free ( buf );
error:
	/* do UAC cleanup */
	cleanup_uac_timers( trans );
	if (trans->is_invite) cancel_uacs( trans, cancel_bitmap );
	/* we did not succeed -- put the transaction on wait */
	put_on_wait(trans);
	return -1;
}

void set_final_timer( /* struct s_table *h_table, */ struct cell *t )
{
	if ( !t->local 
		&& t->uas.request->REQ_METHOD==METHOD_INVITE 
		&& t->uas.status>=300  ) {
			/* crank timers for negative replies */
			start_retr( &t->uas.response );
	} else put_on_wait(t);
}

void cleanup_uac_timers( struct cell *t )
{
	int i;

	/* reset FR/retransmission timers */
	for (i=0; i<t->nr_of_outgoings; i++ )  {
		reset_timer( hash_table, &t->uac[i].request.retr_timer );
		reset_timer( hash_table, &t->uac[i].request.fr_timer );
	}
	DBG("DEBUG: cleanup_uacs: RETR/FR timers reset\n");
}

int store_reply( struct cell *trans, int branch, struct sip_msg *rpl)
{
#		ifdef EXTRA_DEBUG
		if (trans->uac[branch].reply) {
			LOG(L_ERR, "ERROR: replacing stored reply; aborting\n");
			abort();
		}
#		endif

		/* when we later do things such as challenge aggregation,
	   	   we should parse the message here before we conservate
		   it in shared memory; -jiri
		*/
		if (rpl==FAKED_REPLY)
			trans->uac[branch].reply=FAKED_REPLY;
		else
			trans->uac[branch].reply = sip_msg_cloner( rpl );

		if (! trans->uac[branch].reply ) {
			LOG(L_ERR, "ERROR: store_reply: can't alloc' clone memory\n");
			return 0;
		}

		return 1;
}

/* this is the code which decides what and when shall be relayed
   upstream; note well -- it assumes it is entered locked with 
   REPLY_LOCK and it returns unlocked!
*/
enum rps relay_reply( struct cell *t, struct sip_msg *p_msg, int branch, 
	unsigned int msg_status, branch_bm_t *cancel_bitmap )
{
	int relay;
	int save_clone;
	char *buf;
	/* length of outbound reply */
	unsigned int res_len;
	int relayed_code;
	struct sip_msg *relayed_msg;
	str	to_tag;
	enum rps reply_status;
	/* retransmission structure of outbound reply and request */
	struct retr_buf *uas_rb;

	/* keep compiler warnings about use of uninit vars silent */
	res_len=0;
	buf=0;
	relayed_msg=0;
	relayed_code=0;


	/* remember, what was sent upstream to know whether we are
	   forwarding a first final reply or not */

	/* *** store and relay message as needed *** */
	reply_status = t_should_relay_response(t, msg_status, branch, 
		&save_clone, &relay, cancel_bitmap );
	DBG("DEBUG: relay_reply: branch=%d, save=%d, relay=%d\n",
		branch, save_clone, relay );

	/* store the message if needed */
	if (save_clone) /* save for later use, typically branch picking */
	{
		if (!store_reply( t, branch, p_msg ))
			goto error01;
	}

	uas_rb = & t->uas.response;
	if (relay >= 0 ) {

		/* initialize sockets for outbound reply */
		uas_rb->activ_type=msg_status;
		/* only messages known to be relayed immediately will be
		   be called on; we do not evoke this callback on messages
		   stored in shmem -- they are fixed and one cannot change them
		   anyway 
        */
		if (msg_status<300 && branch==relay) {
			callback_event( TMCB_REPLY_IN, t, p_msg, msg_status );
		}
		/* try bulding the outbound reply from either the current
	       or a stored message */
		relayed_msg = branch==relay ? p_msg :  t->uac[relay].reply;
		if (relayed_msg ==FAKED_REPLY) {
			relayed_code = branch==relay
				? msg_status : t->uac[relay].last_received;
			buf = build_res_buf_from_sip_req( relayed_code,
				error_text(relayed_code), 0,0, /* no to-tag */
				t->uas.request, &res_len );
		} else {
			relayed_code=relayed_msg->REPLY_STATUS;
			buf = build_res_buf_from_sip_res( relayed_msg, &res_len );
			/* if we build a message from shmem, we need to remove
			   via delete lumps which are now stirred in the shmem-ed
			   structure
			*/
			if (branch!=relay) {
				free_via_lump(&relayed_msg->repl_add_rm);
			}
		}
		update_reply_stats( relayed_code );
		if (!buf) {
			LOG(L_ERR, "ERROR: relay_reply: "
				"no mem for outbound reply buffer\n");
			goto error02;
		}

		/* attempt to copy the message to UAS's shmem:
		   - copy to-tag for ACK matching as well
		   -  allocate little a bit more for provisionals as
		      larger messages are likely to follow and we will be 
		      able to reuse the memory frag 
		*/
		uas_rb->buffer = (char*)shm_resize( uas_rb->buffer, res_len +
			(msg_status<200 ?  REPLY_OVERBUFFER_LEN : 0));
		if (!uas_rb->buffer) {
			LOG(L_ERR, "ERROR: relay_reply: cannot alloc reply shmem\n");
			goto error03;
		}
		uas_rb->buffer_len = res_len;
		memcpy( uas_rb->buffer, buf, res_len );
		/* to tag now */
		if (relayed_code>=300 && t->is_invite) {
			if (relayed_msg!=FAKED_REPLY) {
				to_tag=get_to(relayed_msg)->tag_value;
				t->uas.to_tag.s=(char *)shm_resize( t->uas.to_tag.s,
					to_tag.len );
				if (!t->uas.to_tag.s) {
					LOG(L_ERR, "ERROR: no shmem for to-tag\n");
					goto error04;
				}
				t->uas.to_tag.len=to_tag.len;
				memcpy(t->uas.to_tag.s, to_tag.s, to_tag.len );
			} else {
				if (t->uas.to_tag.s) shm_free(t->uas.to_tag.s);
				t->uas.to_tag.s=0;
				t->uas.to_tag.len=0;
			}
		}

		/* update the status ... */
		t->uas.status = relayed_code;
		t->relaied_reply_branch = relay;
	}; /* if relay ... */

	UNLOCK_REPLIES( t );

	/* send it now (from the private buffer) */
	if (relay >= 0) {
		SEND_PR_BUFFER( uas_rb, buf, res_len );
		DBG("DEBUG: reply relayed. buf=%p: %.9s..., shmem=%p: %.9s\n", 
			buf, buf, uas_rb->buffer, uas_rb->buffer );
		callback_event( TMCB_REPLY, t, relayed_msg, relayed_code );
		pkg_free( buf );
	}

	/* success */
	return reply_status;

error04:
	shm_free( uas_rb->buffer );
	uas_rb->buffer=0;
error03:
	pkg_free( buf );
error02:
	if (save_clone) {
		if (t->uac[branch].reply!=FAKED_REPLY)
			sip_msg_free( t->uac[branch].reply );
		t->uac[branch].reply = NULL;	
	}
error01:
	t_reply_unsafe( t, t->uas.request, 500, "Reply processing error" );
	UNLOCK_REPLIES(t);
	if (t->is_invite) cancel_uacs( t, *cancel_bitmap );
	/* a serious error occured -- attempt to send an error reply;
	   it will take care of clean-ups 
	*/

	/* failure */
	return RPS_ERROR;
}

/* this is the "UAC" above transaction layer; if a final reply
   is received, it triggers a callback; note well -- it assumes
   it is entered locked with REPLY_LOCK and it returns unlocked!
*/
enum rps local_reply( struct cell *t, struct sip_msg *p_msg, int branch, 
	unsigned int msg_status, branch_bm_t *cancel_bitmap)
{
	/* how to deal with replies for local transaction */
	int local_store, local_winner;
	enum rps reply_status;
	struct sip_msg *winning_msg;
	int winning_code;
	/* branch_bm_t cancel_bitmap; */

	/* keep warning 'var might be used un-inited' silent */	
	winning_msg=0;
	winning_code=0;

	*cancel_bitmap=0;

	reply_status=t_should_relay_response( t, msg_status, branch,
		&local_store, &local_winner, cancel_bitmap );
	DBG("DEBUG: local_reply: branch=%d, save=%d, winner=%d\n",
		branch, local_store, local_winner );
	if (local_store) {
		if (!store_reply(t, branch, p_msg))
			goto error;
	}
	if (local_winner>=0) {
		winning_msg= branch==local_winner 
			? p_msg :  t->uac[local_winner].reply;
		if (winning_msg==FAKED_REPLY) {
			winning_code = branch==local_winner
				? msg_status : t->uac[local_winner].last_received;
		} else {
			winning_code=winning_msg->REPLY_STATUS;
		}
		t->uas.status = winning_code;
		update_reply_stats( winning_code );
	}
	UNLOCK_REPLIES(t);
	if (local_winner>=0 && winning_code>=200 ) {
		DBG("DEBUG: local transaction completed\n");
		callback_event( TMCB_LOCAL_COMPLETED, t, winning_msg, winning_code );
		if (t->completion_cb) 
			t->completion_cb( t, winning_msg, winning_code, 0 /* empty param */);
	}
	return reply_status;

error:
	which_cancel(t, cancel_bitmap);
	UNLOCK_REPLIES(t);
	cleanup_uac_timers(t);
	if ( get_cseq(p_msg)->method.len==INVITE_LEN 
		&& memcmp( get_cseq(p_msg)->method.s, INVITE, INVITE_LEN)==0)
		cancel_uacs( t, *cancel_bitmap );
	put_on_wait(t);
	return RPS_ERROR;
}





/*  This function is called whenever a reply for our module is received; 
  * we need to register  this function on module initialization;
  *  Returns :   0 - core router stops
  *              1 - core router relay statelessly
  */
int t_on_reply( struct sip_msg  *p_msg )
{

	int msg_status;
	char *ack;
	unsigned int ack_len;
	int branch;
	/* has the transaction completed now and we need to clean-up? */
	int reply_status;
	branch_bm_t cancel_bitmap;
	struct ua_client *uac;
	struct cell *t;


	/* make sure we know the assosociated transaction ... */
	if (t_check( p_msg  , &branch )==-1)
		return 1;
	/*... if there is none, tell the core router to fwd statelessly */
	t=get_t();
	if ( t<=0 ) return 1;

	cancel_bitmap=0;
	msg_status=p_msg->REPLY_STATUS;

	uac=&t->uac[branch];
	DBG("DEBUG: t_on_reply: org. status uas=%d, "
		"uac[%d]=%d local=%d is_invite=%d)\n",
		t->uas.status, branch, uac->last_received, 
		t->local, t->is_invite);

	/* it's a cancel ... ? */
	if (get_cseq(p_msg)->method.len==CANCEL_LEN 
		&& memcmp( get_cseq(p_msg)->method.s, CANCEL, CANCEL_LEN)==0
		/* .. which is not e2e ? ... */
		&& t->is_invite ) {
			/* ... then just stop timers */
			reset_timer( hash_table, &uac->local_cancel.retr_timer);
			if ( msg_status >= 200 )
				reset_timer( hash_table, &uac->local_cancel.fr_timer);
			DBG("DEBUG: reply to local CANCEL processed\n");
			goto done;
	}


	/* *** stop timers *** */
	/* stop retransmission */
	reset_timer( hash_table, &uac->request.retr_timer);
	/* stop final response timer only if I got a final response */
	if ( msg_status >= 200 )
		reset_timer( hash_table, &uac->request.fr_timer);

	LOCK_REPLIES( t );
	if (t->local) {
		reply_status=local_reply( t, p_msg, branch, msg_status, &cancel_bitmap );
	} else {
		reply_status=relay_reply( t, p_msg, branch, msg_status, 
			&cancel_bitmap );
	}

	if (reply_status==RPS_ERROR)
		goto done;

	/* acknowledge negative INVITE replies */	
	if (t->is_invite && (msg_status>=300 || (t->local && msg_status>=200))) {
		ack = build_ack( p_msg, t, branch , &ack_len);
		if (ack) {
			SEND_PR_BUFFER( &uac->request, ack, ack_len );
			shm_free(ack);
		}
	} /* ack-ing negative INVITE replies */

	/* clean-up the transaction when transaction completed */
	if (reply_status==RPS_COMPLETED) {
		/* no more UAC FR/RETR (if I received a 2xx, there may
		   be still pending branches ...
		*/
		cleanup_uac_timers( t );	
		if (t->is_invite) cancel_uacs( t, cancel_bitmap );
		/* FR for negative INVITES, WAIT anything else */
		set_final_timer( /* hash_table,*/ t );
	} 

	/* update FR/RETR timers on provisional replies */
	if (msg_status<200) { /* provisional now */
		if (t->is_invite) { 
			/* invite: change FR to longer FR_INV, do not
			   attempt to restart retransmission any more
			*/
			set_timer( hash_table, & uac->request.fr_timer,
				FR_INV_TIMER_LIST );
		} else {
			/* non-invite: restart retransmisssions (slow now) */
			uac->request.retr_list=RT_T2;
			set_timer( hash_table, 
				& uac->request.retr_timer, RT_T2 );
		}
	} /* provisional replies */

done:
#ifdef _OBSOLETED
	/* moved to  script callback */
	UNREF( t );
	T=T_UNDEFINED;
#endif
	/* don't try to relay statelessly neither on success
       (we forwarded statefuly) nor on error; on troubles, 
	   simply do nothing; that will make the other party to 
	   retransmit; hopefuly, we'll then be better off */
	return 0;
}


/* This is the neuralgical point of reply processing -- called
 * from within a REPLY_LOCK, t_should_relay_response decides
 * how a reply shall be processed and how transaction state is
 * affected.
 *
 * Checks if the new reply (with new_code status) should be sent or not
 *  based on the current
 * transactin status.
 * Returns 	- branch number (0,1,...) which should be relayed
 *         -1 if nothing to be relayed
 */
enum rps t_should_relay_response( struct cell *Trans , int new_code,
	int branch , int *should_store, int *should_relay,
	branch_bm_t *cancel_bitmap )
{
	int b, lowest_b, lowest_s, dummy;

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
			*should_store=0;
			Trans->uac[branch].last_received=new_code;
			*should_relay=branch;
			return RPS_PUSHED_AFTER_COMPLETION;
		} else {
			/* except the exception above, too late  messages will
			   be discarded */
			*should_store=0;
			*should_relay=-1;
			return RPS_DISCARDED;
		}
	} 

	/* no final response sent yet */
	/* negative replies subject to fork picking */
	if (new_code >=300 ) {
		/* negative reply received after we have received
		   a final reply previously -- discard , unless
		   a recoverable error occured, in which case
		   retry
	    */
		if (Trans->uac[branch].last_received>=200) {
			/* then drop! */
			*should_store=0;
			*should_relay=-1;
			return RPS_DISCARDED;
		}

		Trans->uac[branch].last_received=new_code;
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
			/* skip 'empty branches' */
			if (!Trans->uac[b].request.buffer) continue;
			/* there is still an unfinished UAC transaction; wait now! */
			if ( Trans->uac[b].last_received<200 ) {
				*should_store=1;	
				*should_relay=-1;
				return RPS_STORE;
			}
			if ( Trans->uac[b].last_received<lowest_s )
			{
				lowest_b =b;
				lowest_s = Trans->uac[b].last_received;
			}
		} /* find lowest branch */
		if (lowest_b==-1) {
			LOG(L_CRIT, "ERROR: t_should_relay_response: lowest==-1\n");
		}
		/* no more pending branches -- try if that changes after
		   a callback
		*/
		callback_event( TMCB_ON_NEGATIVE, Trans, 0, lowest_s );
		/* look if the callback introduced new branches ... */
		init_branch_iterator();
		if (next_branch(&dummy)) {
			if (t_forward_nonack(Trans, Trans->uas.request, 
						(struct proxy_l *) 0 ) <0) {
				/* error ... behave as if we did not try to
				   add a new branch */
				*should_store=0;
				*should_relay=lowest_b;
				return RPS_COMPLETED;
			}
			/* we succeded to launch new branches -- await
			   result
			*/
			*should_store=1;
			*should_relay=-1;
			return RPS_STORE;
		}
		/* look if the callback perhaps replied transaction */
		if (Trans->uas.status >= 200) {
			*should_store=0;
			*should_relay=-1;
			/* this might deserve an improvement -- if something
			   was already replied, it was put on wait and then,
			   returning RPS_COMPLETED will make t_on_reply
			   put it on wait again; perhaps splitting put_on_wait
			   from send_reply or a new RPS_ code would be healthy
			*/
			return RPS_COMPLETED;
		}
		/* really no more pending branches -- return lowest code */
		*should_store=0;
		*should_relay=lowest_b;
		/* we dont need 'which_cancel' here -- all branches 
		   known to have completed */
		/* which_cancel( Trans, cancel_bitmap ); */
		return RPS_COMPLETED;
	} 

	/* not >=300 ... it must be 2xx or provisional 1xx */
	if (new_code>=100) {
		/* 1xx and 2xx except 100 will be relayed */
		Trans->uac[branch].last_received=new_code;
		*should_store=0;
		*should_relay= new_code==100? -1 : branch;
		if (new_code>=200 ) {
			which_cancel( Trans, cancel_bitmap );
			return RPS_COMPLETED;
		} else return RPS_PROVISIONAL;
	}

	/* reply_status didn't match -- it must be something weird */
	LOG(L_CRIT, "ERROR: Oh my gooosh! We don't know whether to relay %d\n",
		new_code);
	*should_store=0;
	*should_relay=-1;
	return RPS_DISCARDED;
}

char *build_ack(struct sip_msg* rpl,struct cell *trans,int branch,
	int *ret_len)
{
	str to;

    if ( parse_headers(rpl,HDR_TO, 0)==-1 || !rpl->to )
    {
        LOG(L_ERR, "ERROR: t_build_ACK: "
            "cannot generate a HBH ACK if key HFs in reply missing\n");
        return NULL;
    }
	to.len=rpl->to->body.s+rpl->to->body.len-rpl->to->name.s;
	to.s=rpl->orig+(rpl->to->name.s-rpl->buf);
    return build_local( trans, branch, ret_len,
        ACK, ACK_LEN, &to );
}

void on_negative_reply( struct cell* t, struct sip_msg* msg, 
	int code, void *param )
{
	int act_ret;
	struct sip_msg faked_msg;

	/* nobody cares about a negative transaction -- ok, return */
	if (!t->on_negative) {
		DBG("DBG: on_negative_reply: no on_negative\n");
		return;
	}

	DBG("DBG: on_negative_reply processed for transaction %p\n", t);

	/* create faked environment  -- uri rewriting stuff needs the
	   original uri
	*/
	memset( &faked_msg, 0, sizeof( struct sip_msg ));
	/* original URI doesn't change -- feel free to refer to shmem */
	faked_msg.first_line.u.request.uri=
		t->uas.request->first_line.u.request.uri;
	/* new_uri can change -- make a private copy */
	if (t->uas.request->new_uri.s!=0 && t->uas.request->new_uri.len!=0) {
		faked_msg.new_uri.s=pkg_malloc(t->uas.request->new_uri.len+1);
		if (!faked_msg.new_uri.s) return;
		faked_msg.new_uri.len=t->uas.request->new_uri.len;
		memcpy( faked_msg.new_uri.s, t->uas.request->new_uri.s, 
			faked_msg.new_uri.len);
		faked_msg.new_uri.s[faked_msg.new_uri.len]=0;
	} else { faked_msg.new_uri.s=0; faked_msg.new_uri.len=0; }
	faked_msg.flags=t->uas.request->flags;	
	/* if we set msg_id to something different from current's message
       id, the first t_fork will properly clean new branch URIs
	*/
	faked_msg.id=t->uas.request->id-1;

	act_ret=run_actions(reply_rlist[t->on_negative], &faked_msg );

	if (act_ret<0) {
		LOG(L_ERR, "on_negative_reply: Error in do_action\n");
	}

#ifdef _OBSOLETED
	/* this didn't work becaue URI is a part of shmem "monoblock";
	   I could split it but it does not seem to be worth the
	   effor
	*/
	/* project changes in faked message back to shmem copy */
	t->uas.request->flags=faked_msg.flags;
	if (faked_msg.new_uri.s) {
		t->uas.request->new_uri.s=shm_resize(t->uas.request->new_uri.s,
			faked_msg.new_uri.len);
		if (!t->uas.request->new_uri.s) goto done;
		memcpy(t->uas.request->new_uri.s, faked_msg.new_uri.s, 
			faked_msg.new_uri.len );
		t->uas.request->new_uri.len=faked_msg.new_uri.len;
	}
done:
#endif
	/* destroy faked environment, new_uri in particular */
	if (faked_msg.new_uri.s) pkg_free(faked_msg.new_uri.s);
}


