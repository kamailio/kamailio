/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * \file
 * \brief TM :: timer support
 * 
 * TM timer support. It has been designed for high performance using
 * some techniques of which timer users need to be aware.
 * - One technique is "fixed-timer-length". We maintain separate 
 * timer lists, all of them include elements of the same time
 * to fire. That allows *appending* new events to the list as
 * opposed to inserting them by time, which is costly due to
 * searching time spent in a mutex. The performance benefit is
 * noticeable. The limitation is you need a new timer list for
 * each new timer length.
 * - Another technique is the timer process slices off expired elements
 * from the list in a mutex, but executes the timer after the mutex
 * is left. That saves time greatly as whichever process wants to
 * add/remove a timer, it does not have to wait until the current
 * list is processed. However, be aware the timers may hit in a delayed
 * manner; you have no guarantee in your process that after resetting a timer, 
 * it will no more hit. It might have been removed by timer process,
 * and is waiting to be executed.
 * 
 * The following example shows it:
 * 
 *		PROCESS1				TIMER PROCESS
 * 
 * -	0.						timer hits, it is removed from queue and
 * 							about to be executed
 * -	1.	process1 decides to
 * 		reset the timer 
 * -	2.						timer is executed now
 * -	3.	if the process1 naively
 * 		thinks the timer could not 
 * 		have been executed after 
 * 		resetting the timer, it is
 * 		WRONG -- it was (step 2.)
 * 
 * So be careful when writing the timer handlers. Currently defined timers 
 * don't hurt if they hit delayed, I hope at least. Retransmission timer 
 * may results in a useless retransmission -- not too bad. FR timer not too
 * bad either as timer processing uses a REPLY mutex making it safe to other
 * processing affecting transaction state. Wait timer not bad either -- processes
 * putting a transaction on wait don't do anything with it anymore.
 * 
 * 	Example when it does not hurt:
 * 
 * 		PROCESS1				TIMER PROCESS
 * 
 * -	0.						RETR timer removed from list and
 * 							scheduled for execution
 * -	1. 200/BYE received->
 * 	   reset RETR, put_on_wait
 * -	2.						RETR timer executed -- too late but it does
 * 							not hurt
 * -	3.						WAIT handler executed
 *
 * The rule of thumb is don't touch data you put under a timer. Create data,
 * put them under a timer, and let them live until they are safely destroyed from
 * wait/delete timer.  The only safe place to manipulate the data is 
 * from timer process in which delayed timers cannot hit (all timers are
 * processed sequentially).
 * 
 * A "bad example" -- rewriting content of retransmission buffer
 * in an unprotected way is bad because a delayed retransmission timer might 
 * hit. Thats why our reply retransmission procedure is enclosed in 
 * a REPLY_LOCK.
 * \ingroup tm
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
#include "../../compiler_opt.h" 
#include "../../sr_compat.h" 
#include "t_funcs.h"
#include "t_reply.h"
#include "t_cancel.h"
#include "t_hooks.h"
#ifdef USE_DNS_FAILOVER
#include "t_fwd.h" /* t_send_branch */
#include "../../cfg_core.h" /* cfg_get(core, core_cfg, use_dns_failover) */
#endif
#ifdef USE_DST_BLACKLIST
#include "../../dst_blacklist.h"
#endif



struct msgid_var user_fr_timeout;
struct msgid_var user_fr_inv_timeout;
#ifdef TM_DIFF_RT_TIMEOUT
struct msgid_var user_rt_t1_timeout_ms;
struct msgid_var user_rt_t2_timeout_ms;
#endif
struct msgid_var user_inv_max_lifetime;
struct msgid_var user_noninv_max_lifetime;


/**
 * \brief Check helper for configuration framework values
 * 
 * Check helper for configuration framework values for internal use
 * The val should be unsigned or positive, use
 * <= instead of < to get read of gcc warning when 
 * sizeof(cell_member)==sizeof(val) (Note that this limits
 * maximum value to max. type -1)
 */
#define SIZE_FIT_CHECK(cell_member, val, cfg_name) \
	if (MAX_UVAR_VALUE(((struct cell*)0)->cell_member) <= (val)){ \
		ERR("tm_init_timers: " cfg_name " too big: %lu (%lu ticks) " \
				"- max %lu (%lu ticks) \n", TICKS_TO_MS((unsigned long)(val)),\
				(unsigned long)(val), \
				TICKS_TO_MS(MAX_UVAR_VALUE(((struct cell*)0)->cell_member)), \
				MAX_UVAR_VALUE(((struct cell*)0)->cell_member)); \
		goto error; \
	} 

/**
 * \brief fix timer values to ticks
 */
int tm_init_timers(void)
{
	default_tm_cfg.fr_timeout=MS_TO_TICKS(default_tm_cfg.fr_timeout); 
	default_tm_cfg.fr_inv_timeout=MS_TO_TICKS(default_tm_cfg.fr_inv_timeout);
	default_tm_cfg.wait_timeout=MS_TO_TICKS(default_tm_cfg.wait_timeout);
	default_tm_cfg.delete_timeout=MS_TO_TICKS(default_tm_cfg.delete_timeout);
	default_tm_cfg.tm_max_inv_lifetime=MS_TO_TICKS(default_tm_cfg.tm_max_inv_lifetime);
	default_tm_cfg.tm_max_noninv_lifetime=MS_TO_TICKS(default_tm_cfg.tm_max_noninv_lifetime);
	/* fix 0 values to 1 tick (minimum possible wait time ) */
	if (default_tm_cfg.fr_timeout==0) default_tm_cfg.fr_timeout=1;
	if (default_tm_cfg.fr_inv_timeout==0) default_tm_cfg.fr_inv_timeout=1;
	if (default_tm_cfg.wait_timeout==0) default_tm_cfg.wait_timeout=1;
	if (default_tm_cfg.delete_timeout==0) default_tm_cfg.delete_timeout=1;
	if (default_tm_cfg.rt_t2_timeout_ms==0) default_tm_cfg.rt_t2_timeout_ms=1;
	if (default_tm_cfg.rt_t1_timeout_ms==0) default_tm_cfg.rt_t1_timeout_ms=1;
	if (default_tm_cfg.tm_max_inv_lifetime==0) default_tm_cfg.tm_max_inv_lifetime=1;
	if (default_tm_cfg.tm_max_noninv_lifetime==0) default_tm_cfg.tm_max_noninv_lifetime=1;
	
	/* size fit checks */
	SIZE_FIT_CHECK(fr_timeout, default_tm_cfg.fr_timeout, "fr_timer");
	SIZE_FIT_CHECK(fr_inv_timeout, default_tm_cfg.fr_inv_timeout, "fr_inv_timer");
#ifdef TM_DIFF_RT_TIMEOUT
	SIZE_FIT_CHECK(rt_t1_timeout_ms, default_tm_cfg.rt_t1_timeout_ms,
					"retr_timer1");
	SIZE_FIT_CHECK(rt_t2_timeout_ms, default_tm_cfg.rt_t2_timeout_ms,
					"retr_timer2");
#endif
	SIZE_FIT_CHECK(end_of_life, default_tm_cfg.tm_max_inv_lifetime, "max_inv_lifetime");
	SIZE_FIT_CHECK(end_of_life, default_tm_cfg.tm_max_noninv_lifetime, "max_noninv_lifetime");
	
	memset(&user_fr_timeout, 0, sizeof(user_fr_timeout));
	memset(&user_fr_inv_timeout, 0, sizeof(user_fr_inv_timeout));
#ifdef TM_DIFF_RT_TIMEOUT
	memset(&user_rt_t1_timeout_ms, 0, sizeof(user_rt_t1_timeout_ms));
	memset(&user_rt_t2_timeout_ms, 0, sizeof(user_rt_t2_timeout_ms));
#endif
	memset(&user_inv_max_lifetime, 0, sizeof(user_inv_max_lifetime));
	memset(&user_noninv_max_lifetime, 0, sizeof(user_noninv_max_lifetime));
	
	DBG("tm: tm_init_timers: fr=%d fr_inv=%d wait=%d delete=%d t1=%d t2=%d"
			" max_inv_lifetime=%d max_noninv_lifetime=%d\n",
			default_tm_cfg.fr_timeout, default_tm_cfg.fr_inv_timeout,
			default_tm_cfg.wait_timeout, default_tm_cfg.delete_timeout,
			default_tm_cfg.rt_t1_timeout_ms, default_tm_cfg.rt_t2_timeout_ms,
			default_tm_cfg.tm_max_inv_lifetime, default_tm_cfg.tm_max_noninv_lifetime);
	return 0;
error:
	return -1;
}

/**
 * \brief Internal macro for timer_fixup()
 * 
 * Internal macro for timer_fixup(), performs size fit
 * check if the timer name matches
 */
#define IF_IS_TIMER_NAME(cell_member, cfg_name) \
	if ((name->len == sizeof(cfg_name)-1) && \
		(memcmp(name->s, cfg_name, sizeof(cfg_name)-1)==0)) { \
			SIZE_FIT_CHECK(cell_member, t, cfg_name); \
	}

/**
 * \brief Fixup function for the timer values
 * 
 * Fixup function for the timer values, (called by the
 * configuration framework)
 * \param handle not used
 * \param gname not used
 * \param name not used
 * \param val fixed timer value
 * \return 0 on success, -1 on error
 */
int timer_fixup(void *handle, str *gname, str *name, void **val)
{
	ticks_t t;

	t = MS_TO_TICKS((unsigned int)(long)(*val));
	/* fix 0 values to 1 tick (minimum possible wait time ) */
	if (t == 0) t = 1;

	/* size fix checks */
	IF_IS_TIMER_NAME(fr_timeout, "fr_timer")
	else IF_IS_TIMER_NAME(fr_inv_timeout, "fr_inv_timer")
	else IF_IS_TIMER_NAME(end_of_life, "max_inv_lifetime")
	else IF_IS_TIMER_NAME(end_of_life, "max_noninv_lifetime")

	*val = (void *)(long)t;
	return 0;

error:
	return -1;
}



/** fixup function for timer values that are kept in ms.
 * (called by the configuration framework)
 * It checks if the value fits in the tm structures 
 */
int timer_fixup_ms(void *handle, str *gname, str *name, void **val)
{
	long	t;

	t = (long)(*val);

	/* size fix checks */
#ifdef TM_DIFF_RT_TIMEOUT
	IF_IS_TIMER_NAME(rt_t1_timeout_ms, "retr_timer1")
	else IF_IS_TIMER_NAME(rt_t2_timeout_ms, "retr_timer2")
#endif

	return 0;

error:
	return -1;
}

/******************** handlers ***************************/


#ifndef TM_DEL_UNREF
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
		return cfg_get(tm, tm_cfg, delete_timeout);
	} else {
		if (unlock) UNLOCK_HASH(p_cell->hash_index);
#ifdef EXTRA_DEBUG
		DBG("DEBUG: delete transaction %p\n", p_cell );
#endif
		free_cell( p_cell );
		return 0;
	}
}
#endif /* TM_DEL_UNREF */




/* generate a fake reply
 * it assumes the REPLY_LOCK is already held and returns unlocked */
static void fake_reply(struct cell *t, int branch, int code )
{
	struct cancel_info cancel_data;
	short do_cancel_branch;
	enum rps reply_status;

	init_cancel_info(&cancel_data);
	do_cancel_branch = is_invite(t) && prepare_cancel_branch(t, branch, 0);
	/* mark branch as canceled */
	t->uac[branch].request.flags|=F_RB_CANCELED;
	t->uac[branch].request.flags|=F_RB_RELAYREPLY;
	if ( is_local(t) ) {
		reply_status=local_reply( t, FAKED_REPLY, branch, 
					  code, &cancel_data );
	} else {
		/* rely reply, but don't put on wait, we still need t
		 * to send the cancels */
		reply_status=relay_reply( t, FAKED_REPLY, branch, code,
					  &cancel_data, 0 );
	}
	/* now when out-of-lock do the cancel I/O */
#ifdef CANCEL_REASON_SUPPORT
	if (do_cancel_branch) cancel_branch(t, branch, &cancel_data.reason, 0);
#else /* CANCEL_REASON_SUPPORT */
	if (do_cancel_branch) cancel_branch(t, branch, 0);
#endif /* CANCEL_REASON_SUPPORT */
	/* it's cleaned up on error; if no error occurred and transaction
	   completed regularly, I have to clean-up myself
	*/
	if (reply_status == RPS_COMPLETED)
		put_on_wait(t);
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
			if (unlikely(has_tran_tmcbs(r_buf->my_T, TMCB_REQUEST_SENT))) 
				run_trans_callbacks_with_buf(TMCB_REQUEST_SENT, r_buf, 
				0, 0, TMCB_RETR_F);
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
	ticks_t now;
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
		/* don't go silent if disallowed globally ... */
		cfg_get(tm, tm_cfg, noisy_ctimer)==0
		/* ... or for this particular transaction */
		&& has_noisy_ctimer(t) == 0
		/* not for UACs */
		&& !is_local(t)
		/* invites only */
		&& is_invite(t)
		/* parallel forking does not allow silent state discarding */
		&& t->nr_of_outgoings==1
		/* on_negativ reply handler not installed -- serial forking 
		 * could occur otherwise */
		&& t->on_failure==0
		/* the same for FAILURE callbacks */
		&& !has_tran_tmcbs( t, TMCB_ON_FAILURE_RO|TMCB_ON_FAILURE) 
		/* something received -- we will not be silent on error */
		&& t->uac[r_buf->branch].last_received==0;
	
	if (silent) {
		UNLOCK_REPLIES(t);
#ifdef EXTRA_DEBUG
		DBG("DEBUG: final_response_handler: transaction silently dropped (%p)"
				", branch %d, last_received %d\n",t, r_buf->branch,
				 t->uac[r_buf->branch].last_received);
#endif
		put_on_wait( t );
		return;
	}
#ifdef EXTRA_DEBUG
	DBG("DEBUG: final_response_handler:stop retr. and send CANCEL (%p)\n", t);
#endif
	if ((r_buf->branch < sr_dst_max_branches) && /* r_buf->branch is always >=0 */
			(t->uac[r_buf->branch].last_received==0) &&
			(t->uac[r_buf->branch].request.buffer!=NULL) /* not a blind UAC */
	){
		/* no reply received */
#ifdef USE_DST_BLACKLIST
		if (r_buf->my_T
			&& r_buf->my_T->uas.request
			&& (r_buf->my_T->uas.request->REQ_METHOD &
					cfg_get(tm, tm_cfg, tm_blst_methods_add))
		)
			dst_blacklist_add( BLST_ERR_TIMEOUT, &r_buf->dst,
								r_buf->my_T->uas.request);
#endif
#ifdef USE_DNS_FAILOVER
		/* if this is an invite, the destination resolves to more ips, and
		 *  it still hasn't passed more than fr_inv_timeout since we
		 *  started, add another branch/uac */
		if (cfg_get(core, core_cfg, use_dns_failover)){
			now=get_ticks_raw();
			if ((s_ticks_t)(t->end_of_life-now)>0){
				branch_ret=add_uac_dns_fallback(t, t->uas.request,
													&t->uac[r_buf->branch], 0);
				prev_branch=-1;
				while((branch_ret>=0) &&(branch_ret!=prev_branch)){
					prev_branch=branch_ret;
					branch_ret=t_send_branch(t, branch_ret, t->uas.request , 
												0, 0);
				}
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
	unsigned long new_retr_interval_ms;
	unsigned long crt_retr_interval_ms;
	struct cell *t;

	rbuf=(struct  retr_buf*)
			((void*)tl-(void*)(&((struct retr_buf*)0)->timer));
	membar_depends(); /* to be on the safe side */
	t=rbuf->my_T;
	
#ifdef TIMER_DEBUG
	DBG("tm: timer retr_buf_handler @%d (%p -> %p -> %p)\n",
			ticks, tl, rbuf, t);
#endif
	if (unlikely(rbuf->flags & F_RB_DEL_TIMER)){
		/* timer marked for deletion */
		rbuf->t_active=0; /* mark it as removed */
		/* a membar is not really needed, in the very unlikely case that 
		 * another process will see old t_active's value and will try to 
		 * delete the timer again, but since timer_del it's safe in this cases
		 * it will be a no-op */
		return 0;
	}
	/* overflow safe check (should work ok for fr_intervals < max ticks_t/2) */
	if ((s_ticks_t)(rbuf->fr_expire-ticks)<=0){
		/* final response */
		rbuf->t_active=0; /* mark the timer as removed 
							 (both timers disabled)
							  a little race risk, but
							  nothing bad would happen */
		rbuf->flags|=F_RB_TIMEOUT;
		/* WARNING:  the next line depends on taking care not to start the 
		 *           wait timer before finishing with t (if this is not 
		 *           guaranteed then comment the timer_allow_del() line) */
		timer_allow_del(); /* [optional] allow timer_dels, since we're done
							  and there is no race risk */
		final_response_handler(rbuf, t);
		return 0;
	}else{
		/*  4 possible states running (t1), t2, paused, disabled */
			if ((s_ticks_t)(rbuf->retr_expire-ticks)<=0){
				if (rbuf->flags & F_RB_RETR_DISABLED)
					goto disabled;
				crt_retr_interval_ms = (unsigned long)p;
				/* get the  current interval from timer param. */
				if (unlikely((rbuf->flags & F_RB_T2) ||
						(crt_retr_interval_ms > RT_T2_TIMEOUT_MS(rbuf)))){
					retr_interval = MS_TO_TICKS(RT_T2_TIMEOUT_MS(rbuf));
					new_retr_interval_ms = RT_T2_TIMEOUT_MS(rbuf);
				}else{
					retr_interval = MS_TO_TICKS(crt_retr_interval_ms);
					new_retr_interval_ms=crt_retr_interval_ms<<1;
				}
#ifdef TIMER_DEBUG
				DBG("tm: timer: retr: new interval %ld ms / %d ticks"
						" (max %d ms)\n", new_retr_interval_ms, retr_interval,
						RT_T2_TIMEOUT_MS(rbuf));
#endif
				/* we could race with the reply_received code, but the 
				 * worst thing that can happen is to delay a reset_to_t2
				 * for crt_interval and send an extra retr.*/
				rbuf->retr_expire=ticks+retr_interval;
				/* set new interval to -1 on error, or retr_int. on success */
				retr_remainder=retransmission_handler(rbuf) | retr_interval;
				/* store the next retr. interval in ms inside the timer struct,
				 * in the data member */
				tl->data=(void*)(new_retr_interval_ms);
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
#ifdef EXTRA_DEBUG
	if  (retr_remainder==0 || fr_remainder==0){
		BUG("tm: timer retr_buf_handler: 0 remainder => disabling timer!: "
				"retr_remainder=%d, fr_remainder=%d\n", retr_remainder,
				fr_remainder);
	}
#endif
	if (retr_remainder<fr_remainder)
		return retr_remainder;
	else{
		/* hack to switch to the slow timer */
#ifdef TM_FAST_RETR_TIMER
		tl->flags&=~F_TIMER_FAST;
#endif
		return fr_remainder;
	}
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

#ifdef TM_DEL_UNREF
	/* stop cancel timers if any running */
	if ( is_invite(p_cell) ) cleanup_localcancel_timers( p_cell );
	/* remove the cell from the hash table */
	LOCK_HASH( p_cell->hash_index );
	remove_from_hash_table_unsafe(  p_cell );
	UNLOCK_HASH( p_cell->hash_index );
	p_cell->flags |= T_IN_AGONY;
	UNREF_FREE(p_cell);
	ret=0;
#else /* TM_DEL_UNREF */
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
	}else {
		/* stop cancel timers if any running */
		if ( is_invite(p_cell) ) cleanup_localcancel_timers( p_cell );
		/* remove the cell from the hash table */
		LOCK_HASH( p_cell->hash_index );
		remove_from_hash_table_unsafe(  p_cell );
		p_cell->flags |= T_IN_AGONY;
		/* delete (returns with UNLOCK-ed_HASH) */
		ret=delete_cell( p_cell, 1 /* unlock on return */ );
	}
#endif /* TM_DEL_UNREF */
	return ret;
}

