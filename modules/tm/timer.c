/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */


/* 
  timer.c is where we implement TM timers. It has been designed
  for high performance using some techniques of which timer users
  need to be aware.

	One technique is "fixed-timer-length". We maintain separate 
	timer lists, all of them include elements of the same time
	to fire. That allows *appending* new events to the list as
	opposed to inserting them by time, which is costly due to
	searching time spent in a mutex. The performance benefit is
	noticeable. The limitation is you need a new timer list for
	each new timer length.

	Another technique is the timer process slices off expired elements
	from the list in a mutex, but executes the timer after the mutex
	is left. That saves time greatly as whichever process wants to
	add/remove a timer, it does not have to wait until the current
	list is processed. However, be aware the timers may hit in a delayed
	manner; you have no guarantee in your process that after resetting a timer, 
	it will no more hit. It might have been removed by timer process,
    and is waiting to be executed.  The following example shows it:

			PROCESS1				TIMER PROCESS

	0.								timer hits, it is removed from queue and
									about to be executed
	1.	process1 decides to
		reset the timer 
	2.								timer is executed now
	3.	if the process1 naively
		thinks the timer could not 
		have been executed after 
		resetting the timer, it is
		WRONG -- it was (step 2.)

	So be careful when writing the timer handlers. Currently defined timers 
	don't hurt if they hit delayed, I hope at least. Retransmission timer 
	may results in a useless retransmission -- not too bad. FR timer not too
	bad either as timer processing uses a REPLY mutex making it safe to other
	processing affecting transaction state. Wait timer not bad either -- processes
	putting a transaction on wait don't do anything with it anymore.

		Example when it does not hurt:

			P1						TIMER
	0.								RETR timer removed from list and
									scheduled for execution
	1. 200/BYE received->
	   reset RETR, put_on_wait
	2.								RETR timer executed -- too late but it does
									not hurt
	3.								WAIT handler executed

	The rule of thumb is don't touch data you put under a timer. Create data,
    put them under a timer, and let them live until they are safely destroyed from
    wait/delete timer.  The only safe place to manipulate the data is 
    from timer process in which delayed timers cannot hit (all timers are
    processed sequentially).

	A "bad example" -- rewriting content of retransmission buffer
	in an unprotected way is bad because a delayed retransmission timer might 
	hit. Thats why our reply retransmission procedure is enclosed in 
	a REPLY_LOCK.

*/
/*
 * History:
 * --------
 *  2003-06-27  timers are not unlinked if timerlist is 0 (andrei)
 *  2004-02-13  t->is_invite, t->local, t->noisy_ctimer replaced;
 *              timer_link.payload removed (bogdan)
 *  2005-10-03  almost completely rewritten to use the new timers (andrei)
 *  2005-12-12  on final response marked the rb as removed to avoid deleting
 *              it from the timer handle; timer_allow_del()  (andrei)
 *  2006-08-11  final_response_handler dns failover support for timeout-ed
 *              invites (andrei)
 *  2006-09-28  removed the 480 on fr_inv_timeout reply: on timeout always 
 *               return a 408
 *              set the corresponding "faked" failure route sip_msg->msg_flags 
 *               on timeout or if the branch received a reply (andrei)
 */

#include "defs.h"



#include "config.h"
#include "h_table.h"
#include "timer.h"
#include "../../dprint.h"
#include "lock.h"
#include "t_stats.h"

#include "../../hash_func.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "../../timer_ticks.h"
#include "t_funcs.h"
#include "t_reply.h"
#include "t_cancel.h"
#ifdef USE_DNS_FAILOVER
#include "t_fwd.h" /* t_send_branch */
#endif
#ifdef USE_DST_BLACKLIST
#include "../../dst_blacklist.h"
#endif



int noisy_ctimer=0;

struct msgid_var user_fr_timeout;
struct msgid_var user_fr_inv_timeout;

/* default values of timeouts for all the timer list */

ticks_t fr_timeout		=	FR_TIME_OUT;
ticks_t fr_inv_timeout	=	INV_FR_TIME_OUT;
ticks_t wait_timeout	=	WT_TIME_OUT;
ticks_t delete_timeout	=	DEL_TIME_OUT;
ticks_t rt_t1_timeout	=	RETR_T1;
ticks_t rt_t2_timeout	=	RETR_T2;

/* fix timer values to ticks */
int tm_init_timers()
{
	fr_timeout=MS_TO_TICKS(fr_timeout); 
	fr_inv_timeout=MS_TO_TICKS(fr_inv_timeout);
	wait_timeout=MS_TO_TICKS(wait_timeout);
	delete_timeout=MS_TO_TICKS(delete_timeout);
	rt_t1_timeout=MS_TO_TICKS(rt_t1_timeout);
	rt_t2_timeout=MS_TO_TICKS(rt_t2_timeout);
	/* fix 0 values to 1 tick (minimum possible wait time ) */
	if (fr_timeout==0) fr_timeout=1;
	if (fr_inv_timeout==0) fr_inv_timeout=1;
	if (wait_timeout==0) wait_timeout=1;
	if (delete_timeout==0) delete_timeout=1;
	if (rt_t2_timeout==0) rt_t2_timeout=1;
	if (rt_t1_timeout==0) rt_t1_timeout=1;
	
	memset(&user_fr_timeout, 0, sizeof(user_fr_timeout));
	memset(&user_fr_inv_timeout, 0, sizeof(user_fr_inv_timeout));
	
	DBG("tm: tm_init_timers: fr=%d fr_inv=%d wait=%d delete=%d t1=%d t2=%d\n",
			fr_timeout, fr_inv_timeout, wait_timeout, delete_timeout,
			rt_t1_timeout, rt_t2_timeout);
	return 0;
}

/******************** handlers ***************************/



inline static void cleanup_localcancel_timers( struct cell *t )
{
	int i;
	for (i=0; i<t->nr_of_outgoings; i++ )
		stop_rb_timers(&t->uac[i].local_cancel);
}



inline static void unlink_timers( struct cell *t )
{
	int i;

	stop_rb_timers(&t->uas.response);
	for (i=0; i<t->nr_of_outgoings; i++)
		stop_rb_timers(&t->uac[i].request);
	cleanup_localcancel_timers(t);
}



/* returns number of ticks before retrying the del, or 0 if the del.
 * was succesfull */
inline static ticks_t  delete_cell( struct cell *p_cell, int unlock )
{
	/* there may still be FR/RETR timers, which have been reset
	   (i.e., time_out==TIMER_DELETED) but are stilled linked to
	   timer lists and must be removed from there before the
	   structures are released
	*/
	unlink_timers( p_cell );
	/* still in use ... don't delete */
	if ( IS_REFFED_UNSAFE(p_cell) ) {
		if (unlock) UNLOCK_HASH(p_cell->hash_index);
		DBG("DEBUG: delete_cell %p: can't delete -- still reffed (%d)\n",
				p_cell, p_cell->ref_count);
		/* delay the delete */
		/* TODO: change refcnts and delete on refcnt==0 */
		return delete_timeout;
	} else {
		if (unlock) UNLOCK_HASH(p_cell->hash_index);
#ifdef EXTRA_DEBUG
		DBG("DEBUG: delete transaction %p\n", p_cell );
#endif
		free_cell( p_cell );
		return 0;
	}
}




/* generate a fake reply
 * it assumes the REPLY_LOCK is already held and returns unlocked */
static void fake_reply(struct cell *t, int branch, int code )
{
	branch_bm_t cancel_bitmap;
	short do_cancel_branch;
	enum rps reply_status;

	do_cancel_branch = is_invite(t) && should_cancel_branch(t, branch);
	if ( is_local(t) ) {
		reply_status=local_reply( t, FAKED_REPLY, branch, 
					  code, &cancel_bitmap );
		if (reply_status == RPS_COMPLETED) {
			put_on_wait(t);
		}
	} else {
		reply_status=relay_reply( t, FAKED_REPLY, branch, code,
					  &cancel_bitmap );

#if 0
		if (reply_status==RPS_COMPLETED) {
			     /* don't need to cleanup uac_timers -- they were cleaned
				branch by branch and this last branch's timers are
				reset now too
			     */
			     /* don't need to issue cancels -- local cancels have been
				issued branch by branch and this last branch was
				canceled now too
			     */
			     /* then the only thing to do now is to put the transaction
				on FR/wait state 
			     */
			     /*
			       set_final_timer(  t );
			     */
		}
#endif

	}
	/* now when out-of-lock do the cancel I/O */
	if (do_cancel_branch) cancel_branch(t, branch, 0);
	/* it's cleaned up on error; if no error occurred and transaction
	   completed regularly, I have to clean-up myself
	*/
}



/* return (ticks_t)-1 on error/disable and 0 on success */
inline static ticks_t retransmission_handler( struct retr_buf *r_buf )
{
#ifdef EXTRA_DEBUG
	if (r_buf->my_T->flags & T_IN_AGONY) {
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" called from RETR timer (flags %x)\n",
			r_buf->my_T, r_buf->my_T->flags );
		abort();
	}	
#endif
	if ( r_buf->activ_type==TYPE_LOCAL_CANCEL 
		|| r_buf->activ_type==TYPE_REQUEST ) {
#ifdef EXTRA_DEBUG
			DBG("DEBUG: retransmission_handler : "
				"request resending (t=%p, %.9s ... )\n", 
				r_buf->my_T, r_buf->buffer);
#endif
			if (SEND_BUFFER( r_buf )==-1) {
				/* disable retr. timers => return -1 */
				fake_reply(r_buf->my_T, r_buf->branch, 503 );
				return (ticks_t)-1;
			}
	} else {
#ifdef EXTRA_DEBUG
			DBG("DEBUG: retransmission_handler : "
				"reply resending (t=%p, %.9s ... )\n", 
				r_buf->my_T, r_buf->buffer);
#endif
			t_retransmit_reply(r_buf->my_T);
	}
	
	return 0;
}



inline static void final_response_handler(	struct retr_buf* r_buf,
											struct cell* t)
{
	int silent;
#ifdef USE_DNS_FAILOVER
	/*int i; 
	int added_branches;
	*/
	int branch_ret;
	int prev_branch;
#endif

#	ifdef EXTRA_DEBUG
	if (t->flags & T_IN_AGONY) 
	{
		LOG( L_ERR, "ERROR: transaction %p scheduled for deletion and"
			" called from FR timer (flags %x)\n", t, t->flags);
		abort();
	}
#	endif
	/* FR for local cancels.... */
	if (r_buf->activ_type==TYPE_LOCAL_CANCEL)
	{
#ifdef TIMER_DEBUG
		DBG("DEBUG: final_response_handler: stop retr for Local Cancel\n");
#endif
		return;
	}
	/* FR for replies (negative INVITE replies) */
	if (r_buf->activ_type>0) {
#		ifdef EXTRA_DEBUG
		if (t->uas.request->REQ_METHOD!=METHOD_INVITE
			|| t->uas.status < 200 ) {
			LOG(L_CRIT, "BUG: final_response_handler: unknown type reply"
					" buffer\n");
			abort();
		}
#		endif
		put_on_wait( t );
		return;
	};

	/* lock reply processing to determine how to proceed reliably */
	LOCK_REPLIES( t );
	/* now it can be only a request retransmission buffer;
	   try if you can simply discard the local transaction 
	   state without compellingly removing it from the
	   world */
	silent=
		/* not for UACs */
		!is_local(t)
		/* invites only */
		&& is_invite(t)
		/* parallel forking does not allow silent state discarding */
		&& t->nr_of_outgoings==1
		/* on_negativ reply handler not installed -- serial forking 
		 * could occur otherwise */
		&& t->on_negative==0
		/* the same for FAILURE callbacks */
		&& !has_tran_tmcbs( t, TMCB_ON_FAILURE_RO|TMCB_ON_FAILURE) 
		/* something received -- we will not be silent on error */
		&& t->uac[r_buf->branch].last_received>0
		/* don't go silent if disallowed globally ... */
		&& noisy_ctimer==0
		/* ... or for this particular transaction */
		&& has_noisy_ctimer(t) == 0;
	if (silent) {
		UNLOCK_REPLIES(t);
#ifdef EXTRA_DEBUG
		DBG("DEBUG: final_response_handler: transaction silently dropped (%p)\n",t);
#endif
		put_on_wait( t );
		return;
	}
#ifdef EXTRA_DEBUG
	DBG("DEBUG: final_response_handler:stop retr. and send CANCEL (%p)\n", t);
#endif
	if ((r_buf->branch < MAX_BRANCHES) && /* r_buf->branch is always >=0 */
			(t->uac[r_buf->branch].last_received==0)){
		/* no reply received */
#ifdef USE_DST_BLACKLIST
		if (use_dst_blacklist)
			dst_blacklist_add( BLST_ERR_TIMEOUT, &r_buf->dst);
#endif
#ifdef USE_DNS_FAILOVER
		/* if this is an invite, the destination resolves to more ips, and
		 *  it still hasn't passed more than fr_inv_timeout since we
		 *  started, add another branch/uac */
		if (is_invite(t) && use_dns_failover &&
				((get_ticks_raw()-(r_buf->fr_expire-t->fr_timeout)) <
				 	t->fr_inv_timeout)){
			branch_ret=add_uac_dns_fallback(t, t->uas.request,
												&t->uac[r_buf->branch], 0);
			prev_branch=-1;
			while((branch_ret>=0) &&(branch_ret!=prev_branch)){
				prev_branch=branch_ret;
				branch_ret=t_send_branch(t, branch_ret, t->uas.request , 0, 0);
			}
		}
#endif
	}
	fake_reply(t, r_buf->branch, 408);
}



/* handles retransmissions and fr timers */
/* the following assumption are made (to avoid deleting/re-adding the timer):
 *  retr_buf->retr_interval < ( 1<<((sizeof(ticks_t)*8-1) )
 *  if retr_buf->retr_interval==0 => timer disabled
 *                            ==(ticks_t) -1 => retr. disabled (fr working)
 *     retr_buf->retr_interval & (1 <<(sizeof(ticks_t)*8-1) => retr. & fr reset
 *     (we never reset only retr, it's either reset both of them or retr 
 *      disabled & reset fr). In this case the fr_origin will contain the 
 *      "time" of the reset and next retr should occur at 
 *      fr->origin+retr_interval (we also assume that we'll never reset retr
 *      to a lower value then the current one)
 */
ticks_t retr_buf_handler(ticks_t ticks, struct timer_ln* tl, void *p)
{
	struct retr_buf* rbuf ;
	ticks_t fr_remainder;
	ticks_t retr_remainder;
	ticks_t retr_interval;
	struct cell *t;

	rbuf=(struct  retr_buf*)
			((void*)tl-(void*)(&((struct retr_buf*)0)->timer));
	t=rbuf->my_T;
	
#ifdef TIMER_DEBUG
	DBG("tm: timer retr_buf_handler @%d (%p -> %p -> %p)\n",
			ticks, tl, rbuf, t);
#endif
	/* overflow safe check (should work ok for fr_intervals < max ticks_t/2) */
	if ((s_ticks_t)(rbuf->fr_expire-ticks)<=0){
		/* final response */
		rbuf->t_active=0; /* mark the timer as removed 
							 (both timers disabled)
							  a little race risk, but
							  nothing bad would happen */
		rbuf->flags|=F_RB_TIMEOUT;
		timer_allow_del(); /* [optional] allow timer_dels, since we're done
							  and there is no race risk */
		final_response_handler(rbuf, t);
		return 0;
	}else{
		/*  4 possible states running (t1), t2, paused, disabled */
			if ((s_ticks_t)(rbuf->retr_expire-ticks)<=0){
				if (rbuf->flags & F_RB_RETR_DISABLED)
					goto disabled;
				/* retr_interval= min (2*ri, rt_t2) */
				/* no branch version: 
					#idef CC_SIGNED_RIGHT_SHIFT
						ri=  rt_t2+((2*ri-rt_t2) & 
						((signed)(2*ri-rt_t2)>>(sizeof(ticks_t)*8-1));
					#else
						ri=rt_t2+((2*ri-rt_t2)& -(2*ri<rt_t2));
					#endif
				*/
				
				/* get the  current interval from timer param. */
				if ((rbuf->flags & F_RB_T2) || 
						(((ticks_t)(unsigned long)p<<1)>rt_t2_timeout))
					retr_interval=rt_t2_timeout;
				else
					retr_interval=(ticks_t)(unsigned long)p<<1;
#ifdef TIMER_DEBUG
				DBG("tm: timer: retr: new interval %d (max %d)\n", 
						retr_interval, rt_t2_timeout);
#endif
				/* we could race with the reply_received code, but the 
				 * worst thing that can happen is to delay a reset_to_t2
				 * for crt_interval and send an extra retr.*/
				rbuf->retr_expire=ticks+retr_interval;
				/* set new interval to -1 on error, or retr_int. on success */
				retr_remainder=retransmission_handler(rbuf) | retr_interval;
				retr_remainder=retr_interval;
				/* store the crt. retr. interval inside the timer struct,
				 * in the data member */
				tl->data=(void*)(unsigned long)retr_interval;
			}else{
				retr_remainder= rbuf->retr_expire-ticks;
				DBG("tm: timer: retr: nothing to do, expire in %d\n", 
						retr_remainder);
			}
	}
/* skip: */
	/* return minimum of the next retransmission handler and the 
	 * final response (side benefit: it properly cancels timer if ret==0 and
	 *  sleeps for fr_remainder if retr. is canceled [==(ticks_t)-1]) */
	fr_remainder=rbuf->fr_expire-ticks; /* to be more precise use
											get_ticks_raw() instead of ticks
											(but make sure that 
											crt. ticks < fr_expire */
#ifdef TIMER_DEBUG
	DBG("tm: timer retr_buf_handler @%d (%p ->%p->%p) exiting min (%d, %d)\n",
			ticks, tl, rbuf, t, retr_remainder, fr_remainder);
#endif
	if (retr_remainder<fr_remainder)
		return retr_remainder;
	else
		return fr_remainder;
disabled:
	return rbuf->fr_expire-ticks;
}



ticks_t wait_handler(ticks_t ti, struct timer_ln *wait_tl, void* data)
{
	struct cell *p_cell;
	ticks_t ret;

	p_cell=(struct cell*)data;
#ifdef TIMER_DEBUG
	DBG("DEBUG: WAIT timer hit @%d for %p (timer_lm %p)\n", 
			ti, p_cell, wait_tl);
#endif

	if (p_cell->flags & T_IN_AGONY){
		/* delayed delete */
		/* we call delete now without any locking on hash/ref_count;
		   we can do that because delete_handler is only entered after
		   the delete timer was installed from wait_handler, which
		   removed transaction from hash table and did not destroy it
		   because some processes were using it; that means that the
		   processes currently using the transaction can unref and no
		   new processes can ref -- we can wait until ref_count is
		   zero safely without locking
		*/
		ret=delete_cell( p_cell, 0 /* don't unlock on return */ );
	}else{
		/* stop cancel timers if any running */
		if ( is_invite(p_cell) ) cleanup_localcancel_timers( p_cell );
		/* remove the cell from the hash table */
		LOCK_HASH( p_cell->hash_index );
		remove_from_hash_table_unsafe(  p_cell );
		p_cell->flags |= T_IN_AGONY;
		/* delete (returns with UNLOCK-ed_HASH) */
		ret=delete_cell( p_cell, 1 /* unlock on return */ );
	}
	return ret;
}

