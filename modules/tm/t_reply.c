/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2003-01-19  faked lump list created in on_reply handlers
 *  2003-01-27  next baby-step to removing ZT - PRESERVE_ZT (jiri)
 *  2003-02-13  updated to use rb->dst (andrei)
 *  2003-02-18  replaced TOTAG_LEN w/ TOTAG_VALUE_LEN (TOTAG_LEN was defined
 *               twice with different values!)  (andrei)
 *  2003-02-28  scratchpad compatibility abandoned (jiri)
 *  2003-03-01  kr set through a function now (jiri)
 *  2003-03-06  saving of to-tags for ACK/200 matching introduced, 
 *              voicemail changes accepted, udpated to new callback
 *              names (jiri)
 *  2003-03-10  fixed new to tag bug/typo (if w/o {})  (andrei)
 *  2003-03-16  removed _TOTAG (jiri)
 *  2003-03-31  200 for INVITE/UAS resent even for UDP (jiri)
 *  2003-03-31  removed msg->repl_add_rm (andrei)
 *  2003-04-05  s/reply_route/failure_route, onreply_route introduced (jiri)
 *  2003-04-14  local acks generated before reply processing to avoid
 *              delays in length reply processing (like opening TCP
 *              connection to an unavailable destination) (jiri)
 *  2003-09-11  updates to new build_res_buf_from_sip_req() interface (bogdan)
 *  2003-09-11  t_reply_with_body() reshaped to use reply_lumps +
 *              build_res_buf_from_sip_req() instead of
 *              build_res_buf_with_body_from_sip_req() (bogdan)
 *  2003-11-05  flag context updated from failure/reply handlers back
 *              to transaction context (jiri)
 *  2003-11-11: build_lump_rpl() removed, add_lump_rpl() has flags (bogdan)
 *  2003-12-04  global TM callbacks switched to per transaction callbacks
 *              (bogdan)
 *  2004-02-06: support for user pref. added - destroy_avps (bogdan)
 *  2003-11-05  flag context updated from failure/reply handlers back
 *              to transaction context (jiri)
 *  2003-11-11: build_lump_rpl() removed, add_lump_rpl() has flags (bogdan)
 *  2004-02-13: t->is_invite and t->local replaced with flags (bogdan)
 *  2004-02-18  fifo_t_reply imported from vm module (bogdan)
 */


#include <assert.h>

#include "../../comp_defs.h"
#include "../../hash_func.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "../../timer.h"
#include "../../error.h"
#include "../../action.h"
#include "../../dset.h"
#include "../../tags.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../usr_avp.h"
#include "../../fifo_server.h"
#include "../../unixsock_server.h"

#include "defs.h"
#include "h_table.h"
#include "t_hooks.h"
#include "t_funcs.h"
#include "t_reply.h"
#include "t_cancel.h"
#include "t_msgbuilder.h"
#include "t_lookup.h"
#include "t_fwd.h"
#include "fix_lumps.h"
#include "t_stats.h"

/* are we processing original or shmemed request ? */
enum route_mode rmode=MODE_REQUEST;

/* private place where we create to-tags for replies */
/* janakj: made public, I need to access this value to store it in dialogs */
char tm_tags[TOTAG_VALUE_LEN];
/* bogdan: pack tm_tag buffer and len into a str to pass them to
 * build_res_buf_from_sip_req() */
static str  tm_tag = {tm_tags,TOTAG_VALUE_LEN};
char *tm_tag_suffix;

/* where to go if there is no positive reply */
static int goto_on_negative=0;
/* where to go on receipt of reply */
static int goto_on_reply=0;



/* we store the reply_route # in private memory which is
   then processed during t_relay; we cannot set this value
   before t_relay creates transaction context or after
   t_relay when a reply may arrive after we set this
   value; that's why we do it how we do it, i.e.,
   *inside*  t_relay using hints stored in private memory
   before t_reay is called
*/


void t_on_negative( unsigned int go_to )
{
	struct cell *t = get_t();

	/* in MODE_REPLY and MODE_ONFAILURE T will be set to current transaction;
	 * in MODE_REQUEST T will be set only if the transaction was already 
	 * created; if not -> use the static variable */
	if (!t || t==T_UNDEFINED )
		goto_on_negative=go_to;
	else
		get_t()->on_negative = go_to;
}


void t_on_reply( unsigned int go_to )
{
	struct cell *t = get_t();

	/* in MODE_REPLY and MODE_ONFAILURE T will be set to current transaction;
	 * in MODE_REQUEST T will be set only if the transaction was already 
	 * created; if not -> use the static variable */
	if (!t || t==T_UNDEFINED )
		goto_on_reply=go_to;
	else
		get_t()->on_reply = go_to;
}


unsigned int get_on_negative()
{
	return goto_on_negative;
}
unsigned int get_on_reply()
{
	return goto_on_reply;
}

void tm_init_tags()
{
	init_tags(tm_tags, &tm_tag_suffix, 
		"SER-TM/tags", TM_TAG_SEPARATOR );
}

/* returns 0 if the message was previously acknowledged
 * (i.e., no E2EACK callback is needed) and one if the
 * callback shall be executed */
int unmatched_totag(struct cell *t, struct sip_msg *ack)
{
	struct totag_elem *i;
	str *tag;

	if (parse_headers(ack, HDR_TO,0)==-1 || 
				!ack->to ) {
		LOG(L_ERR, "ERROR: ack_totag_set: To invalid\n");
		return 1;
	}
	tag=&get_to(ack)->tag_value;
	for (i=t->fwded_totags; i; i=i->next) {
		if (i->tag.len==tag->len
				&& memcmp(i->tag.s, tag->s, tag->len)==0) {
			DBG("DEBUG: totag for e2e ACK found: %d\n", i->acked);
			/* to-tag recorded, and an ACK has been received for it */
			if (i->acked) return 0;
			/* to-tag recorded, but this ACK came for the first time */
			i->acked=1;
			return 1;
		}
	}
	/* surprising: to-tag never sighted before */
	return 1;
}

static inline void update_local_tags(struct cell *trans, 
				struct bookmark *bm, char *dst_buffer,
				char *src_buffer /* to which bm refers */)
{
	if (bm->to_tag_val.s) {
		trans->uas.local_totag.s=bm->to_tag_val.s-src_buffer+dst_buffer;
		trans->uas.local_totag.len=bm->to_tag_val.len;
	}
}


/* append a newly received tag from a 200/INVITE to 
 * transaction's set; (only safe if called from within
 * a REPLY_LOCK); it returns 1 if such a to tag already
 * exists
 */
inline static int update_totag_set(struct cell *t, struct sip_msg *ok)
{
	struct totag_elem *i, *n;
	str *tag;
	char *s;

	if (!ok->to || !ok->to->parsed) {
		LOG(L_ERR, "ERROR: update_totag_set: to not parsed\n");
		return 0;
	}
	tag=&get_to(ok)->tag_value;
	if (!tag->s) {
		LOG(L_ERR, "ERROR: update_totag_set: no tag in to\n");
		return 0;
	}

	for (i=t->fwded_totags; i; i=i->next) {
		if (i->tag.len==tag->len
				&& memcmp(i->tag.s, tag->s, tag->len) ==0 ){
			/* to tag already recorded */
#ifdef XL_DEBUG
			LOG(L_CRIT, "DEBUG: update_totag_set: totag retranmission\n");
#else
			DBG("DEBUG: update_totag_set: totag retranmission\n");
#endif
			return 1;
		}
	}
	/* that's a new to-tag -- record it */
	shm_lock();
	n=(struct totag_elem*) shm_malloc_unsafe(sizeof(struct totag_elem));
	s=(char *)shm_malloc_unsafe(tag->len);
	shm_unlock();
	if (!s || !n) {
		LOG(L_ERR, "ERROR: update_totag_set: no  memory \n");
		if (n) shm_free(n);
		if (s) shm_free(s);
		return 0;
	}
	memset(n, 0, sizeof(struct totag_elem));
	memcpy(s, tag->s, tag->len );
	n->tag.s=s;n->tag.len=tag->len;
	n->next=t->fwded_totags;
	t->fwded_totags=n;
	DBG("DEBUG: update_totag_set: new totag \n");
	return 0;
}


static char *build_ack(struct sip_msg* rpl,struct cell *trans,int branch,
	unsigned int *ret_len)
{
	str to;

    if ( parse_headers(rpl,HDR_TO, 0)==-1 || !rpl->to )
    {
        LOG(L_ERR, "ERROR: t_build_ACK: "
            "cannot generate a HBH ACK if key HFs in reply missing\n");
        return NULL;
    }
	to.s=rpl->to->name.s;
	to.len=rpl->to->len;
    return build_local( trans, branch, ret_len,
        ACK, ACK_LEN, &to );
}

static int _reply_light( struct cell *trans, char* buf, unsigned int len,
			 unsigned int code, char * text, 
			 char *to_tag, unsigned int to_tag_len, int lock,
			 struct bookmark *bm	)
{
	struct retr_buf *rb;
	unsigned int buf_len;
	branch_bm_t cancel_bitmap;

	if (!buf)
	{
		DBG("DEBUG: t_reply: response building failed\n");
		/* determine if there are some branches to be cancelled */
		if ( is_invite(trans) ) {
			if (lock) LOCK_REPLIES( trans );
			which_cancel(trans, &cancel_bitmap );
			if (lock) UNLOCK_REPLIES( trans );
		}
		/* and clean-up, including cancellations, if needed */
		goto error;
	}

	cancel_bitmap=0;
	if (lock) LOCK_REPLIES( trans );
	if ( is_invite(trans) ) which_cancel(trans, &cancel_bitmap );
	if (trans->uas.status>=200) {
		LOG( L_ERR, "ERROR: t_reply: can't generate %d reply"
			" when a final %d was sent out\n", code, trans->uas.status);
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
			goto error3;
	}
	update_local_tags(trans, bm, rb->buffer, buf);

	rb->buffer_len = len ;
	memcpy( rb->buffer , buf , len );
	/* needs to be protected too because what timers are set depends
	   on current transactions status */
	/* t_update_timers_after_sending_reply( rb ); */
	update_reply_stats( code );
	trans->relaied_reply_branch=-2;
	tm_stats->replied_localy++;
	if (lock) UNLOCK_REPLIES( trans );
	
	/* do UAC cleanup procedures in case we generated
	   a final answer whereas there are pending UACs */
	if (code>=200) {
		if ( is_local(trans) ) {
			DBG("DEBUG: local transaction completed from _reply\n");
			if ( has_tran_tmcbs(trans, TMCB_LOCAL_COMPLETED) )
				run_trans_callbacks( TMCB_LOCAL_COMPLETED, trans,
					0, FAKED_REPLY, code);
		} else {
			if ( has_tran_tmcbs(trans, TMCB_RESPONSE_OUT) )
				run_trans_callbacks( TMCB_RESPONSE_OUT, trans,
					trans->uas.request, FAKED_REPLY, code);
		}

		cleanup_uac_timers( trans );
		if (is_invite(trans)) cancel_uacs( trans, cancel_bitmap );
		set_final_timer(  trans );
	}

	/* send it out */
	/* first check if we managed to resolve topmost Via -- if
	   not yet, don't try to retransmit
	*/
	if (!trans->uas.response.dst.send_sock) {
		LOG(L_ERR, "ERROR: _reply: no resolved dst to send reply to\n");
	} else {
		SEND_PR_BUFFER( rb, buf, len );
		DBG("DEBUG: reply sent out. buf=%p: %.9s..., shmem=%p: %.9s\n", 
			buf, buf, rb->buffer, rb->buffer );
	}
	pkg_free( buf ) ;
	DBG("DEBUG: t_reply: finished\n");
	return 1;

error3:
error2:
	if (lock) UNLOCK_REPLIES( trans );
	pkg_free ( buf );
error:
	/* do UAC cleanup */
	cleanup_uac_timers( trans );
	if ( is_invite(trans) ) cancel_uacs( trans, cancel_bitmap );
	/* we did not succeed -- put the transaction on wait */
	put_on_wait(trans);
	return -1;
}


/* send a UAS reply
 * returns 1 if everything was OK or -1 for error
 */
static int _reply( struct cell *trans, struct sip_msg* p_msg, 
	unsigned int code, char * text, int lock )
{
	unsigned int len;
	char * buf;
	struct bookmark bm;

	if (code>=200) set_kr(REQ_RPLD);
	/* compute the buffer in private memory prior to entering lock;
	 * create to-tag if needed */
	if (code>=180 && p_msg->to 
				&& (get_to(p_msg)->tag_value.s==0 
			    || get_to(p_msg)->tag_value.len==0)) {
		calc_crc_suffix( p_msg, tm_tag_suffix );
		buf = build_res_buf_from_sip_req(code,text, &tm_tag, p_msg, &len, &bm);
		return _reply_light( trans, buf, len, code, text,
			tm_tag.s, TOTAG_VALUE_LEN, lock, &bm);
	} else {
		buf = build_res_buf_from_sip_req(code,text, 0 /*no to-tag*/,
			p_msg, &len, &bm);

		return _reply_light(trans,buf,len,code,text,
			0, 0, /* no to-tag */lock, &bm);
	}
}


/*if msg is set -> it will fake the env. vars conforming with the msg; if NULL
 * the env. will be restore to original */
static inline void faked_env( struct cell *t,struct sip_msg *msg)
{
	static enum route_mode backup_mode;
	static struct cell *backup_t;
	static unsigned int backup_msgid;

	if (msg) {
	/* remember we are back in request processing, but process
	 * a shmem-ed replica of the request; advertise it in rmode;
	 * for example t_reply needs to know that
	 */
	backup_mode=rmode;
	rmode=MODE_ONFAILURE;
	/* also, tm actions look in beginning whether tranaction is
	 * set -- whether we are called from a reply-processing 
	 * or a timer process, we need to set current transaction;
	 * otherwise the actions would attempt to look the transaction
	 * up (unnecessary overhead, refcounting)
	 */
	/* backup */
	backup_t=get_t();
	backup_msgid=global_msg_id;
	/* fake transaction and message id */
		global_msg_id=msg->id;
		set_t(t);
	} else {
		/* restore original environment */
		set_t(backup_t);
		global_msg_id=backup_msgid;
		rmode=backup_mode;
	}
}


static inline int fake_req(struct sip_msg *faked_req, 
				struct sip_msg *shmem_msg)
{
	/* on_negative_reply faked msg now copied from shmem msg (as opposed
	 * to zero-ing) -- more "read-only" actions (exec in particular) will
	 * work from reply_route as they will see msg->from, etc.; caution,
	 * rw actions may append some pkg stuff to msg, which will possibly be
	 * never released (shmem is released in a single block) */
	memcpy( faked_req, shmem_msg, sizeof(struct sip_msg));

	/* if we set msg_id to something different from current's message
	 * id, the first t_fork will properly clean new branch URIs */
	faked_req->id=shmem_msg->id-1;
	/* msg->parsed_uri_ok must be reset since msg_parsed_uri is
	 * not cloned (and cannot be cloned) */
	faked_req->parsed_uri_ok = 0;

	/* new_uri can change -- make a private copy */
	if (shmem_msg->new_uri.s!=0 && shmem_msg->new_uri.len!=0) {
		faked_req->new_uri.s=pkg_malloc(shmem_msg->new_uri.len+1);
		if (!faked_req->new_uri.s) {
			LOG(L_ERR, "ERROR: fake_req: no uri/pkg mem\n");
			goto error00;
		}
		faked_req->new_uri.len=shmem_msg->new_uri.len;
		memcpy( faked_req->new_uri.s, shmem_msg->new_uri.s, 
			faked_req->new_uri.len);
		faked_req->new_uri.s[faked_req->new_uri.len]=0;
	}

#if 0
	/* create a duplicated lump list to which actions can add
	 * new pkg items  */
	if (shmem_msg->add_rm) {
		faked_req->add_rm=dup_lump_list(shmem_msg->add_rm);
		if (!faked_req->add_rm) { /* non_emty->empty ... failure */
			LOG(L_ERR, "ERROR: fake_req: lump dup failed\n");
			goto error01;
		}
	}
	/* same for the body lumps */
	if (shmem_msg->body_lumps) {
		faked_req->body_lumps=dup_lump_list(shmem_msg->body_lumps);
		if (!faked_req->body_lumps) { /* non_empty->empty ... failure */
			LOG(L_ERR, "ERROR: on_negative_handlers: lump dup failed\n");
			goto error02;
		}
	}
#endif
	return 1;

#if 0
error02:
	free_duped_lump_list(faked_req->add_rm);
error01:
	if (faked_req->new_uri.s) pkg_free(faked_req->new_uri.s);
#endif
error00:
	return 0;
}

void inline static free_faked_req(struct sip_msg *faked_req, struct cell *t)
{
	struct hdr_field *hdr;

#if 0
	free_duped_lump_list(faked_req->add_rm);
	free_duped_lump_list(faked_req->body_lumps);
	faked_req->add_rm = faked_req->body_lumps = 0;
#endif
	if (faked_req->new_uri.s) {
		pkg_free(faked_req->new_uri.s);
		faked_req->new_uri.s = 0;
	}

	/* free all types of lump that were added in failure handlers */
	del_nonshm_lump( &(faked_req->add_rm) );
	del_nonshm_lump( &(faked_req->body_lumps) );
	del_nonshm_lump_rpl( &(faked_req->reply_lump) );

	/* free header's parsed structures that were added by failure handlers */
	for( hdr=faked_req->headers ; hdr ; hdr=hdr->next ) {
		if ( hdr->parsed && hdr_allocs_parse(hdr) &&
		(hdr->parsed<(void*)t->uas.request ||
		hdr->parsed>=(void*)t->uas.end_request)) {
			/* header parsed filed doesn't point inside uas.request memory
			 * chunck -> it was added by failure funcs.-> free it as pkg */
			DBG("DBG:free_faked_req: removing hdr->parsed %d\n",
					hdr->type);
			clean_hdr_field(hdr);
			hdr->parsed = 0;
		}
	}
}


/* return 1 if a failure_route processes */
static inline int run_failure_handlers(struct cell *t, struct sip_msg *rpl,
																	int code)
{
	static struct sip_msg faked_req;
	struct sip_msg *shmem_msg = t->uas.request;
	int on_failure;

	/* failure_route for a local UAC? */
	if (!shmem_msg) {
		LOG(L_WARN, "Warning: run_failure_handlers: no UAC support\n");
		return 0;
	}

	/* don't start faking anything if we don't have to */
	if ( !has_tran_tmcbs( t, TMCB_ON_FAILURE) && !t->on_negative ) {
		return 1;
	}

	if (!fake_req(&faked_req, shmem_msg)) {
		LOG(L_ERR, "ERROR: run_failure_handlers: fake_req failed\n");
		return 0;
	}
	/* fake also the env. conforming to the fake msg */
	faked_env( t, &faked_req);
	/* DONE with faking ;-) -> run the failure handlers */

	if ( has_tran_tmcbs( t, TMCB_ON_FAILURE) ) {
		run_trans_callbacks( TMCB_ON_FAILURE, t, &faked_req, rpl, code);
	}
	if (t->on_negative) {
		/* avoid recursion -- if failure_route forwards, and does not 
		 * set next failure route, failure_route will not be rentered
		 * on failure */
		on_failure = t->on_negative;
		t->on_negative=0;
		/* run a reply_route action if some was marked */
		if (run_actions(failure_rlist[on_failure], &faked_req)<0)
			LOG(L_ERR, "ERROR: run_failure_handlers: Error in do_action\n");
		/* destroy any eventual avps */
		if (users_avps)
			destroy_avps();
	}

	/* restore original environment and free the fake msg */
	faked_env( t, 0);
	free_faked_req(&faked_req,t);

	/* if failure handler changed flag, update transaction context */
	shmem_msg->flags = faked_req.flags;
	return 1;
}


/* select a branch for forwarding; returns:
 * 0..X ... branch number
 * -1   ... error
 * -2   ... can't decide yet -- incomplete branches present
 */
int t_pick_branch(int inc_branch, int inc_code, struct cell *t, int *res_code)
{
	int lowest_b, lowest_s, b;

	lowest_b=-1; lowest_s=999;
	for ( b=0; b<t->nr_of_outgoings ; b++ ) {
		/* "fake" for the currently processed branch */
		if (b==inc_branch) {
			if (inc_code<lowest_s) {
				lowest_b=b;
				lowest_s=inc_code;
			}
			continue;
		}
		/* skip 'empty branches' */
		if (!t->uac[b].request.buffer) continue;
		/* there is still an unfinished UAC transaction; wait now! */
		if ( t->uac[b].last_received<200 ) 
			return -2;
		if ( t->uac[b].last_received<lowest_s ) {
			lowest_b =b;
			lowest_s = t->uac[b].last_received;
		}
	} /* find lowest branch */

	*res_code=lowest_s;
	return lowest_b;
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
static enum rps t_should_relay_response( struct cell *Trans , int new_code,
	int branch , int *should_store, int *should_relay,
	branch_bm_t *cancel_bitmap, struct sip_msg *reply )
{
	int branch_cnt;
	int picked_branch;
	int picked_code;
	int inv_through;

	/* note: this code never lets replies to CANCEL go through;
	   we generate always a local 200 for CANCEL; 200s are
	   not relayed because it's not an INVITE transaction;
	   >= 300 are not relayed because 200 was already sent
	   out
	*/
	DBG("->>>>>>>>> T_code=%d, new_code=%d\n",Trans->uas.status,new_code);
	inv_through=new_code>=200 && new_code<300 && is_invite(Trans);
	/* if final response sent out, allow only INVITE 2xx  */
	if ( Trans->uas.status >= 200 ) {
		if (inv_through) {
			DBG("DBG: t_should_relay: 200 INV after final sent\n");
			*should_store=0;
			Trans->uac[branch].last_received=new_code;
			*should_relay=branch;
			return RPS_PUSHED_AFTER_COMPLETION;
		} 
		/* except the exception above, too late  messages will
		   be discarded */
		goto discard;
	} 

	/* if final response received at this branch, allow only INVITE 2xx */
	if (Trans->uac[branch].last_received>=200
			&& !(inv_through && Trans->uac[branch].last_received<300)) {
		/* don't report on retranmissions */
		if (Trans->uac[branch].last_received==new_code) {
			DBG("DEBUG: final reply retrasnmission\n");
			goto discard;
		}
		/* if you FR-timed-out, faked a local 408 and 487 came, don't
		 * report on it either */
		if (Trans->uac[branch].last_received==408 && new_code==487) {
			DBG("DEBUG: 487 came for a timed-out branch\n");
			goto discard;
		}
		/* this looks however how a very strange status rewrite attempt;
		 * report on it */
		LOG(L_ERR, "ERROR: t_should_relay: status rewrite by UAS: "
			"stored: %d, received: %d\n",
			Trans->uac[branch].last_received, new_code );
		goto discard;
	}


	/* no final response sent yet */
	/* negative replies subject to fork picking */
	if (new_code >=300 ) {

		Trans->uac[branch].last_received=new_code;

		/* if all_final return lowest */
		picked_branch=t_pick_branch(branch,new_code, Trans, &picked_code);
		if (picked_branch==-2) { /* branches open yet */
			*should_store=1;
			*should_relay=-1;
			return RPS_STORE;
		}
		if (picked_branch==-1) {
			LOG(L_CRIT, "ERROR: t_should_relay_response: lowest==-1\n");
			goto error;
		}

		/* no more pending branches -- try if that changes after
		   a callback; save banch count to be able to determine
		   later if new branches were initiated */
		branch_cnt=Trans->nr_of_outgoings;

		/* run ON_FAILURE handlers ( route and callbacks) */
		if ( has_tran_tmcbs( Trans, TMCB_ON_FAILURE_RO|TMCB_ON_FAILURE)
		|| Trans->on_negative ) {
			run_failure_handlers( Trans,
				picked_branch==branch?reply:Trans->uac[picked_branch].reply, 
				picked_code);
		}

		/* look if the callback perhaps replied transaction; it also
		   covers the case in which a transaction is replied localy
		   on CANCEL -- then it would make no sense to proceed to
		   new branches bellow
		*/
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
		/* look if the callback/failure_route introduced new branches ... */
		if (branch_cnt<Trans->nr_of_outgoings)  {
			/* await then result of new branches */
			*should_store=1;
			*should_relay=-1;
			return RPS_STORE;
		}

		/* really no more pending branches -- return lowest code */
		*should_store=0;
		*should_relay=picked_branch;
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

error:
	/* reply_status didn't match -- it must be something weird */
	LOG(L_CRIT, "ERROR: Oh my gooosh! We don't know whether to relay %d\n",
		new_code);
discard:
	*should_store=0;
	*should_relay=-1;
	return RPS_DISCARDED;
}

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
	if (!t->uas.response.dst.send_sock) {
		LOG(L_WARN, "WARNING: t_retransmit_reply: "
			"no resolved dst to retransmit\n");
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





void set_final_timer( /* struct s_table *h_table, */ struct cell *t )
{
	if ( !is_local(t) && t->uas.request->REQ_METHOD==METHOD_INVITE ) {
		/* crank timers for negative replies */
		if (t->uas.status>=300) {
			start_retr(&t->uas.response);
			return;
		}
		/* local UAS retransmits too */
		if (t->relaied_reply_branch==-2 && t->uas.status>=200) {
			/* we retransmit 200/INVs regardless of transport --
			   even if TCP used, UDP could be used upstream and
			   loose the 200, which is not retransmitted by proxies
			*/
			force_retr( &t->uas.response );
			return;
		}
	} 
	put_on_wait(t);
}

void cleanup_uac_timers( struct cell *t )
{
	int i;

	/* reset FR/retransmission timers */
	for (i=0; i<t->nr_of_outgoings; i++ )  {
		reset_timer( &t->uac[i].request.retr_timer );
		reset_timer( &t->uac[i].request.fr_timer );
	}
	DBG("DEBUG: cleanup_uacs: RETR/FR timers reset\n");
}

static int store_reply( struct cell *trans, int branch, struct sip_msg *rpl)
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
			trans->uac[branch].reply = sip_msg_cloner( rpl, 0 );

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
	struct bookmark bm;
	int totag_retr;
	enum rps reply_status;
	/* retransmission structure of outbound reply and request */
	struct retr_buf *uas_rb;

	/* keep compiler warnings about use of uninit vars silent */
	res_len=0;
	buf=0;
	relayed_msg=0;
	relayed_code=0;
	totag_retr=0;


	/* remember, what was sent upstream to know whether we are
	 * forwarding a first final reply or not */

	/* *** store and relay message as needed *** */
	reply_status = t_should_relay_response(t, msg_status, branch, 
		&save_clone, &relay, cancel_bitmap, p_msg );
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
		 * be called on; we do not evoke this callback on messages
		 * stored in shmem -- they are fixed and one cannot change them
		 * anyway */
		if (msg_status<300 && branch==relay
		&& has_tran_tmcbs(t,TMCB_RESPONSE_FWDED) ) {
			run_trans_callbacks( TMCB_RESPONSE_FWDED, t, t->uas.request,
				p_msg, msg_status );
		}
		/* try bulding the outbound reply from either the current
		 * or a stored message */
		relayed_msg = branch==relay ? p_msg :  t->uac[relay].reply;
		if (relayed_msg==FAKED_REPLY) {
			tm_stats->replied_localy++;
			relayed_code = branch==relay
				? msg_status : t->uac[relay].last_received;

			if (relayed_code>=180 && t->uas.request->to 
					&& (get_to(t->uas.request)->tag_value.s==0 
			    		|| get_to(t->uas.request)->tag_value.len==0)) {
				calc_crc_suffix( t->uas.request, tm_tag_suffix );
				buf = build_res_buf_from_sip_req(
						relayed_code,
						error_text(relayed_code),
						&tm_tag,
						t->uas.request, &res_len, &bm );
			} else {
				buf = build_res_buf_from_sip_req( relayed_code,
					error_text(relayed_code), 0/* no to-tag */,
					t->uas.request, &res_len, &bm );
			}

		} else {
			relayed_code=relayed_msg->REPLY_STATUS;
			buf = build_res_buf_from_sip_res( relayed_msg, &res_len );
			/* if we build a message from shmem, we need to remove
			   via delete lumps which are now stirred in the shmem-ed
			   structure
			*/
			if (branch!=relay) {
				free_via_clen_lump(&relayed_msg->add_rm);
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
		if (relayed_msg==FAKED_REPLY) { /* to-tags for local replies */
			update_local_tags(t, &bm, uas_rb->buffer, buf);
		}
		tm_stats->replied_localy++;

		/* update the status ... */
		t->uas.status = relayed_code;
		t->relaied_reply_branch = relay;

		if (is_invite(t) && relayed_msg!=FAKED_REPLY
		&& relayed_code>=200 && relayed_code < 300
		&& has_tran_tmcbs( t, TMCB_RESPONSE_OUT|TMCB_E2EACK_IN) ) {
			totag_retr=update_totag_set(t, relayed_msg);
		}
	}; /* if relay ... */

	UNLOCK_REPLIES( t );

	/* send it now (from the private buffer) */
	if (relay >= 0) {
		SEND_PR_BUFFER( uas_rb, buf, res_len );
		DBG("DEBUG: reply relayed. buf=%p: %.9s..., shmem=%p: %.9s\n", 
			buf, buf, uas_rb->buffer, uas_rb->buffer );
		if (!totag_retr && has_tran_tmcbs(t, TMCB_RESPONSE_OUT) ) {
			run_trans_callbacks( TMCB_RESPONSE_OUT, t, t->uas.request,
				relayed_msg, relayed_code);
		}
		pkg_free( buf );
	}

	/* success */
	return reply_status;

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
	if (is_invite(t)) cancel_uacs( t, *cancel_bitmap );
	/* a serious error occured -- attempt to send an error reply;
	   it will take care of clean-ups  */

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
	int totag_retr;
	/* branch_bm_t cancel_bitmap; */

	/* keep warning 'var might be used un-inited' silent */	
	winning_msg=0;
	winning_code=0;
	totag_retr=0;

	*cancel_bitmap=0;

	reply_status=t_should_relay_response( t, msg_status, branch,
		&local_store, &local_winner, cancel_bitmap, p_msg );
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
			tm_stats->replied_localy++;
			winning_code = branch==local_winner
				? msg_status : t->uac[local_winner].last_received;
		} else {
			winning_code=winning_msg->REPLY_STATUS;
		}
		t->uas.status = winning_code;
		update_reply_stats( winning_code );
		if (is_invite(t) && winning_msg!=FAKED_REPLY
		&& winning_code>=200 && winning_code <300
		&& has_tran_tmcbs(t,TMCB_RESPONSE_OUT|TMCB_E2EACK_IN) )  {
			totag_retr=update_totag_set(t, winning_msg);
		}
	}
	UNLOCK_REPLIES(t);
	if (local_winner>=0 && winning_code>=200 ) {
		DBG("DEBUG: local transaction completed\n");
		if (!totag_retr) {
			if ( has_tran_tmcbs(t,TMCB_LOCAL_COMPLETED) )
				run_trans_callbacks( TMCB_LOCAL_COMPLETED, t, 0,
					winning_msg, winning_code );
		}
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
int reply_received( struct sip_msg  *p_msg )
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
	if ( (t==0)||(t==T_UNDEFINED)) return 1;

	cancel_bitmap=0;
	msg_status=p_msg->REPLY_STATUS;

	uac=&t->uac[branch];
	DBG("DEBUG: t_on_reply: org. status uas=%d, "
		"uac[%d]=%d local=%d is_invite=%d)\n",
		t->uas.status, branch, uac->last_received, 
		is_local(t), is_invite(t));

	/* it's a cancel ... ? */
	if (get_cseq(p_msg)->method.len==CANCEL_LEN 
		&& memcmp( get_cseq(p_msg)->method.s, CANCEL, CANCEL_LEN)==0
		/* .. which is not e2e ? ... */
		&& is_invite(t) ) {
			/* ... then just stop timers */
			reset_timer( &uac->local_cancel.retr_timer);
			if ( msg_status >= 200 )
				reset_timer( &uac->local_cancel.fr_timer);
			DBG("DEBUG: reply to local CANCEL processed\n");
			goto done;
	}


	/* *** stop timers *** */
	/* stop retransmission */
	reset_timer( &uac->request.retr_timer);
	/* stop final response timer only if I got a final response */
	if ( msg_status >= 200 )
		reset_timer( &uac->request.fr_timer);
		/* acknowledge negative INVITE replies (do it before detailed
		 * on_reply processing, which may take very long, like if it
		 * is attempted to establish a TCP connection to a fail-over dst */
	if (is_invite(t) && (msg_status>=300 || (is_local(t) && msg_status>=200))){
		ack = build_ack( p_msg, t, branch, &ack_len);
		if (ack) {
			SEND_PR_BUFFER( &uac->request, ack, ack_len );
			shm_free(ack);
		}
	} /* ack-ing negative INVITE replies */
	/* processing of on_reply block */
	if (t->on_reply) {
		rmode=MODE_ONREPLY;
		/* transfer transaction flag to message context */
		if (t->uas.request) p_msg->flags=t->uas.request->flags;
	 	if (run_actions(onreply_rlist[t->on_reply], p_msg)<0) 
			LOG(L_ERR, "ERROR: on_reply processing failed\n");
		/* destroy any eventual avps */
		if (users_avps)
			destroy_avps();
		/* transfer current message context back to t */
		if (t->uas.request) t->uas.request->flags=p_msg->flags;
	}
	LOCK_REPLIES( t );
	if ( is_local(t) ) {
		reply_status=local_reply( t, p_msg, branch, msg_status, &cancel_bitmap );
	} else {
		reply_status=relay_reply( t, p_msg, branch, msg_status, 
			&cancel_bitmap );
	}

	if (reply_status==RPS_ERROR)
		goto done;

	/* clean-up the transaction when transaction completed */
	if (reply_status==RPS_COMPLETED) {
		/* no more UAC FR/RETR (if I received a 2xx, there may
		   be still pending branches ...
		*/
		cleanup_uac_timers( t );	
		if (is_invite(t)) cancel_uacs( t, cancel_bitmap );
		/* FR for negative INVITES, WAIT anything else */
		set_final_timer(  t );
	} 

	/* update FR/RETR timers on provisional replies */
	if (msg_status<200) { /* provisional now */
		if (is_invite(t)) {
			/* invite: change FR to longer FR_INV, do not
			   attempt to restart retransmission any more
			*/
			set_timer( & uac->request.fr_timer,
				FR_INV_TIMER_LIST );
		} else {
			/* non-invite: restart retransmisssions (slow now) */
			uac->request.retr_list=RT_T2;
			set_timer(  & uac->request.retr_timer, RT_T2 );
		}
	} /* provisional replies */

done:
	/* don't try to relay statelessly neither on success
       (we forwarded statefuly) nor on error; on troubles, 
	   simply do nothing; that will make the other party to 
	   retransmit; hopefuly, we'll then be better off */
	return 0;
}



int t_reply_with_body( struct cell *trans, unsigned int code, 
		char * text, char * body, char * new_header, char * to_tag )
{
	struct lump_rpl *hdr_lump;
	struct lump_rpl *body_lump;
	str  s_to_tag;
	str  rpl;
	int  ret;
	struct bookmark bm;

	s_to_tag.s = to_tag;
	if(to_tag)
		s_to_tag.len = strlen(to_tag);

	/* mark the transaction as replied */
	if (code>=200) set_kr(REQ_RPLD);

	/* add the lumps for new_header and for body (by bogdan) */
	if (new_header && strlen(new_header)) {
		hdr_lump = add_lump_rpl( trans->uas.request, new_header,
					 strlen(new_header), LUMP_RPL_HDR );
		if ( !hdr_lump ) {
			LOG(L_ERR,"ERROR:tm:t_reply_with_body: cannot add hdr lump\n");
			goto error;
		}
	} else {
		hdr_lump = 0;
	}

	/* body lump */
	if(body && strlen(body)) {
		body_lump = add_lump_rpl( trans->uas.request, body, strlen(body),
			LUMP_RPL_BODY );
		if (body_lump==0) {
			LOG(L_ERR,"ERROR:tm:t_reply_with_body: cannot add body lump\n");
			goto error_1;
		}
	} else {
		body_lump = 0;
	}

	rpl.s = build_res_buf_from_sip_req(
			code, text, &s_to_tag,
			trans->uas.request, (unsigned int*)&rpl.len, &bm);

	/* since the msg (trans->uas.request) is a clone into shm memory, to avoid
	 * memory leak or crashing (lumps are create in private memory) I will
	 * remove the lumps by myself here (bogdan) */
	if ( hdr_lump ) {
		unlink_lump_rpl( trans->uas.request, hdr_lump);
		free_lump_rpl( hdr_lump );
	}
	if( body_lump ) {
		unlink_lump_rpl( trans->uas.request, body_lump);
		free_lump_rpl( body_lump );
	}

	if (rpl.s==0) {
		LOG(L_ERR,"ERROR:tm:t_reply_with_body: failed in doing "
			"build_res_buf_from_sip_req()\n");
		goto error;
	}

	DBG("t_reply_with_body: buffer computed\n");
	// frees 'res.s' ... no panic !
	ret=_reply_light( trans, rpl.s, rpl.len, code, text, 
		s_to_tag.s, s_to_tag.len, 1 /* lock replies */, &bm );
	/* this is ugly hack -- the function caller may wish to continue with
	 * transction and I unref; however, there is now only one use from
	 * vm/fifo_vm_reply and I'm currently to lazy to export UNREF; -jiri
	 */
	UNREF(trans);

	return ret;
error_1:
	if ( hdr_lump ) {
		unlink_lump_rpl( trans->uas.request, hdr_lump);
		free_lump_rpl( hdr_lump );
	}
error:
	return -1;
}



/*
  Syntax:

  ":vm_reply:[response file]\n
  code\n
  reason\n
  trans_id\n
  to_tag\n
  [new headers]\n
  \n
  [Body]\n
  .\n
  \n"
 */
int fifo_t_reply( FILE *stream, char *response_file )
{
	int ret;
	struct cell *trans;
	char code[16];
	char reason[128];
	char trans_id[128];
	char new_headers[MAX_HEADER];
	char body[MAX_BODY];
	char to_tag[128];
	str sc;       /*  code */
	str sr;       /*  reason */
	str sti;      /*  trans_id */
	str snh;      /*  new_headers */
	str sb;       /*  body */
	str sttag;    /*  to-tag */
	unsigned int hash_index,label,icode;

	sc.s=code;
	sr.s=reason;
	sti.s=trans_id;
	snh.s=new_headers; sb.s=body;
	sttag.s=to_tag; sttag.len=0;


	/*  get the infos from FIFO server */

	DBG("DEBUG: fifo_t_reply: ############### begin ##############\n");

	if (!read_line(sc.s, 16, stream, &sc.len)||sc.len==0) {
		LOG(L_ERR, "ERROR: fifo_t_reply: code expected\n");
		fifo_reply(response_file, "400 fifo_t_reply: code expected");
		return -1;
	}

	icode = str2s(sc.s,sc.len,&ret);
	if(ret){
		LOG(L_ERR, "ERROR: fifo_t_reply: code(int) has wrong format\n");
		fifo_reply(response_file, "400 fifo_t_reply: code(int) has"
			" wrong format");
		return -1;
	}

	if(!read_line(sr.s, 128, stream, &sr.len)||sr.len==0){
		LOG(L_ERR, "ERROR: fifo_t_reply: reason expected\n");
		fifo_reply(response_file, "400 fifo_t_reply: reason expected");
		return -1;
	}
	sr.s[sr.len]='\0';

	if (!read_line(sti.s, 128, stream, &sti.len)||sti.len==0) {
		LOG(L_ERR, "ERROR: fifo_t_reply: trans_id expected\n");
		fifo_reply(response_file, "400 fifo_t_reply: trans_id expected");
		return -1;
	}
	sti.s[sti.len]='\0';
	DBG("DEBUG: fifo_t_reply: trans_id=%.*s\n",sti.len,sti.s);

	if(sscanf(sti.s,"%u:%u", &hash_index, &label) != 2){
		LOG(L_ERR, "ERROR: fifo_t_reply: invalid trans_id (%s)\n",sti.s);
		fifo_reply(response_file, "400 fifo_t_reply: invalid trans_id");
		return -1;
	}
	DBG("DEBUG: fifo_t_reply: hash_index=%u label=%u\n",hash_index,label);

	if( !read_line(sttag.s,64,stream,&sttag.len) || sttag.len==0 ){
		LOG(L_ERR, "ERROR: fifo_t_reply: to-tag expected\n");
		fifo_reply(response_file, "400 fifo_t_reply: to-ta expected");
		return -1;
	}
	sttag.s[sttag.len]='\0';
	DBG("DEBUG: fifo_t_reply: to-tag: %.*s\n",sttag.len,sttag.s);

	/* read the new headers */
	if (!read_line_set(snh.s, MAX_HEADER, stream, &snh.len)) {
		LOG(L_ERR, "ERROR: fifo_t_reply: while reading new headers\n");
		fifo_reply(response_file, "400 fifo_t_reply: while reading "
			"new headers");
		return -1;
	}
	snh.s[snh.len]='\0';
	DBG("DEBUG: fifo_t_reply: new headers: %.*s\n", snh.len, snh.s);

	/*  body can be empty ... */
	read_body(sb.s, MAX_BODY, stream, &sb.len);
	sb.s[sb.len]='\0';
	DBG("DEBUG: fifo_t_reply: body: <%.*s>\n", sb.len, sb.s);

	if( t_lookup_ident(&trans,hash_index,label)<0 ) {
		LOG(L_ERR,"ERROR: fifo_t_reply: lookup failed\n");
		fifo_reply(response_file, "481 fifo_t_reply: no such transaction");
		return -1;
	}

	/* it's refcounted now, t_reply_with body unrefs for me -- I can 
	 * continue but may not use T anymore  */
	ret = t_reply_with_body(trans,icode,reason,body,new_headers,to_tag);

	if (ret<0) {
		LOG(L_ERR, "ERROR: fifo_t_reply: reply failed\n");
		fifo_reply(response_file, "500 fifo_t_reply: reply failed");
		return -1;
	}

	fifo_reply(response_file, "200 fifo_t_reply succeeded\n");
	DBG("DEBUG: fifo_t_reply: ################ end ##############\n");
	return 1;
}


static int parse_transid(str* s, unsigned int* index, unsigned int* label)
{
	char* buf;

	if (!s || !index || !label) {
		LOG(L_ERR, "parse_transid: Invalid parameter value\n");
		return -1;
	}

	buf = (char*)pkg_malloc(s->len + 1);
	if (!buf) {
		LOG(L_ERR, "parse_transid: No memory left\n");
		return -1;
	}

	memcpy(buf, s->s, s->len + 1);
	buf[s->len] = '\0';
	
	if (sscanf(buf, "%u:%u", index, label) != 2) {
		LOG(L_ERR, "parse_transid: Invalid trans_id (%s)\n", buf);
		pkg_free(buf);
		return -1;
	}

	DBG("parse_transid: hash_index=%u label=%u\n", *index, *label);
	pkg_free(buf);
	return 0;
}



static int send_reply(struct cell *trans, unsigned int code, str* text, str* body, str* headers, str* to_tag)
{
	struct lump_rpl *hdr_lump, *body_lump;
	str rpl;
	int ret;
	struct bookmark bm;

	     /* mark the transaction as replied */
	if (code >= 200) set_kr(REQ_RPLD);

	     /* add the lumps for new_header and for body (by bogdan) */
	if (headers && headers->len) {
		hdr_lump = add_lump_rpl(trans->uas.request, headers->s, headers->len, LUMP_RPL_HDR);
		if (!hdr_lump) {
			LOG(L_ERR, "send_reply: cannot add hdr lump\n");
			goto sr_error;
		}
	} else {
		hdr_lump = 0;
	}

	     /* body lump */
	if (body && body->len) {
		body_lump = add_lump_rpl(trans->uas.request, body->s, body->len, LUMP_RPL_BODY);
		if (body_lump == 0) {
			LOG(L_ERR,"send_reply: cannot add body lump\n");
			goto sr_error_1;
		}
	} else {
		body_lump = 0;
	}

	     /* We can safely zero-terminate the text here, because it is followed
	      * by next line in the received message
	      */
	text->s[text->len] = '\0';
	rpl.s = build_res_buf_from_sip_req(code, text->s, to_tag, trans->uas.request, (unsigned int*)&rpl.len, &bm);

	     /* since the msg (trans->uas.request) is a clone into shm memory, to avoid
	      * memory leak or crashing (lumps are create in private memory) I will
	      * remove the lumps by myself here (bogdan) */
	if (hdr_lump) {
		unlink_lump_rpl(trans->uas.request, hdr_lump);
		free_lump_rpl(hdr_lump);
	}
	if (body_lump) {
		unlink_lump_rpl(trans->uas.request, body_lump);
		free_lump_rpl(body_lump);
	}

	if (rpl.s == 0) {
		LOG(L_ERR,"send_reply: failed in build_res_buf_from_sip_req\n");
		goto sr_error;
	}

	ret = _reply_light(trans, rpl.s, rpl.len, code, text->s,  to_tag->s, to_tag->len, 1 /* lock replies */, &bm);
	     /* this is ugly hack -- the function caller may wish to continue with
	      * transction and I unref; however, there is now only one use from
	      * vm/fifo_vm_reply and I'm currently to lazy to export UNREF; -jiri
	      */
	UNREF(trans);
	return ret;
 sr_error_1:
	if (hdr_lump) {
		unlink_lump_rpl(trans->uas.request, hdr_lump);
		free_lump_rpl(hdr_lump);
	}
 sr_error:
	return -1;
}



int unixsock_t_reply(str* msg)
{
	int ret;
	struct cell *trans;
	static char new_headers[MAX_HEADER];
	str code, reason, transid, headers, body, to_tag;
	unsigned int hash_index, label, icode;

	headers.s = new_headers;
	headers.len = MAX_HEADER;

	if (unixsock_read_line(&code, msg) != 0) {
		unixsock_reply_asciiz("400 Reason code expected\n");
		goto err;
	}

	icode = str2s(code.s, code.len, &ret);
	if (ret) {
		unixsock_reply_printf("400 Reason code has wrong format\n");
		goto err;
	}

	if (unixsock_read_line(&reason, msg) != 0) {
		unixsock_reply_asciiz("400 Reason phrase expected\n");
		goto err;
	}

	if (unixsock_read_line(&transid, msg) != 0) {
		unixsock_reply_asciiz("400 Transaction ID expected\n");
		goto err;
	}

	if (parse_transid(&transid, &hash_index, &label) < 0) {
		unixsock_reply_asciiz("400 Error while parsing transaction ID\n");
		goto err;
	}

	if (unixsock_read_line(&to_tag, msg) != 0) {
		unixsock_reply_asciiz("400 To tag expected\n");
		goto err;
	}

	     /* read the new headers */
	if (unixsock_read_lineset(&headers, msg) < 0) {
		unixsock_reply_asciiz("400 Error while reading new headers\n");
		goto err;
	}

	DBG("lineset: %.*s\n", headers.len, headers.s);
	     
	/*  body can be empty ... */
	if (unixsock_read_body(&body, msg) < 0) {
		unixsock_reply_asciiz("400 Error while reading body\n");
		goto err;
	}

	DBG("body: %.*s\n", body.len, body.s);
	
	if (t_lookup_ident(&trans, hash_index, label) < 0) {
		LOG(L_ERR,"unixsock_t_reply: lookup failed\n");
		unixsock_reply_asciiz("481 No such transaction\n");
		goto err;
	}

	     /* it's refcounted now, t_reply_with body unrefs for me -- I can 
	      * continue but may not use T anymore  
	      */
	ret = send_reply(trans, icode, &reason, &body, &headers, &to_tag);
	if (ret < 0) {
		LOG(L_ERR, "unixsock_t_reply: reply failed\n");
		unixsock_reply_asciiz("500 Reply failed\n");
		goto err;
	}

	unixsock_reply_asciiz("200 Succeeded\n");
	unixsock_reply_send();
	return 1;

 err:
	unixsock_reply_send();
	return -1;
}
